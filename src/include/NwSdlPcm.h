/*
 * NwSdlPcm -- SDL3 sound-effect sink.
 *
 * This is the cross-platform replacement for the waveOut ring.  The existing
 * mixer (MixActiveAudioSlotsIntoWaveOutBuffer) and the slot ABI are unchanged:
 * the mixer selects a pool buffer and calls the notify handler, which submits
 * that 2048-byte pool buffer to one U8 mono 11025 Hz SDL stream.
 *
 * There is no waveOut call and no import of winmm's waveOut*.  SDL3 is
 * runtime-loaded through NwSdlRuntime; if it is unavailable, NwPcmSink_Init
 * returns 0 and the caller keeps its existing "audio not armed" behaviour.
 */

#ifndef NW_SDLPCM_H
#define NW_SDLPCM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Open the SDL sound-effect stream.  1 on success, 0 on failure. */
int NwPcmSink_Init(void);

/* Stop and free the stream.  Safe when never initialised. */
void NwPcmSink_Shutdown(void);

/* 1 once the stream is running. */
int NwPcmSink_IsReady(void);

/* 1 when the 0x3BD completion tick should mix another block. */
int NwPcmSink_ShouldMix(void);

#ifdef NW_PLATFORM_SDL
/* Main-thread pump used instead of the Win32 0x3BD message: mix another block
 * when the SDL queue is low.  Safe to call every frame. */
void NwPcmSink_Pump(void);
#endif

/* Submit the mixed 2048-byte pool buffer.  1 on success. */
int NwPcmSink_Submit(void);

/* Drop queued output (the old waveOutReset behaviour).  1 on success. */
int NwPcmSink_Reset(void);

/* Pause / resume the device stream.  1 on success. */
int NwPcmSink_Pause(void);
int NwPcmSink_Resume(void);

/* Store the packed device-style volume and apply its high byte as gain. */
int NwPcmSink_SetVolume(uint32_t packedVolume);
uint32_t NwPcmSink_GetVolume(void);

#ifdef __cplusplus
}
#endif

#endif /* NW_SDLPCM_H */
