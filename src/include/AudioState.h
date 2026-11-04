/*
 * AudioState -- audio slot state and subsystem lifecycle.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_AUDIO_AUDIOSTATE_C_H
#define NW_GROUP_AUDIO_AUDIOSTATE_C_H

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
/*   ClearAllSlotEntries                                        audio/ClearAllSlotEntries.c */
/*   DispatchMciAudioReset                                      audio/DispatchMciAudioReset.c */
/*   InitializeAudioSubsystemThenDispatchPlaybackAndPendingSlotHook audio/InitializeAudioSubsystemThenDispatchPlaybackAndPendingSlotHook.c */
/*   TearDownWaveOutAndMidiAudioSubsystem                       audio/TearDownWaveOutAndMidiAudioSubsystem.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern uint32_t ClearAllSlotEntries (void);
extern void DispatchMciAudioReset (void);
extern void InitializeAudioSubsystemThenDispatchPlaybackAndPendingSlotHook (void);
extern void TearDownWaveOutAndMidiAudioSubsystem (void);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_AUDIO_AUDIOSTATE_C_H */
