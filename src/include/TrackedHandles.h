/*
 * TrackedHandles -- Win32 tracked-handle open/close/seek/truncate helpers.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_IO_WIN32_TRACKEDHANDLES_C_H
#define NW_GROUP_IO_WIN32_TRACKEDHANDLES_C_H

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
/*   CloseTrackedHandleSlot                                     io/win32/CloseTrackedHandleSlot.c */
/*   IsTrackedHandleCharDevice                                  io/win32/IsTrackedHandleCharDevice.c */
/*   OpenTrackedHandleSlot                                      io/win32/OpenTrackedHandleSlot.c */
/*   SeekTrackedSlotHandleOffset                                io/win32/SeekTrackedSlotHandleOffset.c */
/*   TruncateTrackedSlotFileToSize                              io/win32/TruncateTrackedSlotFileToSize.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern int CloseTrackedHandleSlot (DWORD dwUnusedArg1, DWORD dwUnusedArg2, int nSlotIndex);
extern BOOL IsTrackedHandleCharDevice (int nSlotIndex);
extern int OpenTrackedHandleSlot (const char *pszFilename, DWORD dwAccessWord, DWORD dwModeBits70src, DWORD dwAttrInput);
extern DWORD SeekTrackedSlotHandleOffset (int nSlotIndex, LONG lDistanceToMove, DWORD dwMoveMethod);
extern int TruncateTrackedSlotFileToSize (int nSlotIndex, DWORD dwNewSize);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_IO_WIN32_TRACKEDHANDLES_C_H */
