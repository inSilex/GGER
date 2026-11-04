/*
 * WindowTables -- window-table stubs and tail handlers.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_GAME_WINDOWTABLES_C_H
#define NW_GROUP_GAME_WINDOWTABLES_C_H

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
/*   WindowTableStubs                                           game/WindowTableStubs.c */
/*   WindowTableTailHandlers                                    game/WindowTableTailHandlers.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern void __stdcall NwNoOpPoolHandler(uint32_t ignored);
extern void __stdcall NwNoOpOpcodeHandler(uint32_t ignored);
extern uint8_t *__stdcall NwClearRecordLowNibble(uint8_t *pRecord);
extern uint32_t NwSetLiveInputByteAndReturnOne (void);
extern uint32_t __stdcall NwStoreByteThroughWindowOffset(uint32_t offset, const uint8_t *source);
extern int8_t __stdcall NwPublishAudioSlotAndMix(int arg1, int arg2, int arg3, const uint8_t *record);
extern void NwBareRet0040dfd8 (void);
extern void NwBareRet0041009c (void);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_GAME_WINDOWTABLES_C_H */
