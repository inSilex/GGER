/*
 * WindowTables -- window-table stubs and tail handlers.
 *
 * Module: game
 * Interface: include/WindowTables.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   WindowTableStubs                                           game/WindowTableStubs.c
 *   WindowTableTailHandlers                                    game/WindowTableTailHandlers.c
 */

/* ==========================================================================
 * Unit: WindowTableStubs
 * Source: game/WindowTableStubs.c
 * ========================================================================== */
#ifdef NW_UNIT_WindowTableStubs
/*
 * Window Table Stubs.
 *
 * Module: game
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#game-windowtablestubs-c
 */

#include <stdint.h>
#include "nwglobals.h"

#define NW_TABLE_STUB __attribute__((section(".nwdiag")))

NW_TABLE_STUB void __stdcall NwNoOpPoolHandler(uint32_t ignored)
{
    (void)ignored;
}

NW_TABLE_STUB void __stdcall NwNoOpOpcodeHandler(uint32_t ignored)
{
    (void)ignored;
}

NW_TABLE_STUB uint8_t * __stdcall NwClearRecordLowNibble(uint8_t *pRecord)
{
    pRecord[1] &= 0xf0u;
    return pRecord;
}

NW_TABLE_STUB uint32_t NwSetLiveInputByteAndReturnOne(void)
{
    *(uint8_t *)(intptr_t)(nw_data_window + 0x67ff) = 1u;
    return 1u;
}
#endif /* NW_UNIT_WindowTableStubs */

/* ==========================================================================
 * Unit: WindowTableTailHandlers
 * Source: game/WindowTableTailHandlers.c
 * ========================================================================== */
#ifdef NW_UNIT_WindowTableTailHandlers
/*
 * Window Table Tail Handlers.
 *
 * Module: game
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#game-windowtabletailhandlers-c
 */

#include <stdint.h>
#include "nwglobals.h"

#define NW_TAIL_HANDLER __attribute__((section(".nwdiag")))

extern void MixActiveAudioSlotsIntoWaveOutBuffer(void);

NW_TAIL_HANDLER uint32_t __stdcall NwStoreByteThroughWindowOffset(
    uint32_t offset, const uint8_t *source)
{
    uint32_t target = (uint32_t)(uintptr_t)nw_data_window + 0x636cu + offset;
    uint8_t value = *source;

    *(volatile uint8_t *)(uintptr_t)target = value;
    return (uint32_t)value;
}

NW_TAIL_HANDLER int8_t __stdcall NwPublishAudioSlotAndMix(
    int arg1, int arg2, int arg3, const uint8_t *record)
{
    uint32_t start = g_dwSlotAuxZeroedA;
    uint32_t cursor = start;
    volatile uint8_t *slot;
    uint32_t flags;
    const uint8_t *recordBody;
    uint8_t mode;

    for (;;) {
        slot = (volatile uint8_t *)(void *)
            (g_adwSlotTable + ((uint32_t)cursor << 4));
        if (*(volatile uint32_t *)(void *)(slot + 0) == 0u) {
            break;
        }
        ++cursor;
        if ((uint32_t)cursor >= g_dwSlotCount) {
            cursor = 0u;
        }
        if (cursor == start) {
            break;
        }
    }

    g_dwSlotAuxZeroedA = cursor;

    flags = *(const uint32_t *)(const void *)record;
    flags &= 0xfffffffcu;

    *(volatile uint32_t *)(void *)(slot + 8) = flags;

    recordBody = record + 4;
    *(volatile uint32_t *)(void *)(slot + 0) = (uint32_t)(uintptr_t)recordBody;
    *(volatile uint32_t *)(void *)(slot + 4) = (uint32_t)(uintptr_t)recordBody;

    *(volatile uint8_t *)(void *)(slot + 0x0d) = (uint8_t)arg2;
    *(volatile uint8_t *)(void *)(slot + 0x0c) = (uint8_t)arg3;
    *(volatile uint8_t *)(void *)(slot + 0x0e) = (uint8_t)arg1;

    if (((uint8_t)arg1 & 0x80u) != 0u) {
        mode = 1u;
    } else if (g_dwSlotAuxZeroedB == 0u) {
        mode = 2u;
    } else {
        mode = 0u;
    }

    if (mode == 2u) {
        MixActiveAudioSlotsIntoWaveOutBuffer();
        MixActiveAudioSlotsIntoWaveOutBuffer();
    } else if (mode == 1u) {
        MixActiveAudioSlotsIntoWaveOutBuffer();
    }

    return 1;
}

NW_TAIL_HANDLER void NwBareRet0040dfd8(void)
{
}

NW_TAIL_HANDLER void NwBareRet0041009c(void)
{
}
#endif /* NW_UNIT_WindowTableTailHandlers */
