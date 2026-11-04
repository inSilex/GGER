/*
 * Slots -- slot tables, hook state, and thread-context helpers.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_RUNTIME_SLOTS_C_H
#define NW_GROUP_RUNTIME_SLOTS_C_H

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
/*   AcquireNextFreeSlot                                        runtime/AcquireNextFreeSlot.c */
/*   ClearTopSlotStatusAndRebuildRegionBounds                   runtime/ClearTopSlotStatusAndRebuildRegionBounds.c */
/*   CopyRecordsToSlotChain                                     runtime/CopyRecordsToSlotChain.c */
/*   DispatchPendingSlotHookForIndex                            runtime/DispatchPendingSlotHookForIndex.c */
/*   ReleaseSlotSemaphore                                       runtime/ReleaseSlotSemaphore.c */
/*   ResetHookTableToDefaults                                   runtime/ResetHookTableToDefaults.c */
/*   ResetPendingSlotHookState                                  runtime/ResetPendingSlotHookState.c */
/*   ResetSlotAndHookTablesToDefaults                           runtime/ResetSlotAndHookTablesToDefaults.c */
/*   ResetSlotTablesFormatApplyAndDispatchSettings              runtime/ResetSlotTablesFormatApplyAndDispatchSettings.c */
/*   ReturnConstantOne                                          runtime/ReturnConstantOne.c */
/*   ReturnTrueSlotHook                                         runtime/ReturnTrueSlotHook.c */
/*   SelectNextActiveSlot                                       runtime/SelectNextActiveSlot.c */
/*   SetPendingFlagFromSlotHookResult                           runtime/SetPendingFlagFromSlotHookResult.c */
/*   SetThreadContextValueAndClearFlag                          runtime/SetThreadContextValueAndClearFlag.c */
/*   SlotHookReturnOne                                          runtime/SlotHookReturnOne.c */
/*   SlotHookReturnOneAlt                                       runtime/SlotHookReturnOneAlt.c */
/*   SlotHookReturnVoid                                         runtime/SlotHookReturnVoid.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern uint8_t *AcquireNextFreeSlot (void);
extern void ResetArenaChainTopStatusAndBounds (void);
extern void __stdcall CopyRecordsToSlotChain(int index);
extern int __stdcall DispatchPendingSlotHookForIndex(uint32_t dwSlotIndex);
extern void ReleaseSlotSemaphore (uint32_t uiSlotIndex);
extern uint32_t ResetHookTableToDefaults (void);
extern void ResetPendingSlotHookState (void);
extern void ResetSlotAndHookTablesToDefaults (void);
extern int ResetSlotTablesFormatApplyAndDispatchSettings (uint32_t param_1);
extern uint8_t ReturnConstantOne (void);
extern int ReturnTrueSlotHook (void);
extern void __stdcall SelectNextActiveSlot(uint8_t *pOldSlot);
extern uint32_t __stdcall SetPendingFlagFromSlotHookResult(uint32_t dwCallerArg);
extern void __fastcall SetThreadContextValueAndClearFlag(DWORD dwUnusedParam1, DWORD dwUnusedParam2, DWORD dwValue);
extern uint8_t SlotHookReturnOne (int dwUnusedOperand);
extern uint8_t SlotHookReturnOneAlt (int dwUnusedOperand);
extern void SlotHookReturnVoid (void);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_RUNTIME_SLOTS_C_H */
