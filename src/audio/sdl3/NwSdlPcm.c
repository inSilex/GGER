#include "nwarch.h"
/*
 * NwSdlPcm -- SDL3 sound-effect sink behind the existing waveOut names.
 *
 * Module: audio/sdl3
 * Interface: include/NwSdlPcm.h
 *
 * Unit:
 *   NwSdlPcm                                                   audio/sdl3/NwSdlPcm.c
 *
 * MixActiveAudioSlotsIntoWaveOutBuffer still mixes the eight slots into the
 * 0x4000-byte pool at nw_data_window+0x1ea8 and still selects buffer 0; the
 * only change is the sink underneath.  SDL_PutAudioStreamData copies the buffer
 * synchronously, so a fixed buffer index is safe and the old eight-header
 * rotation (a waveOut completion artefact) is not needed.
 *
 * The whole translation unit is placed in the pinned `.nwdiag` section.
 */

#ifdef NW_UNIT_NwSdlPcm

#include <windows.h>   /* HWND, PostMessageA, multimedia timer */
#include <mmsystem.h>  /* timeSetEvent, timeKillEvent */
#include <stdint.h>
#include <stdio.h>

#include "NwSdlPcm.h"
#include "NwSdlRuntime.h"

#define NW_DIAG __attribute__((section(".nwdiag")))

#if defined(NW_PORTABLE_PS5) || defined(NW_PS5_NATIVE_TITLE)
extern void platform_log_mark(const char *stage);
#define NW_PCM_BOOT(stage) platform_log_mark(stage)
#else
#define NW_PCM_BOOT(stage) ((void)0)
#endif


/* The slot mixer lives in the game's fixed code and calls back through the
 * notify hook; the SDL pump only decides when to run it. */
extern void MixActiveAudioSlotsIntoWaveOutBuffer(void);

/* The mixed pool the slots are blended into (nw_data_window + 0x1ea8). */
extern unsigned char g_abWaveOutPool[];

/* 11025 Hz, 8-bit unsigned, mono -- the format the waveOut ring used. */
#define NW_PCM_FREQ        11025
#define NW_PCM_CHANNELS    1
#define NW_PCM_BUF_BYTES   0x800
#define NW_PCM_QUEUE_HIGH  (8 * NW_PCM_BUF_BYTES)   /* the old eight-buffer ring */

/* 0x800 bytes at 11025 Hz is 185.76 ms.  The original waveOut ring asked the
 * window for another buffer through MM_WOM_DONE (0x3BD) whenever one finished.
 * The Win32 build keeps that message pump: a 20 ms timer checks the SDL queue
 * and posts 0x3BD to the game's own window procedure.  The SDL build owns no
 * WndProc (SDL does), and SDL's Win32 event pump drains the thread queue before
 * our frame-step pump can see a posted message, so the SDL build polls
 * NwPcmSink_Pump from the main loop and from the frame-step pump instead. */
#define NW_PCM_LOW_WATER   (NW_PCM_BUF_BYTES / 4)

#ifndef NW_PLATFORM_SDL
#define NW_PCM_PUMP_MS     20u
#define NW_PCM_DONE_MSG    0x03BDu
static uint32_t g_pumpTimer;

/* The main GMW_PLAYER window, from the fixed data window (offset 0x6404).
 * Kept as a raw offset so this sink does not depend on the game headers. */
extern unsigned char nw_data_window[];
#define NW_MAIN_HWND NW_WINDOW_PTR_REF(HWND, 0x6404)
#endif

static volatile int g_paused;

static SDL_AudioStream *g_pcm;
static int              g_ready;
static uint32_t         g_packedVolume = 0x7F7F7F7Fu;   /* 127/127, the game's default level */

/* Audit counters. */
static uint32_t         g_bytesSubmitted;
static uint32_t         g_queueClears;
static uint32_t         g_peakDeviation;   /* max |sample - 0x80| over submitted buffers */
static int              g_firstSubmitLogged;
static int              g_firstAudibleLogged;

#ifndef NW_PLATFORM_SDL
static void __stdcall NwPcmPumpTimerProc(UINT uTimerID, UINT uMsg,
                                         DWORD_PTR dwUser, DWORD_PTR dwParam1,
                                         DWORD_PTR dwParam2)
{
    NwSdlApi *sdl;
    HWND hwnd;

    (void)uTimerID; (void)uMsg; (void)dwUser;
    (void)dwParam1; (void)dwParam2;

    if (g_paused || g_pcm == 0)
        return;

    sdl = NwSdl();
    if (sdl == 0)
        return;

    /* The old ring mixed exactly when waveOut had consumed a buffer.  Keep a
     * small queue here and ask the main thread for one more 0x800 block only
     * when it is running low. */
    if ((int)sdl->GetAudioStreamQueued(g_pcm) < NW_PCM_LOW_WATER) {
        hwnd = NW_MAIN_HWND;
        if (hwnd != (HWND)0)
            (void)PostMessageA(hwnd, NW_PCM_DONE_MSG, 0u, 0u);
    }
}
#endif /* !NW_PLATFORM_SDL */

NW_DIAG int NwPcmSink_Init(void)
{
    NwSdlApi     *sdl;
    SDL_AudioSpec spec;

    if (g_ready)
        return 1;

    if (!NwSdl_Load()) {
        return 0;
    }
    if (!NwSdl_AcquireAudio()) {
        return 0;
    }
    sdl = NwSdl();

    spec.format   = SDL_AUDIO_U8;
    spec.channels = NW_PCM_CHANNELS;
    spec.freq     = NW_PCM_FREQ;

    /* No callback: the mixer pushes each mixed buffer with PutAudioStreamData. */
    g_pcm = sdl->OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, 0, 0);
    if (g_pcm == 0) {
        NwSdl_ReleaseAudio();
        return 0;
    }

    sdl->SetAudioStreamGain(g_pcm, (float)((g_packedVolume >> 8) & 0xFFu) / 255.0f);
    sdl->ResumeAudioStreamDevice(g_pcm);
    g_paused = 0;

#ifndef NW_PLATFORM_SDL
    g_pumpTimer = timeSetEvent(NW_PCM_PUMP_MS, 1u, NwPcmPumpTimerProc, 0u,
                               1u /* TIME_PERIODIC */);
#endif
    g_ready = 1;

    NW_PCM_BOOT("pcm_stream_ready");
    return 1;
}

NW_DIAG void NwPcmSink_Shutdown(void)
{
    NwSdlApi        *sdl = NwSdl();
    SDL_AudioStream *pcm;

    g_paused = 1;
#ifndef NW_PLATFORM_SDL
    if (g_pumpTimer != 0u) {
        (void)timeKillEvent(g_pumpTimer);
        g_pumpTimer = 0u;
    }
#endif

    /* Detach first: a timer callback that was already in flight must not see a
     * stream that is being destroyed. */
    pcm = g_pcm;
    g_pcm = 0;

    if (g_ready) {
        char stage[96];
        snprintf(stage, sizeof stage, "pcm_shutdown_b_%u_q_%u_peak_%u",
                 (unsigned)g_bytesSubmitted, (unsigned)g_queueClears,
                 (unsigned)g_peakDeviation);
        NW_PCM_BOOT(stage);
    }

    if (pcm != 0 && sdl != 0) {
        sdl->PauseAudioStreamDevice(pcm);
        sdl->DestroyAudioStream(pcm);
    }

    if (g_ready)
        NwSdl_ReleaseAudio();
    g_ready = 0;
}

NW_DIAG int NwPcmSink_IsReady(void)
{
    return g_ready;
}

/* Called by the 0x3BD (MM_WOM_DONE analogue) handler before mixing.  Keeping
 * the queue near one buffer prevents both underruns and the old eight-buffer
 * latency, and lets a mode-0 slot insertion be picked up on the next tick. */
NW_DIAG int NwPcmSink_ShouldMix(void)
{
    NwSdlApi *sdl;

    if (g_pcm == 0)
        return 1;                            /* no SDL stream: old path */
    if (g_paused)
        return 0;

    sdl = NwSdl();
    if (sdl == 0)
        return 1;
    return ((int)sdl->GetAudioStreamQueued(g_pcm) < NW_PCM_LOW_WATER) ? 1 : 0;
}

#ifdef NW_PLATFORM_SDL
/* Main-thread replacement for the 0x3BD tick.  SDL's Win32 event pump drains
 * the thread queue inside SDL_PollEvent, so a message posted by a timer would
 * be dispatched to SDL's WndProc and never reach ProcessMainWindowMessages.
 * Polling here keeps mixing tied to the same frame/input pump the Win32 build
 * uses, and the low-water threshold bounds how often the mixer actually runs. */
NW_DIAG void NwPcmSink_Pump(void)
{
    NwSdlApi *sdl;

    if (!g_ready || g_paused || g_pcm == 0)
        return;

    sdl = NwSdl();
    if (sdl == 0)
        return;

    if ((int)sdl->GetAudioStreamQueued(g_pcm) < NW_PCM_LOW_WATER)
        MixActiveAudioSlotsIntoWaveOutBuffer();
}
#endif /* NW_PLATFORM_SDL */

NW_DIAG int NwPcmSink_Submit(void)
{
    NwSdlApi *sdl = NwSdl();
    int queued;

    if (g_pcm == 0 || sdl == 0)
        return 0;

    /* Bound latency: the game can push buffers faster than 11025 B/s drains,
     * so drop the backlog once it reaches the old ring's capacity. */
    {
        queued = (int)sdl->GetAudioStreamQueued(g_pcm);
        if (queued > NW_PCM_QUEUE_HIGH) {
            sdl->ClearAudioStream(g_pcm);
            g_queueClears++;
        }
    }

    if (!sdl->PutAudioStreamData(g_pcm, g_abWaveOutPool, NW_PCM_BUF_BYTES))
        return 0;

    /* Audit: track how far the mixed buffer departs from the 0x80 silence
     * midpoint, so the gate can prove the SFX buffer carried real audio. */
    {
        const unsigned char *p = g_abWaveOutPool;
        int i;
        for (i = 0; i < NW_PCM_BUF_BYTES; i++) {
            int v = (int)p[i] - 128;
            unsigned int a = (unsigned int)(v < 0 ? -v : v);
            if (a > g_peakDeviation)
                g_peakDeviation = a;
        }
    }

    g_bytesSubmitted += NW_PCM_BUF_BYTES;

    /* Two one-shot console diagnostics.  A submit with peak 0 proves the game
     * mixer produced silence; a nonzero peak proves audible samples reached
     * the SDL stream, so any remaining silence is downstream in the device. */
    if (!g_firstSubmitLogged) {
        char stage[96];
        snprintf(stage, sizeof stage, "pcm_first_submit_peak_%u_q_%d",
                 (unsigned)g_peakDeviation, queued);
        NW_PCM_BOOT(stage);
        g_firstSubmitLogged = 1;
    }
    if (!g_firstAudibleLogged && g_peakDeviation != 0u) {
        char stage[96];
        snprintf(stage, sizeof stage, "pcm_first_audio_peak_%u",
                 (unsigned)g_peakDeviation);
        NW_PCM_BOOT(stage);
        g_firstAudibleLogged = 1;
    }
#ifdef NW_PLATFORM_SDL
    /* Per-submit audit tick: the SDL pump has no waveOut completion message,
     * so a gate can distinguish one synchronous click buffer from a stream
     * that keeps being fed by NwPcmSink_Pump. */
#endif
    return 1;
}

NW_DIAG int NwPcmSink_Reset(void)
{
    NwSdlApi *sdl = NwSdl();

    if (g_pcm == 0 || sdl == 0)
        return 0;
    return (int)sdl->ClearAudioStream(g_pcm);
}

NW_DIAG int NwPcmSink_Pause(void)
{
    NwSdlApi *sdl = NwSdl();

    if (g_pcm == 0 || sdl == 0)
        return 0;
    g_paused = 1;
    return (int)sdl->PauseAudioStreamDevice(g_pcm);
}

NW_DIAG int NwPcmSink_Resume(void)
{
    NwSdlApi *sdl = NwSdl();

    if (g_pcm == 0 || sdl == 0)
        return 0;
    g_paused = 0;
    return (int)sdl->ResumeAudioStreamDevice(g_pcm);
}

NW_DIAG int NwPcmSink_SetVolume(uint32_t packedVolume)
{
    NwSdlApi *sdl = NwSdl();
    uint32_t level = (packedVolume >> 8) & 0xFFu;

    g_packedVolume = packedVolume;
    if (g_pcm != 0 && sdl != 0)
        sdl->SetAudioStreamGain(g_pcm, (float)level / 255.0f);

    return 1;
}

NW_DIAG uint32_t NwPcmSink_GetVolume(void)
{
    return g_packedVolume;
}

#endif /* NW_UNIT_NwSdlPcm */
