/*
 * NwSdlMidi -- SDL3 audio stream feeding the static TinySoundFont synth.
 *
 * This is the cross-platform MIDI output layer.  SDL3 is linked statically,
 * so it opens one S16 stereo playback stream, loads the vendored SoundFont
 * from the current directory (`NW_SF2` overrides the path), and renders the
 * synth into the stream from SDL's audio callback.  General MIDI short messages are
 * applied under the stream lock so the sequencer thread and the audio thread
 * never race on synth state.
 *
 * There is no WinMM MIDI path and no synth DLL: this replaces midiOut* on
 * every platform.  If SDL3 or the bank cannot be loaded, MIDI is simply
 * unavailable (NwMidiSink_Init returns 0); the caller keeps its existing
 * "audio not armed" behaviour.
 */

#ifndef NW_SDLMIDI_H
#define NW_SDLMIDI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Load SDL3, load the bank, open the music stream.  1 on success, 0 on
 * failure; on failure nothing is left allocated. */
int NwMidiSink_Init(void);

/* Stop and free everything taken by NwMidiSink_Init.  Safe to call when the
 * sink was never initialised. */
void NwMidiSink_Shutdown(void);

/* 1 once the stream is running. */
int NwMidiSink_IsReady(void);

/* Apply one packed MIDI short message (status | d1<<8 | d2<<16). */
void NwMidiSink_ShortMsg(uint32_t msg);

/* All notes off / sustain off / pitch-wheel centre, keeping program and
 * volume.  Used by the MIDI reset and by a zero volume level. */
void NwMidiSink_Reset(void);

/* Store the packed device-style MIDI volume and apply its high byte as the
 * stream gain.  Always succeeds (there is no device to reject it). */
int NwMidiSink_SetVolume(uint32_t packedVolume);

/* The stored packed volume, in the same layout UtNotifyReadMidiVolume
 * decodes (both 16-bit halves carry the level in their high byte). */
uint32_t NwMidiSink_GetVolume(void);

#ifdef __cplusplus
}
#endif

#endif /* NW_SDLMIDI_H */
