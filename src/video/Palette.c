#include "nwarch.h"
/*
 * Palette -- logical palette setup and grid palette rebuild.
 *
 * Module: video
 * Interface: include/Palette.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   ReadPaletteBlockAndSetLogicalPalette                       video/ReadPaletteBlockAndSetLogicalPalette.c
 *   RebuildGridPaletteAndResetExtent                           video/RebuildGridPaletteAndResetExtent.c
 *   RngPaletteHandlers                                         video/RngPaletteHandlers.c
 */

/* ==========================================================================
 * Unit: ReadPaletteBlockAndSetLogicalPalette
 * Source: video/ReadPaletteBlockAndSetLogicalPalette.c
 * ========================================================================== */
#ifdef NW_UNIT_ReadPaletteBlockAndSetLogicalPalette
/*
 * Reads one 256-colour RGBTRIPLE palette block off the stream into the palette staging buffer and, on success.
 *
 * Module: video
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#video-readpaletteblockandsetlogicalpalette-c
 */

#include <windows.h>
extern unsigned char nw_data_window[];

/* ------------------------------------------------------------------ globals */

#define g_cbPaletteBytes  (*(DWORD  *)(nw_data_window + 0x1720))   /* allocation size, argument 1 */
#define g_pbPaletteBuffer NW_WINDOW_PTR_REF(BYTE, 0x1724)   /* staging buffer, argument 4  */

extern BYTE g_bGridPenIndex;

/* The logical palette has PALETTEENTRY[256] inline after the 32-byte LOGPALETTE
 * header, so the count and the table are both fixed by the local in the callee. */
#define PALETTE_ENTRY_COUNT  256

/* ------------------------------------------------------------------ callees */

extern int __stdcall ReadStreamBlockRawOrLzss(DWORD cbUnusedBlockSize, BYTE *pbDestBuf);

extern BOOL WINAPI SetLogicalPaletteDrawGrid(BOOL bUseSystemColors,
                                             BYTE bGridPenIndex,
                                             UINT uNumPaletteEntries,
                                             const RGBTRIPLE *pRgbEntries);

BOOL __cdecl ReadPaletteBlockAndSetLogicalPalette(void)
{

    if (!ReadStreamBlockRawOrLzss(g_cbPaletteBytes, g_pbPaletteBuffer)) {

        return FALSE;
    }

    return SetLogicalPaletteDrawGrid(FALSE,                 /* bUseSystemColors = 0  */
                                     g_bGridPenIndex,       /* pen colour selector   */
                                     PALETTE_ENTRY_COUNT,   /* 0x100 = 256 RGBTRIPLEs */
                                     (const RGBTRIPLE *)g_pbPaletteBuffer);

}
#endif /* NW_UNIT_ReadPaletteBlockAndSetLogicalPalette */

/* ==========================================================================
 * Unit: RebuildGridPaletteAndResetExtent
 * Source: video/RebuildGridPaletteAndResetExtent.c
 * ========================================================================== */
#ifdef NW_UNIT_RebuildGridPaletteAndResetExtent
/*
 * Rebuild Grid Palette And Reset Extent.
 *
 * Module: video
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#video-rebuildgridpaletteandresetextent-c
 */

#include <stdint.h>
#include "nwglobals.h"
#include <windows.h>

/* ------------------------------------------------------------------ globals */


extern uint8_t  g_bGridPenIndex;



extern int      g_nGridMaxY;

/* ------------------------------------------------------------------ callees */

/* Rebuilds the logical palette from an RGBTRIPLE table and repaints the grid.
 * Signature established by round 22 (src/video/win32/SetLogicalPaletteDrawGrid.c).        */
extern BOOL WINAPI SetLogicalPaletteDrawGrid(BOOL bUseSystemColors,
                                             char cGridPenIndex,
                                             UINT uNumPaletteEntries,
                                             const RGBTRIPLE *pRgbEntries);  

extern void AddRectToExtentList(int nMaxY, int nMaxX, int nMinY, int nMinX);   

/* ------------------------------------------------------------------ defines */

#define GRID_PALETTE_ENTRIES  0x100   /* PUSH 0x100 - full 256-entry palette  */
#define COLOR_TABLE_SKIP      0x18    /* ADD ECX,0x18 - table offset past base */
#define OFFSET_TO_BLOCK_FIELD 0x10    

BOOL RebuildGridPaletteAndResetExtent(void)
{
    uint8_t *pBlock;          
    uint8_t *pRgbTable;       /* &block[block->dwOffset + 0x18]            */
    uint8_t  bPenIndex;       /* grid pen palette index, saved before CALL  */

    /* Locate the RGBTRIPLE table inside the colour block. */
    pBlock    = g_pColorTableBlock;
    pRgbTable = pBlock + *(uint32_t *)(pBlock + OFFSET_TO_BLOCK_FIELD)
                    + COLOR_TABLE_SKIP;

    /*
     * The pen index is read once, stashed in the latch global, and the value
     * pushed as the palette's cGridPenIndex argument is the same byte. The
     * binary loads only AL and pushes the full EAX; the callee reads a single
     * byte parameter, so only AL is meaningful.
     */
    bPenIndex               = g_bGridPenIndex;
    g_bLastGridPenIndex     = bPenIndex;

    /* bUseSystemColors = TRUE: take the two 10-entry system colour blocks. */
    SetLogicalPaletteDrawGrid(TRUE,
                              (char)bPenIndex,
                              GRID_PALETTE_ENTRIES,
                              (const RGBTRIPLE *)pRgbTable);

    /* Reset the extent list to the full grid box: origin hard-coded (0,0). */
    AddRectToExtentList(g_nGridMaxY, g_nGridMaxX, 0, 0);

    return TRUE;   
}
#endif /* NW_UNIT_RebuildGridPaletteAndResetExtent */

/* ==========================================================================
 * Unit: RngPaletteHandlers
 * Source: video/RngPaletteHandlers.c
 * ========================================================================== */
#ifdef NW_UNIT_RngPaletteHandlers
/*
 * RNG-driven palette table handlers.
 *
 * Module: video
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#video-rngpalettehandlers-c
 */

#include <stdint.h>

extern unsigned char nw_data_window[];

#define NW_RNG_HANDLER_SECTION __attribute__((section(".nwdiag")))

#define NW_RNG_STATE_OFFSET  0x01c8u
#define NW_LCG_MULTIPLIER    0x41c64e6du
#define NW_LCG_INCREMENT     0x00003039u

static NW_RNG_HANDLER_SECTION uint32_t nw_pick_rng_table_byte(uint8_t *record, uint32_t table_offset,
                                       uint32_t multiplier)
{
    volatile uint32_t *rng =
        (volatile uint32_t *)(void *)(nw_data_window + NW_RNG_STATE_OFFSET);
    uint32_t state = *rng;
    uint32_t index = (((state >> 16) * multiplier) >> 16);
    uint32_t product = state * NW_LCG_MULTIPLIER;

    record[1] = nw_data_window[table_offset + index];
    *rng = product + NW_LCG_INCREMENT;
    return product;
}

/* The fixed data window still points at these functions under their original
 * exported names, so the asm labels preserve those symbols. */
#define NW_RNG_TABLE_HANDLER(cname, symbol, table, mult)                \
    NW_RNG_HANDLER_SECTION uint32_t __stdcall cname(uint8_t *record)    \
        __asm__(symbol);                                                \
    NW_RNG_HANDLER_SECTION uint32_t __stdcall cname(uint8_t *record)    \
    {                                                                   \
        return nw_pick_rng_table_byte(record, (table), (mult));         \
    }

NW_RNG_TABLE_HANDLER(NwRngTablePickA, "_NwRngTablePick_00402154", 0x154u, 2u)
NW_RNG_TABLE_HANDLER(NwRngTablePickB, "_NwRngTablePick_0040218c", 0x156u, 2u)
NW_RNG_TABLE_HANDLER(NwRngTablePickC, "_NwRngTablePick_004021c4", 0x158u, 2u)
NW_RNG_TABLE_HANDLER(NwRngTablePickD, "_NwRngTablePick_004021fc", 0x15cu, 3u)
NW_RNG_TABLE_HANDLER(NwRngTablePickE, "_NwRngTablePick_00402234", 0x160u, 2u)
NW_RNG_TABLE_HANDLER(NwRngTablePickF, "_NwRngTablePick_0040226c", 0x162u, 2u)
NW_RNG_TABLE_HANDLER(NwRngTablePickG, "_NwRngTablePick_004022a4", 0x164u, 3u)
NW_RNG_TABLE_HANDLER(NwRngTablePickH, "_NwRngTablePick_004022dc", 0x168u, 2u)
NW_RNG_TABLE_HANDLER(NwRngTablePickI, "_NwRngTablePick_00402314", 0x16cu, 3u)
NW_RNG_TABLE_HANDLER(NwRngTablePickJ, "_NwRngTablePick_0040234c", 0x170u, 3u)
NW_RNG_TABLE_HANDLER(NwRngTablePickK, "_NwRngTablePick_00402384", 0x174u, 4u)
#endif /* NW_UNIT_RngPaletteHandlers */
