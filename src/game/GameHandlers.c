#include "nwarch.h"
/*
 * GameHandlers -- native-call adapters and save/command handlers.
 *
 * Module: game
 * Interface: include/GameHandlers.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   NativeCallAdapters                                         game/NativeCallAdapters.c
 *   SaveAndCommandHandlers                                     game/SaveAndCommandHandlers.c
 */

/* ==========================================================================
 * Unit: NativeCallAdapters
 * Source: game/NativeCallAdapters.c
 * ========================================================================== */
#ifdef NW_UNIT_NativeCallAdapters
/*
 * Native Call Adapters.
 *
 * Module: game
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#game-nativecalladapters-c
 */

#include <stdint.h>
#include "nwglobals.h"

#define NW_NATIVE_ADAPTER __attribute__((section(".nwdiag")))

#define NW8(off)  (*(uint8_t  *)(intptr_t)(nw_data_window + (off)))
#define NWP32(off) (*(uint32_t *)(intptr_t)(nw_data_window + (off)))
#define NWPP(off) NW_WINDOW_PTR_REF(uint8_t, (off))

extern uint8_t * __stdcall FindPredecessorDescriptorPointerByBackwardScan(
    const uint8_t *pKeyRecord);
extern uint32_t __stdcall IsRemappedIndexArgEqual(uint32_t dwIndexArg,
                                                  uint32_t dwValueArg);
extern uint32_t __stdcall ExecuteOpcodeStreamPreservingGlobals(
    const uint32_t *pOpcodeStream);
extern void DispatchPendingCommandLoop(uint32_t arg0);
extern void DispatchExceptionOnGlobalScopeRecord(void);
extern int __stdcall LoadRecordBlocksIntoTileCache(uint8_t flagA,
                                                   uint32_t recordIndex);
extern int8_t __stdcall NwPublishAudioSlotAndMix(int arg1, int arg2, int arg3,
                                                 const void *arg4);

NW_NATIVE_ADAPTER uint32_t __stdcall NwGridCtxSlotIndexEquals(uint32_t index)
{
    uint16_t wIndex = g_pCurGridCtx->wCurXIdx;

    if (wIndex == 0xffffu) {
        return 0u;
    }

    {
        uint8_t *pKey = g_pGridSpanChainTop - ((uint32_t)wIndex << 4);
        uint8_t *pPred = FindPredecessorDescriptorPointerByBackwardScan(pKey);
        int32_t delta = (int32_t)(uint32_t)((uintptr_t)pPred -
                                            (uintptr_t)NWPP(0x6300));
        return (uint32_t)((delta / 8) == (int32_t)index);
    }
}

NW_NATIVE_ADAPTER uint32_t __stdcall NwGridCtxLayerIndexEquals(uint32_t value)
{
    uint32_t layer = (uint32_t)g_pCurGridCtx->bLayer & 0x4fu;

    if ((layer & 0x40u) == 0u) {
        return 0u;
    }
    if ((layer & 0x08u) != (value & 0x08u)) {
        return 0u;
    }
    return IsRemappedIndexArgEqual(value & 0x07u, layer & 0x07u);
}

NW_NATIVE_ADAPTER uint32_t __stdcall NwRunOpcodeStreamAtArgIndex(uint32_t index)
{
    uint8_t *pEntry = NWPP(0x6324) + ((uint32_t)index * 8u);
    return ExecuteOpcodeStreamPreservingGlobals((const uint32_t *)pEntry);
}

NW_NATIVE_ADAPTER uint32_t __stdcall NwStringContainsLiveInput(const char *pText)
{
    while (*pText != '\0') {
        if ((uint8_t)*pText == g_bLiveInputByte) {
            return 1u;
        }
        ++pText;
    }
    return 0u;
}

NW_NATIVE_ADAPTER void __stdcall NwDispatchPendingAndGlobalException(uint32_t arg0)
{
    DispatchPendingCommandLoop(arg0);
    DispatchExceptionOnGlobalScopeRecord();
}

NW_NATIVE_ADAPTER void __stdcall NwLoadRecordsAndGlobalException(uint32_t recordIndex)
{
    LoadRecordBlocksIntoTileCache(0u, recordIndex);
    DispatchExceptionOnGlobalScopeRecord();
}

NW_NATIVE_ADAPTER uint32_t __stdcall NwDispatchPendingRefSelField(uint32_t index)
{
    if (g_bDispatchAllowedFlag == 0u && NW8(0x6340) != 0u) {
        uint8_t *pEntry = NWPP(0x62fc) + ((uint32_t)index * 8u);
        const uint32_t guestRecord = *(const uint32_t *)(const void *)(pEntry + 4);
        NwPublishAudioSlotAndMix(1, 0, -1,
                                 (const void *)(uintptr_t)guestRecord);
    }
    return 1u;
}
#endif /* NW_UNIT_NativeCallAdapters */

/* ==========================================================================
 * Unit: SaveAndCommandHandlers
 * Source: game/SaveAndCommandHandlers.c
 * ========================================================================== */
#ifdef NW_UNIT_SaveAndCommandHandlers
/*
 * Save And Command Handlers.
 *
 * Module: game
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#game-saveandcommandhandlers-c
 */

#include <stdint.h>
#include <windows.h>
#include "nwglobals.h"

#define NW_SAVE_CMD __attribute__((section(".nwdiag")))

extern int8_t __stdcall DeleteNetWalkSaveVariantByIndex(int nVariantNumber);
extern int8_t __stdcall WriteSaveFileBuffersAndLoadTileCache(int param_1);
extern int8_t __stdcall InitializeDispatchRecords(int param_1);
extern void ResetSlotAndHookTablesToDefaults(void);
extern int __stdcall RunCommandLineAndWait(int commandLine)
    __asm__("RunCommandLineAndWait");
extern void InitializeAudioSubsystemThenDispatchPlaybackAndPendingSlotHook(void);
extern uint32_t __stdcall DispatchWinHelpByPrefix(uint32_t arg);
extern uint32_t dwRngState;
extern void InitializeBlankSpaceFilledField32(void);
extern char *__cdecl FindFirstSubstringInString(const char *pszHaystack,
                                                 const char *pszNeedle);

NW_SAVE_CMD uint32_t __stdcall NwSaveDispatchByMode(uint32_t mode,
                                                    uint32_t index)
{
    int8_t result;

    switch (mode) {
    case 0u:
        result = DeleteNetWalkSaveVariantByIndex((int)index);
        return (uint32_t)(int32_t)result;
    case 1u:
        result = WriteSaveFileBuffersAndLoadTileCache((int)index);
        return (uint32_t)(int32_t)result;
    case 2u:
        result = InitializeDispatchRecords((int)index);
        return (uint32_t)(int32_t)result;
    default:
        return mode;
    }
}

NW_SAVE_CMD uint32_t __stdcall NwSelectGridRecordBase(uint32_t index)
{
    uint8_t *pBase = (uint8_t *)(uintptr_t)g_dwGridCtxRecordBase;
    g_state_417388 = *(int32_t *)(pBase + ((uint32_t)index * 16u));
    return 1u;
}

NW_SAVE_CMD uint32_t NwClearGridRecordBase(void)
{
    g_state_417388 = 0;
    return 1u;
}

NW_SAVE_CMD uint32_t __stdcall NwDispatchWinHelp(uint32_t arg)
{
    return DispatchWinHelpByPrefix(arg);
}

NW_SAVE_CMD uint32_t __stdcall NwRngScaleByArg(uint32_t arg)
{
    uint32_t product = ((dwRngState >> 16) * arg) >> 16;
    dwRngState = dwRngState * 0x41c64e6du + 0x3039u;
    return product;
}

NW_SAVE_CMD uint32_t __stdcall NwResetAndRunCommand(uint32_t commandLine)
{
    int8_t result;

    ResetSlotAndHookTablesToDefaults();
    result = (int8_t)RunCommandLineAndWait((int)commandLine);
    InitializeAudioSubsystemThenDispatchPlaybackAndPendingSlotHook();
    return (uint32_t)(int32_t)result;
}

NW_SAVE_CMD uint32_t __stdcall NwInputFieldContainsArg(const char *arg)
{
    const char *pField = (const char *)(intptr_t)(nw_data_window + 0x1ca4);

    if (FindFirstSubstringInString(pField, arg) != 0) {
        InitializeBlankSpaceFilledField32();
        return 1u;
    }
    return 0u;
}

NW_SAVE_CMD uint32_t __stdcall NwSetCellIndexFromArg(uint32_t flag)
{
    if (flag != 0u) {
        g_wCellIndex = (uint16_t)g_dwGridUserData;
    } else {
        g_wCellIndex = 0xffffu;
    }
    return 1u;
}
#endif /* NW_UNIT_SaveAndCommandHandlers */
