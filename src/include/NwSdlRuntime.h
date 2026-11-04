/*
 * NwSdlRuntime -- one SDL3 entry-point table shared by both sinks.
 *
 * SDL3 is linked into the executable as a static library (libSDL3.a), so the
 * build has no SDL3 DLL dependency and no import library.  Both the MIDI sink
 * (NwSdlMidi.c) and the PCM sink (NwSdlPcm.c) call through this one table, and
 * the audio subsystem is reference-counted so the two sinks can each acquire
 * and release it independently.
 */

#ifndef NW_SDLRUNTIME_H
#define NW_SDLRUNTIME_H

#include <stdint.h>
#include <stddef.h>
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NwSdlApi {
    _Bool  (*InitSubSystem)(SDL_InitFlags flags);
    void   (*QuitSubSystem)(SDL_InitFlags flags);
    const char *(*GetError)(void);
    void  *(*LoadFile)(const char *file, size_t *datasize);
    void   (*Free)(void *mem);
    SDL_AudioStream *(*OpenAudioDeviceStream)(SDL_AudioDeviceID devid,
                                              const SDL_AudioSpec *spec,
                                              SDL_AudioStreamCallback callback,
                                              void *userdata);
    void   (*DestroyAudioStream)(SDL_AudioStream *stream);
    _Bool  (*ResumeAudioStreamDevice)(SDL_AudioStream *stream);
    _Bool  (*PauseAudioStreamDevice)(SDL_AudioStream *stream);
    _Bool  (*PutAudioStreamData)(SDL_AudioStream *stream, const void *buf, int len);
    _Bool  (*LockAudioStream)(SDL_AudioStream *stream);
    _Bool  (*UnlockAudioStream)(SDL_AudioStream *stream);
    _Bool  (*SetAudioStreamGain)(SDL_AudioStream *stream, float gain);
    int    (*GetAudioStreamQueued)(SDL_AudioStream *stream);
    _Bool  (*ClearAudioStream)(SDL_AudioStream *stream);
} NwSdlApi;

/* Fill in every entry point in NwSdlApi.  Always 1 with the static link. */
int NwSdl_Load(void);

/* The populated table, or NULL before NwSdl_Load. */
NwSdlApi *NwSdl(void);

/* Reference-counted SDL_InitSubSystem(SDL_INIT_AUDIO).  1 on success. */
int  NwSdl_AcquireAudio(void);
void NwSdl_ReleaseAudio(void);

#ifdef __cplusplus
}
#endif

#endif /* NW_SDLRUNTIME_H */
