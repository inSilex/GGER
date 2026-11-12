#include "nwarch.h"
/*
 * GridSpanStreams -- grid span command streams and arena setup.
 *
 * Module: grid
 * Interface: include/GridSpanStreams.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   DecodeGridSpanIndexStream                                  grid/DecodeGridSpanIndexStream.c
 *   GridSpanEntryActions                                       grid/GridSpanEntryActions.c
 *   GridSpanStreamWrappers                                     grid/GridSpanStreamWrappers.c
 *   InitializeAxisSpanFromSavedPoint                           grid/InitializeAxisSpanFromSavedPoint.c
 *   InitializeGridArenaFromMapHeader                           grid/InitializeGridArenaFromMapHeader.c
 *   InitializeGridSpanCommandStreamRun                         grid/InitializeGridSpanCommandStreamRun.c
 *   ProcessGridSpanCommandStream                               grid/ProcessGridSpanCommandStream.c
 *   ProcessPendingGridCellSpanEntry                            grid/ProcessPendingGridCellSpanEntry.c
 *   RebuildGridHeapAndCursorStateFromStream                    grid/RebuildGridHeapAndCursorStateFromStream.c
 *   ResetGridSpanGeometryIfModeFlagClear                       grid/ResetGridSpanGeometryIfModeFlagClear.c
 *   ResetTileRecordStateAndSeekStream                          grid/ResetTileRecordStateAndSeekStream.c
 */

/* ==========================================================================
 * Unit: DecodeGridSpanIndexStream
 * Source: grid/DecodeGridSpanIndexStream.c
 * ========================================================================== */
#ifdef NW_UNIT_DecodeGridSpanIndexStream
/*
 * Decode Grid Span Index Stream.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-decodegridspanindexstream-c
 */

#include <windows.h>
#include "nwglobals.h"
#include <stdint.h>
#include <string.h>

/* Size of the whole SUB ESP,0x104 frame. pbText is addressed from +0x10, and
   the terminator is written at pbText[dwLen] with dwLen taken unvalidated from
   the stream, so the buffer is modelled at the frame size rather than invented. */
#define GRIDSPAN_FRAME_SIZE 0x104

/* Sub-opcode field selectors — `(subOpcode & 0x70)`, the ONLY bits the body
   tests for form selection. */
#define SUBOP_FORM_NATIVE_CALL 0x20u   /* base native call + decimal text     */
#define SUBOP_BASE_VM_BASE_A   0x00u   
#define SUBOP_BASE_VM_BASE_B   0x10u   
#define SUBOP_FLAG_WIDE_OFFSET 0x80u   /* bit7: dword displacement, else byte  */

#define SUBOP_FMT_SIGNED_BYTE_ASCII  0u /* *(signed char*)puTarget -> decimal  */
#define SUBOP_FMT_CHARPTR_VIA_VALUE  1u /* char* read from *puTarget, padded   */
#define SUBOP_FMT_STRING_INPLACE     2u /* string at puTarget, padded           */
#define SUBOP_FMT_DWORD_DECIMAL      3u /* *(int32*)puTarget -> decimal         */
#define SUBOP_FMT_DEREF_DWORD_DECIMAL 4u /* **(int32**)puTarget -> decimal      */
#define SUBOP_FMT_SYNTHETIC_OPCODE   5u /* build opcode 0x35 and run the VM     */

#define VM_OPCODE_LOAD_DWORD_ADD_BASE 0x35u


typedef unsigned int (__cdecl *VmNativeCallFn)(void);
extern const VmNativeCallFn g_aVmNativeCallTargets[62];

extern unsigned char *__stdcall RestartCurrentGridSpanGroup(unsigned char *pInstrStream);
extern char *__cdecl ConvertIntegerToString(int iValue, char *pDest, unsigned uBase);
extern char *__cdecl CopyBoundedStringNulPadded(char *pDest, const char *pSrc, unsigned uCount);
extern void     __stdcall AllocGridCellSpanRecord(unsigned short wOffsetY,
                                                  int             *pnCellOffsetX,
                                                  int              nGridTabIndex,
                                                  int              nRefRecIndex);
extern unsigned int __stdcall DispatchScriptOpcode(unsigned char *pInstrStream);

static uint32_t ReadStreamU32(const unsigned char *p)
{
    uint32_t dwValue;
    memcpy(&dwValue, p, sizeof dwValue);
    return dwValue;
}

static int32_t ReadStreamI32(const unsigned char *p)
{
    int32_t iValue;
    memcpy(&iValue, p, sizeof iValue);
    return iValue;
}

unsigned int __stdcall DecodeGridSpanIndexStream(unsigned short wOffsetY,
                                                 int              nCellOffsetX,
                                                 int              nGridTabIndex,
                                                 unsigned char   *pInstrStream)
{

    unsigned char *pCursor = RestartCurrentGridSpanGroup(pInstrStream);

    unsigned char pbText[GRIDSPAN_FRAME_SIZE];
    unsigned int  uIndex;

    for (;;) {
        unsigned char bOp = *pCursor++;      

        if (bOp == 0) {                      
            break;
        }

        if (bOp != 0x0Fu) {

            AllocGridCellSpanRecord(wOffsetY, &nCellOffsetX, nGridTabIndex, (int)bOp);
            continue;
        }

        {
            unsigned char bSub  = *pCursor++;
            unsigned int  dwLen = (unsigned int)*pCursor++;   

            if ((bSub & 0x70u) == SUBOP_FORM_NATIVE_CALL) {

                unsigned int uiResult =
                    g_aVmNativeCallTargets[(unsigned int)*pCursor++]();
                ConvertIntegerToString((int)uiResult, (char *)pbText, 10u);
                continue;
            }

            if (((bSub & 0x70u) == SUBOP_BASE_VM_BASE_A) ||
                ((bSub & 0x70u) == SUBOP_BASE_VM_BASE_B)) {

                unsigned char *puBase = ((bSub & 0x70u) == SUBOP_BASE_VM_BASE_A)
                                            ? g_pVmBaseA : g_pVmBaseB;
                uint32_t dwDisp;

                if (bSub & SUBOP_FLAG_WIDE_OFFSET) {
                    dwDisp = ReadStreamU32(pCursor);
                    pCursor += 4;
                } else {
                    dwDisp = (uint32_t)*pCursor++;
                }

                {
                    unsigned char *puTarget =
                        (unsigned char *)((uintptr_t)puBase + (uintptr_t)dwDisp);

                    switch (bSub & 7u) {
                    case SUBOP_FMT_SIGNED_BYTE_ASCII:
                        ConvertIntegerToString((int)(signed char)puTarget[0],
                                               (char *)pbText, 10u);
                        break;

                    case SUBOP_FMT_CHARPTR_VIA_VALUE: {

                        /* The VM holds a 32-bit guest address here; widen it
                         * through the window mapping before use. */
                        const char *pszSrc = (const char *)(uintptr_t)
                            nw_guest_to_host(ReadStreamU32(puTarget));
                        CopyBoundedStringNulPadded((char *)pbText, pszSrc, dwLen);
                        break;
                    }

                    case SUBOP_FMT_STRING_INPLACE:
                        CopyBoundedStringNulPadded((char *)pbText,
                                                   (const char *)puTarget, dwLen);
                        break;

                    case SUBOP_FMT_DWORD_DECIMAL:
                        ConvertIntegerToString(ReadStreamI32(puTarget),
                                               (char *)pbText, 10u);
                        break;

                    case SUBOP_FMT_DEREF_DWORD_DECIMAL: {

                        const unsigned char *puIndirect =
                            (const unsigned char *)(uintptr_t)nw_guest_to_host(
                                ReadStreamU32(puTarget));
                        ConvertIntegerToString(ReadStreamI32(puIndirect),
                                               (char *)pbText, 10u);
                        break;
                    }

                    case SUBOP_FMT_SYNTHETIC_OPCODE: {

                        uint32_t dwRel = (uint32_t)((uintptr_t)puTarget -
                                                    (uintptr_t)g_pVmBaseA);
                        pbText[0] = (unsigned char)VM_OPCODE_LOAD_DWORD_ADD_BASE;
                        pbText[1] = (unsigned char)(dwRel & 0xffu);
                        pbText[2] = (unsigned char)((dwRel >> 8) & 0xffu);
                        pbText[3] = (unsigned char)((dwRel >> 16) & 0xffu);
                        pbText[4] = (unsigned char)((dwRel >> 24) & 0xffu);
                        pbText[5] = 0;
                        ConvertIntegerToString((int)DispatchScriptOpcode(pbText),
                                               (char *)pbText, 10u);
                        break;
                    }

                    default:

                        break;
                    }
                }

                pbText[dwLen] = 0;
                for (uIndex = 0; pbText[uIndex] != 0; uIndex++) {
                    AllocGridCellSpanRecord(wOffsetY, &nCellOffsetX, nGridTabIndex,
                                            (int)pbText[uIndex]);
                }
            }

        }
    }

    return 1u;                                
}
#endif /* NW_UNIT_DecodeGridSpanIndexStream */

/* ==========================================================================
 * Unit: GridSpanEntryActions
 * Source: grid/GridSpanEntryActions.c
 * ========================================================================== */
#ifdef NW_UNIT_GridSpanEntryActions
/*
 * Grid Span Entry Actions.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-gridspanentryactions-c
 */

#include <stdint.h>
#include "nwglobals.h"

#define NW_ENTRY_ACTION __attribute__((section(".nwdiag")))

extern void ProcessPendingGridCellSpanEntry(void);
extern void __stdcall AdvancePendingGridCellSpanEntryAndDispatch(
    unsigned char bRedrawFromPool, uint32_t dwStateIndex);
extern int __stdcall InstallActiveGridCellRef(uint8_t bRefreshExtents,
                                             int nRecordIndex);
extern uint32_t __stdcall ReleaseGridCellSpanRecord(void *pSpanRec);

NW_ENTRY_ACTION uint32_t NwProcessPendingSpanEntry(void)
{
    ProcessPendingGridCellSpanEntry();
    return 1u;
}

NW_ENTRY_ACTION uint32_t __stdcall NwAdvancePendingSpanEntryKeepIndex(
    uint32_t dwStateIndex)
{
    AdvancePendingGridCellSpanEntryAndDispatch(0u, dwStateIndex);
    return 1u;
}

NW_ENTRY_ACTION uint32_t __stdcall NwAdvancePendingSpanEntryRedrawIndex(
    uint32_t dwStateIndex)
{
    AdvancePendingGridCellSpanEntryAndDispatch(1u, dwStateIndex);
    return 1u;
}

NW_ENTRY_ACTION uint32_t NwReleaseContextSpanByYIndex(void)
{
    uint16_t wIndex = g_pCurGridCtx->wCurYIdx;

    if (wIndex != 0xffffu) {
        g_pCurGridCtx->wCurXIdx = 0xffffu;
        g_pCurGridCtx->wCurYIdx = 0xffffu;
        return ReleaseGridCellSpanRecord(
            (void *)(g_pGridSpanChainTop - ((uint32_t)wIndex << 4)));
    }

    return (uint32_t)(uintptr_t)g_pCurGridCtx;
}

NW_ENTRY_ACTION uint32_t __stdcall NwInstallActiveCellRefWithRefresh(
    uint32_t nRecordIndex)
{
    return (uint32_t)(int32_t)(int8_t)InstallActiveGridCellRef(
        1u, (int)nRecordIndex);
}

NW_ENTRY_ACTION uint32_t NwInstallActiveCellRefZero(void)
{
    return (uint32_t)(int32_t)(int8_t)InstallActiveGridCellRef(1u, 0);
}
#endif /* NW_UNIT_GridSpanEntryActions */

/* ==========================================================================
 * Unit: GridSpanStreamWrappers
 * Source: grid/GridSpanStreamWrappers.c
 * ========================================================================== */
#ifdef NW_UNIT_GridSpanStreamWrappers
/*
 * Grid Span Stream Wrappers.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-gridspanstreamwrappers-c
 */

#include <stdint.h>
#include "nwglobals.h"

#define NW_SPAN_WRAPPER __attribute__((section(".nwdiag")))

#define NW32(off) (*(uint32_t *)(intptr_t)(nw_data_window + (off)))
#define NWPP(off) NW_WINDOW_PTR_REF(uint8_t, (off))

extern uint32_t __stdcall AcquireGridCellSpanRecordAndRetirePrevious(
    uint8_t bRefRelative, uint16_t wCellOffsetY, uint16_t wCellOffsetX,
    uint32_t nRefTabIndex);
extern unsigned int __stdcall InitializeGridSpanCommandStreamRun(
    int nCellOffsetY, int nGridCellOffsetX, int nGridTabIndex,
    unsigned char *pInstrStream);
extern unsigned int __stdcall DecodeGridSpanIndexStream(
    unsigned short wOffsetY, int nCellOffsetX, int nGridTabIndex,
    unsigned char *pInstrStream);
extern uint32_t __stdcall ExecuteOpcodeStreamPreservingGlobals(
    const uint32_t *pOpcodeStream);

NW_SPAN_WRAPPER uint32_t __stdcall NwCellPackedBitForArg(uint32_t index)
{
    uint32_t pending = g_pCurGridCtx->bPendingIndex;
    uint8_t *pBase = NWPP(0x624c);
    uint8_t *pRecord = (uint8_t *)(uintptr_t)nw_guest_to_host(
        *(const uint32_t *)(const void *)(pBase + ((uint32_t)index * 16u)));

    return (1u << (pending & 7u)) &
           (uint32_t)pRecord[pending >> 3];
}

NW_SPAN_WRAPPER uint32_t __stdcall NwAcquireScaledSpanAndArm(uint32_t index)
{
    uint32_t x = (uint32_t)g_vmOperandB *
                 (uint32_t)g_dwCellScaleX;
    uint32_t y = (uint32_t)g_vmOperandA *
                 (uint32_t)g_dwCellScaleY;

    *(uint8_t *)(intptr_t)(nw_data_window + 0x67fc) = 1u;
    return AcquireGridCellSpanRecordAndRetirePrevious(
        0u, (uint16_t)y, (uint16_t)x, index);
}

NW_SPAN_WRAPPER uint32_t __stdcall NwAcquireSpansAndRunStream(uint32_t index)
{
    uint8_t *pEntry = NWPP(0x6324) + ((uint32_t)index * 8u);
    uint8_t *pRecord = (uint8_t *)(uintptr_t)nw_guest_to_host(
        *(const uint32_t *)(const void *)(pEntry + 4));

    if (pRecord != 0) {
        for (;;) {
            AcquireGridCellSpanRecordAndRetirePrevious(
                1u, *(uint16_t *)(pRecord + 2), *(uint16_t *)(pRecord + 0),
                *(uint16_t *)(pRecord + 4));
            if (pRecord[7] != 0u) {
                break;
            }
            pRecord += 8;
        }
    }

    return ExecuteOpcodeStreamPreservingGlobals((const uint32_t *)pEntry);
}

NW_SPAN_WRAPPER uint32_t __stdcall NwInitSpanRunScaled(const uint8_t *pInstrStream,
                                                        uint32_t nGridTabIndex)
{
    uint32_t x = (uint32_t)g_vmOperandB *
                 (uint32_t)g_dwCellScaleX;
    uint32_t y = (uint32_t)g_vmOperandA *
                 (uint32_t)g_dwCellScaleY;

    return InitializeGridSpanCommandStreamRun(
        (int)y, (int)x, (int)nGridTabIndex,
        (unsigned char *)(uintptr_t)nw_guest_to_host(
            (uint32_t)(uintptr_t)pInstrStream));
}

NW_SPAN_WRAPPER uint32_t __stdcall NwInitSpanRunFromTable(const uint8_t *pInstrStream,
                                                           uint32_t index)
{
    uint8_t *pEntry = NWPP(0x6324) + ((uint32_t)index * 8u);
    uint8_t *pRecord = (uint8_t *)(uintptr_t)nw_guest_to_host(
        *(const uint32_t *)(const void *)(pEntry + 4));

    return InitializeGridSpanCommandStreamRun(
        *(uint16_t *)(pRecord + 2), *(uint16_t *)(pRecord + 0),
        *(uint16_t *)(pRecord + 4),
        (unsigned char *)(uintptr_t)nw_guest_to_host(
            (uint32_t)(uintptr_t)pInstrStream));
}

NW_SPAN_WRAPPER uint32_t __stdcall NwDecodeSpanRunScaled(const uint8_t *pInstrStream,
                                                          uint32_t nGridTabIndex)
{
    uint32_t x = (uint32_t)g_vmOperandB *
                 (uint32_t)g_dwCellScaleX;
    uint32_t y = (uint32_t)g_vmOperandA *
                 (uint32_t)g_dwCellScaleY;

    return DecodeGridSpanIndexStream((unsigned short)y, (int)x,
                                     (int)nGridTabIndex,
                                     (unsigned char *)(uintptr_t)nw_guest_to_host(
                                         (uint32_t)(uintptr_t)pInstrStream));
}

NW_SPAN_WRAPPER uint32_t __stdcall NwDecodeSpanRunFromTable(const uint8_t *pInstrStream,
                                                             uint32_t index)
{
    uint8_t *pEntry = NWPP(0x6324) + ((uint32_t)index * 8u);
    uint8_t *pRecord = (uint8_t *)(uintptr_t)nw_guest_to_host(
        *(const uint32_t *)(const void *)(pEntry + 4));

    return DecodeGridSpanIndexStream(
        *(uint16_t *)(pRecord + 2), *(uint16_t *)(pRecord + 0),
        *(uint16_t *)(pRecord + 4),
        (unsigned char *)(uintptr_t)nw_guest_to_host(
            (uint32_t)(uintptr_t)pInstrStream));
}
#endif /* NW_UNIT_GridSpanStreamWrappers */

/* ==========================================================================
 * Unit: InitializeAxisSpanFromSavedPoint
 * Source: grid/InitializeAxisSpanFromSavedPoint.c
 * ========================================================================== */
#ifdef NW_UNIT_InitializeAxisSpanFromSavedPoint
/*
 * Initialize Axis Span From Saved Point.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-initializeaxisspanfromsavedpoint-c
 */

#include <stdint.h>
#include "nwglobals.h"




extern unsigned int g_dwCoordHStart;
extern unsigned int g_dwCoordVStart;

extern unsigned int g_dwCoordHEnd;
extern unsigned int g_dwCoordVEnd;

extern unsigned int g_dwOrientCodeA;
extern unsigned int g_dwOrientCodeB;

extern unsigned char g_bOrientChanged;

void __stdcall InitializeAxisSpanFromSavedPoint(char cSelectCode)
{
    unsigned char ucSelect;   /* zero-extended form of the argument, as compared in asm */

    ucSelect = (unsigned char)cSelectCode;

    if ((ucSelect == 0x82u) || (ucSelect == 0x83u)) {    
        g_dwCoordHEnd = g_dwSelPairAH;                    
        g_dwCoordVEnd = g_dwSelPairAV;                    
    } else {                                              
        g_dwCoordHEnd = g_dwSelPairBH;                    
        g_dwCoordVEnd = g_dwSelPairBV;                    
    }

    g_dwCoordHStart = (unsigned int)g_pCurGridCtx->bCellKeyLow;  /* +0x06 */       /* byte -> dword, zero-extended */
    g_dwCoordVStart = (unsigned int)g_pCurGridCtx->bCellKeyHigh; /* +0x07 */       /* byte -> dword, zero-extended */

    g_dwOrientCodeB = 0xFFFFFFFFu;                        
    g_dwOrientCodeA = 0xFFFFFFFFu;                        
    g_bOrientChanged = 0u;                                
}
#endif /* NW_UNIT_InitializeAxisSpanFromSavedPoint */

/* ==========================================================================
 * Unit: InitializeGridArenaFromMapHeader
 * Source: grid/InitializeGridArenaFromMapHeader.c
 * ========================================================================== */
#ifdef NW_UNIT_InitializeGridArenaFromMapHeader
/*
 * Initialize Grid Arena From Map Header.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-initializegridarenafrommapheader-c
 */

#include <windows.h>
#include "nwglobals.h"
#include <stdint.h>
extern unsigned char nw_data_window[];

extern DWORD __stdcall ReadLzssOrRawTaggedRecordBlock(BYTE *pbDestBuf);

extern uint32_t __stdcall AllocateHeapCursorBlock(uint32_t dwSizeBytes) __asm__("_AllocateHeapCursorBlock");

extern unsigned char *__stdcall FillMemoryWithZeroBytes(uint32_t dwCount,
                                                        unsigned char *pDst);

/* ---------------------------------------------------------------------------
 * THE MAP HEADER BLOCK @(nw_data_window + 0x61e0) -- filled by the tagged-record read above
 * ------------------------------------------------------------------------ */

#define MAP_HDR_CELL_A         (*(uint32_t *)(intptr_t)(nw_data_window + 0x61e0)) /* x MAP_HDR_CELL_B = cell count */
#define MAP_HDR_CELL_B         (*(uint32_t *)(intptr_t)(nw_data_window + 0x61e4))
#define MAP_HDR_ROW_STRIDE     (*(uint32_t *)(intptr_t)(nw_data_window + 0x61e8)) /* bytes per row      */
#define MAP_HDR_ROW_COUNT      (*(uint32_t *)(intptr_t)(nw_data_window + 0x61ec))
#define MAP_HDR_CELL_ENTRY_CNT (*(uint32_t *)(intptr_t)(nw_data_window + 0x61f8)) /* bytes for the 2-byte-stride cell table */
#define MAP_HDR_POOL_ENTRY_CNT (*(uint32_t *)(intptr_t)(nw_data_window + 0x61fc)) /* x16                 */
#define MAP_HDR_REF_ENTRY_CNT  (*(uint32_t *)(intptr_t)(nw_data_window + 0x6200)) /* x16                 */
#define MAP_HDR_AUX8_ENTRY_CNT (*(uint32_t *)(intptr_t)(nw_data_window + 0x6204)) /* x8                  */
#define MAP_HDR_SLOTARG_CNT    (*(uint32_t *)(intptr_t)(nw_data_window + 0x6208)) /* x8                  */
#define MAP_HDR_AUX4_ENTRY_CNT (*(uint32_t *)(intptr_t)(nw_data_window + 0x620c)) /* x4                  */
#define MAP_HDR_TAIL_ENTRY_CNT (*(uint32_t *)(intptr_t)(nw_data_window + 0x6210)) /* x8                  */

/* ---------------------------------------------------------------------------
 * THE CELL-BUCKET SCALE TABLE @(nw_data_window + 0x234) -- 8-byte stride, two dwords per entry
 * ------------------------------------------------------------------------ */

#pragma pack(push, 1)
typedef struct {
    uint32_t dwLimit;  /* smallest row stride this bucket serves */
    uint32_t dwScale;  /* log2 of that width: the axis shift count */
} CellBucketEntry;
#pragma pack(pop)

#define GRID_BUCKET_TABLE ((CellBucketEntry *)(intptr_t)(nw_data_window + 0x234))

/* ---------------------------------------------------------------------------
 * ENGINE GLOBALS PUBLISHED BY THIS FUNCTION
 * ------------------------------------------------------------------------ */

/* @(nw_data_window + 0x234)+4 is the table's scale column, indexed as GRID_BUCKET_TABLE[n].dwScale. */

/* @(nw_data_window + 0x6250) -- base of the 2-BYTE-stride grid-cell table. Named g_pCellEntryBase by
 * src/grid/ReleaseGridCellSpanRecordsAndDispatchEntry.c and src/grid/AllocateGridCellSpanRecordAndDispatch.c,
 * which both address it as `base + ((major << shift) + minor) * 2`. Here it is the FIRST
 * allocation (MAP_HDR_CELL_ENTRY_CNT bytes) and is the one block the trailing zero-fill
 * does NOT cover, because the fill starts at the SECOND block's base. */
#define PUB_CELL_ENTRY_BASE  NW_WINDOW_PTR_VALUE(0x6250)

#define PUB_AXIS_SHIFT        (*(uint32_t *)(intptr_t)(nw_data_window + 0x6254))

/* @(nw_data_window + 0x6304) -- MAP_HDR_ROW_STRIDE * MAP_HDR_ROW_COUNT, the total size in bytes of the
 * grid buffer; also the allocation size of the g_pStateTable block below. */
#define PUB_GRID_BUFFER_BYTES (*(uint32_t *)(intptr_t)(nw_data_window + 0x6304))

#define PUB_CELL_COUNT        (*(uint32_t *)(intptr_t)(nw_data_window + 0x6308))

/* @(nw_data_window + 0x630c) -- base of the bucket-scaled cell buffer, sized
 * MAP_HDR_ROW_COUNT << (scale + 1): every cell occupies a whole power-of-two bucket.
 * This is the block the trailing zero-fill STARTS at. Not named by any sibling yet. */
#define PUB_SCALED_CELL_BUF   NW_WINDOW_PTR_VALUE(0x630c)

#define PUB_ARENA_CURSOR_PRE  NW_WINDOW_PTR_VALUE(0x6310)

/* @(nw_data_window + 0x6248) -- g_pStateTable: 0x10-stride state entries, indexed by a pending-index
 * byte (src/grid/ReleaseGridCellSpanRecordsAndDispatchEntry.c, src/grid/ProcessPendingGridCellSpanEntry.c). */
#define PUB_STATE_TABLE       NW_WINDOW_PTR_VALUE(0x6248)

/* @(nw_data_window + 0x624c) -- g_pPoolTable: a SECOND 0x10-stride table, indexed by an opcode's first
 * byte on the pool path (src/grid/AllocateGridCellSpanRecordAndDispatch.c). */
#define PUB_POOL_TABLE        NW_WINDOW_PTR_VALUE(0x624c)

/* @(nw_data_window + 0x6300) -- g_pGridRefTab: the reference-table descriptor array. src/grid/AllocGridCellSpanRecord.c
 * indexes it with stride 8 off this same base; it is allocated here at 16 bytes per entry,
 * so the reservation is twice the stride its consumers use. Reported, not adjudicated. */
#define PUB_GRID_REF_TAB      NW_WINDOW_PTR_VALUE(0x6300)

/* @(nw_data_window + 0x62fc) -- base of an 8-byte-stride table sized by MAP_HDR_AUX8_ENTRY_CNT << 3.
 * No sibling names this global yet; named here for the shape its allocation gives it. */
#define PUB_AUX8_TABLE        NW_WINDOW_PTR_VALUE(0x62fc)

#define PUB_SLOT_ARG_TABLE    NW_WINDOW_PTR_VALUE(0x6328)

/* @(nw_data_window + 0x6324) -- base of a 4-byte-stride table sized by MAP_HDR_AUX4_ENTRY_CNT << 2.
 * No sibling names this global yet. */
#define PUB_AUX4_TABLE        NW_WINDOW_PTR_VALUE(0x6324)

/* @(nw_data_window + 0x63d4) -- gpGridBase: the grid cell buffer, MAP_HDR_CELL_COUNT * 16 bytes
 * (src/grid/RouteGridPathToTargetCell.c calls it "grid buffer base"). */
#define PUB_GRID_CELL_BUF     NW_WINDOW_PTR_VALUE(0x63d4)

#define PUB_GRID_LAST_BYTE    (*(int32_t *)(intptr_t)(nw_data_window + 0x63d8))
#define PUB_GRID_ROW_ABOVE    (*(int32_t *)(intptr_t)(nw_data_window + 0x63dc))
#define PUB_GRID_LAST_ROW     (*(int32_t *)(intptr_t)(nw_data_window + 0x63e0))

#define PUB_BASE_PTR_A        NW_WINDOW_PTR_VALUE(0x63e4)

/* ---------------------------------------------------------------------------
 * THE SHARED ARENA CURSOR
 * ------------------------------------------------------------------------ */

#define G_ARENA_CURSOR        NW_WINDOW_PTR_VALUE(0x16d4)
#define G_ARENA_REWIND        NW_WINDOW_PTR_VALUE(0x16d8)

/* ---------------------------------------------------------------------------
 * ALLOCATION LEDGER -- every one of the ten AllocateHeapCursorBlock sites
 * ------------------------------------------------------------------------ */

void __cdecl InitializeGridArenaFromMapHeader(void)
{
    uint32_t      dwCellCount;         /* MAP_HDR_CELL_A * MAP_HDR_CELL_B               */
    uint32_t      dwGridBufferBytes;   /* MAP_HDR_ROW_STRIDE * MAP_HDR_ROW_COUNT       */
    uint32_t      dwAxisScale;         /* the shift count chosen from GRID_BUCKET_TABLE  */
    uint32_t      dwBucketIndex;       /* index of the first entry with limit >= stride */
    unsigned char *pbScaledCellBuf;    
    unsigned char *pbArenaTop;         /* cursor after allocation #10                   */

    {
        /* Optional trace: the header block read decides PUB_CELL_COUNT (CELL_A*CELL_B) and
         * hence the record-table walk bound. Log the callee's result and the decoded
         * fields so we can see whether the read failed or decoded different bytes. */
        
        DWORD nw_hdr_rc = ReadLzssOrRawTaggedRecordBlock((BYTE *)(intptr_t)(nw_data_window + 0x61e0));
    }

    dwCellCount       = MAP_HDR_CELL_A * MAP_HDR_CELL_B;
    PUB_CELL_COUNT       = dwCellCount;
    dwGridBufferBytes = MAP_HDR_ROW_STRIDE * MAP_HDR_ROW_COUNT;
    PUB_GRID_BUFFER_BYTES = dwGridBufferBytes;

    PUB_ARENA_CURSOR_PRE = G_ARENA_CURSOR;

    /* Every grid-arena base is consumed as a native pointer by the rest of the
     * engine, so the side-table slots hold full host pointers.  The allocator
     * returns a 32-bit guest address; widen it once here.  The one exception
     * is PUB_SCALED_CELL_BUF (+0x630c), which save-file code treats as a
     * guest base, so that slot keeps the guest address. */
    G_ARENA_CURSOR = (uintptr_t)nw_guest_to_host(
        AllocateHeapCursorBlock(MAP_HDR_CELL_ENTRY_CNT));
    PUB_CELL_ENTRY_BASE = G_ARENA_CURSOR;

    dwBucketIndex = 0;
    while (GRID_BUCKET_TABLE[dwBucketIndex].dwLimit < MAP_HDR_ROW_STRIDE) {
        dwBucketIndex++;
    }
    dwAxisScale   = GRID_BUCKET_TABLE[dwBucketIndex].dwScale;
    PUB_AXIS_SHIFT = dwAxisScale;

    pbScaledCellBuf = (unsigned char *)(uintptr_t)nw_guest_to_host(
        AllocateHeapCursorBlock(
            MAP_HDR_ROW_COUNT << ((dwAxisScale + 1u) & 0x1fu)));
    G_ARENA_CURSOR  = (uintptr_t)pbScaledCellBuf;
    PUB_SCALED_CELL_BUF = (uintptr_t)nw_host_to_guest(pbScaledCellBuf);

    G_ARENA_CURSOR    = (uintptr_t)nw_guest_to_host(
        AllocateHeapCursorBlock(dwCellCount << 4));
    PUB_GRID_CELL_BUF = G_ARENA_CURSOR;

    PUB_GRID_LAST_BYTE = (int32_t)(MAP_HDR_ROW_STRIDE - 1u);  /* last byte in a row  */
    PUB_GRID_ROW_ABOVE = -(int32_t)MAP_HDR_ROW_STRIDE;        /* the row above      */
    PUB_GRID_LAST_ROW  = (int32_t)(MAP_HDR_ROW_COUNT - 1u);   /* the last row       */

    G_ARENA_CURSOR   = (uintptr_t)nw_guest_to_host(
        AllocateHeapCursorBlock(dwGridBufferBytes));
    PUB_STATE_TABLE  = G_ARENA_CURSOR;


    G_ARENA_CURSOR = (uintptr_t)nw_guest_to_host(
        AllocateHeapCursorBlock(MAP_HDR_POOL_ENTRY_CNT << 4));
    PUB_POOL_TABLE = G_ARENA_CURSOR;

    G_ARENA_CURSOR  = (uintptr_t)nw_guest_to_host(
        AllocateHeapCursorBlock(MAP_HDR_REF_ENTRY_CNT << 4));
    PUB_GRID_REF_TAB = G_ARENA_CURSOR;

    G_ARENA_CURSOR = (uintptr_t)nw_guest_to_host(
        AllocateHeapCursorBlock(MAP_HDR_AUX8_ENTRY_CNT << 3));
    PUB_AUX8_TABLE = G_ARENA_CURSOR;

    G_ARENA_CURSOR     = (uintptr_t)nw_guest_to_host(
        AllocateHeapCursorBlock(MAP_HDR_SLOTARG_CNT << 3));
    PUB_SLOT_ARG_TABLE = G_ARENA_CURSOR;

    G_ARENA_CURSOR = (uintptr_t)nw_guest_to_host(
        AllocateHeapCursorBlock(MAP_HDR_AUX4_ENTRY_CNT << 2));
    PUB_AUX4_TABLE = G_ARENA_CURSOR;

    G_ARENA_CURSOR   = (uintptr_t)nw_guest_to_host(
        AllocateHeapCursorBlock(MAP_HDR_TAIL_ENTRY_CNT << 3));
    pbArenaTop       = (unsigned char *)G_ARENA_CURSOR;

    (void)FillMemoryWithZeroBytes((uint32_t)((uintptr_t)pbArenaTop -
                                             (uintptr_t)pbScaledCellBuf),
                                  pbScaledCellBuf);

    G_ARENA_REWIND = G_ARENA_CURSOR;
    PUB_BASE_PTR_A = G_ARENA_CURSOR;

}
#endif /* NW_UNIT_InitializeGridArenaFromMapHeader */

/* ==========================================================================
 * Unit: InitializeGridSpanCommandStreamRun
 * Source: grid/InitializeGridSpanCommandStreamRun.c
 * ========================================================================== */
#ifdef NW_UNIT_InitializeGridSpanCommandStreamRun
/*
 * Begins a grid-span command-stream run: it is a re-entrancy guard on the run-active flag.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-initializegridspancommandstreamrun-c
 */

#include <windows.h>
#include "nwglobals.h"
#include <stdint.h>

extern unsigned char * __stdcall RestartCurrentGridSpanGroup(unsigned char *pInstrStream);

extern unsigned int __stdcall ProcessGridSpanCommandStream(unsigned char bFirstCommand);

/* Run-in-progress flag: non-zero => a run is already live (re-entrancy guard). */
/* Interpreter cursor (g_pStreamCursor), seeded from arg4 via the restart callee. */
/* Grid-context parameters cached from this function's arguments. */
/* 8-byte-stride descriptor entry: base + arg3*8 (read by the interpreter). */

extern unsigned char nw_data_window[] __asm__("nw_data_window");

#define g_pGridRefTabEntry NW_WINDOW_PTR_REF(unsigned char, 0x1770)

/* (definition removed -- the macro above owns this name) */

/* SAVED copies of the live grid-context triple (the interpreter restores them). */
/* The LIVE grid-context triple (the save source). */

extern unsigned char  *g_pGridRefTab;          

unsigned int __stdcall InitializeGridSpanCommandStreamRun(int            nCellOffsetY,
                                                     int            nGridCellOffsetX,
                                                     int            nGridTabIndex,
                                                     unsigned char *pInstrStream)
{

    if (g_bGridSpanRunActive != 0u) {
        return 0u;                                   
    }

    g_pStreamCursor = RestartCurrentGridSpanGroup(pInstrStream);   

    g_nGridCellOffsetX = nGridCellOffsetX;                       
    g_nCellOffsetY     = nCellOffsetY;                           
    g_nGridTabIndex    = nGridTabIndex;                          

    g_pGridRefTabEntry = g_pGridRefTab +
                         (uintptr_t)((uint32_t)nGridTabIndex * 8u);   

    g_dwSavedCurGridCtx   = g_pCurGridCtx;        
    g_dwSavedGridUserData = g_dwGridUserData;     
    g_dwSavedGridCtxAux   = g_dwGridCtxAux;       

    return (unsigned int)(int32_t)(int8_t)(uint8_t)ProcessGridSpanCommandStream(0u);
}
#endif /* NW_UNIT_InitializeGridSpanCommandStreamRun */

/* ==========================================================================
 * Unit: ProcessGridSpanCommandStream
 * Source: grid/ProcessGridSpanCommandStream.c
 * ========================================================================== */
#ifdef NW_UNIT_ProcessGridSpanCommandStream
/*
 * Process Grid Span Command Stream.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-processgridspancommandstream-c
 */

#include <windows.h>
#include <stdint.h>
#include <string.h>

extern void ReleaseTopGridCellSpans(int nSpanCount);                       
extern unsigned int ReleaseCurrentContextSpanGroup(void);                   
extern char *__cdecl CopyNulTerminatedString(char *pszDst, const char *pszSrc); 
extern int   __cdecl ConvertAsciiCharToLowerCase(int iCharCode);           
extern int   __cdecl ConvertAsciiCharToUpperCase(int iCharCode);           
extern int   __cdecl ParseSignedDecimalIntegerFromString(byte *pbStr);     

/* Opaque 16-byte grid-cell span record (layout in src/grid/AllocGridCellSpanRecord.c). */
typedef struct GridCellSpanRec GridCellSpanRec;

#include "nwglobals.h"

/* __stdcall, four dwords, returns the populated record (src/grid/AllocGridCellSpanRecord.c). */
extern GridCellSpanRec *__stdcall AllocGridCellSpanRecord(unsigned short wOffsetY,
                                                          int           *pnCellOffsetX,
                                                          int            nGridTabIndex,
                                                          int            nRefRecIndex); 

/* Live grid-context triple (restored at entry). */

/* Interpreter state. */
/* VM base pointers and the CRT ctype table. */
/* Run-in-progress flag. */

static unsigned char ReadStreamByte(void)
{
    return *g_pStreamCursor++;
}

static uint32_t PeekStreamDword(void)
{
    uint32_t dwValue;
    memcpy(&dwValue, g_pStreamCursor, sizeof dwValue);
    return dwValue;
}

unsigned int __stdcall ProcessGridSpanCommandStream(unsigned char bFirstCommand)
{
    unsigned char bOp = bFirstCommand;   

    g_pCurGridCtx      = (uint8_t *)(uintptr_t)nw_guest_to_host(g_dwSavedCurGridCtx);
    g_dwGridUserData   = g_dwSavedGridUserData;
    g_dwGridCtxAux     = g_dwSavedGridCtxAux;

    for (;;) {                            
        switch (bOp) {
        case 0x08:                        
            if (g_bFieldMode == 0u) goto default_arm;
            if (g_nGridSpanStackDepth != 0) {
                ReleaseTopGridCellSpans(1);
            }
            return 1u;

        case 0x1b:                        
            if (g_bFieldMode == 0u) goto default_arm;
            if (g_nGridSpanStackDepth == 0) goto case_00;
            ReleaseTopGridCellSpans(g_nGridSpanStackDepth);
            return 1u;

        case 0x00:                        
        case_00:
            if (g_bGridSpanRunActive != 0u) {
                g_bGridSpanRunActive = 0u;
                return ReleaseCurrentContextSpanGroup();   /* returns 1 */
            }
            g_bGridSpanRunActive = 1u;
            g_bFieldMode = 0u;
            bOp = ReadStreamByte();
            continue;

        case 0x0d:                        
            if (g_bFieldMode == 0u) goto default_arm;
            g_abFieldBuf[g_nGridSpanStackDepth] = 0u;   
            switch (g_bConvType & 0x07u) {              
            case 0:                                     
                *g_pConvDest = (uint8_t)(int)ParseSignedDecimalIntegerFromString(g_abFieldBuf);
                break;
            case 1:                                     
                CopyNulTerminatedString((char *)(uintptr_t)nw_guest_to_host(*(uint32_t *)(uintptr_t)g_pConvDest),
                                        (const char *)g_abFieldBuf);
                break;
            case 2:                                     
                CopyNulTerminatedString((char *)(uintptr_t)g_pConvDest,
                                        (const char *)g_abFieldBuf);
                break;
            case 3:                                     
                *(int32_t *)(uintptr_t)g_pConvDest =
                    (int32_t)ParseSignedDecimalIntegerFromString(g_abFieldBuf);
                break;
            case 4:                                     
                *(int32_t *)(uintptr_t)nw_guest_to_host(*(uint32_t *)(uintptr_t)g_pConvDest) =
                    (int32_t)ParseSignedDecimalIntegerFromString(g_abFieldBuf);
                break;
            default:
                break;
            }
            g_bFieldMode = 0u;                          
            bOp = ReadStreamByte();
            continue;

        case 0x0f: {                      
            unsigned char bType = ReadStreamByte();     
            g_bConvType = bType;
            g_dwFieldWidth = (int32_t)ReadStreamByte(); 

            switch (bType & 0x70u) {                    
            case 0x00u:
                g_pConvDest = (uint8_t *)(uintptr_t)g_pVmBaseA;   
                break;
            case 0x10u:
                g_pConvDest = (uint8_t *)(uintptr_t)g_pVmBaseB;   
                break;
            default:
                goto default_arm;                       
            }

            if ((bType & 0x80u) != 0u) {                
                g_pConvDest = (uint8_t *)((uintptr_t)g_pConvDest +
                                          (uintptr_t)PeekStreamDword());   
                g_pStreamCursor += 4;                   
            } else {
                g_pConvDest = (uint8_t *)((uintptr_t)g_pConvDest +
                                          (uintptr_t)ReadStreamByte());    
            }

            switch (bType & 0x07u) {                    
            case 0: case 3: case 4:
                g_bDigitsOnly = 1u;                     
                break;
            case 1: case 2:
                g_bDigitsOnly = 0u;                     
                break;
            default:
                break;
            }

            g_nGridSpanStackDepth = 0;                  
            g_bFieldMode = 1u;                          
            return 1u;
        }

        default:
        default_arm:                      
            if (g_bFieldMode == 0u) {                   
                AllocGridCellSpanRecord((unsigned short)g_nCellOffsetY,
                                        &g_nGridCellOffsetX,
                                        g_nGridTabIndex,
                                        (int)bOp);
                bOp = ReadStreamByte();
                continue;
            }

            if ((uint32_t)g_nGridSpanStackDepth >= (uint32_t)g_dwFieldWidth) {
                return 1u;                              
            }

            if (g_pGridRefTabEntry[1] > bOp) {
                bOp = (unsigned char)ConvertAsciiCharToLowerCase((int)bOp);
            }
            if (g_pGridRefTabEntry[2] < bOp) {
                bOp = (unsigned char)ConvertAsciiCharToUpperCase((int)bOp);
            }

            if (g_pGridRefTabEntry[1] > bOp) return 1u;   /* lo > bOp  (JA) */
            if (g_pGridRefTabEntry[2] < bOp) return 1u;   /* hi < bOp  (JC) */

            if (g_bDigitsOnly != 0u) {                    
                if ((g_abCtypeTable[bOp] & 0x04u) == 0u) {
                    return 1u;                            
                }
            }

            g_abFieldBuf[g_nGridSpanStackDepth] = bOp;    
            {
                GridCellSpanRec *pRec =
                    AllocGridCellSpanRecord((unsigned short)g_nCellOffsetY,
                                            &g_nGridCellOffsetX,
                                            g_nGridTabIndex,
                                            (int)bOp);            
                g_apGridSpanStack[g_nGridSpanStackDepth] = pRec;   
                g_nGridSpanStackDepth += 1;                        
            }
            return 1u;                                    
        }
    }
}
#endif /* NW_UNIT_ProcessGridSpanCommandStream */

/* ==========================================================================
 * Unit: ProcessPendingGridCellSpanEntry
 * Source: grid/ProcessPendingGridCellSpanEntry.c
 * ========================================================================== */
#ifdef NW_UNIT_ProcessPendingGridCellSpanEntry
/*
 * Process Pending Grid Cell Span Entry.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-processpendinggridcellspanentry-c
 */

#include <stdint.h>
#include "nwglobals.h"

/* Raw 16-byte arena slot. Only the fields this function touches are modelled. */
#pragma pack(push, 1)
typedef struct {
    uint8_t  bStatusFlags;   /* +0x00 tested as (x & 0xC1) == 0x81 */
    uint8_t  _pad01[3];      /* +0x01 .. +0x03 */
    uint16_t wCellCoord;     /* +0x04 matched against the context's cell stamp */
    uint16_t wDepthIndex;      
    uint8_t  _pad08[8];      /* +0x08 .. +0x0F span payload / reference record */
} GridSpanSlot;
#pragma pack(pop)

#pragma pack(push, 1)
/* GridCtxRec -> src/include/nwgridctxrec.h */
#pragma pack(pop)

/* One 0x10-stride entry of g_pStateTable. */
#pragma pack(push, 1)
typedef struct {
    uint8_t  _pad00[4];      /* +0x00 byte must equal 3 per src/runtime/SelectNextActiveSlot.c */
    uint32_t pCursorA;       /* +0x04 guest script cursor (4 bytes in the image) */
    uint8_t  _pad08[4];      /* +0x08 .. +0x0B */
    uint32_t pCursorB;       /* +0x0C guest script cursor */
} GridEntryRec;
#pragma pack(pop)


extern GridSpanSlot *g_pSlotsTop;

extern GridSpanSlot *g_pSlotsBottom;

extern GridEntryRec *g_pStateTable;

extern void __stdcall ReleaseGridCellSpanRecord(GridSpanSlot *pSpanRec);           
extern void ExecuteScriptInstruction(unsigned char bFlag, unsigned char bOperandA,
                                    unsigned char bOperandB, uint32_t dwCtxA,
                                    uint32_t dwCtxB, uint32_t dwCtxC,
                                    uint32_t dwCtxD, unsigned char *pCursor); 

void ProcessPendingGridCellSpanEntry(void)
{
    GridCtxRec *pCtx = g_pCurGridCtx;

    /* Sentinel 0xFE means no entry is pending: no-op, and it is exactly the state
       the tail of this function re-arms. */
    if (pCtx->bPendingIndex == 0xFEu) {
        return;
    }

    GridEntryRec *pEntry   = &g_pStateTable[pCtx->bPendingIndex];
    const uint16_t uCellStamp = pCtx->wCellStamp;   /* MOVZX EDX, word [EAX+6] */

    GridSpanSlot *pSlot = g_pSlotsTop;
    do {
        /* 0xC1 == 0x80 | 0x40 | 0x01: the match 0x81 means publish flag (bit 7)
           SET, in-use bit (bit 0) set, and bit 6 clear. ReleaseGridCellSpanRecord
           keys its publish on bit 7, which is set here, so a matched slot is
           published to the extent list and then freed. */
        if ((pSlot->bStatusFlags & 0xC1u) == 0x81u &&
            pSlot->wDepthIndex    == 0xFFFFu &&
            pSlot->wCellCoord   == uCellStamp) {

            /* Publish then free, BEFORE any dispatch of this context's script. */
            ReleaseGridCellSpanRecord(pSlot);
            break;
        }
        pSlot = (GridSpanSlot *)(void *)((uint8_t *)(void *)pSlot - 0x10);

    } while ((uintptr_t)(void *)pSlot > (uintptr_t)(void *)g_pSlotsBottom);

    ExecuteScriptInstruction(pCtx->bDispatchFlag,
                             (unsigned char)((pCtx->wCellStamp >> 8) & 0xFFu),
                             (unsigned char)(pCtx->wCellStamp & 0xFFu),
                             0u, 0xFFFFu, 0u,
                             (uint32_t)(uintptr_t)pEntry,
                             (uint8_t *)(uintptr_t)nw_guest_to_host(pEntry->pCursorB));

    /* One 16-bit store 0xF0FE: 0xFE into +2 re-arms the guard, 0xF0 into +3. */
    pCtx->bPendingIndex = 0xFEu;
    pCtx->bDispatchFlag = 0xF0u;
}
#endif /* NW_UNIT_ProcessPendingGridCellSpanEntry */

/* ==========================================================================
 * Unit: RebuildGridHeapAndCursorStateFromStream
 * Source: grid/RebuildGridHeapAndCursorStateFromStream.c
 * ========================================================================== */
#ifdef NW_UNIT_RebuildGridHeapAndCursorStateFromStream
/*
 * Rebuild Grid Heap And Cursor State From Stream.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-rebuildgridheapandcursorstatefromstream-c
 */

#include <stdint.h>
extern unsigned char nw_data_window[];

#if defined(NW_PORTABLE_PS5) || defined(NW_PS5_NATIVE_TITLE)
extern void platform_boot_mark(const char *stage);
#define NW_NATIVE_BOOT(stage) platform_boot_mark(stage)
#else
#define NW_NATIVE_BOOT(stage) ((void)0)
#endif

typedef uint8_t  byte;
typedef uint32_t dword;

/* Flat 32-bit address space: every global is reached by absolute address. */
#define MEM(a) (*(dword *)(uintptr_t)(a))

#define SCREEN_CLAIM_FAILED 0x80000018u

extern dword __stdcall ReadLzssOrRawTaggedRecordBlock(byte *pbDestBuf);             
extern uintptr_t __stdcall CreatePlayerDIBBackBuffer(uintptr_t a1, uintptr_t a2,
                                                      uintptr_t a3);
extern dword __stdcall PublishScreenGeometryAndClaimFrameBuffer(dword *descriptor, dword *arena);
extern void __stdcall ResetStreamCursorState(byte *pStreamSource);       
extern void  ClearGridSpanChainBoundsAndHeapState(void);
extern dword PublishHeapStateTripleFromTwoSources(void);

extern dword __stdcall AllocateHeapCursorBlock(dword byte_count) __asm__("_AllocateHeapCursorBlock");

extern dword CloseSaveSlotAndLatchInvalidDataError(dword unused1, dword unused2);

dword RebuildGridHeapAndCursorStateFromStream(void)
{
    byte *base;
    dword cursor;
    dword slot_cursor;

    NW_NATIVE_BOOT("rb_begin");
    (void)ReadLzssOrRawTaggedRecordBlock((byte *)&MEM((nw_data_window + 0x6258)));

    if (MEM((nw_data_window + 0x6258)) != SCREEN_CLAIM_FAILED) {
        NW_NATIVE_BOOT("rb_magic_fail");
        return CloseSaveSlotAndLatchInvalidDataError(0, 0);
    }
    NW_NATIVE_BOOT("rb_magic_ok");

    
    base = (byte *)(uintptr_t)(nw_data_window + 0x6257);
    base[MEM((nw_data_window + 0x16c8))] = 0;

    MEM((nw_data_window + 0x6314)) = MEM((nw_data_window + 0x625c)) << 10;
    MEM((nw_data_window + 0x631c)) = MEM((nw_data_window + 0x6264)) * MEM((nw_data_window + 0x6268));

    if (!CreatePlayerDIBBackBuffer(
            (uintptr_t)&MEM((nw_data_window + 0x632c)),
            (uintptr_t)&MEM((nw_data_window + 0x6258)),
            (uintptr_t)&MEM((nw_data_window + 0x6314)))) {
        NW_NATIVE_BOOT("rb_dib_fail");
        return 0;
    }
    NW_NATIVE_BOOT("rb_dib_ok");

    if (!PublishScreenGeometryAndClaimFrameBuffer(
            (dword *)&MEM((nw_data_window + 0x6258)),
            (dword *)&MEM((nw_data_window + 0x6314)))) {
        NW_NATIVE_BOOT("rb_publish_fail");
        return 0;
    }
    NW_NATIVE_BOOT("rb_publish_ok");

#ifdef NWDEBUG_ENABLED
    extern void nwtrace_hex(const char *tag, unsigned long v);
    volatile dword _espmark; (void)_espmark;
#endif

#ifdef NWDEBUG_ENABLED
    nwtrace_hex("rb_h", (unsigned long)&_espmark);
#endif
    ClearGridSpanChainBoundsAndHeapState();
    MEM((nw_data_window + 0x63d0)) = PublishHeapStateTripleFromTwoSources();
    NW_NATIVE_BOOT("rb_heap_ok");

#ifdef NWDEBUG_ENABLED
    nwtrace_hex("rb_i", (unsigned long)&_espmark);
    nwtrace_hex("rb_i_sz", MEM((nw_data_window + 0x6274)));
#endif
    cursor = AllocateHeapCursorBlock(MEM((nw_data_window + 0x6274)) << 2);
    NW_NATIVE_BOOT("rb_cursor_ok");
#ifdef NWDEBUG_ENABLED
    nwtrace_hex("rb_icur", (unsigned long)cursor);
#endif
    NW_WINDOW_PTR_REF(uint8_t, 0x16d4) = (uint8_t *)(uintptr_t)nw_guest_to_host(cursor);
    NW_WINDOW_PTR_REF(uint8_t, 0x16d8) = (uint8_t *)(uintptr_t)nw_guest_to_host(cursor);

#ifdef NWDEBUG_ENABLED
    extern void nwtrace_hex(const char *t2, unsigned long v2);
    nwtrace_hex("rb_j", (unsigned long)&_espmark);
#endif
    if (MEM((nw_data_window + 0x6260)) != 0) {

        ResetStreamCursorState((unsigned char *)(uintptr_t)
                               nw_guest_to_host(cursor));
        slot_cursor = AllocateHeapCursorBlock(MEM((nw_data_window + 0x6260)));
        NW_WINDOW_PTR_REF(uint8_t, 0x16d4) = (uint8_t *)(uintptr_t)nw_guest_to_host(slot_cursor);
        NW_WINDOW_PTR_REF(uint8_t, 0x16d8) = (uint8_t *)(uintptr_t)nw_guest_to_host(slot_cursor);
        NW_NATIVE_BOOT("rb_slot_ok");
    }

#ifdef NWDEBUG_ENABLED
    nwtrace_hex("rb_k", (unsigned long)&_espmark);
#endif
    NW_NATIVE_BOOT("rb_done");
    return 1;
}
#endif /* NW_UNIT_RebuildGridHeapAndCursorStateFromStream */

/* ==========================================================================
 * Unit: ResetGridSpanGeometryIfModeFlagClear
 * Source: grid/ResetGridSpanGeometryIfModeFlagClear.c
 * ========================================================================== */
#ifdef NW_UNIT_ResetGridSpanGeometryIfModeFlagClear
/*
 * Reset Grid Span Geometry If Mode Flag Clear.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-resetgridspangeometryifmodeflagclear-c
 */

#include <windows.h>
#include "nwglobals.h"
#include <stdint.h>
extern unsigned char nw_data_window[];

/* ------------------------------------------------------------------ */
/* The guard byte. Non-zero => the whole body is a no-op.             */
/* ------------------------------------------------------------------ */
#define g_bModeFlag  (*(volatile BYTE *)(uintptr_t)(nw_data_window + 0x67f7))

/* ------------------------------------------------------------------ */
/* The 20-byte reset target (nw_data_window + 0x6348)..(nw_data_window + 0x635b), a field group of  */
/* the 76-byte save-header record at (nw_data_window + 0x632c).                      */
/* ------------------------------------------------------------------ */
#define g_dwPointOffsetX  (*(volatile DWORD *)(uintptr_t)(nw_data_window + 0x6348)) /* +0x1c */
#define g_dwPointOffsetY  (*(volatile DWORD *)(uintptr_t)(nw_data_window + 0x634c)) /* +0x20 */
#define g_wSpanSlotIndex  (*(volatile WORD  *)(uintptr_t)(nw_data_window + 0x6350)) /* +0x24  0xffff = no slot */
#define g_wGeometryWord352 (*(volatile WORD *)(uintptr_t)(nw_data_window + 0x6352)) /* +0x26 */
#define g_wGeometryWord354 (*(volatile WORD *)(uintptr_t)(nw_data_window + 0x6354)) /* +0x28 */
#define g_wClipDelta      (*(volatile WORD  *)(uintptr_t)(nw_data_window + 0x6356)) /* +0x2a  signed, = -1 */
#define g_abEdgeInset     ((volatile BYTE  *)(uintptr_t)(nw_data_window + 0x6358))  /* +0x2c .. +0x2f */

#define NO_SLOT_SENTINEL  0xffffu

/* ------------------------------------------------------------------ */
/* Function                                                           */
/* ------------------------------------------------------------------ */

void ResetGridSpanGeometryIfModeFlagClear(void)
{

    if (g_bModeFlag != 0u)
        return;

    g_dwPointOffsetY = 0u;
    g_dwPointOffsetX = 0u;

    g_wSpanSlotIndex   = NO_SLOT_SENTINEL;  /* 0xffff */
    g_wGeometryWord352 = 0x0010u;           /* 16 */
    g_wGeometryWord354 = 0x0050u;           /* 80 */
    g_wClipDelta       = NO_SLOT_SENTINEL;  /* 0xffff, read signed = -1 */

    g_abEdgeInset[3] = 0u;
    g_abEdgeInset[2] = 0u;
    g_abEdgeInset[1] = 0u;
    g_abEdgeInset[0] = 0u;

}
#endif /* NW_UNIT_ResetGridSpanGeometryIfModeFlagClear */

/* ==========================================================================
 * Unit: ResetTileRecordStateAndSeekStream
 * Source: grid/ResetTileRecordStateAndSeekStream.c
 * ========================================================================== */
#ifdef NW_UNIT_ResetTileRecordStateAndSeekStream
/*
 * Re-initialises the whole per-record drawing/streaming state block (17 globals zeroed.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-resettilerecordstateandseekstream-c
 */

#include <windows.h>
#include "nwglobals.h"
#include <stdint.h>
extern unsigned char nw_data_window[];

/* ---------------------------------------------------------------- state block */

/* Kind-1 record: passes still owed, and the absolute offset to rewind to. */
#define g_dwRecordPassCount      (*(DWORD *)(nw_data_window + 0x1704))
#define g_dwRecordRewindOffset   (*(DWORD *)(nw_data_window + 0x1708))
#define g_dwTargetFrameIndex     (*(DWORD *)(nw_data_window + 0x170c))

/* Kind-2: stored byte count + compressed-input staging buffer. */
#define g_cbKind2StoredBytes     (*(DWORD *)(nw_data_window + 0x1710))
#define g_pbKind2ScratchBuf      NW_WINDOW_PTR_REF(BYTE, 0x1714)

/* Kind-3: byte count + destination, then the palette staging pair. */
#define g_cbKind3StoredBytes     (*(DWORD *)(nw_data_window + 0x1718))
#define g_pbKind3DestBuf         NW_WINDOW_PTR_REF(BYTE, 0x171c)
#define g_cbPaletteBytes         (*(DWORD *)(nw_data_window + 0x1720))
#define g_pbPaletteBuffer        NW_WINDOW_PTR_REF(BYTE, 0x1724)

/* Kind-4: byte count + buffer, twice. */
#define g_cbKind4FieldABytes     (*(DWORD *)(nw_data_window + 0x1728))
#define g_pbKind4FieldABuf       NW_WINDOW_PTR_REF(BYTE, 0x172c)
#define g_cbKind4FieldBBytes     (*(DWORD *)(nw_data_window + 0x1730))
#define g_pbKind4FieldBBuf       NW_WINDOW_PTR_REF(BYTE, 0x1734)

#define g_bPendingSlotCleanup    (*(BYTE  *)(nw_data_window + 0x1738))
#define g_bMoreDataPending       (*(BYTE  *)(nw_data_window + 0x1739))

/* Kind-5: the record's origin. Kind-6: its default cell size. */
#define g_nRecordOriginX         (*(DWORD *)(nw_data_window + 0x173c))
#define g_nRecordOriginY         (*(DWORD *)(nw_data_window + 0x1740))
#define g_dwDefaultCellWidth     (*(DWORD *)(nw_data_window + 0x1744))
#define g_dwDefaultCellHeight    (*(DWORD *)(nw_data_window + 0x1748))

/* Read-only level-header dimension pair that seeds the default cell size. */
#define g_dwLevelExtentWidth     (*(DWORD *)(nw_data_window + 0x6264))
#define g_dwLevelExtentHeight    (*(DWORD *)(nw_data_window + 0x6268))

/* ------------------------------------------------------------------ callees */

extern void __cdecl ResetPendingSlotHookState(void);

extern void __cdecl DispatchMciAudioReset(void);

extern BOOL __stdcall SeekStreamFileSlotToOffset(DWORD dwMoveMethod,
                                                 LONG  lOffset);


/* Origin constant: the offset is ABSOLUTE, measured from the start of the file. */
#define SEEK_BEGIN_STREAM 0L

BOOL __stdcall ResetTileRecordStateAndSeekStream(LONG lRecordStreamOffset)
{

    g_dwRecordPassCount = 1;

    g_nRecordOriginY      = 0;   
    g_nRecordOriginX      = 0;   
    g_cbKind4FieldBBytes  = 0;   
    g_cbKind4FieldABytes  = 0;   
    g_cbPaletteBytes      = 0;   
    g_cbKind3StoredBytes  = 0;   
    g_cbKind2StoredBytes  = 0;   
    g_dwTargetFrameIndex  = 0;   
    g_dwRecordRewindOffset = 0;  
    g_pbKind4FieldBBuf    = 0;   
    g_pbKind4FieldABuf    = 0;   
    g_pbPaletteBuffer     = 0;   
    g_pbKind3DestBuf      = 0;   
    g_pbKind2ScratchBuf   = 0;   

    g_bMoreDataPending     = 0;
    g_bPendingSlotCleanup  = 0;

    g_dwDefaultCellWidth  = g_dwLevelExtentWidth;    /* (nw_data_window + 0x1744) := (nw_data_window + 0x6264) */
    g_dwDefaultCellHeight = g_dwLevelExtentHeight;   /* (nw_data_window + 0x1748) := (nw_data_window + 0x6268) */

    ResetPendingSlotHookState();
    DispatchMciAudioReset();

    {
        BOOL _ok = SeekStreamFileSlotToOffset(SEEK_BEGIN_STREAM, lRecordStreamOffset);
        return _ok;
    }

}
#endif /* NW_UNIT_ResetTileRecordStateAndSeekStream */
