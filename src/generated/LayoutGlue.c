/*
 * LayoutGlue -- link-layout glue and empty code targets.
 *
 * Module: generated
 * Interface: include/LayoutGlue.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   NwTextPad                                                  generated/NwTextPad.c
 *   WindowTableCodeTargets                                     generated/WindowTableCodeTargets.c
 */

/* ==========================================================================
 * Unit: NwTextPad
 * Source: generated/NwTextPad.c
 * ========================================================================== */
#ifdef NW_UNIT_NwTextPad
/*
 * Reserves the final 16 bytes of .text so the data window keeps its address.
 *
 * Module: generated
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#generated-nwtextpad-c
 */

/* Layout padding only: these bytes occupy the tail of .text and are never
 * executed. */
__attribute__((section(".text"), used, aligned(1)))
static const unsigned char nw_text_padding[16] = {
    0x90, 0x90, 0x90, 0x90,
    0x90, 0x90, 0x90, 0x90,
    0x90, 0x90, 0x90, 0x90,
    0x90, 0x90, 0x90, 0x90
};
#endif /* NW_UNIT_NwTextPad */

/* ==========================================================================
 * Unit: WindowTableCodeTargets
 * Source: generated/WindowTableCodeTargets.c
 * ========================================================================== */
#ifdef NW_UNIT_WindowTableCodeTargets
/*
 * Comment-only record of window-table targets.
 *
 * Module: generated
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#generated-windowtablecodetargets-c
 */
#endif /* NW_UNIT_WindowTableCodeTargets */
