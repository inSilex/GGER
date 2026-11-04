/*
 * Hooks -- installed hooks, probe dispatch, and exception handoff.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_RUNTIME_HOOKS_C_H
#define NW_GROUP_RUNTIME_HOOKS_C_H

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
/*   CallGmwDispatch206                                         runtime/CallGmwDispatch206.c */
/*   CallHookIfInstalled                                        runtime/CallHookIfInstalled.c */
/*   CallRegisteredGlobalHookIfInstalled                        runtime/CallRegisteredGlobalHookIfInstalled.c */
/*   CallRegisteredGlobalHookWithTrue                           runtime/CallRegisteredGlobalHookWithTrue.c */
/*   DisableHookCallback                                        runtime/DisableHookCallback.c */
/*   DispatchExceptionOnGlobalScopeRecord                       runtime/DispatchExceptionOnGlobalScopeRecord.c */
/*   DispatchIndexedProbeEntry                                  runtime/DispatchIndexedProbeEntry.c */
/*   DispatchLockedProbeEntryIfArmed                            runtime/DispatchLockedProbeEntryIfArmed.c */
/*   ForwardStackArgToSlotHook                                  runtime/ForwardStackArgToSlotHook.c */
/*   HandleSlotHookWithArg                                      runtime/HandleSlotHookWithArg.c */
/*   InvokeSlotHookThunk                                        runtime/InvokeSlotHookThunk.c */
/*   NativeProbeReturnsTrue                                     runtime/NativeProbeReturnsTrue.c */
/*   NoOpStackCleanupStub                                       runtime/NoOpStackCleanupStub.c */
/*   ProcessSlotHookThunkArg                                    runtime/ProcessSlotHookThunkArg.c */
/*   RetryLockedProbeDispatch                                   runtime/RetryLockedProbeDispatch.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern uint32_t CallGmwDispatch206 (void);
extern uint32_t CallHookIfInstalled (void);
extern void CallRegisteredGlobalHookIfInstalled (void);
extern void CallRegisteredGlobalHookWithTrue (void);
extern void DisableHookCallback (void);
extern void DispatchExceptionOnGlobalScopeRecord (void);
extern int DispatchIndexedProbeEntry (int nIndex);
extern int DispatchLockedProbeEntryIfArmed (void);
extern uint32_t __stdcall ForwardStackArgToSlotHook(uint32_t dwArg);
extern uint32_t __stdcall HandleSlotHookWithArg(uint32_t dwArg1, uint32_t dwArg2, uint32_t dwArg3);
extern uint32_t InvokeSlotHookThunk (void);
extern int NativeProbeReturnsTrue (void);
extern void NoOpStackCleanupStub (uint32_t dwUnused);
extern uint32_t __stdcall ProcessSlotHookThunkArg(uint32_t dwArg);
extern uint8_t ProbeActionResetSubsystems (void);
extern uint8_t ProbeActionComposeFrame (void);
extern uint8_t ProbeActionReadPalette (void);
extern uint8_t ProbeActionUpdateSlot (void);
extern uint8_t ProbeActionReadBlock (void);
extern uint32_t RetryLockedProbeDispatch (uint32_t uEntry);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_RUNTIME_HOOKS_C_H */
