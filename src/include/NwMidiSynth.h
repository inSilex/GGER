/*
 * NwMidiSynth -- static TinySoundFont wrapper for cross-platform MIDI.
 *
 * The synth is compiled into the game (no DLL) and renders general MIDI
 * short messages to interleaved S16 stereo PCM.  The bank is supplied as a
 * memory buffer by the caller; TSF copies the samples it needs, so the
 * caller may free the buffer after NwSynth_Create returns.
 *
 * This module is portable C.  It has no SDL or Win32 dependency.
 */

#ifndef NW_MIDISYNTH_H
#define NW_MIDISYNTH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NwSynth NwSynth;

/* Create a synth from an in-memory SF2 bank.  Returns NULL on failure. */
NwSynth *NwSynth_Create(const void *sf2_data, int sf2_size, int sample_rate);

/* Free a synth.  NULL is ignored. */
void NwSynth_Destroy(NwSynth *synth);

/* Apply one packed MIDI short message (status | d1<<8 | d2<<16). */
void NwSynth_ShortMsg(NwSynth *synth, uint32_t msg);

/* All notes off, sustain off, pitch wheel centred; program/volume survive. */
void NwSynth_Reset(NwSynth *synth);

/* Render `frames` interleaved S16 stereo frames.  Silences the buffer when
 * the synth is NULL, so callers can render unconditionally. */
void NwSynth_Render(NwSynth *synth, int16_t *out, int frames);

/* Number of presets in the loaded bank, or 0 when no synth is loaded. */
int NwSynth_PresetCount(const NwSynth *synth);

#ifdef __cplusplus
}
#endif

#endif /* NW_MIDISYNTH_H */
