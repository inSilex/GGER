/*
 * InputRouting -- input record queues and grid-span dispatch.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_INPUT_INPUTROUTING_C_H
#define NW_GROUP_INPUT_INPUTROUTING_C_H

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
/*   StoreArgsAndProcessQueuedRecords                           input/StoreArgsAndProcessQueuedRecords.c */
/*   WalkGridSpanChainAndDispatchInputByte                      input/WalkGridSpanChainAndDispatchInputByte.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern uint32_t StoreArgsAndProcessQueuedRecords (uint32_t bPointerInsideClient, uint32_t bKeyCode, uint32_t bModifierSlot, uint32_t bClickAction, uint32_t dwAnchorY, uint32_t dwAnchorX, uint32_t dwCursorY, uint32_t dwCursorX, uint32_t dwSizeHighWord, uint32_t dwSizeLowWord);
extern int __stdcall WalkGridSpanChainAndDispatchInputByte(unsigned char bArg);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_INPUT_INPUTROUTING_C_H */
