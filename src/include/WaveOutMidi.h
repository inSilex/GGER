/*
 * WaveOutMidi -- waveOut buffers, MIDI output, and audio shutdown.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_AUDIO_WIN32_WAVEOUTMIDI_C_H
#define NW_GROUP_AUDIO_WIN32_WAVEOUTMIDI_C_H

#include <stdint.h>
#include <stddef.h>
#include <windows.h>

#ifndef NW_GROUP_COMPAT_TYPES
#define NW_GROUP_COMPAT_TYPES
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef unsigned int uint;
typedef unsigned char byte;
typedef uint32_t dword;
typedef void *HANDLE_T;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Units in this group: */
/*   FindWaveHeaderInUseSlotIndex                               audio/win32/FindWaveHeaderInUseSlotIndex.c */
/*   InitializeMidiOutAndTimerResolution                        audio/win32/InitializeMidiOutAndTimerResolution.c */
/*   OpenWaveOutAndPrepareHeaderRing                            audio/win32/OpenWaveOutAndPrepareHeaderRing.c */
/*   ResetMidiOutAndTimerEvent                                  audio/win32/ResetMidiOutAndTimerEvent.c */
/*   ShutdownAudioSubsystem                                     audio/win32/ShutdownAudioSubsystem.c */
/*   UnprepareAndCloseWaveOut                                   audio/win32/UnprepareAndCloseWaveOut.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern unsigned char FindWaveHeaderInUseSlotIndex (void);
extern BOOL InitializeMidiOutAndTimerResolution (void);
extern void ResetMidiOutAndTimerEvent (void);
extern void ShutdownAudioSubsystem (void);
extern void UnprepareAndCloseWaveOut (void);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_AUDIO_WIN32_WAVEOUTMIDI_C_H */
