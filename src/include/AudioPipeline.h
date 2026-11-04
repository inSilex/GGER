/*
 * AudioPipeline -- MIDI decoding, MCI playback requests, and notification plumbing.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_AUDIO_AUDIOPIPELINE_C_H
#define NW_GROUP_AUDIO_AUDIOPIPELINE_C_H

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
/*   DecodeMidiTrackVariableLengthQuantity                      audio/DecodeMidiTrackVariableLengthQuantity.c */
/*   InitializeAudioSubsystemAndUtHandlerTable                  audio/InitializeAudioSubsystemAndUtHandlerTable.c */
/*   InvokeUtNotifySlot8Thunk                                   audio/InvokeUtNotifySlot8Thunk.c */
/*   PlayMciWavesCommandIfDeviceArmed                           audio/PlayMciWavesCommandIfDeviceArmed.c */
/*   SendMciSetOnWithPointerToGlobalWord                        audio/SendMciSetOnWithPointerToGlobalWord.c */
/*   UpdateMciAudioStateAndPlay                                 audio/UpdateMciAudioStateAndPlay.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern unsigned int DecodeMidiTrackVariableLengthQuantity (void);
extern uint32_t InvokeUtNotifySlot8Thunk (void);
extern int __stdcall PlayMciWavesCommandIfDeviceArmed(DWORD_PTR param_1, DWORD_PTR param_2);
extern int SendMciSetOnWithPointerToGlobalWord (void);
extern int __stdcall UpdateMciAudioStateAndPlay(unsigned char bMciAudioMode);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_AUDIO_AUDIOPIPELINE_C_H */
