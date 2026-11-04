/*
 * MciTransport -- MCI device open/close, command dispatch, and error reporting.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_AUDIO_WIN32_MCITRANSPORT_C_H
#define NW_GROUP_AUDIO_WIN32_MCITRANSPORT_C_H

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
/*   CloseMciCdAudioDeviceAndClearEnabledFlag                   audio/win32/CloseMciCdAudioDeviceAndClearEnabledFlag.c */
/*   DispatchMciCommandViaGlobalHandleAndShowError              audio/win32/DispatchMciCommandViaGlobalHandleAndShowError.c */
/*   DispatchMciPlaybackCommand                                 audio/win32/DispatchMciPlaybackCommand.c */
/*   DispatchMciSetFlagsAndPlay                                 audio/win32/DispatchMciSetFlagsAndPlay.c */
/*   InitializeAudioSubsystemByOsVersionAndOpenMciDevice        audio/win32/InitializeAudioSubsystemByOsVersionAndOpenMciDevice.c */
/*   OpenMciCdAudioDeviceAndArmEnabledFlag                      audio/win32/OpenMciCdAudioDeviceAndArmEnabledFlag.c */
/*   ShowMciErrorMessage                                        audio/win32/ShowMciErrorMessage.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern void CloseMciCdAudioDeviceAndClearEnabledFlag (void);
extern int __stdcall DispatchMciCommandViaGlobalHandleAndShowError(uint32_t param_1, uint32_t param_2, uint32_t param_3);
extern DWORD __stdcall DispatchMciPlaybackCommand(DWORD dwOpCode);
extern DWORD DispatchMciSetFlagsAndPlay (BYTE bFlag, DWORD dwInEaxImage);
extern BOOL __stdcall InitializeAudioSubsystemByOsVersionAndOpenMciDevice(BYTE *pbOutSlot5, BYTE *pbOutSlot4, DWORD dwUnusedArg3, BYTE bSlotCount);
extern DWORD OpenMciCdAudioDeviceAndArmEnabledFlag (void);
extern int __stdcall ShowMciErrorMessage(unsigned int uCode);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_AUDIO_WIN32_MCITRANSPORT_C_H */
