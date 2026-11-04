/*
 * GridSpanLifecycle -- grid span allocation, publication, and release.
 *
 * Module: grid
 * Interface: include/GridSpanLifecycle.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   AcquireGridCellSpanRecordAndRetirePrevious                 grid/AcquireGridCellSpanRecordAndRetirePrevious.c
 *   AddGridCellSpanToExtentList                                grid/AddGridCellSpanToExtentList.c
 *   AdvanceActiveGridCellSpanToOffset                          grid/AdvanceActiveGridCellSpanToOffset.c
 *   AdvanceGridCellSubSpan                                     grid/AdvanceGridCellSubSpan.c
 *   AdvancePendingGridCellSpanEntryAndDispatch                 grid/AdvancePendingGridCellSpanEntryAndDispatch.c
 *   AllocGridCellSpanRecord                                    grid/AllocGridCellSpanRecord.c
 *   AllocateGridCellSpanRecordAndDispatch                      grid/AllocateGridCellSpanRecordAndDispatch.c
 *   ClearGridSpanChainBoundsAndHeapState                       grid/ClearGridSpanChainBoundsAndHeapState.c
 *   InstallActiveGridCellRef                                   grid/InstallActiveGridCellRef.c
 *   NotifyCellOperandHookAndAllocateGridSpanRecord             grid/NotifyCellOperandHookAndAllocateGridSpanRecord.c
 *   PublishCursorSlotSpanAndFlagContextOnOperandMismatch       grid/PublishCursorSlotSpanAndFlagContextOnOperandMismatch.c
 *   PublishGridCellSpanByDepthIndexToExtentList                grid/PublishGridCellSpanByDepthIndexToExtentList.c
 *   ReleaseCurrentContextSpanGroup                             grid/ReleaseCurrentContextSpanGroup.c
 *   ReleaseGridCellSpanRecord                                  grid/ReleaseGridCellSpanRecord.c
 *   ReleaseGridCellSpanRecordsAndDispatchEntry                 grid/ReleaseGridCellSpanRecordsAndDispatchEntry.c
 *   ReleaseSameDepthGridCellSpans                              grid/ReleaseSameDepthGridCellSpans.c
 *   ReleaseSpanRecordByDepthIndex                              grid/ReleaseSpanRecordByDepthIndex.c
 *   ReleaseTopGridCellSpans                                    grid/ReleaseTopGridCellSpans.c
 *   ResolveGridReferenceAndAddCellSpan                         grid/ResolveGridReferenceAndAddCellSpan.c
 *   RestartCurrentGridSpanGroup                                grid/RestartCurrentGridSpanGroup.c
 *   SelectGridSpanRecordByPointAndPublishSelectionState        grid/SelectGridSpanRecordByPointAndPublishSelectionState.c
 *   SetDefaultGridCellOffsetAndExtent                          grid/SetDefaultGridCellOffsetAndExtent.c
 *   TestPointInsideGridSpanRecord                              grid/TestPointInsideGridSpanRecord.c
 */

/* ==========================================================================
 * Unit: AcquireGridCellSpanRecordAndRetirePrevious
 * Source: grid/AcquireGridCellSpanRecordAndRetirePrevious.c
 * ========================================================================== */
#ifdef NW_UNIT_AcquireGridCellSpanRecordAndRetirePrevious
/*
 * Acquires a free 16-byte arena slot, stamps it as the current grid-cell span record.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-acquiregridcellspanrecordandretireprevious-c
 */

#include <stdint.h>
#include "nwglobals.h"

/* Packed: the record is a raw 16-byte arena slot, no padding anywhere. */
#pragma pack(push, 1)
typedef struct {
    uint8_t  bStatus;    /* +0x00 in-use bit 0 | 0x80 | 0x04              */
    uint8_t  _pad01;     /* +0x01 untouched here                          */
    uint8_t  bKind;      /* +0x02 from the refsel entry's +0x03           */
    uint8_t  bLink;      /* +0x03 unmasked copy of a context byte         */
    uint8_t  bLimitA;    /* +0x04 0xff on the bRefRelative path          */
    uint8_t  bLimitB;    /* +0x05 0xff on the bRefRelative path          */
    uint16_t wUserData;  /* +0x06 0xffff, or g_wGridUserData             */
    int16_t  iOffsetX;   /* +0x08                                        */
    int16_t  iOffsetY;   /* +0x0A                                        */
    uint32_t pRefRec;    /* +0x0C guest reference record                  */
} ArenaRec;

typedef struct {
    uint8_t  bCtxFlags;  /* +0x00 OR'd with 0x04 here                     */
    uint8_t  _pad01[4];  /* +0x01..+0x04 not read here                   */
    uint8_t  bLinkSrc;   /* +0x05 copied into the record's +0x03         */
    uint8_t  _pad06;     /* +0x06                                        */
    uint8_t  _pad07;     /* +0x07                                        */
    uint16_t wCurXIdx;   /* +0x08 arena cursor X, 0xffff = none          */
    uint16_t wCurYIdx;   /* +0x0A arena cursor Y, 0xffff = none          */
} CurGridCtxRec;

#include <stddef.h>
_Static_assert(offsetof(CurGridCtxRec, wCurXIdx) == 0x08,
               "CurGridCtxRec.wCurXIdx must sit at +0x08");
_Static_assert(offsetof(CurGridCtxRec, wCurYIdx) == 0x0A,
               "CurGridCtxRec.wCurYIdx must sit at +0x0A");

/* Reference-selection entry, 8 bytes, read at +0x01 (bit 0 -> record bit 2)
   and +0x03 (-> the record's bKind). Same 8-byte-stride table that
   src/grid/AllocGridCellSpanRecord.c addresses as g_pRefSelTable. */
typedef struct {
    uint8_t  _pad00;      /* +0x00 not read here                */
    uint8_t  bEntryFlags; /* +0x01 bit 0 -> record 0x04         */
    uint8_t  _pad02;      /* +0x02                             */
    uint8_t  bEntryKind;  /* +0x03 -> record +0x02             */
    void    *pUnused;     /* +0x04 not read here                */
} RefSelEntry;
#pragma pack(pop)

extern uint8_t *g_pRefSelTable;

extern uint8_t *g_pSlotsTop;


extern uint8_t *g_pAltGridCtx;

extern uint16_t g_wGridUserData;

/* operand-B state word 1 */
/* operand-A state word 1 */
/* operand-B state word 2 */
/* operand-A state word 2 */

extern uint8_t *AcquireNextFreeSlot(void);
extern void __stdcall AddGridCellSpanToExtentList(ArenaRec *pRec);
extern void FreeArenaSlot(ArenaRec *pRec);
extern void __stdcall ResolveGridReferenceAndAddCellSpan(uint8_t bRefRelative, ArenaRec *pGridRec,
                                               const RefSelEntry *pRefSel);

static ArenaRec *SlotFromIndex(uint16_t wIndex)
{
    return (ArenaRec *)(void *)(g_pSlotsTop - (uintptr_t)wIndex * 0x10u);
}

static int IndexFromSlot(ArenaRec *pSlot)
{
    intptr_t iDelta = (intptr_t)(uintptr_t)g_pSlotsTop - (intptr_t)(uintptr_t)pSlot;
    return (int)((int32_t)iDelta / 16);
}

uint32_t __stdcall AcquireGridCellSpanRecordAndRetirePrevious(uint8_t bRefRelative,
                                                    uint16_t wCellOffsetY,
                                                    uint16_t wCellOffsetX,
                                                    uint32_t nRefTabIndex)
{
    ArenaRec *pRecord;             /* the slot this function just claimed        */
    const RefSelEntry *pRefSel;    /* caller's reference-selection table entry  */
    uint16_t wCurXIdx;             /* ctx cursor X before the rebind            */
    uint16_t wCurYIdx;             /* ctx cursor Y before the rebind            */
    int nNewIndex;                 /* arena index of the new record            */

    /* 1. The table entry is resolved BEFORE the allocation, so a bad index
          cannot leave a claimed slot behind. */
    pRefSel = (const RefSelEntry *)(const void *)(g_pRefSelTable + nRefTabIndex * 8u);

    /* 2. ACQUIRE. */
    pRecord = (ArenaRec *)(void *)AcquireNextFreeSlot();

    /* 3. FILL the header. */
    pRecord->bStatus = (uint8_t)(pRecord->bStatus | 0x80u);       /* current top */
    if ((pRefSel->bEntryFlags & 1u) != 0u) {
        pRecord->bStatus = (uint8_t)(pRecord->bStatus | 0x04u);
    }
    pRecord->bKind     = pRefSel->bEntryKind;

    pRecord->wUserData = (bRefRelative != 0u) ? (uint16_t)0xffffu : g_wGridUserData;
    pRecord->iOffsetX  = (int16_t)wCellOffsetX;
    pRecord->iOffsetY  = (int16_t)wCellOffsetY;

    if (bRefRelative != 0u) {

        pRecord->bLimitB = 0xffu;
        pRecord->bLimitA = 0xffu;
        pRecord->bLink   = g_pCurGridCtx->bLinkSrc;
    } else if (g_pCurGridCtx == NULL) {

        pRecord->bLink = g_pAltGridCtx[2];
    } else {
        ArenaRec *pOldYRec;

        /* 4. RETIRE THE OLD RECORDS AND REBIND THE CURSORS. */
        nNewIndex = IndexFromSlot(pRecord);

        wCurXIdx = g_pCurGridCtx->wCurXIdx;     /* MOVZX EDI,word [ECX+8]  */
        wCurYIdx = g_pCurGridCtx->wCurYIdx;     /* MOVZX EDX,word [ECX+0xA] */

        /* Y is retired first. That record survives when both cursors named
           the SAME slot: the X branch below is what frees it in that case,
           and only when record bit 2 is clear. */
        if (wCurYIdx != 0xffffu) {
            pOldYRec = SlotFromIndex(wCurYIdx);
            AddGridCellSpanToExtentList(pOldYRec);
            pOldYRec->bStatus = (uint8_t)(pOldYRec->bStatus & 0x7fu);
            if (wCurYIdx != wCurXIdx) {
                FreeArenaSlot(pOldYRec);
            }
        }

        g_pCurGridCtx->wCurYIdx = (uint16_t)nNewIndex;

        if ((pRecord->bStatus & 0x04u) == 0u) {
            if (wCurXIdx != 0xffffu) {
                FreeArenaSlot(SlotFromIndex(wCurXIdx));
            }
            g_pCurGridCtx->wCurXIdx = (uint16_t)nNewIndex;
        }

        pRecord->bLink = g_pCurGridCtx->bLinkSrc;

        if ((g_vmOperandB != g_vmOperandBKey) ||
            (g_vmOperandA != g_vmOperandAKey)) {
            g_pCurGridCtx->bCtxFlags = (uint8_t)(g_pCurGridCtx->bCtxFlags | 0x04u);
        }
    }

    ResolveGridReferenceAndAddCellSpan(bRefRelative, pRecord, pRefSel);

    /* 6. Unconditional success flag -- NOT the slot pointer. */
    return 1u;
}
#endif /* NW_UNIT_AcquireGridCellSpanRecordAndRetirePrevious */

/* ==========================================================================
 * Unit: AddGridCellSpanToExtentList
 * Source: grid/AddGridCellSpanToExtentList.c
 * ========================================================================== */
#ifdef NW_UNIT_AddGridCellSpanToExtentList
/*
 * Add Grid Cell Span To Extent List.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-addgridcellspantoextentlist-c
 */

#include <stdint.h>

/* Packed: pRefRec really is the dword at +0x0C, immediately after the two
   int16 offsets, with no alignment padding. */
#pragma pack(push, 1)
typedef struct {
    uint8_t  bFlags;      /* +0x00 */
    uint8_t  _pad01[7];   /* +0x01 .. +0x07 untouched here */
    int16_t  iOffsetX;    /* +0x08 */
    int16_t  iOffsetY;    /* +0x0A */
    uint32_t pRefRec;     /* +0x0C guest pointer */
} GridCellRec;
#pragma pack(pop)

typedef struct {
    uint32_t pSourceWords;  /* +0x00 guest pointer -> two int16 words */
    int16_t  iBaseX;       /* +0x04 signed baseline X */
    int16_t  iBaseY;       /* +0x06 signed baseline Y */
} GridRefRec;

extern void AddRectToExtentList(int nMaxY, int nMaxX, int nMinY, int nMinX);


void __stdcall AddGridCellSpanToExtentList(GridCellRec *pRec)
{
    const GridRefRec *pRef = (const GridRefRec *)(uintptr_t)pRec->pRefRec;

    const uint16_t   *puSrc =
        (const uint16_t *)(uintptr_t)pRef->pSourceWords;

    /* Cell offsets, sign-extended. */
    int nMinX = (int)pRec->iOffsetX;                              /* MOVSX ESI,word [EBX+8]  */
    int nMinY = (int)pRec->iOffsetY;                              /* MOVSX EDI,word [EBX+0xA] */

    /* Flagged: shift into reference space by removing the baseline origin. */
    if ((pRec->bFlags & 0x40u) != 0u) {
        nMinX -= pRef->iBaseX;                                    /* MOVSX ECX,word [EAX+4] */
        nMinY -= pRef->iBaseY;                                    /* MOVSX ECX,word [EAX+6] */
    }

    /*
     * Far corner = the reference's source-extent corner (read zero-extended)
     * grown by the same span, so the rectangle covers the grid cell plus the
     * area its source extent displaces.
     */
    AddRectToExtentList((int)puSrc[1] + nMinY,   /* arg1 nMaxY */
                        (int)puSrc[0] + nMinX,   /* arg2 nMaxX */
                        nMinY,                   /* arg3 nMinY */
                        nMinX);                  /* arg4 nMinX */
}
#endif /* NW_UNIT_AddGridCellSpanToExtentList */

/* ==========================================================================
 * Unit: AdvanceActiveGridCellSpanToOffset
 * Source: grid/AdvanceActiveGridCellSpanToOffset.c
 * ========================================================================== */
#ifdef NW_UNIT_AdvanceActiveGridCellSpanToOffset
/*
 * Moves the active grid cell record to a new pair of cell offsets: retires the outgoing cell's extent, steps or wraps the sub-span cursor.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-advanceactivegridcellspantooffset-c
 */

#include <windows.h>
#include "nwglobals.h"
#include <stdint.h>

/* Record flag bits tested here. */
#define GRID_REC_GROUP_MEMBER 0x04u  /* +0x00 bit2: no free-standing cell */

/* Packed: the dword cursor really does sit at +0x0C, right after the int16 pair. */
#pragma pack(push, 1)
typedef struct {
    uint8_t  bFlags;
    uint8_t  bSubIndex;
    uint8_t  bSubCount;
    uint8_t  _pad03[5];    /* +0x03 .. +0x07 untouched here */
    int16_t  iOffsetX;
    int16_t  iOffsetY;
    int32_t  dwSpanCursor;
} ActiveGridCellRec;
#pragma pack(pop)

/* Globals. */
extern BYTE g_bActiveGridInstallGuard;                      /* one-shot install guard */

/* Resolved callees. */
extern void __stdcall AddGridCellSpanToExtentList(ActiveGridCellRec *pRec);
extern BOOL __stdcall InstallActiveGridCellRef(BYTE bRefreshExtents, int nRecordIndex);

BOOL __stdcall AdvanceActiveGridCellSpanToOffset(int nOffsetY, int nOffsetX)
{
    ActiveGridCellRec *pRec = &g_ActiveGridCell;
    const DWORD dwSubCount = (DWORD)pRec->bSubCount;

    if (g_bActiveGridInstallGuard == 0) {

        if (dwSubCount > 1u) {
            /* Multi-cell group: advance the sub-span instead of replacing it. */
            AddGridCellSpanToExtentList(pRec);
            pRec->bSubIndex = (uint8_t)(pRec->bSubIndex + 1u);

            if ((DWORD)pRec->bSubIndex == dwSubCount) {
                if ((pRec->bFlags & GRID_REC_GROUP_MEMBER) != 0u) {
                    InstallActiveGridCellRef(1, 0);
                } else {
                    pRec->bSubIndex = 0u;
                    pRec->dwSpanCursor -= (int32_t)(dwSubCount * 8u - 8u);  /* LEA EAX,[EBX*8-8] */
                }
            } else {
                pRec->dwSpanCursor += 8;
            }
        } else if ((pRec->bFlags & GRID_REC_GROUP_MEMBER) != 0u) {
            InstallActiveGridCellRef(1, 0);
        } else {
            /* Free-standing cell: compare the existing offsets (MOVSX sign-
               extended words against the full 32-bit arguments) to detect a
               repeat of the cell already in place. */
            if (!((int32_t)pRec->iOffsetX == (int32_t)nOffsetX &&
                  (int32_t)pRec->iOffsetY == (int32_t)nOffsetY &&
                  g_bTrackCellExtents != 0)) {
                AddGridCellSpanToExtentList(pRec);
            } else {

                return TRUE;
            }
        }

        pRec->iOffsetX = (int16_t)nOffsetX;
        pRec->iOffsetY = (int16_t)nOffsetY;
        if (g_bTrackCellExtents != 0) {
            AddGridCellSpanToExtentList(pRec);
        }
    }

    g_bActiveCellReady = 1;
    return TRUE;
}
#endif /* NW_UNIT_AdvanceActiveGridCellSpanToOffset */

/* ==========================================================================
 * Unit: AdvanceGridCellSubSpan
 * Source: grid/AdvanceGridCellSubSpan.c
 * ========================================================================== */
#ifdef NW_UNIT_AdvanceGridCellSubSpan
/*
 * Walks a grid cell record's sub-span group: emits each 8-byte sub-span to the extent list, advances and wraps the byte cursor at the group end.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-advancegridcellsubspan-c
 */

#include <stdint.h>

/* Grid cell record flags. */
#define GRID_REC_ALWAYS_EXTENT 0x04u  /* +0x00 bit2 */
#define GRID_REC_REF_RELATIVE   0x40u  /* +0x00 bit6 */

/* Packed: the dword cursor really does sit at +0x0C, right after the int16 pair. */
#pragma pack(push, 1)
typedef struct {
    uint8_t  bFlags;       /* +0x00 */
    uint8_t  bSubIndex;    /* +0x01 */
    uint8_t  bSubCount;    /* +0x02 */
    uint8_t  _pad03[5];    /* +0x03 .. +0x07 untouched here */
    int16_t  iOffsetX;     /* +0x08 */
    int16_t  iOffsetY;     /* +0x0A */
    int32_t  dwSpanCursor; /* +0x0C byte cursor into the sub-span group */
} GridCellSpanRec;
#pragma pack(pop)

/* Resolved callees - see src/grid/AddGridCellSpanToExtentList.c, src/grid/ComputeScaledGridOffset.c,
   src/runtime/FreeArenaSlot.c. Declared against void* because each peer file defines its own
   view of the same record. */
extern void __stdcall AddGridCellSpanToExtentList(void *pRec);
extern void __stdcall ComputeScaledGridOffset(void *pRec) __asm__("_ComputeScaledGridOffset");
extern void FreeArenaSlot(uint8_t *pSlot);

/* Emit the final sub-span's extent, then release the arena slot on request. */
static int FinishGridCellSpan(GridCellSpanRec *pRec, uint8_t bReleaseSlot)
{
    if (bReleaseSlot != 0u) {
        FreeArenaSlot((uint8_t *)(void *)pRec);
    }
    return 1;
}

int __stdcall AdvanceGridCellSubSpan(uint8_t, uint8_t, GridCellSpanRec *) __asm__("_AdvanceGridCellSubSpan");
int __stdcall AdvanceGridCellSubSpan(uint8_t bReleaseSlot, uint8_t bFinalPass, GridCellSpanRec *pRec)
{
    const uint32_t uSubCount = (uint32_t)pRec->bSubCount;   /* MOVZX ESI,byte [EBX+2] */
    int bEmitFinal;

    /* Single-sub-span group (count 0 or 1): no cursor stepping, just the final emit. */
    if (uSubCount < 2u) {
        bEmitFinal = (bFinalPass != 0u) ||
                     ((pRec->bFlags & GRID_REC_ALWAYS_EXTENT) != 0u);
        if (bEmitFinal == 0) {
            return 0;
        }
        AddGridCellSpanToExtentList(pRec);
        return FinishGridCellSpan(pRec, bReleaseSlot);
    }

    /* Emit the current sub-span, then step to the next one. */
    AddGridCellSpanToExtentList(pRec);
    pRec->bSubIndex = (uint8_t)(pRec->bSubIndex + 1u);        /* INC byte [EBX+1] */
    if ((uint32_t)pRec->bSubIndex == uSubCount) {
        /* Wrapped to the end of the group: rewind the cursor to the first entry. */
        pRec->bSubIndex = 0u;
        pRec->dwSpanCursor -= (int32_t)((uSubCount - 1u) * 8u);  /* SUB [EBX+0xC],ECX, ECX = ESI*8-8 */

        bEmitFinal = (bFinalPass != 0u) ||
                     ((pRec->bFlags & GRID_REC_ALWAYS_EXTENT) != 0u);
        if (bEmitFinal != 0) {

            return FinishGridCellSpan(pRec, bReleaseSlot);
        }
    } else {
        pRec->dwSpanCursor += 8;                              /* ADD dword [EBX+0xC],8 */
    }

    /* Mid-group: refresh the cell offsets unless they are already reference-relative. */
    if ((pRec->bFlags & GRID_REC_REF_RELATIVE) == 0u) {
        ComputeScaledGridOffset(pRec);
    }
    AddGridCellSpanToExtentList(pRec);
    return 0;                                                 /* XOR EAX,EAX */
}
#endif /* NW_UNIT_AdvanceGridCellSubSpan */

/* ==========================================================================
 * Unit: AdvancePendingGridCellSpanEntryAndDispatch
 * Source: grid/AdvancePendingGridCellSpanEntryAndDispatch.c
 * ========================================================================== */
#ifdef NW_UNIT_AdvancePendingGridCellSpanEntryAndDispatch
/*
 * Advance Pending Grid Cell Span Entry And Dispatch.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-advancependinggridcellspanentryanddispatch-c
 */

#include <stdint.h>
#include "nwglobals.h"
#include <stddef.h>

/* GridCtxRec -> src/include/nwgridctxrec.h (three local copies disagreed on +0x05) */

typedef struct {
    uint8_t  _pad00[4];      /* +0x00 .. +0x03 */
    uint32_t pPickArray;     /* +0x04 guest pointer to the pick array */
    uint32_t pPickHandlers;  /* +0x08 guest pointer, unused here */
    uint8_t  _pad0C[4];      /* +0x0C zero in the live table; ABI stride 16 */
} PoolPickEntry;

/* One 0x10-stride entry of g_pStateTable. Same table and same cursor placement as
 * src/grid/ProcessPendingGridCellSpanEntry.c's GridEntryRec; the cursor dispatched here
 * is the +0x04 one, not that file's +0x0C. */
#pragma pack(push, 1)
typedef struct {
    uint8_t  _pad00[4];      /* +0x00 .. +0x03 */
    uint32_t pCursorA;       /* +0x04 guest script cursor */
    uint8_t  _pad08[4];      /* +0x08 .. +0x0B */
    uint32_t pCursorB;       /* +0x0C guest cursor used by the drain */
} GridStateEntry;
#pragma pack(pop)


extern GridStateEntry *g_pStateTable;

extern PoolPickEntry *g_pPoolTable;

extern uint32_t dwRngState;

/* Drains the entry the context already had pending and re-arms ctx+2/ctx+3.
 * Plain RET, no stack cleanup, no parameters — everything through globals. */
extern void ProcessPendingGridCellSpanEntry(void);

extern void ExecuteScriptInstruction(unsigned char bFlag, unsigned char bOperandA,
                                    unsigned char bOperandB, uint32_t dwCtxA,
                                    uint32_t dwCtxB, uint32_t dwCtxC,
                                    uint32_t dwCtxD, unsigned char *pCursor);

void __stdcall AdvancePendingGridCellSpanEntryAndDispatch(unsigned char bRedrawFromPool,
                                                uint32_t dwStateIndex)
{

    ProcessPendingGridCellSpanEntry();

    /* [S+0x10]: the caller's index on entry, scratch from here on. */
    uint32_t dwIndex = dwStateIndex;

    if (bRedrawFromPool != 0u) {

        const PoolPickEntry *pPoolRec = &g_pPoolTable[dwStateIndex];

        const unsigned char *pPickArray =
            (const unsigned char *)(uintptr_t)pPoolRec->pPickArray;

        const uint32_t dwSeedHigh = dwRngState >> 16;

        dwIndex = pPickArray[((((dwSeedHigh * (uint32_t)pPickArray[0])) >> 16) + 1u)];

        dwRngState = dwRngState * 0x41c64e6du + 0x3039u;
    }

    GridCtxRec *pCtx = g_pCurGridCtx;

    pCtx->bPendingIndex = (unsigned char)dwIndex;

    pCtx->bDispatchFlag = 0x10u;

    GridStateEntry *pEntry = &g_pStateTable[dwIndex];

    ExecuteScriptInstruction(0x10u,
                             (unsigned char)((pCtx->wCellStamp >> 8) & 0xFFu),
                             (unsigned char)(pCtx->wCellStamp & 0xFFu),
                             0u,
                             0xFFFFu,
                             0u,
                             (uint32_t)(uintptr_t)pEntry,
                             (uint8_t *)(uintptr_t)pEntry->pCursorA);

    /* EAX holds no meaningful result on either exit; see the header note. */
}
#endif /* NW_UNIT_AdvancePendingGridCellSpanEntryAndDispatch */

/* ==========================================================================
 * Unit: AllocGridCellSpanRecord
 * Source: grid/AllocGridCellSpanRecord.c
 * ========================================================================== */
#ifdef NW_UNIT_AllocGridCellSpanRecord
/*
 * Claims a free 16-byte slot, fills it as a grid-cell span record whose offsets are relative to a reference record.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-allocgridcellspanrecord-c
 */

#include <stdint.h>
#include "nwglobals.h"

/* Packed: the record is a raw 16-byte arena slot with no padding anywhere. */
#pragma pack(push, 1)
typedef struct {
    uint8_t   bFlags;      /* +0x00 status | 0xe0 */
    uint8_t   bAux;        /* +0x01 zero          */
    uint8_t   bKind;       /* +0x02 = 1           */
    uint8_t   bLayer;      /* +0x03 g_pCurGridCtx[5] */
    uint8_t   bLimitA;     /* +0x04 = 0xff        */
    uint8_t   bLimitB;     /* +0x05 = 0xff        */
    uint16_t  wUserData;   /* +0x06 g_wGridUserData */
    int16_t   iOffsetX;    /* +0x08 cell offset X */
    int16_t   iOffsetY;    /* +0x0A cell offset Y */
    uint32_t  pRefRec;     /* +0x0C guest reference record */
} GridCellSpanRec;

typedef struct {
    uint32_t pSrcWords;  /* +0x00 guest pointer to int16 pairs */
    int16_t   iBaseX;      /* +0x04 MOVSX-signed baseline   */
    int16_t   iBaseY;      /* +0x06 baseline Y, not read here */
} GridRefEntry;

typedef struct {
    uint8_t   bUnknown00;  /* +0x00 not read by this function   */
    uint8_t   bBaseIndex;
    uint8_t   _pad02[2];   /* +0x02 .. +0x03 untouched          */
    uint32_t pEntries;      /* +0x04 guest base of the entry array */
} GridRefTabRec;
#pragma pack(pop)

extern GridRefTabRec *g_pGridRefTab;


extern uint16_t        g_wGridUserData;

/* Resolved callee: allocates and claims the 16-byte arena slot. */
uint8_t *AcquireNextFreeSlot(void);
/* Resolved callee: registers the cell's bounding rectangle globally. */
void __stdcall AddGridCellSpanToExtentList(GridCellSpanRec *pRec);

/**
 * AllocGridCellSpanRecord
 *
 * Allocates one grid-cell span record and registers it.
 *
 * Takes a fresh 16-byte slot from the custom-heap arena, stamps it as a
 * grid-cell span record whose cell offsets are expressed relative to a
 * reference record (flag bit 0x40), resolves that reference record from the
 * table selected by nGridTabIndex at position nRefRecIndex, stores the caller's
 * current X offset as the cell's own offset and then advances that running
 * offset by the reference's span (source X minus its baseline), and finally
 * hands the record to AddGridCellSpanToExtentList so the covered rectangle
 * joins the global extent list.
 *
 * @param wOffsetY    cell Y offset written at +0x0A
 * @param pnCellOffsetX   in/out running X offset; old value goes to +0x08, and the
 *                    reference's span is added back into it
 * @param nGridTabIndex which reference table (8-byte-stride descriptor)
 * @param nRefRecIndex index of the reference entry within that table
 * @return the populated record
 */
GridCellSpanRec * __stdcall AllocGridCellSpanRecord(uint16_t wOffsetY,
                                         int      *pnCellOffsetX,
                                         int       nGridTabIndex,
                                         int       nRefRecIndex)
{
    GridRefTabRec *pTab = &g_pGridRefTab[nGridTabIndex];   /* LEA ESI,[EDX+ECX*8] */

    /* Claim a slot and force the "relative to reference baseline" flag. */
    GridCellSpanRec *pRec = (GridCellSpanRec *)(void *)AcquireNextFreeSlot();
    pRec->bFlags    |= 0xe0u;                 /* OR  byte ptr [EDI],0xe0  */
    pRec->bAux       = 0;
    pRec->bKind      = 1;
    pRec->bLayer     = g_pCurGridCtx->bLayer;   /* +0x05, same byte as g_pCurGridCtx[5] */
    pRec->bLimitA    = 0xffu;
    pRec->bLimitB    = 0xffu;
    pRec->wUserData  = g_wGridUserData;

    /* Cell offsets: the running X offset is stored before it is advanced. */
    pRec->iOffsetX = (int16_t)*pnCellOffsetX;
    pRec->iOffsetY = (int16_t)wOffsetY;

    /* Table-relative entry lookup: base pointer + (index - base index). */
    GridRefEntry *pRef =
        (GridRefEntry *)(uintptr_t)pTab->pEntries +
        (nRefRecIndex - (int)pTab->bBaseIndex);
    pRec->pRefRec = (uint32_t)(uintptr_t)pRef;

    /* Advance the running X offset by the reference's span.  wSrcX is read
     * THROUGH the entry's pointer, zero-extended, exactly as MOVZX [EDX] does. */
    *pnCellOffsetX = *pnCellOffsetX + ((int)(uint16_t)
        ((const uint16_t *)(uintptr_t)pRef->pSrcWords)[0]
                                       - (int)pRef->iBaseX);

    AddGridCellSpanToExtentList(pRec);

    return pRec;
}
#endif /* NW_UNIT_AllocGridCellSpanRecord */

/* ==========================================================================
 * Unit: AllocateGridCellSpanRecordAndDispatch
 * Source: grid/AllocateGridCellSpanRecordAndDispatch.c
 * ========================================================================== */
#ifdef NW_UNIT_AllocateGridCellSpanRecordAndDispatch
/*
 * Allocates and inserts a grid-cell span record for one cell, retiring that cell's existing spans first, then dispatches the resulting script step.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-allocategridcellspanrecordanddispatch-c
 */

#include <stdint.h>
#include "nwglobals.h"
#include <stddef.h>

/* Raw 16-byte arena slot, GROUPED flavor only — the flavor this function
   creates. The arena itself is heterogeneous; see the note above. Bit 0x02 of
   byte +0x00 is set here and is what distinguishes this layout from the orphan
   span layout in src/grid/ReleaseGridCellSpanRecordsAndDispatchEntry.c, in which +0x06
   is the unset/depth-stamp word rather than a cell key. */
#pragma pack(push, 1)
typedef struct {
    uint8_t   bStatusFlags;   /* +0x00 (dwOpcode byte1 & 0xF0) | 0x03, then
                                |= 0x08 when a heap payload is attached;
                                bit0 arena in-use, bit1 grouped,
                                bit3 payload at +0x0C is heap-allocated     */
    uint8_t   bEntryIndex;    /* +0x01 index into g_pStateTable            */
    uint8_t   bCellIndexLow;  /* +0x02 low byte of the cell entry word      */
    uint8_t   bLinkFlags;     /* +0x03 high byte of that word; bit0 set when
                                this slot is below g_pCurGridCtx, bit1 is the
                                group-pending bit (RestartCurrentGridSpanGroup) */
    uint8_t   bRefA;          /* +0x04 written 0                           */
    uint8_t   bEntryByte2;    /* +0x05 = pStateEntry->byte2                 */
    uint16_t  wCellKey;       /* +0x06 low byte = (byte)nCellMinor,
                                high byte = (byte)nCellMajor               */
    uint16_t  wDepthIndexA;   /* +0x08 = 0xFFFF, never set here            */
    uint16_t  wDepthIndexB;   /* +0x0A = 0xFFFF, never set here            */
    uint32_t  pPayload;       /* +0x0C guest heap block, or inline payload */
} GroupedSpanSlot;
#pragma pack(pop)

/* One 0x10-stride entry of g_pStateTable. Same table and same stride as
   src/grid/ReleaseGridCellSpanRecordsAndDispatchEntry.c's GridEntryRec, which also
   places its two cursors at +0x04 and +0x0C; this function reads +0x00, +0x02,
   +0x03 and +0x04. */
#pragma pack(push, 1)
typedef struct {
    uint8_t   bEntryKind;     /* +0x00 0 = inactive (mode-1 sweep, no new
                                record); == 3 makes the record adoptable as
                                g_pActiveSlot; otherwise just non-zero      */
    uint8_t   bPad01;         /* +0x01 not read by this function            */
    uint8_t   bByte2;         /* +0x02 copied to slot +0x05                 */
    uint8_t   bPayloadBytes;  /* +0x03 payload size = this + 1 bytes        */
    uint32_t  pCursorA;       /* +0x04 guest dispatch argument              */
    uint8_t   bPad08[4];      /* +0x08 .. +0x0B not read by this function   */
    uint32_t  pCursorB;       /* +0x0C guest cursor used by the releaser     */
} GridStateEntry;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint8_t   bPad00[4];
    uint32_t  pPickArray;     /* +0x04 guest pointer to the pick array      */
    uint32_t  pPickHandlers;  /* +0x08 guest pointer to the handler array   */
    uint8_t   bPad0C[4];      /* +0x0C zero in the live table; ABI stride 16 */
} PoolPickEntry;
#pragma pack(pop)

/* The 2-BYTE-stride cell entry, reached through a uint16_t pointer because this
   function reads and writes it as one 16-bit word. Same table and same
   `((major << shift) + minor) * 2` addressing as
   src/grid/ReleaseGridCellSpanRecordsAndDispatchEntry.c. */
typedef struct {
    uint16_t  wEntryWord;     /* low byte = pending index into g_pStateTable,
                                high byte = the stamp/flag nibble that becomes
                                the new record's +0x02/+0x03                */
} GridCellEntry;

extern GroupedSpanSlot *g_pSlotsTop;
extern GridStateEntry  *g_pStateTable;
extern PoolPickEntry   *g_pPoolTable;
extern GridCellEntry   *g_pCellEntryBase;
extern uint8_t         *g_pActiveSlot;
extern uint32_t         dwRngState;

/* Two opcode dispatch tables, both indexed by (dwOpcode byte 1) >> 4 and both
   called with a POINTER to the 3-byte opcode word. Each callee pops its own
   argument (neither site adds to ESP afterwards), so they are stdcall; the
   declarations below are left plain because this file is never linked. */
extern void (*g_apPoolHandlers[16])(uint32_t *);
extern void (*g_apOpcodeHandlers[16])(uint32_t *);

/* Retires up to two span records belonging to (nCellMajor, nCellMinor) and
   dispatches that cell's state entry. Mode 1 also considers orphan spans and
   permits a second grouped release; mode 0 considers neither beyond the first
   grouped record. */
extern void ReleaseGridCellSpanRecordsAndDispatchEntry(unsigned char bFullSweepMode,
                                                       int nCellMajor, int nCellMinor);

/* Returns a free 16-byte slot from the same arena g_pSlotsTop bounds. */
extern uint8_t *AcquireNextFreeSlot(void);

/* Byte-size bump allocator: rounds the byte count up to dwords plus one. */
extern void *__stdcall AllocHeapBlock(uint32_t dwSizeBytes);

/* memset(pDst, 0, dwCount) in BYTES — see src/runtime/FillMemoryWithZeroBytes.c. */
extern void __stdcall FillMemoryWithZeroBytes(uint32_t dwCount, uint8_t *pDst);

extern void ExecuteScriptInstruction(unsigned char bFlag, unsigned char bOperandA,
                                     unsigned char bOperandB, uint32_t dwCtxA,
                                     uint32_t dwCtxB, uint32_t dwCtxC,
                                     uint32_t dwCtxD, uint8_t *pCursor);

void __stdcall AllocateGridCellSpanRecordAndDispatch(int nCellMajor, int nCellMinor,
                                                     uint32_t dwOpcode)
{

    GridCellEntry *pCellEntry =
        &g_pCellEntryBase[((uint32_t)nCellMajor << g_dwAxisShift) + (uint32_t)nCellMinor];

    uint32_t dwPickIndex = (uint32_t)(uint8_t)dwOpcode;

    /* dwOpcode byte 1: bit 8 selects the pool path, the high nibble becomes the
       new record's status byte, and the high nibble also selects the handler
       table entry. Byte 2 is only ever visible to the handlers, which get a
       pointer to the whole word. */
    if ((dwOpcode & 0x100u) != 0u) {
        PoolPickEntry *pPoolRec = &g_pPoolTable[dwPickIndex];
        uint8_t *pHandlers = (uint8_t *)(uintptr_t)pPoolRec->pPickHandlers;

        const uint8_t *pArray =
            (const uint8_t *)(uintptr_t)pPoolRec->pPickArray;
        const uint32_t dwHigh = (dwRngState >> 16) & 0xFFFFu;
        dwPickIndex = (uint32_t)pArray[(((dwHigh * (uint32_t)pArray[0]) >> 16) + 1u)];

        /* Advance AFTER reading the seed, so the pick uses the previous
           value. */
        dwRngState = dwRngState * 0x41c64e6du + 0x3039u;

        if (pHandlers != NULL) {

            const uint32_t dwPoolHandler = (dwOpcode >> 12) & 0xFu;
            const uint8_t bNewByte1 = pHandlers[dwPickIndex];
            dwOpcode = (dwOpcode & ~0xFF00u) | ((uint32_t)bNewByte1 << 8);
            g_apPoolHandlers[dwPoolHandler](&dwOpcode);
        }

    } else if (dwPickIndex == 0xFEu) {

        ReleaseGridCellSpanRecordsAndDispatchEntry(1, nCellMajor, nCellMinor);
        pCellEntry->wEntryWord = (uint16_t)dwOpcode;
        return;
    }

    g_apOpcodeHandlers[(dwOpcode >> 12) & 0xFu](&dwOpcode);
    dwRngState = dwRngState * 0x41c64e6du + 0x3039u;

    dwOpcode = (dwOpcode & ~0xFFu) | (dwPickIndex & 0xFFu);
    GridStateEntry *pStateEntry = &g_pStateTable[dwOpcode & 0xFFu];

    GroupedSpanSlot *pRec = NULL;
    uint8_t         *pPayloadArg = NULL;   /* dispatch argument 4 */
    uint32_t         dwDepthIndex = 0x0000FFFFu;

    if (pStateEntry->bEntryKind != 0u) {
        /* Mode 0: retire only this cell's grouped spans, then insert a fresh
           record. An inactive entry (byte0 == 0) took the mode-1 sweep above
           and allocates nothing at all. */
        ReleaseGridCellSpanRecordsAndDispatchEntry(0, nCellMajor, nCellMinor);

        pRec = (GroupedSpanSlot *)(void *)AcquireNextFreeSlot();

        /* +0x00: bit 0 (arena in-use) and bit 1 (GROUPED) are forced on, and
           the opcode's high nibble is carried across. Bit 1 is what makes
           +0x06 a cell key below rather than the orphan unset stamp. */
        pRec->bStatusFlags = (uint8_t)(((dwOpcode >> 8) & 0xF0u) | 0x03u);
        pRec->bEntryIndex   = (uint8_t)(dwOpcode & 0xFFu);
        pRec->bRefA         = 0;
        pRec->bEntryByte2   = pStateEntry->bByte2;

        pRec->wCellKey = (uint16_t)(((uint32_t)nCellMinor & 0xFFu) |
                                    (((uint32_t)nCellMajor & 0xFFu) << 8));

        /* Both depth indices are left at the 0xFFFF "unset" sentinel, so this
           record is inert as an orphan record as well as being grouped. */
        pRec->wDepthIndexA = 0xFFFFu;
        pRec->wDepthIndexB = 0xFFFFu;

        /* +0x02/+0x03 receive the cell entry's whole word UNMASKED. */
        pRec->bCellIndexLow = (uint8_t)(pCellEntry->wEntryWord & 0xFFu);
        pRec->bLinkFlags    = (uint8_t)(pCellEntry->wEntryWord >> 8);

        if ((uintptr_t)pRec < (uintptr_t)g_pCurGridCtx) {
            pRec->bLinkFlags |= 0x01u;
        }

        const uint32_t dwPayloadBytes = (uint32_t)pStateEntry->bPayloadBytes + 1u;
        if (dwPayloadBytes > 4u) {
            pRec->pPayload = (uint32_t)(uintptr_t)AllocHeapBlock(dwPayloadBytes);
            /* memset over the same BYTE count — this is what refutes the
               8-byte-stride reading of +0x0C. */
            FillMemoryWithZeroBytes(dwPayloadBytes,
                                    (uint8_t *)(uintptr_t)pRec->pPayload);
            pRec->bStatusFlags |= 0x08u;
            pPayloadArg = (uint8_t *)(uintptr_t)pRec->pPayload;
        } else {
            /* Inline: the slot's own last dword is the payload, and argument 4
               points AT it rather than at a block. */
            pRec->pPayload  = 0u;
            pPayloadArg     = (uint8_t *)(void *)&pRec->pPayload;
        }

        dwDepthIndex = ((uintptr_t)g_pSlotsTop - (uintptr_t)pRec) / 0x10u;

        if (pStateEntry->bEntryKind == 3u && g_pActiveSlot == NULL) {
            g_pActiveSlot = (uint8_t *)(void *)pRec;
        }
    } else {

        ReleaseGridCellSpanRecordsAndDispatchEntry(1, nCellMajor, nCellMinor);

    }

    pCellEntry->wEntryWord = (uint16_t)dwOpcode;

    ExecuteScriptInstruction((unsigned char)((dwOpcode >> 8) & 0xFFu),
                             (unsigned char)((uint32_t)nCellMajor & 0xFFu),
                             (unsigned char)((uint32_t)nCellMinor & 0xFFu),
                             (uint32_t)(uintptr_t)pPayloadArg,
                             dwDepthIndex,
                             (uint32_t)(uintptr_t)pRec,
                             (uint32_t)(uintptr_t)pStateEntry,
                             (uint8_t *)(uintptr_t)pStateEntry->pCursorA);
}
#endif /* NW_UNIT_AllocateGridCellSpanRecordAndDispatch */

/* ==========================================================================
 * Unit: ClearGridSpanChainBoundsAndHeapState
 * Source: grid/ClearGridSpanChainBoundsAndHeapState.c
 * ========================================================================== */
#ifdef NW_UNIT_ClearGridSpanChainBoundsAndHeapState
/*
 * Clear Grid Span Chain Bounds And Heap State.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-cleargridspanchainboundsandheapstate-c
 */

#include <stdint.h>
#include "nwglobals.h"
extern unsigned long g_pSlotsBottom;
extern unsigned long g_pSlotsTop;
extern void nwtrace_hex(const char *, unsigned long);  /* Optional trace. */
extern unsigned long g_pHeapBlockList;
extern uint32_t *g_pCursorBlock;

void ClearGridSpanChainBoundsAndHeapState(void)
{
#ifdef NWDEBUG_ENABLED
    nwtrace_hex("arena_clear", (unsigned long)g_pSlotsTop);
#endif
    /* Group A -- empty the grid-span record chain: bottom == top == NULL. */
    g_pSlotsBottom   = 0;
    g_pSlotsTop      = 0;

    /* Group B -- clear the custom-heap state record, all three dwords. */
    g_pCursorBlock   = 0;
    g_pHeapBlockList = 0;
    g_dwCursorWords  = 0;
}
#endif /* NW_UNIT_ClearGridSpanChainBoundsAndHeapState */

/* ==========================================================================
 * Unit: InstallActiveGridCellRef
 * Source: grid/InstallActiveGridCellRef.c
 * ========================================================================== */
#ifdef NW_UNIT_InstallActiveGridCellRef
/*
 * Installs the indexed reference descriptor into the global active grid cell record (flag byte, cell byte and reference pointer).
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-installactivegridcellref-c
 */

#include <windows.h>
#include <stdint.h>

#pragma pack(push, 1)
typedef struct {
    BYTE  bUnused0;     /* +0x00 never touched by this function */
    BYTE  bStyleBits;   /* +0x01 bit0 is OR'd into the active record's flag byte */
    BYTE  bRefKind;     /* +0x02 1 = reference pointer is direct, 0 = pointer to pointer */
    BYTE  bCellFlags;   /* +0x03 copied into the active record's +0x02 byte */
    DWORD pRefCell;     /* +0x04 the reference object (see bRefKind) */
} GridRefDesc;           /* exactly 8 bytes -- matches the *8 index stride */
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    BYTE   bFlags;
    BYTE   _pad01;
    BYTE   bCellFlags;
    BYTE   _pad03[5];  /* +0x03 .. +0x07 untouched */
    short  iOffsetX;
    short  iOffsetY;
    uint32_t pRefRec;
} ActiveGridCellRec;
#pragma pack(pop)

/* Globals. */
extern BYTE   g_bActiveGridInstallGuard;      /* one-shot install guard */
extern DWORD  g_dwActiveGridDescriptorIndex;      /* receives the descriptor index */
extern BYTE   g_pRefSelTable[];    /* slot holding the POINTER to the 8-byte descriptor array */
extern ActiveGridCellRec g_ActiveGridCellRecord;

extern void __stdcall AddGridCellSpanToExtentList(ActiveGridCellRec *pRec);

BOOL __stdcall InstallActiveGridCellRef(BYTE bRefreshExtents, int nRecordIndex)
{
    if (g_bActiveGridInstallGuard == 0) {
        /* Still holding the outgoing reference: fold its span into the
           extents before the pointer is replaced. */
        if ((bRefreshExtents != 0) && (g_ActiveGridCellRecord.pRefRec != 0u)) {
            AddGridCellSpanToExtentList(&g_ActiveGridCellRecord);
        }

        g_dwActiveGridDescriptorIndex = (DWORD)nRecordIndex;

        {

            const BYTE *pRefTab = *(const BYTE *const *)(const void *)g_pRefSelTable;
            const BYTE *pBase = pRefTab + (size_t)nRecordIndex * 8u;
            const GridRefDesc *pDesc = (const GridRefDesc *)(const void *)pBase;

            g_ActiveGridCellRecord.bFlags     = (BYTE)((pDesc->bStyleBits & 1u) | 0xC1u);
            g_ActiveGridCellRecord.bCellFlags = pDesc->bCellFlags;
            g_ActiveGridCellRecord._pad01     = 0;
            g_ActiveGridCellRecord.iOffsetX   = 0;
            g_ActiveGridCellRecord.iOffsetY   = 0;

            /* bRefKind selects direct pointer vs. pointer to the pointer. */
            if (pDesc->bRefKind == 1) {
                g_ActiveGridCellRecord.pRefRec =
                    (uint32_t)(uintptr_t)pDesc->pRefCell;
            } else {
                g_ActiveGridCellRecord.pRefRec =
                    *(const uint32_t *)(const void *)(uintptr_t)pDesc->pRefCell;
            }
        }
    }

    return TRUE;
}
#endif /* NW_UNIT_InstallActiveGridCellRef */

/* ==========================================================================
 * Unit: NotifyCellOperandHookAndAllocateGridSpanRecord
 * Source: grid/NotifyCellOperandHookAndAllocateGridSpanRecord.c
 * ========================================================================== */
#ifdef NW_UNIT_NotifyCellOperandHookAndAllocateGridSpanRecord
/*
 * Notify Cell Operand Hook And Allocate Grid Span Record.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-notifycelloperandhookandallocategridspanrecord-c
 */

#include <stdint.h>

extern void (*g_pfnCellOperandHook)(uint32_t *);

/* The stub currently bound to that slot. Declared for completeness of the ABI
   story only; nothing here calls it directly. */
extern void CellOperandHookNoopStub(uint32_t *pBlock);

extern void __stdcall AllocateGridCellSpanRecordAndDispatch(int nCellMajor,
                                                           int nCellMinor,
                                                           uint32_t dwOpcode);

#define CELL_BLOCK_AXIS_MINOR(dw)    ((int8_t)(((dw) >> 16) & 0xFFu))
#define CELL_BLOCK_AXIS_MAJOR(dw)    ((int8_t)(((dw) >> 24) & 0xFFu))

int __stdcall NotifyCellOperandHookAndAllocateGridSpanRecord(uint32_t dwCellOperandBlock)
{

    g_pfnCellOperandHook(&dwCellOperandBlock);

    AllocateGridCellSpanRecordAndDispatch((int)CELL_BLOCK_AXIS_MAJOR(dwCellOperandBlock),
                                          (int)CELL_BLOCK_AXIS_MINOR(dwCellOperandBlock),
                                          dwCellOperandBlock);

    return 0;
}
#endif /* NW_UNIT_NotifyCellOperandHookAndAllocateGridSpanRecord */

/* ==========================================================================
 * Unit: PublishCursorSlotSpanAndFlagContextOnOperandMismatch
 * Source: grid/PublishCursorSlotSpanAndFlagContextOnOperandMismatch.c
 * ========================================================================== */
#ifdef NW_UNIT_PublishCursorSlotSpanAndFlagContextOnOperandMismatch
/*
 * Publish Cursor Slot Span And Flag Context On Operand Mismatch.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-publishcursorslotspanandflagcontextonoperandmismatch-c
 */

#include <windows.h>
#include "nwglobals.h"
#include <stdint.h>

extern uint8_t *g_pSlotsTop;


/* ---- Shared 16-byte records, layouts shared with the callee files ---- */

/* Packed: these records are raw 16-byte arena cells with no padding. */
#pragma pack(push, 1)

typedef struct {
    uint8_t  bCtxFlags;   /* +0x00 OR'd with 0x04 here (bit 2 = cursor moved)   */
    uint8_t  _pad01[4];   /* +0x01..+0x04 not read here                          */
    uint8_t  bLinkSrc;    /* +0x05 not read here                                */
    uint8_t  _pad06;      /* +0x06 not read here                                */
    uint8_t  _pad07;      /* +0x07 not read here                                */
    uint16_t wCurXIdx;    /* +0x08 arena cursor X: slot index, 0xffff = none    */
    uint16_t wCurYIdx;    /* +0x0A arena cursor Y: slot index, 0xffff = none    */
} CurGridCtxRec;

#include <stddef.h>
_Static_assert(offsetof(CurGridCtxRec, wCurYIdx) == 0x0A,
               "CurGridCtxRec.wCurYIdx must sit at +0x0A");

/* The 16-byte span slot this body DECOMPUTES from the chain top (GridCellRec of
   src/grid/ResolveGridReferenceAndAddCellSpan.c). Never dereferenced here -- only its
   address is formed and handed to the three callees. */
typedef struct {
    uint8_t  bFlags;      /* +0x00                                           */
    uint8_t  bLerpLevel;  /* +0x01                                           */
    uint8_t  bLerpDivisor;/* +0x02                                           */
    uint8_t  _pad03;      /* +0x03                                           */
    uint8_t  bGridX;      /* +0x04                                           */
    uint8_t  bGridY;      /* +0x05                                           */
    uint16_t wAnchorIndex;/* +0x06                                           */
    int16_t  iOffsetX;    /* +0x08                                           */
    int16_t  iOffsetY;    /* +0x0A                                           */
    uint32_t pRefRec;     /* +0x0C guest reference record / search key       */
} GridSlotRec;

/* The 8-byte reference-selection / descriptor entry (GridRefSelRec of
   src/grid/ResolveGridReferenceAndAddCellSpan.c). Stage 1 returns a pointer to one. */
typedef struct {
    uint8_t  _pad00;      /* +0x00                                           */
    uint8_t  bRefFlags;   /* +0x01                                           */
    uint8_t  bRefKind;    /* +0x02                                           */
    uint8_t  _pad03;      /* +0x03                                           */
    void    *pRefEntry;   /* +0x04 reference record, or table base           */
} GridRefSelRec;

#pragma pack(pop)

uint8_t * __stdcall FindPredecessorDescriptorPointerByBackwardScan(const GridSlotRec *pKeyRecord);
void      __stdcall AddGridCellSpanToExtentList(GridSlotRec *pRec);
void      __stdcall ResolveGridReferenceAndAddCellSpan(uint8_t bRefRelative,
                                                       GridSlotRec *pGridRec,
                                                       const GridRefSelRec *pRefSel);

static GridSlotRec *SlotFromCursorIndex(uint16_t wIndex)
{
    intptr_t iTop = (intptr_t)(uintptr_t)g_pSlotsTop;
    intptr_t iDisp = (intptr_t)(int32_t)((uint32_t)wIndex << 4);
    return (GridSlotRec *)(void *)(uintptr_t)(iTop - iDisp);
}

void __stdcall PublishCursorSlotSpanAndFlagContextOnOperandMismatch(CurGridCtxRec *pCtx)
{
    uint16_t     wIdx;         /* ESI  ctx->wCurYIdx                          */
    GridSlotRec *pSlot;        /* EBX  &slot[wCurYIdx], from the chain top    */
    uint8_t     *pDescriptor;  /* EDI  the predecessor descriptor, stage 1     */

    wIdx = pCtx->wCurYIdx;

    if (wIdx == 0xffffu) {
        return;
    }

    pSlot = SlotFromCursorIndex(wIdx);

    pDescriptor = FindPredecessorDescriptorPointerByBackwardScan(pSlot);

    AddGridCellSpanToExtentList(pSlot);

    if ((g_vmOperandBKey != g_vmOperandB) ||
        (g_vmOperandAKey != g_vmOperandA)) {
        pCtx->bCtxFlags = (uint8_t)(pCtx->bCtxFlags | 0x04u);
    }

    ResolveGridReferenceAndAddCellSpan(0u, pSlot, (const GridRefSelRec *)(const void *)pDescriptor);
}
#endif /* NW_UNIT_PublishCursorSlotSpanAndFlagContextOnOperandMismatch */

/* ==========================================================================
 * Unit: PublishGridCellSpanByDepthIndexToExtentList
 * Source: grid/PublishGridCellSpanByDepthIndexToExtentList.c
 * ========================================================================== */
#ifdef NW_UNIT_PublishGridCellSpanByDepthIndexToExtentList
/*
 * Publish Grid Cell Span By Depth Index To Extent List.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-publishgridcellspanbydepthindextoextentlist-c
 */

#include <stdint.h>

extern uint8_t *g_pSlotsTop;

extern void __stdcall AddGridCellSpanToExtentList(uint8_t *pRec);

void __stdcall PublishGridCellSpanByDepthIndexToExtentList(int nIndex)
{
    /* nIndex == 0 addresses the top record itself, nIndex == 1 the one below it,
       and so on -- the chain is indexed downward from g_pSlotsTop. */
    uint8_t *pRec = g_pSlotsTop - (intptr_t)nIndex * 16;

    AddGridCellSpanToExtentList(pRec);
}
#endif /* NW_UNIT_PublishGridCellSpanByDepthIndexToExtentList */

/* ==========================================================================
 * Unit: ReleaseCurrentContextSpanGroup
 * Source: grid/ReleaseCurrentContextSpanGroup.c
 * ========================================================================== */
#ifdef NW_UNIT_ReleaseCurrentContextSpanGroup
/*
 * Releases the grid-cell span group still pending under the current grid context record by handing that record to the same-depth span sweep.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-releasecurrentcontextspangroup-c
 */

#include <stdint.h>
#include "nwglobals.h"

/* Opaque arena record; the 16-byte layout is defined once in
   src/grid/ReleaseSameDepthGridCellSpans.c and is not duplicated here. */
typedef struct GridSpanSlotRec *PGridSpanSlot;


/* Resolved callee — clears the record's pending-link bit and sweeps every
   still-pending span record at the same depth index. */
extern void ReleaseSameDepthGridCellSpans(PGridSpanSlot pSpanRec);

unsigned int ReleaseCurrentContextSpanGroup(void)
{
    /* The record is taken from the global, not from a caller argument. */
    ReleaseSameDepthGridCellSpans(g_pCurGridCtx);

    /* Constant success flag: the callee's return value is intentionally
       discarded and EAX is reloaded with 1 before RET. */
    return 1u;
}
#endif /* NW_UNIT_ReleaseCurrentContextSpanGroup */

/* ==========================================================================
 * Unit: ReleaseGridCellSpanRecord
 * Source: grid/ReleaseGridCellSpanRecord.c
 * ========================================================================== */
#ifdef NW_UNIT_ReleaseGridCellSpanRecord
/*
 * Retires one grid span record: if its flag byte has bit 7 set, the record's cell span is first folded into the global extent list via...
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-releasegridcellspanrecord-c
 */

#include <stdint.h>

/* One 0x10-stride arena slot. Byte[0] carries both the publish flag (bit 7)
   read here and the in-use bit (bit 0) owned by FreeArenaSlot; the remainder is
   the span payload consumed by AddGridCellSpanToExtentList. Only the fields this
   function actually touches are modelled — the struct exists to give the flag
   byte a name and the pointer a type. */
typedef struct {
    uint8_t bStatusFlags;   /* +0x00 bit7 = publish span, bit0 = arena in-use */
    uint8_t _pad01[7];      /* +0x01 .. +0x07 span payload (see callee notes)   */
    int16_t iOffsetX;       /* +0x08 cell offset X, read by the callee         */
    int16_t iOffsetY;       /* +0x0A cell offset Y, read by the callee         */
    uint32_t pRefRec;       /* +0x0C guest reference record                    */
} GridSpanSlot;

extern void __stdcall AddGridCellSpanToExtentList(GridSpanSlot *pRec);

extern void FreeArenaSlot(uint8_t *pSlotToFree);

void __stdcall ReleaseGridCellSpanRecord(GridSpanSlot *pSpanRec)
{
    /* Publish the span while the record is still allocated. */
    if ((pSpanRec->bStatusFlags & 0x80u) != 0u) {
        AddGridCellSpanToExtentList(pSpanRec);
    }

    /* Always return the slot to the arena, published or not. */
    FreeArenaSlot((uint8_t *)pSpanRec);
}
#endif /* NW_UNIT_ReleaseGridCellSpanRecord */

/* ==========================================================================
 * Unit: ReleaseGridCellSpanRecordsAndDispatchEntry
 * Source: grid/ReleaseGridCellSpanRecordsAndDispatchEntry.c
 * ========================================================================== */
#ifdef NW_UNIT_ReleaseGridCellSpanRecordsAndDispatchEntry
/*
 * Release Grid Cell Span Records And Dispatch Entry.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-releasegridcellspanrecordsanddispatchentry-c
 */

#include <stdint.h>
#include <stddef.h>

/*
 * Raw 16-byte arena slot. THE ARENA CHAIN IS HETEROGENEOUS — there are at least
 * two record flavors discriminated by byte +0x00 bit 1, and this word is a
 * union between them:
 *
 *   bit 0x02 CLEAR ("orphan span"): +0x04 = packed cell key, +0x06 = 0xFFFF
 *                                   unset / depth-stamp word
 *   bit 0x02 SET   ("grouped"):     +0x06 = packed cell key,
 *                                   +0x08 and +0x0A = a pair of depth indices
 *                                   (each retired via ReleaseSpanRecordByDepthIndex,
 *                                   de-duplicated when equal),
 *                                   +0x0C = heap block when bit 0x08 is set
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t   bStatusFlags;   /* +0x00 bit0 = arena in-use, bit1 = grouped,
                                bit3 = +0x0C heap block present,
                                bit6 = offsets reference-relative,
                                bit7 = publish span to extent list          */
    uint8_t   bAux;           /* +0x01                                       */
    uint8_t   bKind;          /* +0x02 low byte of the stamp word the epilogue
                                copies back into the cell entry               */
    uint8_t   bGroupFlags;    /* +0x03 high byte of that word; its low nibble
                                is what the masked store keeps               */
    uint16_t  wCellKeyOrRefA; /* +0x04 cell key (orphan flavor)               */
    uint16_t  wKeyOrStamp;    /* +0x06 cell key (grouped) / unset stamp (orphan) */
    uint16_t  wDepthIndexA;   /* +0x08 first recorded depth index             */
    uint16_t  wDepthIndexB;   /* +0x0A second recorded depth index            */
    uint32_t  pHeapBlock;     /* +0x0C guest heap block of sub-spans          */
} GridSpanSlot;
#pragma pack(pop)

typedef struct {
    uint16_t wEntryWord;
} GridCellEntry;

#define CELL_ENTRY_LOW_BYTE(e)  ((uint8_t)((e)->wEntryWord & 0xFFu))
#define CELL_ENTRY_HIGH_BYTE(e) ((uint8_t)((e)->wEntryWord >> 8))

/* One 0x10-stride entry of g_pStateTable, as modelled by
   src/grid/ProcessPendingGridCellSpanEntry.c — only the field this function reads. */
#pragma pack(push, 1)
typedef struct {
    uint8_t _pad00[4];
    uint32_t pCursorA;        /* +0x04 guest cursor, not used here             */
    uint8_t _pad08[4];
    uint32_t pCursorB;        /* +0x0C guest script cursor dispatched here     */
} GridEntryRec;
#pragma pack(pop)

extern GridSpanSlot *g_pSlotsTop;

extern GridSpanSlot *g_pSlotsBottom;

extern GridEntryRec *g_pStateTable;

extern GridCellEntry *g_pCellEntryBase;

extern uint8_t g_bCellIndexShift;

extern void ReleaseSpanRecordByDepthIndex(int nDepthIndex);

/* Returns the 8-byte-stride sub-span block at slot +0x0C to the custom heap.
   Guarded here by slot byte +0x00 bit 3. */
extern void __stdcall FreeHeapBlock(unsigned char *pData);

/* Clears this record's group-pending bit, then sweeps and retires every
   still-pending record at the same depth index. */
extern void ReleaseSameDepthGridCellSpans(GridSpanSlot *pSpanRec);

/* Clears the slot's status byte and, if it was the tail occupant, shrinks the
   arena — this is why g_pSlotsBottom stays a live read in the loop. */
extern void FreeArenaSlot(unsigned char *pSlotToFree);

/* Re-points the active-slot selection away from the slot just freed. */
extern void __stdcall SelectNextActiveSlot(unsigned char *pOldSlot);

/* Publishes the record's span into the global extent list when bit 7 is set,
   then ALWAYS frees the slot. Bit 7 is NOT tested by this caller, so a matched
   orphan record can still publish. */
extern void __stdcall ReleaseGridCellSpanRecord(GridSpanSlot *pSpanRec);

/* Runs one script opcode under a temporary context, restoring the interpreter's
   nine persistent state words afterwards. A NULL cursor is a no-op. */
extern void ExecuteScriptInstruction(unsigned char bFlag, unsigned char bOperandA,
                                     unsigned char bOperandB, uint32_t dwCtxA,
                                     uint32_t dwCtxB, uint32_t dwCtxC,
                                     uint32_t dwCtxD, unsigned char *pCursor);

void ReleaseGridCellSpanRecordsAndDispatchEntry(unsigned char bFullSweepMode,
                                                int nCellMajor, int nCellMinor)
{
    /* Cell-keyed lookup into the 2-byte-stride table. The shift is masked to
       5 bits exactly as the asm does (`SHL EAX,CL` after loading the byte). */
    GridCellEntry *pCellEntry =
        &g_pCellEntryBase[((uint32_t)nCellMajor << (g_bCellIndexShift & 0x1fu))
                          + (uint32_t)nCellMinor];

    if (CELL_ENTRY_LOW_BYTE(pCellEntry) == 0xFEu) {
        return;
    }

    GridEntryRec *pEntry = &g_pStateTable[CELL_ENTRY_LOW_BYTE(pCellEntry)];

    const uint16_t wCellKey =
        (uint16_t)(((uint32_t)nCellMinor & 0xFFu) | (((uint32_t)nCellMajor & 0xFFu) << 8));

    unsigned char *pLastReleased = NULL;
    unsigned char  bReleaseCount = 0;

    GridSpanSlot *pSlot = g_pSlotsTop;
    for (;;) {

        uint8_t bFlags = pSlot->bStatusFlags;
        int bDidRelease = 0;

        if ((bFlags & 0x01u) != 0u) {      /* bit 0 = arena in-use */

            if ((bFlags & 0x02u) != 0u) {
                /* === GROUPED record: +0x06 carries the packed cell key === */
                if (pSlot->wKeyOrStamp == wCellKey) {

                    ReleaseSpanRecordByDepthIndex((int)pSlot->wDepthIndexB);
                    if (pSlot->wDepthIndexB != pSlot->wDepthIndexA) {
                        ReleaseSpanRecordByDepthIndex((int)pSlot->wDepthIndexA);
                    }

                    if ((pSlot->bStatusFlags & 0x08u) != 0u) {
                        /* bit 3 = an 8-byte-stride sub-span block is attached */
                        FreeHeapBlock((unsigned char *)(uintptr_t)
                                      pSlot->pHeapBlock);
                    }

                    /* Recorded BEFORE the release calls, because the epilogue
                       reads +2/+3 of this record after FreeArenaSlot has cleared
                       its byte 0. */
                    pLastReleased = (unsigned char *)(void *)pSlot;

                    ReleaseSameDepthGridCellSpans(pSlot);
                    FreeArenaSlot((unsigned char *)(void *)pSlot);
                    SelectNextActiveSlot((unsigned char *)(void *)pSlot);
                    bDidRelease = 1;

                    if (bFullSweepMode != 1u) {
                        break;
                    }
                    /* Second release of the call -> straight to the epilogue. */
                    if (bReleaseCount != 0u) {
                        break;
                    }
                }
            } else {
                /* === ORPHAN span record: +0x04 carries the packed cell key === */

                if (bFullSweepMode != 0u &&
                    pSlot->wCellKeyOrRefA == wCellKey &&
                    /* +0x06 must still hold the unset marker — this record was
                       never stamped into a depth group. */
                    pSlot->wKeyOrStamp == 0xFFFFu &&

                    (bFlags & 0x40u) == 0u) {

                    ReleaseGridCellSpanRecord(pSlot);
                    bDidRelease = 1;

                    if (bReleaseCount != 0u) {
                        break;
                    }
                }
            }
        }

        if (bDidRelease) {
            bReleaseCount++;
        }

        pSlot = (GridSpanSlot *)(void *)((unsigned char *)(void *)pSlot - 0x10);
        /* g_pSlotsBottom is RE-READ every iteration: the release path calls
           FreeArenaSlot, which can shrink the arena and move that sentinel. */
        if ((unsigned long)(size_t)pSlot <= (unsigned long)(size_t)g_pSlotsBottom) {
            break;
        }
    }

    if (pLastReleased != NULL) {
        const GridSpanSlot *pRel = (const GridSpanSlot *)(void *)pLastReleased;
        uint16_t wStamp = (uint16_t)((uint32_t)pRel->bKind |
                                     ((uint32_t)(pRel->bGroupFlags & 0xF0u) << 8));
        pCellEntry->wEntryWord = wStamp;
    }

    ExecuteScriptInstruction(CELL_ENTRY_HIGH_BYTE(pCellEntry),
                             (unsigned char)((uint32_t)nCellMajor & 0xFFu),
                             (unsigned char)((uint32_t)nCellMinor & 0xFFu),
                             0u, 0xFFFFu, 0u,
                             (uint32_t)(uintptr_t)pEntry,
                             (uint8_t *)(uintptr_t)pEntry->pCursorB);
}
#endif /* NW_UNIT_ReleaseGridCellSpanRecordsAndDispatchEntry */

/* ==========================================================================
 * Unit: ReleaseSameDepthGridCellSpans
 * Source: grid/ReleaseSameDepthGridCellSpans.c
 * ========================================================================== */
#ifdef NW_UNIT_ReleaseSameDepthGridCellSpans
/*
 * Clears the caller's pending-release link bit and then sweeps the whole 16-byte arena chain.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-releasesamedepthgridcellspans-c
 */

#include <stdint.h>

/* Raw 16-byte arena slot. Only the fields this function touches are modelled. */
#pragma pack(push, 1)
typedef struct {
    uint8_t  bStatusFlags;   /* +0x00 bit0 = in-use, bits 0xe0 = grid-span stamp */
    uint8_t  bAux;           /* +0x01                                     */
    uint8_t  bKind;          /* +0x02                                     */
    uint8_t  bLinkFlags;     /* +0x03 bit1 = "group still pending"         */
    uint8_t  _pad04[2];      /* +0x04 .. +0x05                             */
    uint16_t wDepthIndex;    /* +0x06 stamp compared against the caller's  */
    uint8_t  _pad08[8];      /* +0x08 .. +0x0F span payload / pRefRec      */
} GridSpanSlot;
#pragma pack(pop)

_Static_assert(sizeof(GridSpanSlot) == 0x10,
               "GridSpanSlot must stay a raw 16-byte arena slot");

extern GridSpanSlot *g_pSlotsTop;

extern GridSpanSlot *g_pSlotsBottom;

/* Resolved callee — publishes (if flagged) then frees one span record. */
extern void __stdcall ReleaseGridCellSpanRecord(GridSpanSlot *pSpanRec);

void ReleaseSameDepthGridCellSpans(GridSpanSlot *pSpanRec)
{

    if ((pSpanRec->bLinkFlags & 0x02u) == 0u) {
        return;
    }
    pSpanRec->bLinkFlags &= (uint8_t)0xfdu;

    /* This record's depth index, in 0x10-byte strides below the arena top. */
    int nDepthIndex =
        (int)(((uintptr_t)(void *)g_pSlotsTop - (uintptr_t)(void *)pSpanRec) / 0x10u);

    if (g_pSlotsTop <= g_pSlotsBottom) {
        return; /* empty chain */
    }

    /* Walk the whole chain downward. g_pSlotsBottom is re-read every iteration
       because the callee can shrink the arena and move that sentinel. */
    for (GridSpanSlot *pSlot = g_pSlotsTop; pSlot > g_pSlotsBottom;
         pSlot = (GridSpanSlot *)(void *)((uint8_t *)(void *)pSlot - 0x10u)) {
        /* Stamped span, allocated, not yet swept... */
        if ((pSlot->bStatusFlags & 0xe3u) != 0xe1u) {
            continue;
        }
        /* ...and stamped at the caller's depth index. MOVZX: compare unsigned. */
        if ((uint32_t)pSlot->wDepthIndex != (uint32_t)nDepthIndex) {
            continue;
        }
        ReleaseGridCellSpanRecord(pSlot);
    }
}
#endif /* NW_UNIT_ReleaseSameDepthGridCellSpans */

/* ==========================================================================
 * Unit: ReleaseSpanRecordByDepthIndex
 * Source: grid/ReleaseSpanRecordByDepthIndex.c
 * ========================================================================== */
#ifdef NW_UNIT_ReleaseSpanRecordByDepthIndex
/*
 * Release Span Record By Depth Index.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-releasespanrecordbydepthindex-c
 */

#include <stdint.h>

#pragma pack(push, 1)
typedef struct {
    uint8_t  bStatusFlags;   /* +0x00 bit7 = publish span, bit0 = arena in-use   */
    uint8_t  _pad01[2];      /* +0x01 .. +0x02                                    */
    uint8_t  bLinkFlags;     /* +0x03 bit1 = "depth group still pending"         */
    uint16_t wCellCoord;
    uint16_t wDepthIndex;    /* +0x06 strides below g_pSlotsTop; 0xFFFF = unset.
                                NOTE: src/grid/ProcessPendingGridCellSpanEntry.c calls
                                this wTagStamp — see its header for the dispute. */
    uint8_t  _pad08[2];      /* +0x08 .. +0x09                                    */
    uint16_t wRefDepthIndex; /* +0x0A second recorded depth index (caller reads
                                +0x08 and +0x0A and releases each)               */
    uint32_t pRefRec;        /* +0x0C guest heap block                          */
} GridSpanSlot;
#pragma pack(pop)

extern GridSpanSlot *g_pSlotsTop;

extern void __stdcall ReleaseGridCellSpanRecord(GridSpanSlot *pSpanRec);

void ReleaseSpanRecordByDepthIndex(int nDepthIndex)
{
    /* Sentinel: no depth index recorded. Tested before the SHL — 0xFFFF is not
       a reachable index and would address 64 KiB below the arena top. */
    if (nDepthIndex == 0xFFFF) {
        return;
    }

    /* SHL EAX,0x4 / SUB ECX,EAX — 32-bit arithmetic, exactly as the asm does it. */
    uint32_t uByteOffset = (uint32_t)nDepthIndex * 0x10u;
    GridSpanSlot *pSpanRec =
        (GridSpanSlot *)(void *)((uint8_t *)(void *)g_pSlotsTop - uByteOffset);

    /* Unconditional: ReleaseGridCellSpanRecord frees the arena slot even when
       the publish flag is clear, so the caller's equal-index de-duplication is
       required, not merely an optimisation. */
    ReleaseGridCellSpanRecord(pSpanRec);
}
#endif /* NW_UNIT_ReleaseSpanRecordByDepthIndex */

/* ==========================================================================
 * Unit: ReleaseTopGridCellSpans
 * Source: grid/ReleaseTopGridCellSpans.c
 * ========================================================================== */
#ifdef NW_UNIT_ReleaseTopGridCellSpans
/*
 * Release Top Grid Cell Spans.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-releasetopgridcellspans-c
 */

#include <stdint.h>
#include "nwglobals.h"

/* Raw 16-byte arena slot. Only the +0x0C field this function reads is modelled;
   the offset fixup that writes +0x08 is the allocator's business, not this
   function's — see src/grid/AllocGridCellSpanRecord.c. */
#pragma pack(push, 1)
typedef struct {
    uint8_t  bStatusFlags;   /* +0x00 bit7 = publish pending, bit0 = in-use */
    uint8_t  _pad01[11];     /* +0x01 .. +0x0B span payload                */
    uint32_t pRefRec;        /* +0x0C guest reference record                */
} GridCellSpanRec;

/* Reference record behind pRefRec: a pointer to the source word pair, plus the
   signed baseline. The source word is read by dereferencing +0x00. */
typedef struct {
    uint32_t pSrcWords;        /* +0x00 guest pointer to int16 pair        */
    int16_t  iBaseX;         /* +0x04 baseline X        (MOVSX) */
} GridRefEntry;
#pragma pack(pop)



/* Resolved callee — publishes the span when bit 7 is set, then ALWAYS frees
   the slot to the arena, so it must run after the +0x0C read above. */
extern void __stdcall ReleaseGridCellSpanRecord(GridCellSpanRec *pSpanRec);

void ReleaseTopGridCellSpans(int32_t nSpanCount)
{
    do {
        /* Pop before touching the record: the depth is decremented first so the
           index below is the already-shrunk depth (LIFO from the top). */
        g_nGridSpanStackDepth -= 1;

        GridCellSpanRec *pRec = g_apGridSpanStack[g_nGridSpanStackDepth];

        /* Undo the offset advance this record caused when it was pushed. The
           arithmetic is done unsigned then subtracted, as SUB does. */
        const GridRefEntry *pRef =
            (const GridRefEntry *)(uintptr_t)pRec->pRefRec;
        uint32_t uSpan =
            (uint32_t)((const uint16_t *)(uintptr_t)pRef->pSrcWords)[0] -
            (uint32_t)(int32_t)pRef->iBaseX;
        g_nGridCellOffsetX = (int32_t)((uint32_t)g_nGridCellOffsetX - uSpan);

        /* Publish-if-flagged, then always back to the arena. */
        ReleaseGridCellSpanRecord(pRec);

        nSpanCount -= 1;
    } while (nSpanCount != 0);
}
#endif /* NW_UNIT_ReleaseTopGridCellSpans */

/* ==========================================================================
 * Unit: ResolveGridReferenceAndAddCellSpan
 * Source: grid/ResolveGridReferenceAndAddCellSpan.c
 * ========================================================================== */
#ifdef NW_UNIT_ResolveGridReferenceAndAddCellSpan
/*
 * Resolve Grid Reference And Add Cell Span.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-resolvegridreferenceandaddcellspan-c
 */

#include <stdint.h>
#include "nwglobals.h"

/* Packed: pRefEntry really is the dword at +0x04, right after the three bytes. */
#pragma pack(push, 1)
typedef struct {
    uint8_t   bFlags;        /* +0x00 grid flags (0x08 / 0x10 / 0x40)      */
    uint8_t   bLerpLevel;    /* +0x01 zeroed here                          */
    uint8_t   bLerpDivisor;  /* +0x02                                       */
    uint8_t   _pad03;        /* +0x03                                       */
    uint8_t   bGridX;        /* +0x04                                       */
    uint8_t   bGridY;        /* +0x05                                       */
    uint16_t  wAnchorIndex;  /* +0x06                                       */
    int16_t   iOffsetX;      /* +0x08                                       */
    int16_t   iOffsetY;      /* +0x0A                                       */
    uint32_t  pRefRec;       /* +0x0C guest reference record                */
} GridCellRec;

typedef struct {
    uint8_t   _pad00;        /* +0x00 untouched here                        */
    uint8_t   bRefFlags;     /* +0x01 see table above                       */
    uint8_t   bRefKind;      /* +0x02 1 = direct, else nine-slot table      */
    uint8_t   _pad03;        /* +0x03                                       */
    uint32_t  pRefEntry;     /* +0x04 guest reference record or table base  */
} GridRefSelRec;
#pragma pack(pop)

/* operand-B state word 1 (zero-extended byte) */
/* operand-A state word 1 (zero-extended byte) */

extern uint8_t  g_vmModeFlag; /* opcode-mode / bFlag byte                   */

extern void __stdcall ComputeScaledGridOffset(GridCellRec *pRec) __asm__("_ComputeScaledGridOffset");
extern void __stdcall AddGridCellSpanToExtentList(GridCellRec *pRec);

static unsigned int PickRefEntryIndex(uint8_t bRefFlags, uint8_t *pbGridFlags)
{
    const int32_t nOpA1 = (int32_t)g_vmOperandA;  /* compared against nOpA2 (JLE/JGE) */
    const int32_t nOpA2 = (int32_t)g_vmOperandAKey;
    const int32_t nOpB1 = (int32_t)g_vmOperandB;
    const int32_t nOpB2 = (int32_t)g_vmOperandBKey;
    const int bBit1 = (bRefFlags & 2u) != 0;   /* -> grid 0x08 */
    const int bBit2 = (bRefFlags & 4u) != 0;   /* -> grid 0x10 */
    const int bBit3 = (bRefFlags & 8u) != 0;   /* index-0 eligibility */

    switch (g_vmModeFlag)
    {
    case 0x10u:

        if ((nOpA1 == nOpA2) && bBit3)
            return 0u;
        return 1u;

    case 0x20u:

        if (nOpA1 > nOpA2)
            return 2u;
        if (nOpA1 < nOpA2) {
            if (!bBit2)
                return 4u;
            *pbGridFlags |= 0x10u;
            return 2u;
        }

        if (bBit3 && (nOpB1 == nOpB2))
            return 0u;
        return 3u;

    case 0x40u:

        if ((nOpA1 == nOpA2) && bBit3)
            return 0u;
        if (bBit2) {
            *pbGridFlags |= 0x10u;
            return 1u;
        }
        return 5u;

    default:
        /* 0x409ab */
        if (nOpA1 == nOpA2) {
            if (bBit3 && (nOpB1 == nOpB2))
                return 0u;
            if (bBit1) {
                *pbGridFlags |= 0x08u;
                return 3u;
            }
            return 7u;                          /* ESI still 7 */
        }
        if (nOpA1 > nOpA2) {
            if (bBit1) {
                *pbGridFlags |= 0x08u;
                return 2u;
            }
            return 8u;
        }

        if (bBit2) {
            *pbGridFlags |= 0x10u;
            if (bBit1) {
                *pbGridFlags |= 0x08u;
                return 2u;
            }
            return 8u;
        }
        if (bBit1) {
            *pbGridFlags |= 0x08u;
            return 4u;
        }
        return 6u;
    }
}

void __stdcall ResolveGridReferenceAndAddCellSpan(uint8_t bRefRelative, GridCellRec *pGridRec,
                                        const GridRefSelRec *pRefSel)
{
    /* Clear grid bits 3 and 4 (0x08/0x10); 0xE7 preserves bit 6 (0x40).
       Then zero the blend level (+1). */
    pGridRec->bFlags &= 0xE7u;                  /* AND byte ptr [EBX],0xe7 */
    pGridRec->bLerpLevel = 0u;

    if (pRefSel->bRefKind == 1u) {

        pGridRec->pRefRec = pRefSel->pRefEntry;
        if (g_vmModeFlag == 0x80u) {
            if ((pRefSel->bRefFlags & 2u) != 0u)
                pGridRec->bFlags |= 0x08u;
        } else if (g_vmModeFlag == 0x40u) {
            if ((pRefSel->bRefFlags & 4u) != 0u)
                pGridRec->bFlags |= 0x10u;
        }
    } else {

        const unsigned int uIdx = PickRefEntryIndex(pRefSel->bRefFlags, &pGridRec->bFlags);
        const uint32_t *papTable =
            (const uint32_t *)(uintptr_t)pRefSel->pRefEntry;

        pGridRec->pRefRec = papTable[uIdx];
    }

    if (bRefRelative != 0u) {
        /* Offsets are already in reference-baseline space: skip the computation. */
        pGridRec->bFlags |= 0x40u;
    } else {

        pGridRec->bGridX = (uint8_t)g_vmOperandBKey;  /* operand-B state word 2 */
        pGridRec->bGridY = (uint8_t)g_vmOperandAKey;  /* operand-A state word 2 */
        ComputeScaledGridOffset(pGridRec);
    }

    AddGridCellSpanToExtentList(pGridRec);
}
#endif /* NW_UNIT_ResolveGridReferenceAndAddCellSpan */

/* ==========================================================================
 * Unit: RestartCurrentGridSpanGroup
 * Source: grid/RestartCurrentGridSpanGroup.c
 * ========================================================================== */
#ifdef NW_UNIT_RestartCurrentGridSpanGroup
/*
 * Restart Current Grid Span Group.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-restartcurrentgridspangroup-c
 */

#include <stdint.h>
#include "nwglobals.h"

/* byte +3, bit 0x02 — set here, tested-then-cleared by the same-depth sweep. */
#define GRID_CTX_GROUP_PENDING 0x02u


/* Resolved callee — hands the current context to the same-depth span sweep.
   Its return value (always 1) is intentionally discarded here. */
extern unsigned int ReleaseCurrentContextSpanGroup(void);

unsigned char * __stdcall RestartCurrentGridSpanGroup(unsigned char *pInstrStream)
{
    /* 1. Retire whatever group is still pending at this context's depth. The
          callee's "success" 1 is dropped — it is not this function's result. */
    ReleaseCurrentContextSpanGroup();

    /* 2. Re-arm the context: re-read the global (the sweep may have moved the
          arena chain) and set the bit the next same-depth sweep triggers on. */
    g_pCurGridCtx->bLinkFlags |= GRID_CTX_GROUP_PENDING;

    /* 3. Identity return: the caller gets its own cursor back, byte for byte. */
    return pInstrStream;
}
#endif /* NW_UNIT_RestartCurrentGridSpanGroup */

/* ==========================================================================
 * Unit: SelectGridSpanRecordByPointAndPublishSelectionState
 * Source: grid/SelectGridSpanRecordByPointAndPublishSelectionState.c
 * ========================================================================== */
#ifdef NW_UNIT_SelectGridSpanRecordByPointAndPublishSelectionState
/*
 * Select Grid Span Record By Point And Publish Selection State.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-selectgridspanrecordbypointandpublishselectionstate-c
 */

#include <windows.h>
#include "nwglobals.h"
#include <stdint.h>

extern uint8_t  *g_pSlotsTop;
extern uint8_t  *g_pSlotsBottom;
extern uint8_t  *g_pStateTable;
extern uint8_t  *g_pActiveSlot;





extern void __stdcall SelectNextActiveSlot(uint8_t *pOldSlot);
extern int  __stdcall TestPointInsideGridSpanRecord(int32_t nPointY, int32_t nPointX,
                                                    const void *pIndexSrc);

/* One 16-byte grid-span slot. Only the bytes THIS body touches are named. (The siblings
   model +0x04 and +0x06 as 16-bit words in places -- ReleaseSpanRecordByDepthIndex.c calls
   +0x06 wDepthIndex -- but THIS body reads +0x04, +0x06 and +0x07 as single BYTES, so the
   byte view is used here.) */
#pragma pack(push, 1)
typedef struct {
    uint8_t  bStatus;     /* +0x00  low two bits == 3 marks a live span slot            */
    uint8_t  bTableIdx;   /* +0x01  index into g_pStateTable (stride 0x10)              */
    uint8_t  _pad02[2];   /* +0x02..+0x03                                              */
    uint8_t  bAttr;       /* +0x04  attribute/stamp byte written during the walk        */
    uint8_t  bPriority;   /* +0x05  tie-break key; lower value wins                     */
    uint8_t  bCoordH;     /* +0x06  published H coordinate on a hit                     */
    uint8_t  bCoordV;     /* +0x07  published V coordinate on a hit                     */
    uint8_t  _pad08[2];   /* +0x08..+0x09                                              */
    uint16_t wSpanIndex;  /* +0x0A  chain index consumed by TestPointInsideGridSpanRecord */
    uint8_t  _pad0c[4];   /* +0x0C..+0x0F                                              */
} GridSpanSlot;
#pragma pack(pop)

#define CMD_ADVANCE_ACTIVE 0x8Fu  /* advances g_pActiveSlot via SelectNextActiveSlot */
#define CMD_ATTR_90        0x82u  /* sets the flag byte to 0x90 */
#define CMD_ATTR_A0        0x83u  /* sets the flag byte to 0xa0 */
#define CMD_PASS_84        0x84u  /* no local flag change; goes straight to the join */
#define CMD_PASS_81        0x81u  /* no local flag change; goes straight to the join */
#define CMD_SUBST_PAIR     0x0Eu  /* substitutes the secondary coordinate pair */
#define ATTR_SENTINEL      0x0Eu  /* ARG1 value that suppresses the OR into the flag byte */
#define ATTR_MASK          0x4Fu  /* flag mask applied for the mapped stamp */
#define PRIORITY_NONE      0xFFu  /* initial running-best priority */

void __stdcall SelectGridSpanRecordByPointAndPublishSelectionState(
        uint8_t bAttrCode, uint8_t bCommand,
        int32_t nPointY1, int32_t nPointX1, int32_t nPointY2, int32_t nPointX2)
{
    GridSpanSlot *pBest = NULL;          /* ESI -- the currently chosen slot            */
    GridSpanSlot *pSlot;
    uint8_t       bAttr = 0u;            /* [ESP+0x11] flag/attribute byte (init 0)     */
    uint8_t       bBestPriority = PRIORITY_NONE; /* [ESP+0x12] running best (init 0xff) */
    uint8_t       bBestTableByte = 0u;   /* [ESP+0x10] picked slot's state-table byte   */
    int32_t       nPointX = nPointX1;    /* EBP        -- the callee's X                */
    int32_t       nPointY = nPointY1;    /* [ESP+0x14] -- the callee's Y                */

    switch (bCommand) {
    case CMD_ADVANCE_ACTIVE:
        SelectNextActiveSlot(g_pActiveSlot);
        break;
    case CMD_ATTR_90:
        bAttr = 0x90u;
        break;
    case CMD_ATTR_A0:
        bAttr = 0xA0u;
        break;
    case CMD_PASS_84:
    case CMD_PASS_81:
        break;
    case CMD_SUBST_PAIR:
        nPointX = nPointX2;
        nPointY = nPointY2;
        break;
    default:
        break;
    }

    if (bAttrCode != ATTR_SENTINEL) {
        bAttr = (uint8_t)(bAttr | (uint8_t)(bAttrCode | 0x40u));
    }

    nPointX += g_dwPointOffsetX;
    nPointY += g_dwPointOffsetY;

    if ((uintptr_t)g_pSlotsTop > (uintptr_t)g_pSlotsBottom) {
        pSlot = (GridSpanSlot *)(void *)g_pSlotsTop;
        do {
            uint8_t bAttrMasked = (uint8_t)(bAttr & ATTR_MASK); /* [ESP+0x18] */

            if ((pSlot->bStatus & 0x03u) == 0x03u) {
                const uint8_t *pEntry =
                    (const uint8_t *)(void *)((uint8_t *)(void *)g_pStateTable
                                              + ((uintptr_t)pSlot->bTableIdx << 4));
                uint8_t bTableByte = pEntry[0];

                switch (bTableByte) {
                case 1u:
                    pSlot->bAttr = bAttr;
                    break;
                case 2u:
                    pSlot->bAttr = bAttrMasked;
                    break;
                case 3u:
                    pSlot->bAttr = (pSlot == (GridSpanSlot *)(void *)g_pActiveSlot)
                                   ? bAttrMasked : 0u;
                    break;
                default:
                    break;
                }

                if ((pBest == NULL) || (pSlot->bPriority <= bBestPriority)) {
                    if (TestPointInsideGridSpanRecord(nPointY, nPointX, pSlot) != 0) {
                        pBest           = pSlot;
                        bBestTableByte  = bTableByte;
                        bBestPriority   = pSlot->bPriority;
                    }
                }
            }

            pSlot = (GridSpanSlot *)(void *)((uint8_t *)(void *)pSlot - 0x10u);
        } while ((uintptr_t)pSlot > (uintptr_t)g_pSlotsBottom);
    }

    if (pBest != NULL) {
        g_bPickedCellLow = bBestTableByte;

        if (bBestTableByte == 2u) {
            if (bCommand == CMD_SUBST_PAIR) {
                bAttr = (uint8_t)(bAttr | 0x10u);
            } else if (bCommand == CMD_PASS_81) {
                bAttr = (uint8_t)(bAttr | 0x20u);
            }
            pBest->bAttr = bAttr;
        } else if (bBestTableByte == 3u) {
            if ((bAttr == 0x90u) || (bAttr == 0xA0u)) {
                g_pActiveSlot = (uint8_t *)(void *)pBest;
                pBest->bAttr = bAttr;
            }
        }

        g_dwSelPairBH = (uint32_t)pBest->bCoordH;
        g_dwSelPairBV = (uint32_t)pBest->bCoordV;
    } else {
        g_dwSelPairBH = (uint32_t)(nPointX / g_dwCellScaleX);
        g_dwSelPairBV = (uint32_t)(nPointY / g_dwCellScaleY);
        {
            uint32_t uCellIndex =
                ((uint32_t)g_dwSelPairBV << (g_dwCellRowShift & 0x1Fu))
                + (uint32_t)g_dwSelPairBH;
            g_bPickedCellLow = (uint8_t)(g_pCellGrid[uCellIndex] & 0xFFu);
        }
    }

    g_bCommandOut = bCommand;
    if ((bCommand == 0x82u) || (bCommand == 0x83u)) {
        g_bGateCommand   = bCommand;
        g_bGateCellIndex = g_bPickedCellLow;
        g_dwSelPairAH    = g_dwSelPairBH;
        g_dwSelPairAV    = g_dwSelPairBV;
    } else if (bCommand == CMD_PASS_81) {
        g_bGateCommand = CMD_SUBST_PAIR;
        g_bCommandOut  = CMD_SUBST_PAIR;
    } else {
        g_bCommandOut = CMD_SUBST_PAIR;
    }
}
#endif /* NW_UNIT_SelectGridSpanRecordByPointAndPublishSelectionState */

/* ==========================================================================
 * Unit: SetDefaultGridCellOffsetAndExtent
 * Source: grid/SetDefaultGridCellOffsetAndExtent.c
 * ========================================================================== */
#ifdef NW_UNIT_SetDefaultGridCellOffsetAndExtent
/*
 * Set Default Grid Cell Offset And Extent.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-setdefaultgridcelloffsetandextent-c
 */

#include <stdint.h>
#include "nwglobals.h"

#pragma pack(push, 1)
typedef struct {
    uint8_t  bFlags;      /* +0x00 */
    uint8_t  _pad01[7];   /* +0x01 .. +0x07 */
    int16_t  iOffsetX;
    int16_t  iOffsetY;
    uint32_t pRefRec;
} GridCellRec;
#pragma pack(pop)

/* ---- parent-resolved callee signatures (do not redefine) ---- */
extern void __stdcall AddGridCellSpanToExtentList(GridCellRec *pRec);
extern int  CopySpriteRowsToDisplayBuffer(void);



void __stdcall SetDefaultGridCellOffsetAndExtent(uint16_t wOffsetY, uint16_t wOffsetX)
{

    if (g_view_flag_7d != 0u) { return; }
    if (g_state_417388 != 0)   { return; }
    if (g_flag_417801 != 0u)   { return; }
    if (g_default_grid_record.pRefRec == 0u) { return; }
    if (g_bActiveCellReady == 0u)   { return; }

    /* Gate passed: arm the default record and submit its PREVIOUS extent, i.e.
       the rectangle the record still describes before the new offsets land. */
    g_default_recsel = 1u;
    AddGridCellSpanToExtentList(&g_default_grid_record);

    g_default_grid_record.iOffsetX = (int16_t)wOffsetX;
    g_default_grid_record.iOffsetY = (int16_t)wOffsetY;

    /* Submit the NEW extent for the same record, then repaint. */
    AddGridCellSpanToExtentList(&g_default_grid_record);
    (void)CopySpriteRowsToDisplayBuffer();
}
#endif /* NW_UNIT_SetDefaultGridCellOffsetAndExtent */

/* ==========================================================================
 * Unit: TestPointInsideGridSpanRecord
 * Source: grid/TestPointInsideGridSpanRecord.c
 * ========================================================================== */
#ifdef NW_UNIT_TestPointInsideGridSpanRecord
/*
 * Test Point Inside Grid Span Record.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-testpointinsidegridspanrecord-c
 */

#include <stdint.h>   /* intptr_t / int16_t / uint16_t / uint32_t */

#pragma pack(push, 1)

/* The index source (ARG3). Only +0x0A is read here. +0x08 is shown because the sibling
   files model this pair as the grid context's (wCurXIdx @+0x08, wCurYIdx @+0x0A), but
   THIS body never touches it. */
typedef struct {
    uint8_t  _pad00[8];      /* +0x00..+0x07 not read in this function            */
    uint16_t wSiblingIdx;    /* +0x08 chain cursor index (not read here)          */
    uint16_t wSlotIndex;     /* +0x0A chain cursor index, 0xFFFF = none (the index) */
} IndexSourceRec;

/* One 16-byte grid-span arena slot, as addressed by the chain index. */
typedef struct {
    uint8_t  _pad00[8];      /* +0x00..+0x07 not read in this function   */
    int16_t  iOffsetX;       /* +0x08 min-corner X  (MOVSX, signed)      */
    int16_t  iOffsetY;       /* +0x0A min-corner Y  (MOVSX, signed)      */
    uint32_t pExtentDesc;    /* +0x0C guest pointer to the descriptor     */
} GridSpanRecord;

/* What [slot+0x0C] points to: its first DWORD is a guest pointer to the
 * two-word extent pair.  Both fields stay 32-bit in the recovered layout. */
typedef struct {
    uint32_t pWords;         /* +0x00 guest pointer to two uint16 words   */
} ExtentDescriptor;

#pragma pack(pop)

extern uint8_t *g_pSlotsTop;

static const GridSpanRecord *SlotFromIndex(uint16_t wIndex)
{
    uint32_t uStride = (uint32_t)wIndex << 4;                 /* SHL EAX,0x4   */
    intptr_t iTop    = (intptr_t)g_pSlotsTop;
    return (const GridSpanRecord *)(const void *)(iTop - (intptr_t)(int32_t)uStride); /* SUB */
}

/* r53 PARENT PATCH: the parameter ORDER here now matches the machine order, which this file's
 * own header already documented (ARG1 = Y at [ESP+0x8], ARG2 = X at [ESP+0x0C], ARG3 = the
 * index-source pointer at [ESP+0x10]). It previously declared the pointer FIRST, which
 * contradicted BOTH its own header comment and the extern that
 * src/grid/SelectGridSpanRecordByPointAndPublishSelectionState.c writes from the verified machine order.
 * Two prototypes of one function disagreeing is a trap for the next round that reads either file. */
int __stdcall TestPointInsideGridSpanRecord(int32_t nPointY, int32_t nPointX,
                                  const IndexSourceRec *pIndexRec)
{
    const GridSpanRecord   *pRec;
    const ExtentDescriptor *pExt;
    const uint16_t         *pwExt;
    uint16_t                wIndex;
    int32_t                 iMinX, iMinY;

    wIndex = pIndexRec->wSlotIndex;
    if (wIndex == 0xFFFF) {
        return 0;
    }

    pRec = SlotFromIndex(wIndex);                /* record = chain-top - index*0x10 */

    iMinX = (int32_t)pRec->iOffsetX;
    iMinY = (int32_t)pRec->iOffsetY;

    if (iMinX > nPointX) {
        return 0;
    }
    if (iMinY > nPointY) {
        return 0;
    }

    pExt  = (const ExtentDescriptor *)(uintptr_t)pRec->pExtentDesc;
    pwExt = (const uint16_t *)(uintptr_t)pExt->pWords; /* EDX[0], EDX[1] */

    if (iMinX + (int32_t)pwExt[0] < nPointX) {
        return 0;
    }
    return (iMinY + (int32_t)pwExt[1] >= nPointY) ? 1 : 0;
}
#endif /* NW_UNIT_TestPointInsideGridSpanRecord */
