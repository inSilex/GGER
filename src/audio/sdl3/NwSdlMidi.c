/*
 * NwSdlMidi -- SDL3 music stream feeding the static TinySoundFont synth.
 *
 * Module: audio/sdl3
 * Interface: include/NwSdlMidi.h
 *
 * Unit:
 *   NwSdlMidi                                                   audio/sdl3/NwSdlMidi.c
 *
 * SDL3 is linked statically; NwSdlRuntime fills one table with the real entry
 * points (no SDL3.dll, no import library, no SDL3_mixer).  One S16 stereo
 * 44100 Hz stream renders the statically linked TinySoundFont synth (see
 * audio/NwMidiSynth.c).  The whole translation unit is placed in the pinned
 * `.nwdiag` section.
 *
 * Threading: SDL runs the get callback on its audio thread.  The MIDI
 * sequencer runs on the winmm timer thread and must not touch synth state
 * while the callback renders, so ShortMsg/Reset take the stream lock.
 */

#ifdef NW_UNIT_NwSdlMidi

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <SDL3/SDL_filesystem.h>
#if defined(NW_PLATFORM_SDL)
#include <windows.h>
#endif

#include "NwMidiSynth.h"
#include "NwSdlMidi.h"
#include "NwSdlRuntime.h"

#define NW_DIAG __attribute__((section(".nwdiag")))

#if defined(NW_PORTABLE_PS5) || defined(NW_PS5_NATIVE_TITLE)
extern void platform_log_mark(const char *stage);
#define NW_MIDI_LOG(stage) platform_log_mark(stage)
#else
#define NW_MIDI_LOG(stage) ((void)0)
#endif

/* The native title does not share the payload's working directory.  The app
 * layer knows where overlay.nw (and therefore gm.sf2) was deployed; keep that
 * path here so soundfont lookup does not depend on SDL_GetBasePath or cwd. */
static char g_soundfontPath[1024];

NW_DIAG void NwMidiSink_SetSoundfontPath(const char *path)
{
    if (path == NULL) {
        g_soundfontPath[0] = '\0';
        return;
    }
    strncpy(g_soundfontPath, path, sizeof g_soundfontPath - 1);
    g_soundfontPath[sizeof g_soundfontPath - 1] = '\0';
}

/* Audio diagnostics are disabled in the lean build. */

/* Music stream format: CD-rate S16 stereo, the native output of the synth. */
#define NW_MUSIC_FREQ         44100
#define NW_MUSIC_CHANNELS     2
#define NW_MUSIC_CHUNK_FRAMES 2048

static NwSynth         *g_synth;
static SDL_AudioStream *g_music;
static int16_t         *g_renderBuf;
static int              g_renderFrames;
static int              g_ready;
static uint32_t         g_packedVolume = 0x7F7F7F7Fu;   /* 127/127, the game's default level */

/* Audit counters, summed on the audio thread and logged at shutdown. */
static uint32_t         g_framesRendered;
static uint32_t         g_peakAbs;

/* ---------------------------------------------------------------------------
 * Music stream callback: render synth frames and hand them to SDL.
 * ------------------------------------------------------------------------- */
NW_DIAG static void SDLCALL NwMusicStreamGetCallback(void *userdata,
                                                     SDL_AudioStream *stream,
                                                     int additionalAmount,
                                                     int totalAmount)
{
    NwSdlApi *sdl = NwSdl();
    int frames;
    (void)userdata;
    (void)totalAmount;

    if (additionalAmount <= 0 || sdl == 0)
        return;

    frames = additionalAmount / (NW_MUSIC_CHANNELS * (int)sizeof(int16_t));
    while (frames > 0) {
        int chunk = (frames > g_renderFrames) ? g_renderFrames : frames;
        int i;

        NwSynth_Render(g_synth, g_renderBuf, chunk);

        g_framesRendered += (uint32_t)chunk;
        for (i = 0; i < chunk * NW_MUSIC_CHANNELS; i++) {
            int v = g_renderBuf[i];
            unsigned int a = (unsigned int)(v < 0 ? -v : v);
            if (a > g_peakAbs)
                g_peakAbs = a;
        }

        if (sdl->PutAudioStreamData(stream, g_renderBuf,
                                    chunk * NW_MUSIC_CHANNELS * (int)sizeof(int16_t)) == 0)
            break;
        frames -= chunk;
    }
}

#if defined(NW_PLATFORM_SDL)
/* SDL's file APIs and the game's Win32 compatibility layer do not necessarily
 * resolve the console's app/data mounts the same way.  The overlay is read
 * through the Win32 layer, so use it as a second chance for the soundfont
 * before giving up on music.  The buffer is allocated with SDL_malloc because
 * NwMidiSink_Init releases every bank through the SDL function table. */
static void *NwTrySoundfontWin32(const char *path, size_t *pSize)
{
    HANDLE h;
    DWORD  cb;
    DWORD  got = 0;
    void  *bank;

    *pSize = 0;
    if (path == NULL || path[0] == '\0')
        return 0;
    h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    cb = GetFileSize(h, NULL);
    if (cb == INVALID_FILE_SIZE || cb == 0) {
        CloseHandle(h);
        return 0;
    }
    bank = SDL_malloc(cb);
    if (bank == NULL) {
        CloseHandle(h);
        return 0;
    }
    if (!ReadFile(h, bank, cb, &got, NULL) || got != cb) {
        SDL_free(bank);
        CloseHandle(h);
        return 0;
    }
    CloseHandle(h);
    *pSize = cb;
    return bank;
}
#endif

#if defined(NW_PORTABLE_PS5) || defined(NW_PS5_NATIVE_TITLE)
/* A missing bank is far easier to diagnose when the log says whether the
 * deployed directory even contains one.  Keep this to startup: it is three
 * short directory reads, not a per-frame path. */
static void NwLogSoundfontDirectory(const char *dir)
{
    WIN32_FIND_DATAA fd;
    char pattern[320];
    char stage[384];
    HANDLE h;
    int n;

    n = snprintf(pattern, sizeof pattern, "%s/*.sf2", dir);
    if (n <= 0 || (size_t)n >= sizeof pattern)
        return;
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        snprintf(stage, sizeof stage, "sf2_dir_none_%s", dir);
        NW_MIDI_LOG(stage);
        return;
    }
    do {
        snprintf(stage, sizeof stage, "sf2_dir_%s_%s", dir, fd.cFileName);
        NW_MIDI_LOG(stage);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}
#endif

static void *NwTrySoundfont(NwSdlApi *sdl, const char *path, size_t *pSize)
{
    char  stage[256];
    void *bank;
    int   savedErrno = 0;

    *pSize = 0;
    if (path == NULL || path[0] == '\0')
        return 0;
    errno = 0;
    bank = sdl->LoadFile(path, pSize);
    if (bank != 0 && *pSize != 0) {
        snprintf(stage, sizeof stage, "midi_bank_ok_%u_%s",
                 (unsigned)*pSize, path);
        NW_MIDI_LOG(stage);
        return bank;
    }
    if (bank != 0) {
        sdl->Free(bank);
        *pSize = 0;
    }
#if defined(NW_PLATFORM_SDL)
    bank = NwTrySoundfontWin32(path, pSize);
    if (bank != 0) {
        snprintf(stage, sizeof stage, "midi_bank_win32_%u_%s",
                 (unsigned)*pSize, path);
        NW_MIDI_LOG(stage);
        return bank;
    }
#endif
    savedErrno = errno;
    snprintf(stage, sizeof stage, "midi_bank_miss_e%d_%s", savedErrno, path);
    NW_MIDI_LOG(stage);
    return 0;
}

/* The native PS5 title has no useful working directory, so "gm.sf2" alone
 * only works when the loader happens to start in /app0.  Prefer the bank
 * beside the deployed overlay (set by the app layer), then the executable,
 * the console's writable/data locations, and the bare relative name. */
static void *NwLoadSoundfont(NwSdlApi *sdl, size_t *pSize)
{
    static const char *const fallbacks[] = {
        "/app0/gm.sf2",
        "/data/gm.sf2",
        "/download0/gm.sf2",
        "Z:\\app0\\gm.sf2",
        "Z:\\download0\\gm.sf2",
        "gm.sf2"
    };
    const char *env = getenv("NW_SF2");
    const char *base;
    char path[1024];
    void *bank;
    size_t i;

    bank = NwTrySoundfont(sdl, g_soundfontPath, pSize);
    if (bank != 0)
        return bank;

    if (env != 0 && env[0] != '\0') {
        bank = NwTrySoundfont(sdl, env, pSize);
        if (bank != 0)
            return bank;
    }

    base = SDL_GetBasePath();
    if (base != 0) {
        if (snprintf(path, sizeof path, "%sgm.sf2", base) > 0) {
            bank = NwTrySoundfont(sdl, path, pSize);
            if (bank != 0)
                return bank;
        }
        /* SDL_GetBasePath returns a cached string owned by SDL; do not free
         * it. */
    }

    for (i = 0; i < sizeof(fallbacks) / sizeof(fallbacks[0]); i++) {
        bank = NwTrySoundfont(sdl, fallbacks[i], pSize);
        if (bank != 0)
            return bank;
    }
    NW_MIDI_LOG("midi_bank_none");
    return 0;
}

/* ---------------------------------------------------------------------------
 * Public sink
 * ------------------------------------------------------------------------- */
NW_DIAG int NwMidiSink_Init(void)
{
    NwSdlApi     *sdl;
    SDL_AudioSpec spec;
    NwSynth      *synth;
    void         *bank;
    size_t        bankSize = 0;
    char          stage[64];

    if (g_ready)
        return 1;

    if (!NwSdl_Load()) {
        NW_MIDI_LOG("midi_no_sdl");
        return 0;
    }
    if (!NwSdl_AcquireAudio()) {
        NW_MIDI_LOG("midi_no_audio");
        return 0;
    }
    sdl = NwSdl();

#if defined(NW_PORTABLE_PS5) || defined(NW_PS5_NATIVE_TITLE)
    NwLogSoundfontDirectory("/app0");
    NwLogSoundfontDirectory("/data");
    NwLogSoundfontDirectory("/download0");
#endif

    /* NW_SF2 lets a player audition a different General MIDI bank without a
     * rebuild.  The default is the vendored conversion of the Windows gm.dls
     * (the Microsoft GS Wavetable Synth sample set); ChaosBank.sf2 remains a
     * smaller, freely licensed alternative. */
    bank = NwLoadSoundfont(sdl, &bankSize);
    if (bank == 0 || bankSize == 0) {
        NW_MIDI_LOG("midi_no_bank");
        NwSdl_ReleaseAudio();
        return 0;
    }

    synth = NwSynth_Create(bank, (int)bankSize, NW_MUSIC_FREQ);
    sdl->Free(bank);                         /* TSF copied the sample data */
    if (synth == 0) {
        NW_MIDI_LOG("midi_bad_synth");
        NwSdl_ReleaseAudio();
        return 0;
    }

    g_renderBuf = (int16_t *)malloc((size_t)NW_MUSIC_CHUNK_FRAMES *
                                    NW_MUSIC_CHANNELS * sizeof(int16_t));
    if (g_renderBuf == 0) {
        NW_MIDI_LOG("midi_no_buf");
        NwSynth_Destroy(synth);
        NwSdl_ReleaseAudio();
        return 0;
    }

    g_renderFrames = NW_MUSIC_CHUNK_FRAMES;
    g_synth        = synth;

    spec.format   = SDL_AUDIO_S16;
    spec.channels = NW_MUSIC_CHANNELS;
    spec.freq     = NW_MUSIC_FREQ;

    g_music = sdl->OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec,
                                         NwMusicStreamGetCallback, 0);
    if (g_music == 0) {
        NW_MIDI_LOG("midi_no_stream");
        free(g_renderBuf);  g_renderBuf = 0; g_renderFrames = 0;
        NwSynth_Destroy(synth); g_synth = 0;
        NwSdl_ReleaseAudio();
        return 0;
    }

    sdl->SetAudioStreamGain(g_music, 1.0f);
    sdl->ResumeAudioStreamDevice(g_music);
    g_ready = 1;

    snprintf(stage, sizeof stage, "midi_ready_%u",
             (unsigned)NwSynth_PresetCount(synth));
    NW_MIDI_LOG(stage);

    /* kind 310 = synth ready; payload = number of bank presets. */
    return 1;
}

NW_DIAG void NwMidiSink_Shutdown(void)
{
    NwSdlApi *sdl = NwSdl();


    if (g_music != 0 && sdl != 0) {
        sdl->PauseAudioStreamDevice(g_music);
        sdl->DestroyAudioStream(g_music);
        g_music = 0;
    }
    if (g_synth != 0) {
        NwSynth_Destroy(g_synth);
        g_synth = 0;
    }
    if (g_renderBuf != 0) {
        free(g_renderBuf);
        g_renderBuf = 0;
    }
    g_renderFrames = 0;

    if (g_ready)
        NwSdl_ReleaseAudio();
    g_ready = 0;
}

NW_DIAG int NwMidiSink_IsReady(void)
{
    return g_ready;
}

NW_DIAG void NwMidiSink_ShortMsg(uint32_t msg)
{
    NwSdlApi *sdl = NwSdl();
    int locked;

    if (g_synth == 0 || sdl == 0)
        return;

    locked = (g_music != 0) ? (int)sdl->LockAudioStream(g_music) : 0;
    NwSynth_ShortMsg(g_synth, msg);
    if (locked)
        sdl->UnlockAudioStream(g_music);
}

NW_DIAG void NwMidiSink_Reset(void)
{
    NwSdlApi *sdl = NwSdl();
    int locked;

    if (g_synth == 0 || sdl == 0)
        return;

    locked = (g_music != 0) ? (int)sdl->LockAudioStream(g_music) : 0;
    NwSynth_Reset(g_synth);
    if (locked)
        sdl->UnlockAudioStream(g_music);
}

NW_DIAG int NwMidiSink_SetVolume(uint32_t packedVolume)
{
    NwSdlApi *sdl = NwSdl();
    uint32_t level = (packedVolume >> 8) & 0xFFu;

    g_packedVolume = packedVolume;
    if (g_music != 0 && sdl != 0)
        sdl->SetAudioStreamGain(g_music, (float)level / 255.0f);

    return 1;
}

NW_DIAG uint32_t NwMidiSink_GetVolume(void)
{
    return g_packedVolume;
}

#endif /* NW_UNIT_NwSdlMidi */
