/*
 * GridMarking -- grid marking, path routing, and cursor wrapping.
 *
 * Module: grid
 * Interface: include/GridMarking.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   AnimateScrollTowardSelectedGridCell                        grid/AnimateScrollTowardSelectedGridCell.c
 *   ApplyDeltaToUnmarkedEntries                                grid/ApplyDeltaToUnmarkedEntries.c
 *   ClassifyAxisSpanOrientation                                grid/ClassifyAxisSpanOrientation.c
 *   ComputeScaledGridOffset                                    grid/ComputeScaledGridOffset.c
 *   MarkEmptyGridCellsAdjacentToTypeFourAndLocateTypeTwo       grid/MarkEmptyGridCellsAdjacentToTypeFourAndLocateTypeTwo.c
 *   MarkZeroCellsAdjacentToFiveCellsWithSixAndRecordTwo        grid/MarkZeroCellsAdjacentToFiveCellsWithSixAndRecordTwo.c
 *   MarkZeroCellsAdjacentToSixCellsAndRecordTwo                grid/MarkZeroCellsAdjacentToSixCellsAndRecordTwo.c
 *   RouteGridPathToTargetCell                                  grid/RouteGridPathToTargetCell.c
 *   SequenceGridMarkingSweepsUntilMatchAndDispatch             grid/SequenceGridMarkingSweepsUntilMatchAndDispatch.c
 *   WrapGridAxisCursorsStampCellKeyAndRetireSpans              grid/WrapGridAxisCursorsStampCellKeyAndRetireSpans.c
 */

/* ==========================================================================
 * Unit: AnimateScrollTowardSelectedGridCell
 * Source: grid/AnimateScrollTowardSelectedGridCell.c
 * ========================================================================== */
#ifdef NW_UNIT_AnimateScrollTowardSelectedGridCell
/*
 * Animate Scroll Toward Selected Grid Cell.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-animatescrolltowardselectedgridcell-c
 */

#include <stdint.h>
#include "nwglobals.h"

typedef struct {
    uint8_t pad0[4];  /* +0x00..+0x03 record header, not read here */
    uint8_t b04;      /* +0x04 */
    uint8_t b05;      /* +0x05 */
    uint8_t col;      /* +0x06, multiplied by the cell width  */
    uint8_t row;      /* +0x07, multiplied by the cell height */
    uint8_t pad[8];   /* +0x08..+0x0f */
} GridCellTableEntry;

/* The cell table is walked DOWNWARD from this pointer: the code computes
 * base - index*0x10, so the stored records live below the pointer. */

/* Both callees clean their own arguments: there is no ADD ESP after either call
 * and the frame offsets below are unshifted once the calls return. */

extern int ScrollCachedMainWindowByDeltaAndRepaint(int nYAmount, int nXAmount);
extern int CopySpriteRowsToDisplayBuffer(void);

static int32_t imin(int32_t a, int32_t b)
{
    return (a < b) ? a : b;
}

static int32_t ease_step(int32_t step, int32_t remaining)
{
    if (remaining == 0) {
        return step;
    }
    if (remaining > (int32_t)g_wBounceThreshold) {
        return step;
    }
    step += g_wBounceDelta;
    return (step > 1) ? step : 1;
}

int32_t AnimateScrollTowardSelectedGridCell(void)
{
    GridCellTableEntry *rec;
    int32_t targetX, targetY;         /* ESI / EBX: the selected cell in pixels */
    int32_t limitXNeg, boundXPos;     /* left viewport edge,   X travel seeds  */
    int32_t boundYNear, limitYFar;    /* vertical edges,       Y travel seeds  */
    int32_t travelXNeg, travelXPos;   /* [ESP+0x14] / [ESP+0x18]              */
    int32_t travelYNeg, travelYPos;   /* [ESP+0x1c] / [ESP+0x20]              */
    int32_t stepXNeg, stepXPos;       /* [ESP+0x28] / [ESP+0x2c]              */
    int32_t stepYNeg, stepYPos;       /* [ESP+0x30] / [ESP+0x34]              */
    int32_t stepCapXNeg, stepCapXPos; 
    int32_t stepCapYNeg, stepCapYPos; /* step size, decayed by ease_step()     */

    if (g_wCellIndex != 0xFFFFu) {
        rec       = &g_pCellTableEnd[-(int32_t)g_wCellIndex];
        targetX   = (int32_t)rec->col * g_dwCellWidth;
        targetY   = (int32_t)rec->row * g_dwCellHeight;

        boundXPos  = g_dwClientLeft + g_dwScrollX
                   - ((int32_t)g_bColFar + 1) * g_dwCellWidth;
        boundYNear = (int32_t)g_bRowFar * g_dwCellHeight + g_dwScrollY;
        limitYFar  = g_dwClientTop + g_dwScrollY
                   - ((int32_t)g_bRowOrigin + 1) * g_dwCellHeight;
        limitXNeg  = (int32_t)g_bColOrigin * g_dwCellWidth + g_dwScrollX;

        travelXNeg = 0;
        travelXPos = 0;
        travelYNeg = 0;
        travelYPos = 0;

        if (limitXNeg > targetX) {
            travelXNeg = limitXNeg - targetX;
            if (travelXNeg > g_dwScrollX) {
                travelXNeg = g_dwScrollX;
            }

        } else if (targetX > boundXPos) {
            travelXPos = targetX - boundXPos;
            if (travelXPos > g_dwViewWidth - g_dwClientLeft - g_dwScrollX) {
                travelXPos = g_dwViewWidth - g_dwClientLeft - g_dwScrollX;
            }
        }

        if (targetY < boundYNear) {
            travelYNeg = boundYNear - targetY;
            if (travelYNeg > g_dwScrollY) {
                travelYNeg = g_dwScrollY;
            }
        } else if (targetY > limitYFar) {
            travelYPos = targetY - limitYFar;
            if (travelYPos > g_dwViewHeight - g_dwClientTop - g_dwScrollY) {
                travelYPos = g_dwViewHeight - g_dwClientTop - g_dwScrollY;
            }
        }

        if ((travelXNeg | travelXPos | travelYNeg | travelYPos) != 0) {
            stepCapXNeg = g_wMaxStepPixels;
            stepCapXPos = g_wMaxStepPixels;
            stepCapYNeg = g_wMaxStepPixels;
            stepCapYPos = g_wMaxStepPixels;

            for (;;) {

                stepXNeg = imin(travelXNeg, stepCapXNeg);
                stepXPos = imin(travelXPos, stepCapXPos);
                stepYNeg = imin(travelYNeg, stepCapYNeg);
                stepYPos = imin(travelYPos, stepCapYPos);

                g_dwScrollX += stepXPos - stepXNeg;
                g_dwScrollY += stepYPos - stepYNeg;

                ScrollCachedMainWindowByDeltaAndRepaint(stepYNeg - stepYPos,
                                                        stepXNeg - stepXPos);

                CopySpriteRowsToDisplayBuffer();

                travelXNeg -= stepXNeg;
                stepCapXNeg = ease_step(stepCapXNeg, travelXNeg);
                travelXPos -= stepXPos;
                stepCapXPos = ease_step(stepCapXPos, travelXPos);
                travelYNeg -= stepYNeg;
                stepCapYNeg = ease_step(stepCapYNeg, travelYNeg);
                travelYPos -= stepYPos;
                stepCapYPos = ease_step(stepCapYPos, travelYPos);

                if ((travelXNeg | travelXPos | travelYNeg | travelYPos) == 0) {
                    break;
                }
            }
        }
    }

    CopySpriteRowsToDisplayBuffer();
    return 1;
}
#endif /* NW_UNIT_AnimateScrollTowardSelectedGridCell */

/* ==========================================================================
 * Unit: ApplyDeltaToUnmarkedEntries
 * Source: grid/ApplyDeltaToUnmarkedEntries.c
 * ========================================================================== */
#ifdef NW_UNIT_ApplyDeltaToUnmarkedEntries
/*
 * Apply Delta To Unmarked Entries.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-applydeltatounmarkedentries-c
 */

#include <windows.h>   /* DWORD */

/* Caller-supplied 16-byte record, reconstructed from the two offsets the code touches:
   flags byte at +0 (bit 1 == "skip"), accumulate field DWORD at +0xC. */
typedef struct NW_ENTRY_TAG {
    BYTE  bFlags;            /* +0x00 — bit 1 (0x2) set => entry skipped           */
    BYTE  abPad1[11];        /* +0x01 .. +0x0B — never read by this function         */
    DWORD dwAccumulator;     /* +0x0C — receives nDelta on unmarked entries         */
} NW_ENTRY;                  /* total size 0x10, matching the 0x10-byte stride      */

void __stdcall ApplyDeltaToUnmarkedEntries(int nDelta, int nEntryCount, BYTE *pEntries)
{
    int i;

    for (i = 0; i < nEntryCount; i++, pEntries += sizeof(NW_ENTRY)) {
        NW_ENTRY *pEntry = (NW_ENTRY *)(void *)pEntries;

        if ((pEntry->bFlags & 2) == 0) {
            pEntry->dwAccumulator += (DWORD)nDelta;
        }
    }
}
#endif /* NW_UNIT_ApplyDeltaToUnmarkedEntries */

/* ==========================================================================
 * Unit: ClassifyAxisSpanOrientation
 * Source: grid/ClassifyAxisSpanOrientation.c
 * ========================================================================== */
#ifdef NW_UNIT_ClassifyAxisSpanOrientation
/*
 * Classify Axis Span Orientation.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-classifyaxisspanorientation-c
 */

extern int g_iCoordHStart;

extern int g_dwCoordHEnd;

extern int g_iCoordVStart;

extern int g_dwCoordVEnd;

extern unsigned int g_dwOrientCodeA;

extern unsigned int g_dwOrientCodeB;

extern unsigned char g_bOrientChanged;

int ClassifyAxisSpanOrientation(void)
{
    int iAbsH;   /* horizontal span magnitude |dx| (non-negative after abs)          */
    int iAbsV;   /* vertical span magnitude  |dy| (non-negative after abs)           */

    iAbsH = g_iCoordHStart - g_dwCoordHEnd;      
    if (iAbsH < 0) { iAbsH = -iAbsH; }          /* |span| — matches TEST/JG + NEG   */

    iAbsV = g_iCoordVStart - g_dwCoordVEnd;      
    if (iAbsV < 0) { iAbsV = -iAbsV; }          /* |span|                            */

    if ((iAbsH == 0) && (iAbsV == 0)) {         /* degenerate span: nothing to do   */
        return 0;
    }

    if ((iAbsH == 0) || (iAbsV / iAbsH > 3)) {           /* branch A — tall or no width */
        if (iAbsV > 1) {                                 /* primary code, gated by V span > 1 */
            g_dwOrientCodeA = (g_dwCoordVEnd <= g_iCoordVStart) ? 0u : 4u;   /* c9c */
        }
        g_dwOrientCodeB = (g_dwCoordVEnd <= g_iCoordVStart) ? 0u : 4u;       /* ca0, unconditional in A */
    } else if ((iAbsV == 0) || (iAbsH / iAbsV > 3)) {    /* branch B — wide or no height */
        if (iAbsH > 1) {                                 /* primary code, gated by H span > 1 */
            g_dwOrientCodeA = (g_dwCoordHEnd <= g_iCoordHStart) ? 6u : 2u;   /* c9c */
        }
        g_dwOrientCodeB = (g_dwCoordHEnd <= g_iCoordHStart) ? 6u : 2u;       /* ca0, unconditional in B */
    } else {                                             /* branch C — balanced aspect ratio */
        if (g_iCoordHStart < g_dwCoordHEnd) {             /* c8c < c94                       */
            g_dwOrientCodeA = 3u;                        /* c9c                              */
            if (g_dwCoordVEnd <= g_iCoordVStart) {         /* c98 <= c90                      */
                g_dwOrientCodeA = 1u;
            }
        } else {                                          /* c8c >= c94                     */
            g_dwOrientCodeA = 5u;                        /* c9c                              */
            if (g_dwCoordVEnd <= g_iCoordVStart) {         /* c98 <= c90                      */
                g_dwOrientCodeA = 7u;
            }
        }

        if (iAbsV < iAbsH) {                             /* V span smaller than H span      */
            g_dwOrientCodeB = 2u;                        /* ca0                              */
            if (g_dwCoordHEnd <= g_iCoordHStart) {         /* c94 <= c8c                      */
                g_dwOrientCodeB = 6u;
            }
        } else {                                          /* V span >= H span              */
            g_dwOrientCodeB = 4u;                        /* ca0                              */
            if (g_dwCoordVEnd <= g_iCoordVStart) {         /* c98 <= c90                      */
                g_dwOrientCodeB = 0u;
            }
        }
    }

    g_bOrientChanged = 1u;                               
    return 1;
}
#endif /* NW_UNIT_ClassifyAxisSpanOrientation */

/* ==========================================================================
 * Unit: ComputeScaledGridOffset
 * Source: grid/ComputeScaledGridOffset.c
 * ========================================================================== */
#ifdef NW_UNIT_ComputeScaledGridOffset
/*
 * Compute Scaled Grid Offset.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-computescaledgridoffset-c
 */

#include <stdint.h>   /* intptr_t for the 32-bit anchor-base widening cast (r42) */

typedef struct {
    uint8_t       bFlags;        /* +0x00 */
    uint8_t       bLerpLevel;    /* +0x01 */
    uint8_t       bLerpDivisor;  /* +0x02 */
    uint8_t       _pad3;         /* +0x03 */
    uint8_t       bGridX;        /* +0x04 */
    uint8_t       bGridY;        /* +0x05 */
    uint16_t      wAnchorIndex;  /* +0x06 */
    int16_t       iOffsetX;      /* +0x08 output */
    int16_t       iOffsetY;      /* +0x0A output */
    uint32_t      pRefRecord;    /* +0x0C guest pointer */
} GridPositionRec;

typedef struct {
    uint32_t      pSourceWords;  /* +0x00 guest pointer -> two int16 words */
    int16_t       iBaseX;        /* +0x04 signed baseline X */
    int16_t       iBaseY;        /* +0x06 signed baseline Y */
} GridRefRec;

extern int32_t       g_dwScaleX;           
extern int32_t       g_dwScaleY;           

extern uintptr_t     g_pSlotsTop;          /* native pointer side-table slot */

void __stdcall ComputeScaledGridOffset(GridPositionRec *) __asm__("_ComputeScaledGridOffset");
void __stdcall ComputeScaledGridOffset(GridPositionRec *pRec)
{
    const GridRefRec *pRef =
        (const GridRefRec *)(uintptr_t)nw_guest_to_host(pRec->pRefRecord);
    const int16_t    *piSrc =
        (const int16_t *)(uintptr_t)nw_guest_to_host(pRef->pSourceWords);
    uint32_t          uAccX, uAccY;                                     /* ESP-relative slots 0x14/0x18 */
    int32_t           sSubX, sSubY;

    uAccX = (uint32_t)pRec->bGridX * (uint32_t)g_dwScaleX;             
    uAccY = (uint32_t)pRec->bGridY * (uint32_t)g_dwScaleY;             

    /* Anchor blend: only when wAnchorIndex is not FFFF and the entry's byte0 has bit2 set.
     * The slot chain is indexed downward from a native host pointer, so the
     * anchor arithmetic must stay pointer-width. */
    if ((pRec->wAnchorIndex != 0xFFFFu) &&
        ((*(const uint8_t *)(uintptr_t)(g_pSlotsTop -
                                       (uintptr_t)(pRec->wAnchorIndex << 4))) & 4u) != 0u) {
        const uint8_t *pbEntry = (const uint8_t *)(uintptr_t)(g_pSlotsTop -
                                (uintptr_t)(pRec->wAnchorIndex << 4)); /* base - idx*16 */
        int32_t sDx = (int32_t)(pbEntry[6] - pRec->bGridX);            
        int32_t sDy = (int32_t)(pbEntry[7] - pRec->bGridY);
        uint32_t ubLevelPlus1 = (uint32_t)pRec->bLerpLevel + 1u;       
        int32_t sQx = (sDx * g_dwScaleX * (int32_t)ubLevelPlus1) / (int32_t)pRec->bLerpDivisor; /* IDIV, signed */
        int32_t sQy = (sDy * g_dwScaleY * (int32_t)ubLevelPlus1) / (int32_t)pRec->bLerpDivisor;
        uAccX += (uint32_t)sQx;                                        /* ADD [slot],EAX */
        uAccY += (uint32_t)sQy;
    }

    /* Baseline subtraction: direct ref value, or source-word minus scale, then subtract ref baseline. */
    if ((pRec->bFlags & 0x08u) == 0u) {
        sSubX = pRef->iBaseX;                                          /* MOVSX ECX,[EBP+4] */
    } else {
        sSubX = (int32_t)*(const uint16_t *)(const void *)piSrc - g_dwScaleX;  /* MOVZX word; SUB dword scale */
        sSubX -= pRef->iBaseX;
    }

    if ((pRec->bFlags & 0x10u) == 0u) {
        sSubY = pRef->iBaseY;                                          /* MOVSX ECX,[EBP+6] */
    } else {
        sSubY = (int32_t)*(const uint16_t *)(const void *)(piSrc + 1) - g_dwScaleY; /* word at src+2 */
        sSubY -= pRef->iBaseY;
    }

    pRec->iOffsetX = (int16_t)(uAccX - (uint32_t)sSubX);               
    pRec->iOffsetY = (int16_t)(uAccY - (uint32_t)sSubY);               
}
#endif /* NW_UNIT_ComputeScaledGridOffset */

/* ==========================================================================
 * Unit: MarkEmptyGridCellsAdjacentToTypeFourAndLocateTypeTwo
 * Source: grid/MarkEmptyGridCellsAdjacentToTypeFourAndLocateTypeTwo.c
 * ========================================================================== */
#ifdef NW_UNIT_MarkEmptyGridCellsAdjacentToTypeFourAndLocateTypeTwo
/*
 * Sweeps the width-by-height byte grid, writing 5 into every empty (0) cell that touches a type-4 cell.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-markemptygridcellsadjacenttotypefourandlocatetypetwo-c
 */

#include <windows.h>

/* Grid geometry. All six of these live in uninitialised data and read back as
   0 in the file image, so their runtime values - and therefore the grid's real
   dimensions - are not recoverable from the binary alone. */
extern DWORD  gdwGridCellWidth;   
extern DWORD  gdwGridRowCount;    
extern BYTE  *gpbGridCellArray;   
extern DWORD  gdwGridRightLimit;  
extern DWORD  gdwGridUpOffset;    
extern DWORD  gdwGridDownLimit;   

/* Outputs, written only on the early exit that returns 1. */
extern DWORD  gdwLocatedCellX;    
extern DWORD  gdwLocatedCellY;    

#define CELL_EMPTY         0x00
#define CELL_TYPE_TWO      0x02
#define CELL_NEIGHBOUR     0x04
#define CELL_MARKED_EMPTY  0x05
#define RESULT_NONE        0x00
#define RESULT_MARKED      0x05
#define RESULT_LOCATED     0x01

unsigned char __cdecl MarkEmptyGridCellsAdjacentToTypeFourAndLocateTypeTwo(void)
{
    BYTE  *pCell = gpbGridCellArray;
    DWORD  dwX   = 0;                     /* ECX - column index */
    DWORD  dwY   = 0;                     /* EBX - row index    */
    BYTE   bResult = RESULT_NONE;         

    /* Widened once so the pointer arithmetic below stays sign-correct and
       64-bit-clean under mingw. */
    const long lStride   = (long)gdwGridCellWidth;
    const long lUpOffset = (long)gdwGridUpOffset;

    for (;;) {
        if (*pCell == CELL_EMPTY) {
            /* Mark this empty cell if any of its four sides is a type-4 cell.
               The x guards bound the horizontal steps and the y guards bound
               the vertical ones, so no read can walk off the grid. The tests
               short-circuit in the order the asm chains them. */
            if ((dwX != 0                     && pCell[-1]      == CELL_NEIGHBOUR) ||
                (dwX <  gdwGridRightLimit     && pCell[ 1]      == CELL_NEIGHBOUR) ||
                (dwY != 0                     && pCell[lUpOffset] == CELL_NEIGHBOUR) ||
                (dwY <  gdwGridDownLimit      && pCell[lStride]   == CELL_NEIGHBOUR)) {
                bResult = RESULT_MARKED;
                *pCell  = CELL_MARKED_EMPTY;
            }
        }
        else if (*pCell == CELL_TYPE_TWO) {
            /* Same four-sided test, but a hit is the answer to the search:
               record where it is and leave immediately with 1. */
            if ((dwX != 0                     && pCell[-1]      == CELL_NEIGHBOUR) ||
                (dwX <  gdwGridRightLimit     && pCell[ 1]      == CELL_NEIGHBOUR) ||
                (dwY != 0                     && pCell[lUpOffset] == CELL_NEIGHBOUR) ||
                (dwY <  gdwGridDownLimit      && pCell[lStride]   == CELL_NEIGHBOUR)) {
                gdwLocatedCellX = dwX;
                gdwLocatedCellY = dwY;
                return RESULT_LOCATED;
            }
        }

        /* Advance one cell, row-major. The x counter is the inner loop; when it
           runs off the row the y counter advances and x wraps. Exhausting both
           is the only way to reach the first RET. */
        dwX++;
        if (dwX >= gdwGridCellWidth) {
            dwX = 0;
            dwY++;
            if (dwY >= gdwGridRowCount) {
                return bResult;
            }
        }
        pCell++;
    }
}
#endif /* NW_UNIT_MarkEmptyGridCellsAdjacentToTypeFourAndLocateTypeTwo */

/* ==========================================================================
 * Unit: MarkZeroCellsAdjacentToFiveCellsWithSixAndRecordTwo
 * Source: grid/MarkZeroCellsAdjacentToFiveCellsWithSixAndRecordTwo.c
 * ========================================================================== */
#ifdef NW_UNIT_MarkZeroCellsAdjacentToFiveCellsWithSixAndRecordTwo
/*
 * Sweeps the width-by-height byte grid row-major, rewriting every empty (0) cell that has a 5 cell on a bounds-checked 4-neighbour side to 6.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-markzerocellsadjacenttofivecellswithsixandrecordtwo-c
 */

#include <windows.h>
#include <stdint.h>

/* grid cell byte codes this body compares against / writes -------------------- */
#define CELL_EMPTY       0x00   
#define CELL_TYPE_TWO    0x02   
#define CELL_NEIGHBOUR   0x05   /* the eight CMP ...,0x05 neighbour tests */
#define CELL_MARKED      0x06   

extern BYTE *gpGridBase;        
extern DWORD gdwGridCellWidth;  
extern DWORD gdwGridRowCount;   
extern DWORD gdwGridRightLimit; 
extern DWORD gdwGridUpOffset;   
extern DWORD gdwGridDownLimit;  

/* the located coordinate pair -- written ONLY on the early exit that returns 1 */
extern DWORD gdwLocatedCellX;   
extern DWORD gdwLocatedCellY;   

#define RESULT_NONE    0x00
#define RESULT_MARKED  CELL_MARKED      /* 6 -- same value the cell is stamped with */
#define RESULT_LOCATED 0x01

unsigned char __cdecl MarkZeroCellsAdjacentToFiveCellsWithSixAndRecordTwo(void)
{
    BYTE  *pCell = gpGridBase;
    DWORD  dwX   = 0;                /* ECX - column counter */
    DWORD  dwY   = 0;                /* EBX - row counter    */
    BYTE   bResult = RESULT_NONE;    

    const long lStride   = (long)gdwGridCellWidth;
    const long lUpOffset = (long)gdwGridUpOffset;

    for (;;) {
        if (*pCell == CELL_EMPTY) {
            /* Mark this empty cell if any bounds-checked 4-neighbour is a 5 cell.
             * The tests short-circuit in the order the asm chains them. */
            if ((dwX != 0                      && pCell[-1]        == CELL_NEIGHBOUR) ||
                (dwX <  gdwGridRightLimit      && pCell[ 1]        == CELL_NEIGHBOUR) ||
                (dwY != 0                      && pCell[lUpOffset] == CELL_NEIGHBOUR) ||
                (dwY <  gdwGridDownLimit       && pCell[lStride]   == CELL_NEIGHBOUR)) {
                bResult = RESULT_MARKED;       
                *pCell  = CELL_MARKED;          
            }
        }
        else if (*pCell == CELL_TYPE_TWO) {

            if ((dwX != 0                      && pCell[-1]        == CELL_NEIGHBOUR) ||
                (dwX <  gdwGridRightLimit      && pCell[ 1]        == CELL_NEIGHBOUR) ||
                (dwY != 0                      && pCell[lUpOffset] == CELL_NEIGHBOUR) ||
                (dwY <  gdwGridDownLimit       && pCell[lStride]   == CELL_NEIGHBOUR)) {
                gdwLocatedCellX = dwX;
                gdwLocatedCellY = dwY;
                return RESULT_LOCATED;         
            }
        }

        dwX++;
        if (dwX >= gdwGridCellWidth) {        
            dwY++;
            if (dwY >= gdwGridRowCount) {     
                return bResult;               /* grid exhausted: 0 or 6 */
            }
            dwX = 0;                          
        }
        pCell++;
    }
}
#endif /* NW_UNIT_MarkZeroCellsAdjacentToFiveCellsWithSixAndRecordTwo */

/* ==========================================================================
 * Unit: MarkZeroCellsAdjacentToSixCellsAndRecordTwo
 * Source: grid/MarkZeroCellsAdjacentToSixCellsAndRecordTwo.c
 * ========================================================================== */
#ifdef NW_UNIT_MarkZeroCellsAdjacentToSixCellsAndRecordTwo
/*
 * Walks the byte grid in row-major order; a 0 cell touching a 6 cell on a bounds-checked 4-neighbour side is rewritten to 4.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-markzerocellsadjacenttosixcellsandrecordtwo-c
 */

#include <windows.h>
#include "nwglobals.h"
#include <stdint.h>

/* grid cell byte codes this body compares against / writes --------------------- */
#define CELL_ZERO  0x00        
#define CELL_TWO   0x02        
#define CELL_FOUR  0x04        
#define CELL_SIX   0x06        /* the neighbour value that triggers both actions */

extern BYTE *gpGridBase;       

/* the located coordinate pair -- written here only on the CELL_TWO exit */

BYTE MarkZeroCellsAdjacentToSixCellsAndRecordTwo(void)
{
    int   nCol = 0;            /* ECX, column counter */
    int   nRow = 0;            /* EBX, row counter    */
    BYTE  bResult = 0;         
    BYTE *pCell = gpGridBase;   /* EDX, advanced by pointer INC, not by index */

    for (;;) {
        if (*pCell == CELL_ZERO) {

            /* Four-neighbour test, in the exact order the body performs it.
             * Left and up are skipped at index 0; right and down are skipped at the
             * LAST index. That asymmetry is deliberate: it stops pCell[1] at the last
             * column and pCell[stride] at the last row from wrapping onto the next row. */
            if (((nCol != 0)                && (pCell[-1]            == CELL_SIX)) ||
                ((nCol < g_nGridColumnsM1)  && (pCell[1]             == CELL_SIX)) ||
                ((nRow != 0)                && (pCell[g_nCellUp]     == CELL_SIX)) ||
                ((nRow < g_nGridRowsM1)     && (pCell[g_nRowStride]  == CELL_SIX))) {

                bResult = CELL_FOUR;   
                *pCell  = CELL_FOUR;   
            }
        }
        else if (*pCell == CELL_TWO) {

            if (((nCol != 0)                && (pCell[-1]            == CELL_SIX)) ||
                ((nCol < g_nGridColumnsM1)  && (pCell[1]             == CELL_SIX)) ||
                ((nRow != 0)                && (pCell[g_nCellUp]     == CELL_SIX)) ||
                ((nRow < g_nGridRowsM1)     && (pCell[g_nRowStride]  == CELL_SIX))) {

                g_nCoordH = nCol;     
                g_nCoordV = nRow;     
                return 1;             
            }
        }

        nCol++;                               
        if (nCol >= g_nRowStride) {          
            nRow++;                           
            if (nRow >= g_nGridRowCount) {    
                return bResult;               /* grid exhausted: 0 or 4 */
            }
            nCol = 0;                         
        }
        pCell++;                              
    }
}
#endif /* NW_UNIT_MarkZeroCellsAdjacentToSixCellsAndRecordTwo */

/* ==========================================================================
 * Unit: RouteGridPathToTargetCell
 * Source: grid/RouteGridPathToTargetCell.c
 * ========================================================================== */
#ifdef NW_UNIT_RouteGridPathToTargetCell
/*
 * Walks a byte grid one 8-neighbour step at a time toward a target cell, preferring the axis with the larger remaining distance.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-routegridpathtotargetcell-c
 */

#include <windows.h>
#include "nwglobals.h"

/* grid geometry and buffer -------------------------------------------------- */
extern BYTE *gpGridBase;      

#define g_nGridColumns g_nGridColumnsM1
#define g_nGridRows    g_nGridRowsM1

/* cursor state owned by the caller ------------------------------------------ */

/* results ------------------------------------------------------------------ */

/* RouteGridPathToTargetCell
 *
 * bCellMatch selects which neighbouring cell value counts as "traversable"; the
 * same four values 4/5/6 are rotated into one another after every step, so the
 * caller can follow a band of alternating cell types.
 *
 * Returns 1 once the cursor has landed on the target cell, otherwise the
 * vertical distance (rows) still left before an adjacent matching cell could
 * not be found.
 */
int __stdcall RouteGridPathToTargetCell(BYTE bCellMatch)
{
    int nCurRow  = g_nStartRow;
    int nCurCol  = g_nStartCol;
    int nPrevRow = g_nStartRow;  /* STEP-code reference: seeded with the START cell */
    int nPrevCol = g_nStartCol;  
    int nPrev2Row = -1;          /* BEARING reference: the cell TWO steps back, -1 = none */
    int nPrev2Col = -1;          
    int nVertDist;
    int nColDist;
    int nDelta   = 0;
    BYTE *pCell  = gpGridBase + (nCurRow * g_nRowStride) + nCurCol;

    for (;;) {
        int nNextRow = nCurRow;
        int nNextCol = nCurCol;

        nVertDist = g_nTargetRow - nCurRow;
        if (nVertDist < 0) {
            nVertDist = -nVertDist;
        }
        nColDist = g_nTargetCol - nCurCol;
        if (nColDist < 0) {
            nColDist = -nColDist;
        }

        nDelta = 0;
        if (nVertDist < nColDist) {
            /* rows are closer together than columns: sweep columns first */
            if ((nCurCol > 0) && (pCell[-1] == bCellMatch)) {
                nNextCol = nCurCol - 1;
                nDelta   = -1;
            } else if ((nCurCol < g_nGridColumns) && (pCell[1] == bCellMatch)) {
                nNextCol = nCurCol + 1;
                nDelta   = 1;
            } else if ((nCurRow > 0) && (pCell[g_nCellUp] == bCellMatch)) {
                nNextRow = nCurRow - 1;
                nDelta   = g_nCellUp;
            } else if ((nCurRow < g_nGridRows) && (pCell[g_nRowStride] == bCellMatch)) {
                nNextRow = nCurRow + 1;
                nDelta   = g_nRowStride;
            }
        } else {
            /* same or the other way round: sweep rows first */
            if ((nCurRow > 0) && (pCell[g_nCellUp] == bCellMatch)) {
                nNextRow = nCurRow - 1;
                nDelta   = g_nCellUp;
            } else if ((nCurRow < g_nGridRows) && (pCell[g_nRowStride] == bCellMatch)) {
                nNextRow = nCurRow + 1;
                nDelta   = g_nRowStride;
            } else if ((nCurCol > 0) && (pCell[-1] == bCellMatch)) {
                nNextCol = nCurCol - 1;
                nDelta   = -1;
            } else if ((nCurCol < g_nGridColumns) && (pCell[1] == bCellMatch)) {
                nNextCol = nCurCol + 1;
                nDelta   = 1;
            }
        }

        if (nDelta == 0) {
            return nVertDist;   /* no matching neighbour anywhere */
        }

        pCell  += nDelta;
        nCurRow = nNextRow;
        nCurCol = nNextCol;

        if ((nCurCol == g_nTargetCol) && (nCurRow == g_nTargetRow)) {
            /* arrived: describe the step just taken */
            if (nCurCol < nPrevCol) {
                g_nStepCode = 2;
            } else if (nCurCol > nPrevCol) {
                g_nStepCode = 6;
            } else if (nCurRow < nPrevRow) {
                g_nStepCode = 4;
            } else {
                g_nStepCode = 0;
            }

            if (nPrev2Col != -1) {
                if (nCurRow < nPrev2Row) {
                    g_nBearingCode = (nCurCol < nPrev2Col) ? 3
                                    : (nCurCol > nPrev2Col) ? 5 : 4;
                } else if (nCurRow > nPrev2Row) {
                    g_nBearingCode = (nCurCol < nPrev2Col) ? 1
                                    : (nCurCol > nPrev2Col) ? 7 : 0;
                } else {
                    g_nBearingCode = (nCurCol < nPrev2Col) ? 2 : 6;
                }
            }

            g_bReachedCell = 1;
            return 1;
        }

        nPrev2Row = nPrevRow;
        nPrev2Col = nPrevCol;
        nPrevRow = nCurRow;
        nPrevCol = nCurCol;

        /* rotate the 4/5/6 cell kinds: 4 -> 6 -> 5 -> 4 */
        if (bCellMatch == 4) {
            bCellMatch = 6;
        } else if (bCellMatch == 5) {
            bCellMatch = 4;
        } else if (bCellMatch == 6) {
            bCellMatch = 5;
        }
    }
}
#endif /* NW_UNIT_RouteGridPathToTargetCell */

/* ==========================================================================
 * Unit: SequenceGridMarkingSweepsUntilMatchAndDispatch
 * Source: grid/SequenceGridMarkingSweepsUntilMatchAndDispatch.c
 * ========================================================================== */
#ifdef NW_UNIT_SequenceGridMarkingSweepsUntilMatchAndDispatch
/*
 * Seeds the target cell with a 6, then repeatedly runs the three grid-marking sweeps in order until one of them finds a type-2 cell.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-sequencegridmarkingsweepsuntilmatchanddispatch-c
 */

#include <windows.h>
#include "nwglobals.h"
#include <stdint.h>

extern BYTE *gpGridBase;       


extern unsigned char __cdecl MarkZeroCellsAdjacentToSixCellsAndRecordTwo(void);            
extern unsigned char __cdecl MarkEmptyGridCellsAdjacentToTypeFourAndLocateTypeTwo(void);   
extern unsigned char __cdecl MarkZeroCellsAdjacentToFiveCellsWithSixAndRecordTwo(void);    

/* ---- the two things a winning sweep can be dispatched to ----------------- */
extern int __cdecl     ClassifyAxisSpanOrientation(void);              

extern int __stdcall   RouteGridPathToTargetCell(BYTE bCellMatch);     

#define SEED_CELL_VALUE  ((BYTE)0x06)   
#define SWEEP1_BAND      ((BYTE)0x06)   
#define SWEEP2_BAND      ((BYTE)0x04)   
#define SWEEP3_BAND      ((BYTE)0x05)   

int __stdcall SequenceGridMarkingSweepsUntilMatchAndDispatch(BYTE bMode)
{

    BYTE *pTargetCell = (BYTE *)(intptr_t)gpGridBase
                      + ((intptr_t)g_nTargetRow * (intptr_t)g_nRowStride)
                      +  (intptr_t)g_nTargetCol;

    *pTargetCell = SEED_CELL_VALUE;      

    for (;;) {

        unsigned char bSweep1 = MarkZeroCellsAdjacentToSixCellsAndRecordTwo();          
        int nSweep1 = (int)(signed char)bSweep1;              

        if (nSweep1 == 0) {                                   
            return 0;                                         
        }
        if (nSweep1 == 1) {                                   

            if (bMode == 0) {                                  /* 80 7C 24 04 00 / 74 07 */
                return RouteGridPathToTargetCell(SWEEP1_BAND); 
            }
            return ClassifyAxisSpanOrientation();             
        }

        unsigned char bSweep2 = MarkEmptyGridCellsAdjacentToTypeFourAndLocateTypeTwo(); 
        int nSweep2 = (int)(signed char)bSweep2;               

        if (nSweep2 == 0) {
            return 0;                                         
        }
        if (nSweep2 == 1) {                                   

            if (bMode == 0) {
                return RouteGridPathToTargetCell(SWEEP2_BAND); 
            }
            return ClassifyAxisSpanOrientation();             
        }

        unsigned char bSweep3 = MarkZeroCellsAdjacentToFiveCellsWithSixAndRecordTwo();  
        int nSweep3 = (int)(signed char)bSweep3;               

        if (nSweep3 == 0) {
            return 0;                                         
        }
        if (nSweep3 == 1) {                                   

            if (bMode == 0) {
                return RouteGridPathToTargetCell(SWEEP3_BAND); 
            }
            return ClassifyAxisSpanOrientation();             
        }

    }
}
#endif /* NW_UNIT_SequenceGridMarkingSweepsUntilMatchAndDispatch */

/* ==========================================================================
 * Unit: WrapGridAxisCursorsStampCellKeyAndRetireSpans
 * Source: grid/WrapGridAxisCursorsStampCellKeyAndRetireSpans.c
 * ========================================================================== */
#ifdef NW_UNIT_WrapGridAxisCursorsStampCellKeyAndRetireSpans
/*
 * Wraps grid-axis cursors, stamps the cell key, and retires old spans.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-wrapgridaxiscursorsstampcellkeyandretirespans-c
 */

#include <stdint.h>
#include "nwglobals.h"
#include <stddef.h>

/* GridCtxRec -> src/include/nwgridctxrec.h (three local copies disagreed on +0x05) */

#pragma pack(push, 1)
typedef struct {
    uint16_t wEntryWord;    /* low byte = pending index, high byte = stamp     */
} GridCellEntry;
#pragma pack(pop)

#define CELL_ENTRY_STAMP(e)  ((uint8_t)((e)->wEntryWord >> 8))


/* X cursor, and the index addend       */
/* Y cursor, and the SHIFTED index axis */
/* X mirror word — NOT a table, see below */
/* Y mirror word                       */

extern uint8_t g_vmModeFlag;

extern uint32_t gdwGridCellWidth;     
extern uint32_t gdwGridRowCount;      

extern GridCellEntry *g_pCellEntryBase;

extern uint8_t g_bCellIndexShift;

extern uint8_t g_bGridDirty;

/* Retires up to one grouped span record for a cell (arg1 = 0 here suppresses the
   orphan-record branch and the second release) and dispatches that cell's state
   table entry through the script VM. arg2 is the major axis, arg3 the minor. */
extern void ReleaseGridCellSpanRecordsAndDispatchEntry(
        unsigned char bFullSweepMode, int nCellMajor, int nCellMinor); 

#define ARG_BYTE1(dw)     ((uint8_t)((dw) >> 8))    /* [ESP+0xd] mode byte   */
#define ARG_DELTA_X(dw)   ((int8_t)((dw) >> 16))    
#define ARG_DELTA_Y(dw)   ((int8_t)((dw) >> 24))    

void __stdcall WrapGridAxisCursorsStampCellKeyAndRetireSpans(uint32_t dwAxisDeltaAndMode)
{
    const uint8_t bModeByte  = ARG_BYTE1(dwAxisDeltaAndMode);
    const int8_t  nDeltaX    = ARG_DELTA_X(dwAxisDeltaAndMode);
    const int8_t  nDeltaY    = ARG_DELTA_Y(dwAxisDeltaAndMode);

    if ((nDeltaX != 0) || (nDeltaY != 0)) {

        const int32_t nDividendX =
            (int32_t)((uint32_t)nDeltaX + g_vmOperandB + gdwGridCellWidth);
        g_vmOperandB =
            (uint32_t)(nDividendX % (int32_t)gdwGridCellWidth);     

        const int32_t nDividendY =
            (int32_t)((uint32_t)nDeltaY + g_vmOperandA + gdwGridRowCount);
        g_vmOperandA =
            (uint32_t)(nDividendY % (int32_t)gdwGridRowCount);      

        GridCtxRec *pCtx = g_pCurGridCtx;                         
        const uint8_t  bSavedLowNibble = (uint8_t)(pCtx->bStampHigh & 0x0Fu);
        const uint16_t wMaskedStamp =
            (uint16_t)(((uint16_t)pCtx->bStampLow)
                     | (uint16_t)((uint16_t)(pCtx->bStampHigh & 0xF0u) << 8));

        /* Index form A — the MIRRORED axis pair. */
        GridCellEntry *pCellHanded =
            &g_pCellEntryBase[((g_vmOperandAKey << (g_bCellIndexShift & 0x1Fu))
                               + g_vmOperandBKey)];            
        pCellHanded->wEntryWord = wMaskedStamp;                   

        /* arg1 = 0: the callee then skips its orphan branch entirely and breaks
           after ONE grouped release. arg2 = Y (major), arg3 = X (minor). */
        ReleaseGridCellSpanRecordsAndDispatchEntry(
            0u, (int)g_vmOperandA, (int)g_vmOperandB); 

        /* Re-read the global: the callee walks the arena chain and can move it.
           The context's own global pointer is re-loaded at every use, exactly as
           src/grid/RestartCurrentGridSpanGroup.c does after ITS call. */
        g_pCurGridCtx->bCellKeyLow  = (uint8_t)g_vmOperandB; 
        g_pCurGridCtx->bCellKeyHigh = (uint8_t)g_vmOperandA; 

        /* Index form B — the UPDATED axis pair, the same expression the callee
           builds internally from the two arguments just pushed. */
        GridCellEntry *pCellReclaimed =
            &g_pCellEntryBase[((g_vmOperandA << (g_bCellIndexShift & 0x1Fu))
                               + g_vmOperandB)];            

        /* Reclaim: pull the index the sweep published back into the context,
           then put byte +3's low nibble back exactly as it was. */
        g_pCurGridCtx->bStampLow  = (uint8_t)(pCellReclaimed->wEntryWord & 0xFFu);
        g_pCurGridCtx->bStampHigh = (uint8_t)(pCellReclaimed->wEntryWord >> 8); 
        g_pCurGridCtx->bStampHigh = (uint8_t)(g_pCurGridCtx->bStampHigh
                                              | bSavedLowNibble);            

        /* Restamp the cell entry from the context: byte +0's high nibble into
           the entry's high byte, byte +1 into its low byte. */
        const uint16_t wRestamp =
            (uint16_t)((((uint16_t)g_pCurGridCtx->bStatusFlags & 0xF0u) << 8)
                       | (uint16_t)g_pCurGridCtx->bAux);                 
        pCellReclaimed->wEntryWord = wRestamp;                           

        g_bGridDirty = 1u;                                                
    }

    const uint8_t bModeNibble = (uint8_t)(bModeByte & 0xF0u);
    if (bModeNibble != g_vmModeFlag) {                             

        g_vmModeFlag = bModeNibble;                                 

        /* Clears the high nibble of the context's status byte, then ORs the mode
           nibble back. BITS 0..3 SURVIVE: bit 0 (arena in-use) and bit 1 (the
           record-class discriminator) both live in the low nibble, so this
           cannot silently reclassify the record. */
        g_pCurGridCtx->bStatusFlags =
            (uint8_t)((g_pCurGridCtx->bStatusFlags & 0x0Fu) | bModeNibble); 

        /* Same index form B, and the same HIGH byte of the entry that the
           restamp above writes — but reached without re-reading the entry. */
        GridCellEntry *pCellMode =
            &g_pCellEntryBase[((g_vmOperandA << (g_bCellIndexShift & 0x1Fu))
                               + g_vmOperandB)];                   

        ((uint8_t *)(void *)&pCellMode->wEntryWord)[1] = bModeNibble;      

        g_bGridDirty = 1u;                                                
    }
}
#endif /* NW_UNIT_WrapGridAxisCursorsStampCellKeyAndRetireSpans */
