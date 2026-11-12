/*
 * GridMapping -- command-to-grid tile/cell mapping helpers.
 *
 * Module: grid
 * Interface: include/GridMapping.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   DispatchOneBasedCommandToGridRenderMapper                  grid/DispatchOneBasedCommandToGridRenderMapper.c
 *   DispatchOneBasedCommandToSentinelGroupMapper               grid/DispatchOneBasedCommandToSentinelGroupMapper.c
 *   DispatchThreeToElevenCommandToSentinelGroupMapper          grid/DispatchThreeToElevenCommandToSentinelGroupMapper.c
 *   ExpandCellTileIndexBitmapBitsToGridBytes                   grid/ExpandCellTileIndexBitmapBitsToGridBytes.c
 *   IsRemappedIndexArgEqual                                    grid/IsRemappedIndexArgEqual.c
 *   MapGridCellsAgainstCallerSentinelToDisplayByte             grid/MapGridCellsAgainstCallerSentinelToDisplayByte.c
 *   MapGridCellsAgainstCallerSentinelToTwoOnMatchElseZero      grid/MapGridCellsAgainstCallerSentinelToTwoOnMatchElseZero.c
 *   MapGridTilesByFeAndCallerSentinelToDisplayZeroTwoThree     grid/MapGridTilesByFeAndCallerSentinelToDisplayZeroTwoThree.c
 *   MapGridTilesViaBitmapToDisplayGrid                         grid/MapGridTilesViaBitmapToDisplayGrid.c
 *   MapGridTilesViaSentinelFeToDisplayGrid                     grid/MapGridTilesViaSentinelFeToDisplayGrid.c
 *   MapTileBitmapBitToGridZeroOrThree                          grid/MapTileBitmapBitToGridZeroOrThree.c
 *   SelectAndDispatchGridRenderMappingByCommand                grid/SelectAndDispatchGridRenderMappingByCommand.c
 *   SelectAndDispatchGridRenderMappingBySentinelGroup          grid/SelectAndDispatchGridRenderMappingBySentinelGroup.c
 */

/* ==========================================================================
 * Unit: DispatchOneBasedCommandToGridRenderMapper
 * Source: grid/DispatchOneBasedCommandToGridRenderMapper.c
 * ========================================================================== */
#ifdef NW_UNIT_DispatchOneBasedCommandToGridRenderMapper
/*
 * Dispatch One Based Command To Grid Render Mapper.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-dispatchonebasedcommandtogridrendermapper-c
 */

#include <windows.h>
#include <stdint.h>

extern int __stdcall SelectAndDispatchGridRenderMappingByCommand(BYTE bMapperSelect, BYTE bRecordIndex, BYTE bCommand);

#define MAPPER_SELECT_BITMAP       0xFFu   
#define MAPPER_SELECT_SENTINEL_FE  0xFEu   

/* The four command bytes the dispatcher pushes into the callee's bCommand slot. */
#define CMD_DIRECT_NO_GATE   0x00u   /* callee takes its DEFAULT path (no bitmap gate)        */
#define CMD_GATE_B           0x0Eu   /* callee takes GATE B                                   */
#define CMD_GATE_A_82        0x82u   /* callee takes GATE A (pushed as the literal -0x7e)     */
#define CMD_GATE_A_83        0x83u   /* callee takes GATE A (pushed as the literal -0x7d)     */

int __stdcall DispatchOneBasedCommandToGridRenderMapper(uint32_t nCommand, uint32_t dwRecordIndex)
{
    BYTE bRecordIndex = (BYTE)dwRecordIndex;   /* the callee consumes only the low byte */

    switch (nCommand) {
    case 1:  
        return SelectAndDispatchGridRenderMappingByCommand(MAPPER_SELECT_BITMAP,       bRecordIndex, CMD_DIRECT_NO_GATE);
    case 2:  
        return SelectAndDispatchGridRenderMappingByCommand(MAPPER_SELECT_SENTINEL_FE,  bRecordIndex, CMD_DIRECT_NO_GATE);
    case 3:  
        return SelectAndDispatchGridRenderMappingByCommand(MAPPER_SELECT_BITMAP,       bRecordIndex, CMD_GATE_B);
    case 4:  
        return SelectAndDispatchGridRenderMappingByCommand(MAPPER_SELECT_SENTINEL_FE,  bRecordIndex, CMD_GATE_B);
    case 5:  
        return SelectAndDispatchGridRenderMappingByCommand(bRecordIndex,               bRecordIndex, CMD_GATE_B);
    case 6:  
        return SelectAndDispatchGridRenderMappingByCommand(MAPPER_SELECT_BITMAP,       bRecordIndex, CMD_GATE_A_82);
    case 7:  
        return SelectAndDispatchGridRenderMappingByCommand(MAPPER_SELECT_SENTINEL_FE,  bRecordIndex, CMD_GATE_A_82);
    case 8:  
        return SelectAndDispatchGridRenderMappingByCommand(bRecordIndex,               bRecordIndex, CMD_GATE_A_82);
    case 9:  
        return SelectAndDispatchGridRenderMappingByCommand(MAPPER_SELECT_BITMAP,       bRecordIndex, CMD_GATE_A_83);
    case 10: 
        return SelectAndDispatchGridRenderMappingByCommand(MAPPER_SELECT_SENTINEL_FE,  bRecordIndex, CMD_GATE_A_83);
    case 11: 
        return SelectAndDispatchGridRenderMappingByCommand(bRecordIndex,               bRecordIndex, CMD_GATE_A_83);
    default:

        return 0;
    }
}
#endif /* NW_UNIT_DispatchOneBasedCommandToGridRenderMapper */

/* ==========================================================================
 * Unit: DispatchOneBasedCommandToSentinelGroupMapper
 * Source: grid/DispatchOneBasedCommandToSentinelGroupMapper.c
 * ========================================================================== */
#ifdef NW_UNIT_DispatchOneBasedCommandToSentinelGroupMapper
/*
 * Dispatch One Based Command To Sentinel Group Mapper.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-dispatchonebasedcommandtosentinelgroupmapper-c
 */

#include <windows.h>
#include <stdint.h>

extern int __stdcall SelectAndDispatchGridRenderMappingBySentinelGroup(BYTE bMapperSelector, BYTE bSentinelByte, BYTE bCommand);

#define MAPPER_SELECT_CLASSIFY     0xFFu   
#define MAPPER_SELECT_SENTINEL_FE  0xFEu   

/* The four bCommand literals the dispatcher pushes into the callee's THIRD slot. */
#define CMD_DEFAULT_NO_GATE  0x00u   /* callee takes its DEFAULT path (dispatch straight on ARG1) */
#define CMD_GATE_B           0x0Eu   /* callee takes its 0x0e path (sentinel-group B)              */
#define CMD_GATE_A_82        0x82u   /* callee takes its 0x82 path (sentinel-group A)               */
#define CMD_GATE_A_83        0x83u   /* callee takes its 0x83 path (sentinel-group A)               */

int __stdcall DispatchOneBasedCommandToSentinelGroupMapper(uint32_t nCommand, uint32_t dwSentinel)
{
    BYTE bSentinel = (BYTE)dwSentinel;   /* the callee consumes only the low byte */

    switch (nCommand) {
    case 1:  
        return SelectAndDispatchGridRenderMappingBySentinelGroup(MAPPER_SELECT_CLASSIFY,     bSentinel, CMD_DEFAULT_NO_GATE);
    case 2:  
        return SelectAndDispatchGridRenderMappingBySentinelGroup(MAPPER_SELECT_SENTINEL_FE, bSentinel, CMD_DEFAULT_NO_GATE);
    case 3:  
        return SelectAndDispatchGridRenderMappingBySentinelGroup(MAPPER_SELECT_CLASSIFY,     bSentinel, CMD_GATE_B);
    case 4:  
        return SelectAndDispatchGridRenderMappingBySentinelGroup(MAPPER_SELECT_SENTINEL_FE, bSentinel, CMD_GATE_B);
    case 5:  
        return SelectAndDispatchGridRenderMappingBySentinelGroup(bSentinel,                 bSentinel, CMD_GATE_B);
    case 6:  
        return SelectAndDispatchGridRenderMappingBySentinelGroup(MAPPER_SELECT_CLASSIFY,     bSentinel, CMD_GATE_A_82);
    case 7:  
        return SelectAndDispatchGridRenderMappingBySentinelGroup(MAPPER_SELECT_SENTINEL_FE, bSentinel, CMD_GATE_A_82);
    case 8:  
        return SelectAndDispatchGridRenderMappingBySentinelGroup(bSentinel,                 bSentinel, CMD_GATE_A_82);
    case 9:  
        return SelectAndDispatchGridRenderMappingBySentinelGroup(MAPPER_SELECT_CLASSIFY,     bSentinel, CMD_GATE_A_83);
    case 10: 
        return SelectAndDispatchGridRenderMappingBySentinelGroup(MAPPER_SELECT_SENTINEL_FE, bSentinel, CMD_GATE_A_83);
    case 11: 
        return SelectAndDispatchGridRenderMappingBySentinelGroup(bSentinel,                 bSentinel, CMD_GATE_A_83);
    default:

        return 0;
    }
}
#endif /* NW_UNIT_DispatchOneBasedCommandToSentinelGroupMapper */

/* ==========================================================================
 * Unit: DispatchThreeToElevenCommandToSentinelGroupMapper
 * Source: grid/DispatchThreeToElevenCommandToSentinelGroupMapper.c
 * ========================================================================== */
#ifdef NW_UNIT_DispatchThreeToElevenCommandToSentinelGroupMapper
/*
 * Dispatch Three To Eleven Command To Sentinel Group Mapper.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-dispatchthreetoelevencommandtosentinelgroupmapper-c
 */

#include <windows.h>
#include <stdint.h>

extern int __stdcall SelectAndDispatchGridRenderMappingBySentinelGroup(BYTE bMapperSelector, BYTE bSentinelByte, BYTE bCommand);

/* The two mapper-selector literals (callee ARG1), pushed as -1 / -2. */
#define MAPPER_SELECT_BITMAP       0xFFu   
#define MAPPER_SELECT_SENTINEL_FE  0xFEu   

/* The two sentinel-byte literals (callee ARG2), pushed as -1 / -2. */
#define SENTINEL_BYTE_FF           0xFFu   /* pushed as the literal -1 */
#define SENTINEL_BYTE_FE           0xFEu   /* pushed as the literal -2 */

/* The three command literals (callee ARG3), pushed FIRST in each block. */
#define CMD_GROUP_0E               0x0Eu   /* pushed as the literal 0x0e */
#define CMD_GROUP_82               0x82u   
#define CMD_GROUP_83               0x83u   

int __stdcall DispatchThreeToElevenCommandToSentinelGroupMapper(uint32_t nCommand)
{

    switch (nCommand) {
    case 3:  
        return SelectAndDispatchGridRenderMappingBySentinelGroup(MAPPER_SELECT_BITMAP,      SENTINEL_BYTE_FF, CMD_GROUP_0E);
    case 4:  
        return SelectAndDispatchGridRenderMappingBySentinelGroup(MAPPER_SELECT_BITMAP,      SENTINEL_BYTE_FE, CMD_GROUP_0E);
    case 5:  
        return SelectAndDispatchGridRenderMappingBySentinelGroup(MAPPER_SELECT_SENTINEL_FE, SENTINEL_BYTE_FE, CMD_GROUP_0E);
    case 6:  
        return SelectAndDispatchGridRenderMappingBySentinelGroup(MAPPER_SELECT_BITMAP,      SENTINEL_BYTE_FF, CMD_GROUP_82);
    case 7:  
        return SelectAndDispatchGridRenderMappingBySentinelGroup(MAPPER_SELECT_BITMAP,      SENTINEL_BYTE_FE, CMD_GROUP_82);
    case 8:  
        return SelectAndDispatchGridRenderMappingBySentinelGroup(MAPPER_SELECT_SENTINEL_FE, SENTINEL_BYTE_FE, CMD_GROUP_82);
    case 9:  
        return SelectAndDispatchGridRenderMappingBySentinelGroup(MAPPER_SELECT_BITMAP,      SENTINEL_BYTE_FF, CMD_GROUP_83);
    case 10: 
        return SelectAndDispatchGridRenderMappingBySentinelGroup(MAPPER_SELECT_BITMAP,      SENTINEL_BYTE_FE, CMD_GROUP_83);
    case 11: 
        return SelectAndDispatchGridRenderMappingBySentinelGroup(MAPPER_SELECT_SENTINEL_FE, SENTINEL_BYTE_FE, CMD_GROUP_83);
    default:

        return 0;
    }
}
#endif /* NW_UNIT_DispatchThreeToElevenCommandToSentinelGroupMapper */

/* ==========================================================================
 * Unit: ExpandCellTileIndexBitmapBitsToGridBytes
 * Source: grid/ExpandCellTileIndexBitmapBitsToGridBytes.c
 * ========================================================================== */
#ifdef NW_UNIT_ExpandCellTileIndexBitmapBitsToGridBytes
/*
 * Expand Cell Tile Index Bitmap Bits To Grid Bytes.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-expandcelltileindexbitmapbitstogridbytes-c
 */

#include <windows.h>
#include "nwglobals.h"
#include <stdint.h>

/* ---- grid geometry, destination grid, source table, and the shift ---------- */
extern BYTE     *gpGridBase;          
extern const BYTE *gpSourceCellTable; 

/* ---- the sole callee: the conductor, __stdcall, one BYTE mode argument ----- */
extern int __stdcall SequenceGridMarkingSweepsUntilMatchAndDispatch(BYTE bMode); 

#define GRID_BIT_SET     ((BYTE)0x02)  /* stored when the bitmap bit is SET   */
#define GRID_BIT_CLEAR   ((BYTE)0x00)  /* stored when the bitmap bit is CLEAR */
#define CONDUCTOR_MODE   ((BYTE)0x01)  

int __stdcall ExpandCellTileIndexBitmapBitsToGridBytes(const BYTE *pBitmap)
{
    BYTE *pDst = gpGridBase;   
    int   nRow;

    for (nRow = 0; nRow < g_nGridRows; nRow++) {          
        int nCol;
        /* EDI = gpSourceCellTable + (row << shift) * 2 ; +2 per inner step. */
        const BYTE *pSrcCell = (const BYTE *)gpSourceCellTable
                             + ((nRow << g_dwSourceRowShift) * 2);

        for (nCol = 0; nCol < g_nGridColumns; nCol++) {   
            unsigned int uTileIndex = *pSrcCell;          
            unsigned int uMask      = 1u << (uTileIndex & 7u);            
            unsigned int uByte      = pBitmap[uTileIndex >> 3];          
            unsigned int uBit       = uMask & uByte;                     

            *pDst = (uBit != 0u) ? GRID_BIT_SET : GRID_BIT_CLEAR;        

            pSrcCell += 2;                                               
            pDst++;                                                      
        }
    }

    return SequenceGridMarkingSweepsUntilMatchAndDispatch(CONDUCTOR_MODE); 
}
#endif /* NW_UNIT_ExpandCellTileIndexBitmapBitsToGridBytes */

/* ==========================================================================
 * Unit: IsRemappedIndexArgEqual
 * Source: grid/IsRemappedIndexArgEqual.c
 * ========================================================================== */
#ifdef NW_UNIT_IsRemappedIndexArgEqual
/*
 * Is Remapped Index Arg Equal.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-isremappedindexargequal-c
 */

#include <stdint.h>
extern unsigned char nw_data_window[];

#define G_DWORD_4173EC (*(volatile uint32_t *)(uintptr_t)(nw_data_window + 0x63ec))
#define G_BYTE_TABLE_411404 ((const uint8_t *)(uintptr_t)(nw_data_window + 0x404))

uint32_t __stdcall IsRemappedIndexArgEqual(uint32_t dwIndexArg, uint32_t dwValueArg)
{
    uint32_t dwIndex = dwIndexArg;         
    uint32_t dwScale = G_DWORD_4173EC;     

    if (G_DWORD_4173EC > 3u) {             
        dwScale = dwScale - 4u;            /* ADD ECX,-4                   */

        dwIndex = (uint32_t)G_BYTE_TABLE_411404[dwIndex];
    }

    dwIndex = (dwIndex + dwScale * 2u) & 7u;   /* LEA EAX,[EDX+ECX*2]; AND EAX,7 */

    if (dwIndex == dwValueArg) {               
        return 1u;                             
    }
    return 0u;
}
#endif /* NW_UNIT_IsRemappedIndexArgEqual */

/* ==========================================================================
 * Unit: MapGridCellsAgainstCallerSentinelToDisplayByte
 * Source: grid/MapGridCellsAgainstCallerSentinelToDisplayByte.c
 * ========================================================================== */
#ifdef NW_UNIT_MapGridCellsAgainstCallerSentinelToDisplayByte
/*
 * Maps a word-per-cell grid into a byte-per-cell display grid: each cell's low byte is tested against the caller-supplied sentinel and 0 is written on...
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-mapgridcellsagainstcallersentineltodisplaybyte-c
 */

#include <windows.h>
#include "nwglobals.h"
#include <stdint.h>

extern BYTE *gpSourceCellWordBase;    
extern BYTE *gpDisplayCellByteBase;   

/* ---- the sole callee (live name), the marker-sweep conductor ------------- */
extern int __stdcall SequenceGridMarkingSweepsUntilMatchAndDispatch(BYTE bMode); 

int __stdcall MapGridCellsAgainstCallerSentinelToDisplayByte(BYTE bSentinel)
{
    BYTE *pDest = gpDisplayCellByteBase;              
    int   nRow  = 0;                                  

    do {

        uint16_t *pSrc = (uint16_t *)((BYTE *)gpSourceCellWordBase
                                    + ((intptr_t)nRow << (g_nSourceRowShift & 0x1f)) * 2);
        int       nCol = 0;                           

        do {
            BYTE bCell = (BYTE)(*pSrc & 0xFF);        

            *pDest = (bCell == bSentinel) ? 0 : 3;
            pSrc++;                                   
            pDest++;                                  
            nCol++;                                   
        } while (nCol < g_nRowStride);                

        nRow++;                                       
    } while (nRow < g_nRowCount);                     

    ((BYTE *)gpDisplayCellByteBase)[(intptr_t)g_nStartRow * g_nRowStride + g_nStartCol] = 2;

    return SequenceGridMarkingSweepsUntilMatchAndDispatch(0);
}
#endif /* NW_UNIT_MapGridCellsAgainstCallerSentinelToDisplayByte */

/* ==========================================================================
 * Unit: MapGridCellsAgainstCallerSentinelToTwoOnMatchElseZero
 * Source: grid/MapGridCellsAgainstCallerSentinelToTwoOnMatchElseZero.c
 * ========================================================================== */
#ifdef NW_UNIT_MapGridCellsAgainstCallerSentinelToTwoOnMatchElseZero
/*
 * Walks the 16-bit-per-cell tile table and writes a byte per cell into the flat display grid: 2 when the cell's low byte equals the caller-supplied...
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-mapgridcellsagainstcallersentineltotwoonmatchelsezero-c
 */

#include <windows.h>
#include "nwglobals.h"
#include <stdint.h>

/* ---- grid geometry and buffers (32-bit PE globals) ----------------------
 * Names shared with the sibling src/grid/MapGridTilesViaSentinelFeToDisplayGrid.c
 * so the grid-to-display family reads consistently. All read as zero in the file
 * image (runtime state). */

/* Destination: the 1-byte-per-cell display grid, walked flat by INC ESI. */
extern BYTE *gpGridBase;            

/* Loop bounds: inner (columns/cells-per-row) and outer (rows). */

/* Source: the flat 16-bit-per-cell tile table, row-major, cell = (row << shift) + col. */
extern WORD *gpTileCellTable;       

/* ---- the conductor this function dispatches last ------------------------
 * Declared to match its own file: __stdcall, one BYTE mode argument. Called here
 * with mode 1 (PUSH 0x1). Return value passed through untouched. */
extern int __stdcall SequenceGridMarkingSweepsUntilMatchAndDispatch(BYTE bMode); 

#define MATCH_CODE    ((BYTE)0x02)  
#define NOMATCH_CODE  ((BYTE)0x00)  
#define CONDUCTOR_MODE ((BYTE)0x01) 

int __stdcall MapGridCellsAgainstCallerSentinelToTwoOnMatchElseZero(BYTE bSentinel)
{
    BYTE *pDst = gpGridBase;                 
    int   nRow = 0;                          

    do {

        WORD *pSrc = gpTileCellTable
                   + (intptr_t)(nRow << (g_nTileTableRowShift & 0x1f));
        int   nCol = 0;                      

        do {
            BYTE bTile = (BYTE)(*pSrc);      

            *pDst = (bTile == bSentinel) ? MATCH_CODE : NOMATCH_CODE;  

            ++pSrc;                          
            ++pDst;                          
        } while (++nCol < g_nColumnsPerRow); 

        ++nRow;                              
    } while (nRow < g_nRowCount);            

    return SequenceGridMarkingSweepsUntilMatchAndDispatch(CONDUCTOR_MODE);
}
#endif /* NW_UNIT_MapGridCellsAgainstCallerSentinelToTwoOnMatchElseZero */

/* ==========================================================================
 * Unit: MapGridTilesByFeAndCallerSentinelToDisplayZeroTwoThree
 * Source: grid/MapGridTilesByFeAndCallerSentinelToDisplayZeroTwoThree.c
 * ========================================================================== */
#ifdef NW_UNIT_MapGridTilesByFeAndCallerSentinelToDisplayZeroTwoThree
/*
 * Map Grid Tiles By Fe And Caller Sentinel To Display Zero Two Three.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-mapgridtilesbyfeandcallersentineltodisplayzerotwothree-c
 */

#include <windows.h>
#include "nwglobals.h"
#include <stdint.h>

/* ---- grid geometry and buffers (32-bit PE globals) ----------------------
 * All read as zero in the file image (runtime state), verified by read_memory.
 * Names follow the sibling src/grid/MapGridTilesViaSentinelFeToDisplayGrid.c so the
 * family reads consistently. */

/* Destination: the flat 1-byte-per-cell display grid (seeded from, and walked
 * by INC ESI, one byte per cell). Same base the conductor and the marking
 * sweeps use. */
extern BYTE *gpGridBase;            

/* Loop bounds: inner (columns, EBX) and outer (rows, EDI) -- the same geometry
 * pair the sibling mappers use. Neither is written by this body. */

/* Source: a flat 16-bit-per-cell tile table, row-major, indexed by
 * cell = (row << shift) + column. Only the LOW byte of each word is read. */
extern WORD *gpTileCellTable;       

/* ---- the conductor this function dispatches last ------------------------
 * Declared to match its own file: __stdcall, one BYTE mode argument. Always
 * called here with mode 0 (PUSH 0x0). */
extern int __stdcall SequenceGridMarkingSweepsUntilMatchAndDispatch(BYTE bMode); 

#define TILE_INDEX_SENTINEL  ((BYTE)0xFE)  /* fixed literal: renders as 0 (tested FIRST) */
#define DISPLAY_SENTINEL     ((BYTE)0x00)  /* 0xfe outcome                              */
#define DISPLAY_CALLER_MATCH ((BYTE)0x02)  /* equals the caller's sentinel byte         */
#define DISPLAY_OTHER        ((BYTE)0x03)  /* every other low byte                      */

int __stdcall MapGridTilesByFeAndCallerSentinelToDisplayZeroTwoThree(BYTE bSentinel)
{
    BYTE *pDst = gpGridBase;                          
    int   nRow = 0;                                   

    do {

        WORD *pSrcCell = gpTileCellTable + (nRow << (g_nTileTableRowShift & 0x1f));
        int   nCol = 0;                               

        do {
            BYTE bCell = (BYTE)(*pSrcCell);           
            BYTE bCode;

            if (bCell == TILE_INDEX_SENTINEL) {           /* fixed 0xfe sentinel -- tested FIRST */
                bCode = DISPLAY_SENTINEL;
            } else if (bCell == bSentinel) {              /* caller's sentinel byte */
                bCode = DISPLAY_CALLER_MATCH;
            } else {
                bCode = DISPLAY_OTHER;                    
            }

            *pDst = bCode;                                
            ++pSrcCell;                                   
            ++pDst;                                       
            ++nCol;                                       
        } while (nCol < g_nColumnsPerRow);                

        ++nRow;                                           
    } while (nRow < g_nRowCount);                         

    return SequenceGridMarkingSweepsUntilMatchAndDispatch(0);
}
#endif /* NW_UNIT_MapGridTilesByFeAndCallerSentinelToDisplayZeroTwoThree */

/* ==========================================================================
 * Unit: MapGridTilesViaBitmapToDisplayGrid
 * Source: grid/MapGridTilesViaBitmapToDisplayGrid.c
 * ========================================================================== */
#ifdef NW_UNIT_MapGridTilesViaBitmapToDisplayGrid
/*
 * Map Grid Tiles Via Bitmap To Display Grid.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-mapgridtilesviabitmaptodisplaygrid-c
 */

#include <windows.h>
#include "nwglobals.h"
#include <stdint.h>

/* ---- grid geometry and buffers (32-bit PE globals) ----------------------
 * Read as zero in the file image (runtime state), verified by read_memory. */

/* Destination: the 1-byte-per-cell display grid. Same base the conductor
 * seeds its target cell from (src/grid/SequenceGridMarkingSweepsUntilMatchAndDispatch.c). */
extern BYTE *gpGridBase;            

/* Loop bounds: inner (columns) and outer (rows). */

/* Source: a flat 16-bit-per-cell tile table, row-major, indexed by
 * cell = row * 2^shift + column. */
extern WORD *gpTileCellTable;       

/* ---- the conductor this function dispatches last ------------------------
 * Declared to match its own file: __stdcall, one BYTE mode argument. Here it
 * is always called with mode 0 (PUSH 0x0). The return is unused here. */
extern int __stdcall SequenceGridMarkingSweepsUntilMatchAndDispatch(BYTE bMode); 

#define TILE_INDEX_SENTINEL  ((BYTE)0xFE)  /* low byte that bypasses the bitmap */
#define DISPLAY_BIT_SET      ((BYTE)0x02)  /* CF=0 outcome */
#define DISPLAY_BIT_CLEAR    ((BYTE)0x03)  /* CF=1 outcome */
#define DISPLAY_SENTINEL     ((BYTE)0x00)  /* 0xfe outcome */

void __stdcall MapGridTilesViaBitmapToDisplayGrid(BYTE *pTileBitmap)
{
    BYTE *pDst = gpGridBase;              
    int   nRow = 0;

    do {                                          
        /* src cell = row * 2^shift cells into gpTileCellTable; WORD* arithmetic
         * reproduces LEA EDI,[ECX + (row<<shift)*2] exactly. */
        WORD *pSrcCell = gpTileCellTable + (nRow << g_nTileTableRowShift); 
        int   nCol = 0;

        do {                                           
            BYTE bTile = (BYTE)(*pSrcCell);   
            BYTE bCode;

            if (bTile == TILE_INDEX_SENTINEL) {            
                bCode = DISPLAY_SENTINEL;                  
            } else {

                DWORD dwBit    = 1u << (bTile & 7u);        
                DWORD dwByte   = (DWORD)pTileBitmap[bTile >> 3]; 
                DWORD dwMasked = dwBit & dwByte;            
                DWORD dwCF     = (dwMasked < 1u) ? 1u : 0u; 
                DWORD dwSel    = (0u - dwCF) & 1u;          
                bCode = (BYTE)(dwSel + 2u);                 
            }

            *pDst = bCode;                                
            ++pSrcCell;                                   
            ++pDst;                                       
            ++nCol;                                       
        } while (nCol < g_nColumnsPerRow);                
        ++nRow;                                           
    } while (nRow < g_nRowCount);                         

    /* Tail dispatch: mode 0. No coordinate write and no byte-2 stamp precede it. */
    (void)SequenceGridMarkingSweepsUntilMatchAndDispatch(0);     
}
#endif /* NW_UNIT_MapGridTilesViaBitmapToDisplayGrid */

/* ==========================================================================
 * Unit: MapGridTilesViaSentinelFeToDisplayGrid
 * Source: grid/MapGridTilesViaSentinelFeToDisplayGrid.c
 * ========================================================================== */
#ifdef NW_UNIT_MapGridTilesViaSentinelFeToDisplayGrid
/*
 * Map Grid Tiles Via Sentinel Fe To Display Grid.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-mapgridtilesviasentinelfetodisplaygrid-c
 */

#include <windows.h>
#include "nwglobals.h"
#include <stdint.h>

/* ---- grid geometry and buffers (32-bit PE globals) ----------------------
 * All read as zero in the file image (runtime state), verified by read_memory.
 * Names follow the sibling src/grid/MapGridTilesViaBitmapToDisplayGrid.c so the family
 * reads consistently. */

/* Destination: the 1-byte-per-cell display grid. Same base the conductor seeds
 * its target cell from and the marking sweeps walk. */
extern BYTE *gpGridBase;            


/* Source: a flat 16-bit-per-cell tile table, row-major, indexed by
 * cell = (row << shift) + column. */
extern WORD *gpTileCellTable;       


/* ---- the conductor this function dispatches last ------------------------
 * Declared to match its own file: __stdcall, one BYTE mode argument. Always
 * called here with mode 0 (PUSH 0x0). Return value unused. */
extern int __stdcall SequenceGridMarkingSweepsUntilMatchAndDispatch(BYTE bMode); 

#define TILE_INDEX_SENTINEL  ((BYTE)0xFE)  /* low byte that renders as empty */
#define DISPLAY_SENTINEL     ((BYTE)0x00)  /* 0xfe outcome                   */
#define DISPLAY_NON_SENTINEL ((BYTE)0x03)  /* any other low byte             */
#define LOCATED_CELL_STAMP   ((BYTE)0x02)  /* written into the located cell  */

void MapGridTilesViaSentinelFeToDisplayGrid(void)
{
    BYTE *pDst = gpGridBase;              
    int   nRow;

    for (nRow = 0; nRow < g_nRowCount; ++nRow) {          

        WORD *pSrcCell = gpTileCellTable + (nRow << g_nTileTableRowShift);
        int   nCol;

        for (nCol = 0; nCol < g_nColumnsPerRow; ++nCol) { 
            BYTE bTile = (BYTE)(*pSrcCell);               
            BYTE bCode;

            if (bTile == TILE_INDEX_SENTINEL) {           
                bCode = DISPLAY_SENTINEL;                 
            } else {
                bCode = DISPLAY_NON_SENTINEL;             
            }

            *pDst = bCode;                                
            ++pSrcCell;                                   
            ++pDst;                                       
        }
    }

    *(BYTE *)((intptr_t)gpGridBase
              + (intptr_t)g_nLocatedCellY * (intptr_t)g_nColumnsPerRow
              + (intptr_t)g_nLocatedCellX) = LOCATED_CELL_STAMP;

    (void)SequenceGridMarkingSweepsUntilMatchAndDispatch(0);
}
#endif /* NW_UNIT_MapGridTilesViaSentinelFeToDisplayGrid */

/* ==========================================================================
 * Unit: MapTileBitmapBitToGridZeroOrThree
 * Source: grid/MapTileBitmapBitToGridZeroOrThree.c
 * ========================================================================== */
#ifdef NW_UNIT_MapTileBitmapBitToGridZeroOrThree
/*
 * Sweeps the 16-bit grid-cell table, maps each cell's low-byte tile index through one bit of the caller's bitmap to a grid byte of 0 (bit SET) or 3...
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-maptilebitmapbittogridzeroorthree-c
 */

#include <windows.h>
#include "nwglobals.h"
#include <stdint.h>

/* ---- grid geometry, cell table, buffer (32-bit PE globals) --------------- */
extern BYTE    *gpGridBase;       
extern BYTE    *g_pCellEntryBase; 

/* ---- the located start coordinate, READ ONLY here ------------------------ */

/* ---- the one callee: the sweep conductor, __stdcall, one BYTE mode arg ---- */
extern int __stdcall SequenceGridMarkingSweepsUntilMatchAndDispatch(BYTE bMode); 

#define STAMP_CELL_VALUE ((BYTE)0x02)  

int __stdcall MapTileBitmapBitToGridZeroOrThree(const BYTE *pTileBitmap)
{
    BYTE *pDst = gpGridBase;      
    int   nRow = 0;              

    /* Both loops are bottom-tested on the machine (JL back-edges), so a
     * do/while is the faithful shape. */
    do {

        const BYTE *pSrc = g_pCellEntryBase
                         + ((uintptr_t)nRow << (g_dwAxisShift & 0x1f)) * 2u;
        int nCol = 0;            

        do {

            unsigned int uTile = (unsigned int)*pSrc;

            unsigned int uMaskedBit =
                (1u << (uTile & 7u)) & (unsigned int)pTileBitmap[uTile >> 3];

            BYTE bDisplay = (uMaskedBit == 0u) ? (BYTE)3 : (BYTE)0;

            *pDst++ = bDisplay;  

            pSrc += 2;           
            ++nCol;              
        } while (nCol < g_nRowStride);   

        ++nRow;                  
    } while (nRow < g_nRowCount);    

    gpGridBase[(intptr_t)g_nStartRow * (intptr_t)g_nRowStride
               + (intptr_t)g_nStartCol] = STAMP_CELL_VALUE;

    return SequenceGridMarkingSweepsUntilMatchAndDispatch((BYTE)0);
}
#endif /* NW_UNIT_MapTileBitmapBitToGridZeroOrThree */

/* ==========================================================================
 * Unit: SelectAndDispatchGridRenderMappingByCommand
 * Source: grid/SelectAndDispatchGridRenderMappingByCommand.c
 * ========================================================================== */
#ifdef NW_UNIT_SelectAndDispatchGridRenderMappingByCommand
/*
 * Picks a per-slot render bitmap out of a 16-byte-strided record table, re-seeds the axis span.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-selectanddispatchgridrendermappingbycommand-c
 */

#include <windows.h>
#include "nwglobals.h"
#include <stdint.h>


/* ---- probe globals (runtime state; all zero in the on-disk image) --------
 * Each is used as a BIT ADDRESS into the record bitmap: bit number = (value & 7),
 * byte index = (value >> 3). Their MEANINGS are not established by these bytes. */

extern void __stdcall InitializeAxisSpanFromSavedPoint(char cSelectCode);              
extern int  __stdcall ExpandCellTileIndexBitmapBitsToGridBytes(const BYTE *pBitmap);   
extern void           MapGridTilesViaSentinelFeToDisplayGrid(void);                    
extern void __stdcall MapGridTilesViaBitmapToDisplayGrid(BYTE *pTileBitmap);           
extern int  __stdcall MapTileBitmapBitToGridZeroOrThree(const BYTE *pTileBitmap);      
extern int            ClassifyAxisSpanOrientation(void);                               

#define CMD_SELECT_PAIR_A     0x82u   
#define CMD_SELECT_PAIR_B     0x83u   
#define CMD_GATE_B            0x0eu   
#define SELECTOR_BITMAP       0xFFu   /* ARG1 = 0xff : the bitmap/sentinel mapper            */
#define SELECTOR_SENTINEL_FE  0xFEu   /* ARG1 = 0xfe : the 0xfe-sentinel mapper              */

int __stdcall SelectAndDispatchGridRenderMappingByCommand(BYTE bMapperSelect, BYTE bRecordIndex, BYTE bCommand)
{
    const BYTE *pBitmap;
    int         bProceedToDispatch;

    InitializeAxisSpanFromSavedPoint((char)bCommand);

    pBitmap = (const BYTE *)(uintptr_t)nw_guest_to_host(
              *(const uint32_t *)((uintptr_t)nw_guest_to_host(
                                      (uint32_t)g_dwGridCtxRecordBase +
                                      ((uint32_t)bRecordIndex << 4))));

    if ((bCommand == CMD_SELECT_PAIR_A) || (bCommand == CMD_SELECT_PAIR_B)) {

        bProceedToDispatch =
            (bCommand == g_bProbeAExpect) &&
            ((((1u << ((unsigned int)g_bProbeAIndex & 7u))
               & (unsigned int)pBitmap[(unsigned int)g_bProbeAIndex >> 3]) != 0u));
    }
    else if (bCommand == CMD_GATE_B) {

        bProceedToDispatch =
            ((((1u << ((unsigned int)g_bProbeBIndex & 7u))
               & (unsigned int)pBitmap[(unsigned int)g_bProbeBIndex >> 3]) != 0u));
    }
    else {

        if (bMapperSelect == SELECTOR_BITMAP) {
            return ExpandCellTileIndexBitmapBitsToGridBytes(pBitmap);   
        }
        /* void callee (its file types it void): the machine passes this callee's leftover EAX out. */
        MapGridTilesViaBitmapToDisplayGrid((BYTE *)pBitmap);           
        return 0;
    }

    if (!bProceedToDispatch) {
        return 0;   
    }

    if (bMapperSelect == SELECTOR_BITMAP) {          
        return ClassifyAxisSpanOrientation();        
    }
    if (bMapperSelect == SELECTOR_SENTINEL_FE) {     
        MapGridTilesViaSentinelFeToDisplayGrid();    
        return 0;                                    /* void callee: machine returns its leftover EAX */
    }

    return MapTileBitmapBitToGridZeroOrThree(pBitmap);   
}
#endif /* NW_UNIT_SelectAndDispatchGridRenderMappingByCommand */

/* ==========================================================================
 * Unit: SelectAndDispatchGridRenderMappingBySentinelGroup
 * Source: grid/SelectAndDispatchGridRenderMappingBySentinelGroup.c
 * ========================================================================== */
#ifdef NW_UNIT_SelectAndDispatchGridRenderMappingBySentinelGroup
/*
 * Select And Dispatch Grid Render Mapping By Sentinel Group.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-selectanddispatchgridrendermappingbysentinelgroup-c
 */

#include <windows.h>
#include "nwglobals.h"
#include <stdint.h>

/* ---- sentinel-group globals (runtime state; all read 0 in the on-disk image,
 *      verified by read_memory). These are the SAME three addresses the sibling
 *      dispatcher uses -- but here they are read as plain SENTINEL/GUARD BYTES,
 *      not as bitmap bit addresses. Their meanings are not further established
 *      by these bytes; only their use as comparison targets is proven. */

extern void __stdcall InitializeAxisSpanFromSavedPoint(char cSelectCode);              
extern int            ClassifyAxisSpanOrientation(void);                               
extern int  __stdcall MapGridCellsAgainstCallerSentinelToTwoOnMatchElseZero(BYTE bSentinel); 
extern int  __stdcall MapGridTilesByFeAndCallerSentinelToDisplayZeroTwoThree(BYTE bSentinel); 
extern void           MapGridTilesViaSentinelFeToDisplayGrid(void);                    
extern int  __stdcall MapGridCellsAgainstCallerSentinelToDisplayByte(BYTE bSentinel);  

#define CMD_RENDER_A      0x82u   
#define CMD_RENDER_B      0x83u   
#define CMD_RENDER_C      0x0eu   
#define SELECTOR_CLASSIFY 0xFFu   /* ARG1 = 0xff : classify the span (inner) / two-on-match mapper (default) */
#define SELECTOR_SENTINEL_FE 0xFEu/* ARG1 = 0xfe : the fixed-0xfe mapper (inner only)                          */
#define SENTINEL_ANY      0xFFu   /* ARG2 = 0xff : accepted by both sentinel-group gates                     */

int __stdcall SelectAndDispatchGridRenderMappingBySentinelGroup(BYTE bMapperSelector, BYTE bSentinelByte, BYTE bCommand)
{

    InitializeAxisSpanFromSavedPoint((char)bCommand);

    if ((bCommand == CMD_RENDER_A) || (bCommand == CMD_RENDER_B)) {

        if ((bCommand != g_bCommandGuardByte) ||
            ((bSentinelByte != SENTINEL_ANY) && (bSentinelByte != g_bSentinelGroupA))) {
            return 0;   
        }
    }
    else if (bCommand == CMD_RENDER_C) {

        if ((bSentinelByte != SENTINEL_ANY) && (bSentinelByte != g_bSentinelGroupB)) {
            return 0;   
        }
    }
    else {

        if (bMapperSelector == SELECTOR_CLASSIFY) {
            return MapGridCellsAgainstCallerSentinelToTwoOnMatchElseZero(bSentinelByte); 
        }
        return MapGridTilesByFeAndCallerSentinelToDisplayZeroTwoThree(bSentinelByte);    
    }

    if (bMapperSelector == SELECTOR_CLASSIFY) {       
        return ClassifyAxisSpanOrientation();         
    }
    if (bMapperSelector == SELECTOR_SENTINEL_FE) {    
        MapGridTilesViaSentinelFeToDisplayGrid();     
        return 0;                                     /* void callee: machine passes its leftover EAX out */
    }

    return MapGridCellsAgainstCallerSentinelToDisplayByte(bSentinelByte);  
}
#endif /* NW_UNIT_SelectAndDispatchGridRenderMappingBySentinelGroup */
