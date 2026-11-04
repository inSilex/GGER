/*
 * GridQueries -- grid context queries and descriptor lookup.
 *
 * Module: grid
 * Interface: include/GridQueries.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   FindPredecessorDescriptorPointerByBackwardScan             grid/FindPredecessorDescriptorPointerByBackwardScan.c
 *   GridContextQueries                                         grid/GridContextQueries.c
 *   GridStateHandlers                                          grid/GridStateHandlers.c
 *   StampGridContextLayerAndPublishDepthSlot                   grid/StampGridContextLayerAndPublishDepthSlot.c
 */

/* ==========================================================================
 * Unit: FindPredecessorDescriptorPointerByBackwardScan
 * Source: grid/FindPredecessorDescriptorPointerByBackwardScan.c
 * ========================================================================== */
#ifdef NW_UNIT_FindPredecessorDescriptorPointerByBackwardScan
/*
 * Find Predecessor Descriptor Pointer By Backward Scan.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-findpredecessordescriptorpointerbybackwardscan-c
 */

#include <windows.h>
#include "nwglobals.h"
#include <stdint.h>

extern uint8_t  *g_pGridRefTab;         


/* The descriptor is 8 bytes; this function only reads its +0x4 dword. Modelled
   as raw bytes so the untouched +0x0 field is never implied. */
#define DESC_STRIDE    (8u)
#define DESC_KEY_OFF   (4u)   /* dword compared against the argument's +0xc */

uint8_t * __stdcall FindPredecessorDescriptorPointerByBackwardScan(const uint8_t *pKeyRecord)
{
    uint32_t       dwKey;   /* ESI: the search key, argument+0xc          */
    const uint8_t *pDesc;   /* EAX: the running descriptor cursor          */

    dwKey = *(const uint32_t *)(const void *)(pKeyRecord + 0xc);

    pDesc = g_pGridRefTab + (uintptr_t)(g_dwRefDescriptorCount * DESC_STRIDE);
    pDesc -= DESC_STRIDE;

    if (dwKey >= *(const uint32_t *)(const void *)(pDesc + DESC_KEY_OFF)) {
        return (uint8_t *)(uintptr_t)pDesc;               /* EAX = &desc[count-1] */
    }

    for (;;) {
        pDesc -= DESC_STRIDE;                             
        if (*(const uint32_t *)(const void *)(pDesc + DESC_KEY_OFF) <= dwKey) {
            return (uint8_t *)(uintptr_t)pDesc;           /* EAX = &desc[k], AT the match */
        }

    }
}
#endif /* NW_UNIT_FindPredecessorDescriptorPointerByBackwardScan */

/* ==========================================================================
 * Unit: GridContextQueries
 * Source: grid/GridContextQueries.c
 * ========================================================================== */
#ifdef NW_UNIT_GridContextQueries
/*
 * Grid Context Queries.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-gridcontextqueries-c
 */

#include <stdint.h>
#include "nwglobals.h"

#define NW_CTX_QUERY __attribute__((section(".nwdiag")))

NW_CTX_QUERY uint32_t NwGridCtxYIsNone(void)
{
    return g_pCurGridCtx->wCurYIdx == 0xffffu;
}

NW_CTX_QUERY uint32_t NwGridCtxLayerHasMask90_10(void)
{
    return (g_pCurGridCtx->bLayerFlags & 0x90u) == 0x10u;
}

NW_CTX_QUERY uint32_t NwGridCtxLayerHasMaskA0_20(void)
{
    return (g_pCurGridCtx->bLayerFlags & 0xa0u) == 0x20u;
}

NW_CTX_QUERY uint32_t __stdcall NwGridCtxLayerMaskB0Equals(uint32_t value)
{
    return ((uint32_t)g_pCurGridCtx->bLayerFlags & 0xb0u) == value;
}

NW_CTX_QUERY uint32_t __stdcall NwGridCtxLayerMask4FEquals(uint32_t value)
{
    return ((uint32_t)g_pCurGridCtx->bLayerFlags & 0x4fu) == value;
}

NW_CTX_QUERY uint32_t __stdcall NwGridCtxLayerEquals(uint32_t value)
{
    return (uint32_t)g_pCurGridCtx->bLayer == value;
}

NW_CTX_QUERY uint32_t NwGridCtxPendingIsNone(void)
{
    return g_pCurGridCtx->bPendingIndex == 0xfeu;
}

NW_CTX_QUERY uint32_t __stdcall NwGridCtxPendingEquals(uint32_t value)
{
    return (uint32_t)g_pCurGridCtx->bPendingIndex == value;
}

NW_CTX_QUERY uint32_t NwGridCtxLayerFromRowCount(void)
{
    g_pCurGridCtx->bLayer = (uint8_t)((uint8_t)g_nGridRowCount -
                                      g_pCurGridCtx->bCellKeyHigh);
    return 1u;
}

NW_CTX_QUERY uint32_t __stdcall NwGridCtxReadLayer(uint8_t *out)
{
    *out = g_pCurGridCtx->bLayer;
    return 1u;
}
#endif /* NW_UNIT_GridContextQueries */

/* ==========================================================================
 * Unit: GridStateHandlers
 * Source: grid/GridStateHandlers.c
 * ========================================================================== */
#ifdef NW_UNIT_GridStateHandlers
/*
 * Grid State Handlers.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-gridstatehandlers-c
 */

#include <stdint.h>
#include <windows.h>
#include "nwglobals.h"
#include "nwgridctxrec.h"

#define NW_GRID_STATE __attribute__((section(".nwdiag")))

extern uint8_t g_vmModeFlag;
extern uint32_t __stdcall IsRemappedIndexArgEqual(uint32_t dwIndexArg,
                                                   uint32_t dwValueArg);
extern char *__cdecl CopyNulTerminatedString(char *pszDst,
                                              const char *pszSrc);
extern void __stdcall AllocateGridCellSpanRecordAndDispatch(int nCellMajor,
                                                            int nCellMinor,
                                                            uint32_t dwOpcode);
extern void __stdcall WrapGridAxisCursorsStampCellKeyAndRetireSpans(
    uint32_t dwPackedArgs);

NW_GRID_STATE uint32_t __stdcall NwReachedStepCodeEquals(uint32_t value)
{
    if (g_bReachedCell == 0u) {
        return 0u;
    }
    return (uint32_t)(value == (uint32_t)g_nStepCode);
}

NW_GRID_STATE uint32_t __stdcall NwReachedBearingCodeEquals(uint32_t value)
{
    if (g_bReachedCell == 0u) {
        return 0u;
    }
    return (uint32_t)(value == (uint32_t)g_nBearingCode);
}

NW_GRID_STATE uint32_t __stdcall NwReachedStepCodeMatches(uint32_t value)
{
    if (g_bReachedCell == 0u) {
        return 0u;
    }
    return IsRemappedIndexArgEqual(value, (uint32_t)g_nStepCode);
}

NW_GRID_STATE uint32_t __stdcall NwReachedBearingCodeMatches(uint32_t value)
{
    if (g_bReachedCell == 0u) {
        return 0u;
    }
    return IsRemappedIndexArgEqual(value, (uint32_t)g_nBearingCode);
}

NW_GRID_STATE uint32_t __stdcall NwReachedCellCommand(uint32_t mode)
{
    if (g_bReachedCell == 0u) {
        return 1u;
    }

    switch (mode) {
    case 0u:
        g_bReachedCell = 0u;
        g_bCommandGuardByte = 0xffu;
        g_bCommandOut = 0xffu;
        g_bProbeAIndex = 0xffu;
        break;

    case 1u:
        AllocateGridCellSpanRecordAndDispatch(
            (int)g_nCoordV, (int)g_nCoordH,
            (uint32_t)0xfeu | ((uint32_t)g_vmModeFlag << 8));
        break;

    case 2u:
        WrapGridAxisCursorsStampCellKeyAndRetireSpans(
            ((uint32_t)g_vmModeFlag << 8) |
            ((uint32_t)(uint8_t)(g_nCoordH - g_nTargetCol) << 16) |
            ((uint32_t)(uint8_t)(g_nCoordV - g_nTargetRow) << 24));
        break;

    default:
        break;
    }

    return 1u;
}

NW_GRID_STATE uint32_t __stdcall NwReachedCellAllocFromArg(uint32_t arg)
{
    if (g_bReachedCell != 0u) {
        AllocateGridCellSpanRecordAndDispatch(
            (int)g_nCoordV, (int)g_nCoordH,
            ((uint32_t)g_vmModeFlag << 8) | (arg & 0xffu));
    }
    return 1u;
}

NW_GRID_STATE uint32_t __stdcall NwReachedCellAllocFromArgOrOne(uint32_t arg)
{
    if (g_bReachedCell != 0u) {
        AllocateGridCellSpanRecordAndDispatch(
            (int)g_nCoordV, (int)g_nCoordH,
            (((uint32_t)g_vmModeFlag | 1u) << 8) | (arg & 0xffu));
    }
    return 1u;
}

NW_GRID_STATE uint32_t __stdcall NwGridCtxFlagsMatch(uint32_t mask)
{
    return (uint32_t)((g_pCurGridCtx->bStatusFlags & mask) != 0u);
}

NW_GRID_STATE char * __stdcall NwCopyStringSwapArgs(uint32_t arg1,
                                                    uint32_t arg2)
{
    return CopyNulTerminatedString((char *)(uintptr_t)arg2,
                                   (const char *)(uintptr_t)arg1);
}
#endif /* NW_UNIT_GridStateHandlers */

/* ==========================================================================
 * Unit: StampGridContextLayerAndPublishDepthSlot
 * Source: grid/StampGridContextLayerAndPublishDepthSlot.c
 * ========================================================================== */
#ifdef NW_UNIT_StampGridContextLayerAndPublishDepthSlot
/*
 * Latches the caller's byte into the current grid context's layer field when it differs.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-stampgridcontextlayerandpublishdepthslot-c
 */

#include <stdint.h>
#include "nwglobals.h"

#define GRID_DEPTH_INDEX_NONE 0xFFFFu

/* GridCtxRec -> src/include/nwgridctxrec.h (three local copies disagreed on +0x05) */

/* ---- One 16-byte arena slot of the span chain, reached as g_pSlotsTop - n*16.
 * Only byte +0x03 is touched. See note 4 above about the flavor precondition. */
#pragma pack(push, 1)
typedef struct {
    uint8_t  bByte00;        /* +0x00 not read here                          */
    uint8_t  bByte01;        /* +0x01 not touched                            */
    uint8_t  bByte02;        /* +0x02 not touched                            */
    uint8_t  bLinkByte;      /* +0x03 the stamped byte, all 8 bits           */
} SpanSlot;
#pragma pack(pop)


extern uint8_t *g_pSlotsTop;

extern void __stdcall AddGridCellSpanToExtentList(SpanSlot *pRec); 

unsigned int __stdcall StampGridContextLayerAndPublishDepthSlot(uint32_t dwStampByte)
{
    const uint8_t bStamp = (uint8_t)dwStampByte;   

    /* Unchanged layer: latch nothing, stamp nothing, publish nothing. Still 1. */
    if (g_pCurGridCtx->bLayer == bStamp) {          
        return 1u;
    }

    g_pCurGridCtx->bLayer = bStamp;                  

    /* X-side slot. The index is UNSIGNED (MOVZX) and 0xFFFF means "not set" —
     * skipping it here still allows the Y side and the publish to run. */
    const uint16_t wDepthX = g_pCurGridCtx->wDepthIndexX;   
    if (wDepthX != GRID_DEPTH_INDEX_NONE) {                 
        /* SHL EDX,0x4 then SUB from the chain top: one 16-byte record per unit,
         * counted DOWNWARD from g_pSlotsTop. */
        SpanSlot *pSlotX =
            (SpanSlot *)(g_pSlotsTop - (intptr_t)wDepthX * 16);   
        pSlotX->bLinkByte = bStamp;                            
    }

    const uint16_t wDepthY = g_pCurGridCtx->wDepthIndexY;   
    if (wDepthY != GRID_DEPTH_INDEX_NONE) {                 
        SpanSlot *pSlotY =
            (SpanSlot *)(g_pSlotsTop - (intptr_t)wDepthY * 16);   
        pSlotY->bLinkByte = bStamp;                            

        AddGridCellSpanToExtentList(pSlotY);                   
    }

    return 1u;                                                  
}
#endif /* NW_UNIT_StampGridContextLayerAndPublishDepthSlot */
