/*
 * AppLifecycle -- process startup, global tables, subsystem reset, and orderly teardown.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_APP_APPLIFECYCLE_C_H
#define NW_GROUP_APP_APPLIFECYCLE_C_H

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
/*   ClearWritableDataSpanOnce                                  app/ClearWritableDataSpanOnce.c */
/*   CrtInitApiThunks                                           app/CrtInitApiThunks.c */
/*   DispatchExitHooksAndClearShutdownFlags                     app/DispatchExitHooksAndClearShutdownFlags.c */
/*   DispatchPendingCommandLoop                                 app/DispatchPendingCommandLoop.c */
/*   DispatchSubsystemInitByOpcode                              app/DispatchSubsystemInitByOpcode.c */
/*   DrainHandlerTableAndCallExitHooks                          app/DrainHandlerTableAndCallExitHooks.c */
/*   InitializeAppSubsystemsFromSavedPath                       app/InitializeAppSubsystemsFromSavedPath.c */
/*   InitializeCrtRuntimeAndCtors                               app/InitializeCrtRuntimeAndCtors.c */
/*   InitializeHookAndFlagGlobals                               app/InitializeHookAndFlagGlobals.c */
/*   ParseCommandLineIntoArgcArgv                               app/ParseCommandLineIntoArgcArgv.c */
/*   ProcessQueuedTaskRecords                                   app/ProcessQueuedTaskRecords.c */
/*   ResetSubsystemsAndFreeGlobalSlots                          app/ResetSubsystemsAndFreeGlobalSlots.c */
/*   ResetSubsystemsAndLatchStateFlag                           app/ResetSubsystemsAndLatchStateFlag.c */
/*   RunInitTablesAndStoreBlockSizeInWords                      app/RunInitTablesAndStoreBlockSizeInWords.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern void ClearWritableDataSpanOnce (void);
extern uint32_t ThunkGetCurrentThreadId (void);
extern char *ThunkGetEnvironmentStrings (void);
extern uint32_t ThunkGetVersion (void);
extern uint32_t DispatchExitHooksAndClearShutdownFlags (uint32_t exit_code);
extern uint32_t __stdcall DispatchPendingCommandLoop(uint32_t arg);
extern uint32_t DispatchSubsystemInitByOpcode (void);
extern void __stdcall DrainHandlerTableAndCallExitHooks(void);
extern uint32_t InitializeAppSubsystemsFromSavedPath (const char *pszPath);
extern uint32_t InitializeCrtRuntimeAndCtors (void);
extern void InitializeHookAndFlagGlobals (void);
extern void ParseCommandLineIntoArgcArgv (void);
extern uint8_t ProcessQueuedTaskRecords (void);
extern int ResetSubsystemsAndFreeGlobalSlots (void);
extern void ResetSubsystemsAndLatchStateFlag (void);
extern uint32_t RunInitTablesAndStoreBlockSizeInWords (void);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_APP_APPLIFECYCLE_C_H */
