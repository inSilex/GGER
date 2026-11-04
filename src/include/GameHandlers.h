/*
 * GameHandlers -- native-call adapters and save/command handlers.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_GAME_GAMEHANDLERS_C_H
#define NW_GROUP_GAME_GAMEHANDLERS_C_H

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
/*   NativeCallAdapters                                         game/NativeCallAdapters.c */
/*   SaveAndCommandHandlers                                     game/SaveAndCommandHandlers.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern uint32_t __stdcall NwGridCtxSlotIndexEquals(uint32_t index);
extern uint32_t __stdcall NwGridCtxLayerIndexEquals(uint32_t value);
extern uint32_t __stdcall NwRunOpcodeStreamAtArgIndex(uint32_t index);
extern uint32_t __stdcall NwStringContainsLiveInput(const char *pText);
extern void __stdcall NwDispatchPendingAndGlobalException(uint32_t arg0);
extern void __stdcall NwLoadRecordsAndGlobalException(uint32_t recordIndex);
extern uint32_t __stdcall NwDispatchPendingRefSelField(uint32_t index);
extern uint32_t __stdcall NwSaveDispatchByMode(uint32_t mode, uint32_t index);
extern uint32_t __stdcall NwSelectGridRecordBase(uint32_t index);
extern uint32_t NwClearGridRecordBase (void);
extern uint32_t __stdcall NwDispatchWinHelp(uint32_t arg);
extern uint32_t __stdcall NwRngScaleByArg(uint32_t arg);
extern uint32_t __stdcall NwResetAndRunCommand(uint32_t commandLine);
extern uint32_t __stdcall NwInputFieldContainsArg(const char *arg);
extern uint32_t __stdcall NwSetCellIndexFromArg(uint32_t flag);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_GAME_GAMEHANDLERS_C_H */
