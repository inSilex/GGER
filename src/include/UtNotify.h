/*
 * UtNotify -- UT driver callbacks and MIDI/wave volume notification.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_AUDIO_WIN32_UTNOTIFY_C_H
#define NW_GROUP_AUDIO_WIN32_UTNOTIFY_C_H

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
/*   DispatchMidiSequencerEventsWithRunningStatus               audio/win32/DispatchMidiSequencerEventsWithRunningStatus.c */
/*   InitializeUtDriverAndHandlerTable                          audio/win32/InitializeUtDriverAndHandlerTable.c */
/*   ShutdownUtDriverAndFreeResources                           audio/win32/ShutdownUtDriverAndFreeResources.c */
/*   UtNotifyAudioHandlers                                      audio/win32/UtNotifyAudioHandlers.c */
/*   UtNotifyReadMidiVolume                                     audio/win32/UtNotifyReadMidiVolume.c */
/*   UtNotifyReadWaveOutVolume                                  audio/win32/UtNotifyReadWaveOutVolume.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern void DispatchMidiSequencerEventsWithRunningStatus (void);
extern BOOL __stdcall InitializeUtDriverAndHandlerTable(void *pUtContext);
extern void ShutdownUtDriverAndFreeResources (void);
extern uint8_t UtNotifyUnprepareCurrentWaveHeader (void);
extern uint8_t UtNotifyWaveOutCommand2 (void);
extern uint8_t UtNotifyWaveOutCommand7 (void);
extern uint8_t UtNotifyWaveOutCommand8 (void);
extern uint8_t __stdcall UtNotifySetWaveOutVolume(uint32_t dwLevel);
extern uint8_t UtNotifyResetMidi (void);
extern uint8_t __stdcall UtNotifySetMidiVolume(uint32_t dwLevel);
extern uint8_t __stdcall UtNotifyStartMidiSequence(uint32_t dwTrackBlock);
extern void __stdcall MidiSequencerTimerProc(uint32_t uTimerID, uint32_t uMsg, uint32_t dwUser, uint32_t dwParam1, uint32_t dwParam2);
extern uint8_t UtNotifyReadMidiVolume (void);
extern uint8_t UtNotifyReadWaveOutVolume (void);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_AUDIO_WIN32_UTNOTIFY_C_H */
