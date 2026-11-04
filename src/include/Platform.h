/*
 * Platform -- Win32 thread, semaphore, SEH, and tracked-allocation support.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_RUNTIME_WIN32_PLATFORM_C_H
#define NW_GROUP_RUNTIME_WIN32_PLATFORM_C_H

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
/*   AllocateTrackedBlock                                       runtime/win32/AllocateTrackedBlock.c */
/*   ApiSlotPointers                                            runtime/win32/ApiSlotPointers.c */
/*   CreateSlotSemaphore                                        runtime/win32/CreateSlotSemaphore.c */
/*   DispatchPendingSignalHandler                               runtime/win32/DispatchPendingSignalHandler.c */
/*   FindCurrentThreadContext                                   runtime/win32/FindCurrentThreadContext.c */
/*   NwEntrySehShim                                             runtime/win32/NwEntrySehShim.c */
/*   TranslateLastErrorToErrno                                  runtime/win32/TranslateLastErrorToErrno.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern void *AllocateTrackedBlock (uint32_t requested_size);
extern void CreateSlotSemaphore (uint32_t uiSlotIndex);
extern void *FindCurrentThreadContext (DWORD dwUnusedParam1, DWORD dwUnusedParam2);
extern DWORD TranslateLastErrorToErrno (DWORD dwUnusedArg1, DWORD dwUnusedArg2);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_RUNTIME_WIN32_PLATFORM_C_H */
