#include "nwarch.h"
/*
 * GridCore -- grid-delta application and frame composition.
 *
 * Module: grid
 * Interface: include/GridCore.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   ApplyGridDeltaPlain                                        grid/ApplyGridDeltaPlain.c
 *   ApplyGridDeltaSignedExt                                    grid/ApplyGridDeltaSignedExt.c
 *   ApplyGridDeltaSignedLoop                                   grid/ApplyGridDeltaSignedLoop.c
 *   ApplyGridDeltaZeroSkip                                     grid/ApplyGridDeltaZeroSkip.c
 *   ComposeFrameCellsFromStreamRecords                         grid/ComposeFrameCellsFromStreamRecords.c
 *   ComputeCellAddressAndDispatchGridDelta                     grid/ComputeCellAddressAndDispatchGridDelta.c
 *   WrapGridAxesAndDispatch                                    grid/WrapGridAxesAndDispatch.c
 */

/* ==========================================================================
 * Unit: ApplyGridDeltaPlain
 * Source: grid/ApplyGridDeltaPlain.c
 * ========================================================================== */
#ifdef NW_UNIT_ApplyGridDeltaPlain
/*
 * Grid-delta op 56: each row adds colCount stream bytes into consecutive destination cells, then rewinds the destination pointer by the row pitch
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-applygriddeltaplain-c
 */

#include <stdint.h>
#include "GridDeltaApply.h"

__attribute__((section(".nwdiag")))
uint8_t *ApplyGridDeltaPlain(uint8_t *pStream, uint32_t rowCount,
                             uint32_t colCount, uint8_t *pDst, uint32_t rewind)
{
    uintptr_t stream = (uintptr_t)pStream;
    uintptr_t dst = (uintptr_t)pDst;
    uint32_t rows = rowCount;

    do {
        uint32_t budget = colCount;

        do {
            uint8_t byte = *(const uint8_t *)stream;
            stream += 1u;
            *(uint8_t *)dst = (uint8_t)(*(uint8_t *)dst + byte);
            dst += 1u;
        } while (--budget != 0u);

        dst -= rewind;
    } while (--rows != 0u);

    return (uint8_t *)stream;
}
#endif /* NW_UNIT_ApplyGridDeltaPlain */

/* ==========================================================================
 * Unit: ApplyGridDeltaSignedExt
 * Source: grid/ApplyGridDeltaSignedExt.c
 * ========================================================================== */
#ifdef NW_UNIT_ApplyGridDeltaSignedExt
/*
 * Grid-delta op 57: row decoder with three lead-byte forms
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-applygriddeltasignedext-c
 */

#include <stdint.h>
#include "GridDeltaApply.h"

__attribute__((section(".nwdiag")))
uint8_t *ApplyGridDeltaSignedExt(uint8_t *pStream, uint32_t rowCount,
                                 uint32_t colCount, uint8_t *pDst,
                                 uint32_t rewind)
{
    uintptr_t stream = (uintptr_t)pStream;
    uintptr_t dst = (uintptr_t)pDst;
    uint32_t rows = rowCount;

    do {
        uint32_t budget = colCount;

        for (;;) {
            uint8_t lead = *(const uint8_t *)stream;
            stream += 1u;

            if (lead & 0x80u) {
                uint32_t code = (uint32_t)(lead & 0x3fu);

                if (!(lead & 0x40u)) {
                    uint32_t n = code + 1u;
                    budget -= n;
                    NwGridDeltaAddRun(&dst, &stream, n);
                    if (budget != 0u)
                        continue;
                    break;
                }

                if (code == 0u) {
                    dst += budget;
                    break;
                }
                if (code == 1u) {
                    NwGridDeltaAddRun(&dst, &stream, budget);
                    break;
                }

                {
                    uint32_t n = code + 1u;
                    budget -= n;
                    NwGridDeltaFillRun(&dst, &stream, n);
                    if (budget != 0u)
                        continue;
                    break;
                }
            } else {
                uint32_t n = (uint32_t)lead + 1u;
                dst += n;
                budget -= n;
                if (budget != 0u)
                    continue;
                break;
            }
        }

        dst -= rewind;
    } while (--rows != 0u);

    return (uint8_t *)stream;
}
#endif /* NW_UNIT_ApplyGridDeltaSignedExt */

/* ==========================================================================
 * Unit: ApplyGridDeltaSignedLoop
 * Source: grid/ApplyGridDeltaSignedLoop.c
 * ========================================================================== */
#ifdef NW_UNIT_ApplyGridDeltaSignedLoop
/*
 * Grid-delta op 59: row decoder whose positive-lead form always copies bytes
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-applygriddeltasignedloop-c
 */

#include <stdint.h>
#include "GridDeltaApply.h"

__attribute__((section(".nwdiag")))
uint8_t *ApplyGridDeltaSignedLoop(uint8_t *pStream, uint32_t rowCount,
                                  uint32_t colCount, uint8_t *pDst,
                                  uint32_t rewind)
{
    uintptr_t stream = (uintptr_t)pStream;
    uintptr_t dst = (uintptr_t)pDst;
    uint32_t rows = rowCount;

    do {
        uint32_t budget = colCount;

        for (;;) {
            uint8_t lead = *(const uint8_t *)stream;
            stream += 1u;

            if (lead & 0x80u) {
                uint32_t code = (uint32_t)(lead & 0x3fu);

                if (!(lead & 0x40u)) {
                    uint32_t n = code + 1u;
                    dst += n;
                    budget -= n;
                    if (budget != 0u)
                        continue;
                    break;
                }

                if (code == 0u) {
                    dst += budget;
                    break;
                }
                if (code == 1u) {
                    NwGridDeltaAddRun(&dst, &stream, budget);
                    break;
                }

                {
                    uint32_t n = code + 1u;
                    budget -= n;
                    NwGridDeltaFillRun(&dst, &stream, n);
                    if (budget != 0u)
                        continue;
                    break;
                }
            } else {
                uint32_t n = (uint32_t)lead + 1u;
                budget -= n;
                NwGridDeltaAddRun(&dst, &stream, n);
                if (budget != 0u)
                    continue;
                break;
            }
        }

        dst -= rewind;
    } while (--rows != 0u);

    return (uint8_t *)stream;
}
#endif /* NW_UNIT_ApplyGridDeltaSignedLoop */

/* ==========================================================================
 * Unit: ApplyGridDeltaZeroSkip
 * Source: grid/ApplyGridDeltaZeroSkip.c
 * ========================================================================== */
#ifdef NW_UNIT_ApplyGridDeltaZeroSkip
/*
 * Grid-delta op 58: row decoder with explicit zero and one lead cases
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-applygriddeltazeroskip-c
 */

#include <stdint.h>
#include "GridDeltaApply.h"

__attribute__((section(".nwdiag")))
uint8_t *ApplyGridDeltaZeroSkip(uint8_t *pStream, uint32_t rowCount,
                                uint32_t colCount, uint8_t *pDst,
                                uint32_t rewind)
{
    uintptr_t stream = (uintptr_t)pStream;
    uintptr_t dst = (uintptr_t)pDst;
    uint32_t rows = rowCount;

    do {
        uint32_t budget = colCount;

        for (;;) {
            uint8_t lead = *(const uint8_t *)stream;
            stream += 1u;

            if (lead & 0x80u) {
                uint32_t n = (uint32_t)(lead & 0x3fu) + 1u;

                if (!(lead & 0x40u)) {
                    dst += n;
                    budget -= n;
                    if (budget != 0u)
                        continue;
                    break;
                }

                budget -= n;
                NwGridDeltaAddRun(&dst, &stream, n);
                if (budget != 0u)
                    continue;
                break;
            }

            if (lead == 0u) {
                dst += budget;
                break;
            }
            if (lead == 1u) {
                NwGridDeltaAddRun(&dst, &stream, budget);
                break;
            }

            {
                uint32_t n = (uint32_t)lead + 1u;
                budget -= n;
                NwGridDeltaFillRun(&dst, &stream, n);
                if (budget != 0u)
                    continue;
                break;
            }
        }

        dst -= rewind;
    } while (--rows != 0u);

    return (uint8_t *)stream;
}
#endif /* NW_UNIT_ApplyGridDeltaZeroSkip */

/* ==========================================================================
 * Unit: ComposeFrameCellsFromStreamRecords
 * Source: grid/ComposeFrameCellsFromStreamRecords.c
 * ========================================================================== */
#ifdef NW_UNIT_ComposeFrameCellsFromStreamRecords
/*
 * Decode one record-type-1 block: a list of cell rectangles
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-composeframecellsfromstreamrecords-c
 */

#include <windows.h>
#include <stdint.h>
#include "nwglobals.h"

#define g_bBlockSubType        (*(uint8_t  *)(void *)(nw_data_window + 0x16f9))
#define g_bBlockFlagB          (*(uint8_t  *)(void *)(nw_data_window + 0x16fb))
#define g_cbBlockSize          (*(uint32_t *)(void *)(nw_data_window + 0x16fc))

/* Record state block reset by ResetTileRecordStateAndSeekStream. */
#define g_dwRecordPassCount    (*(uint32_t *)(void *)(nw_data_window + 0x1704))
#define g_dwRecordRewindOffset (*(uint32_t *)(void *)(nw_data_window + 0x1708))
#define g_dwTargetFrameIndex   (*(uint32_t *)(void *)(nw_data_window + 0x170c))
#define g_cbKind3StoredBytes   (*(uint32_t *)(void *)(nw_data_window + 0x1718))
#define g_pbKind3DestBuf       NW_WINDOW_PTR_REF(uint8_t, 0x171c)
#define g_cbKind4FieldABytes   (*(uint32_t *)(void *)(nw_data_window + 0x1728))
#define g_bFrameAbandonSeen    (*(uint8_t  *)(void *)(nw_data_window + 0x173a))
#define g_nRecordOriginX       (*(uint32_t *)(void *)(nw_data_window + 0x173c))
#define g_nRecordOriginY       (*(uint32_t *)(void *)(nw_data_window + 0x1740))
#define g_dwDefaultCellWidth   (*(uint32_t *)(void *)(nw_data_window + 0x1744))
#define g_dwDefaultCellHeight  (*(uint32_t *)(void *)(nw_data_window + 0x1748))

/* Frame-flush gate and the three counters stamped by the message pump. */
#define g_bFrameFlushGate      (*(uint8_t  *)(void *)(nw_data_window + 0x174c))
#define g_dwAnimationStartTick (*(uint32_t *)(void *)(nw_data_window + 0x1750))
#define g_dwAnimationStep      (*(uint32_t *)(void *)(nw_data_window + 0x1754))
#define g_bFrameSkipCounter    (*(uint8_t  *)(void *)(nw_data_window + 0x1758))

/* The palette index used for the whole-frame blit in the subtype-1 arm. */
extern BYTE g_bGridPenIndex;   /* window+0x627e */

extern int __stdcall ReadStreamBlockRawOrLzss(uint32_t cbUnusedBlockSize,
                                               uint8_t *pbDestBuf);
extern uint32_t InvokeUtNotifySlot8Thunk(void);
extern void __stdcall FillActiveSurfaceWithReplicatedIndexDword(BYTE bIndex);
extern DWORD __cdecl GetTrackedSlotFilePosition(void);
extern BOOL __stdcall SeekStreamFileSlotToOffset(DWORD dwMoveMethod, LONG lOffset);

extern uint32_t __stdcall ComputeCellAddressAndDispatchGridDelta(
    uint32_t rowCount, uint32_t colCount, uint32_t row, uint32_t column,
    uint32_t decoderIndex, uint8_t *pStream);

extern void __stdcall BlitRectOrTileGridToPlayerSurface(
    char cTileGridMode, int nRectBottom, int nRectRight, int nRectTop,
    int nRectLeft);

extern void __stdcall FlushPendingSurfaceRectsAndReseedRandom(char bDrawPending);
extern BYTE __stdcall DispatchQueuedMessagesAndStampStartTime(DWORD *pdwStartTime);
extern void __cdecl FlushGdiDrawingBatch(void);

#define NW_DIAG __attribute__((section(".nwdiag")))

#ifdef NW_PLATFORM_SDL
extern void NwSdlVideoComposeBegin(unsigned target, unsigned step);
extern void NwSdlVideoComposeEnd(int present);
#define NW_COMPOSE_BEGIN(target, step) \
    NwSdlVideoComposeBegin((target), (step))
#define NW_COMPOSE_END(present) NwSdlVideoComposeEnd((present))
#else
#define NW_COMPOSE_BEGIN(target, step) ((void)0)
#define NW_COMPOSE_END(present) ((void)0)
#endif

NW_DIAG uint32_t __cdecl ComposeFrameCellsFromStreamRecords(void)
{
#ifdef NW_PLATFORM_SDL
    extern void NwProfEvent(unsigned kind, unsigned a, unsigned b);
    NwProfEvent(5u, g_dwTargetFrameIndex, g_dwAnimationStep);
#endif
    NW_COMPOSE_BEGIN(g_dwTargetFrameIndex, g_dwAnimationStep);
    uint32_t dwPumpStamp = 1u;       /* [S+0x14], seeded to 1 before the switch */
    uint32_t dwCellHeight;           /* [S+0x18], item row count */
    uint8_t  bFlags;
    uint32_t nCellX;
    uint32_t nCellY;
    uint32_t nCellWidth;
    uint8_t *pStream;
    BYTE     bCommand;

    if (!ReadStreamBlockRawOrLzss(g_cbKind3StoredBytes, g_pbKind3DestBuf)) {
        NW_COMPOSE_END(0);
        return 0u;
    }

    pStream = g_pbKind3DestBuf;
    dwPumpStamp = 1u;

    switch (g_bBlockSubType) {
    case 1:

        g_bFrameAbandonSeen = 0u;
        (void)InvokeUtNotifySlot8Thunk();
        FillActiveSurfaceWithReplicatedIndexDword((BYTE)g_bBlockFlagB);

        if (g_dwRecordPassCount > 1u)
            g_dwRecordRewindOffset = (uint32_t)GetTrackedSlotFilePosition();

        dwPumpStamp = 0u;
        g_bFrameFlushGate = 0u;
        break;

    case 2:

        g_bFrameAbandonSeen = 1u;
        if (--g_dwRecordPassCount != 0u)
            (void)SeekStreamFileSlotToOffset(0u,
                                            (LONG)g_dwRecordRewindOffset);
        break;

    default:
        break;
    }

    if (g_cbBlockSize != 0u) {
        for (;;) {

            nCellX = g_nRecordOriginX;
            nCellY = g_nRecordOriginY;
            bFlags = *pStream++;
            dwCellHeight = 0u;
            nCellWidth = 0u;

            if ((bFlags & 0x40u) == 0u) {

                nCellX += *pStream++;
                if ((bFlags & 0x04u) != 0u)
                    nCellX += (uint32_t)*pStream++ << 8;

                nCellY += *pStream++;
                if ((bFlags & 0x08u) != 0u)
                    nCellY += (uint32_t)*pStream++ << 8;

                nCellWidth = *pStream++;
                if ((bFlags & 0x10u) != 0u)
                    nCellWidth += (uint32_t)*pStream++ << 8;

                dwCellHeight = *pStream++;
                if ((bFlags & 0x20u) != 0u)
                    dwCellHeight += (uint32_t)*pStream++ << 8;
            } else {

                nCellWidth = g_dwDefaultCellWidth;
                dwCellHeight = g_dwDefaultCellHeight;
            }

            pStream += ComputeCellAddressAndDispatchGridDelta(
                dwCellHeight, nCellWidth, nCellY, nCellX,
                (uint32_t)(bFlags & 0x03u), pStream);

            if (g_bFrameFlushGate != 0u) {
                BlitRectOrTileGridToPlayerSurface(
                    0,
                    (int)(nCellY + dwCellHeight),
                    (int)(nCellX + nCellWidth),
                    (int)nCellY,
                    (int)nCellX);
            }

            if ((bFlags & 0x80u) != 0u)
                break;
        }
    }

    if (g_bFrameFlushGate != 0u)
        FlushPendingSurfaceRectsAndReseedRandom(1);

    bCommand = DispatchQueuedMessagesAndStampStartTime((DWORD *)&dwPumpStamp);
    if (bCommand == 0x82u) {
        NW_COMPOSE_END(0);
        return 2u;
    }
    if (bCommand == 0x83u) {
        NW_COMPOSE_END(0);
        return 3u;
    }

    if (g_bBlockSubType == 1u) {

        BlitRectOrTileGridToPlayerSurface(
            (char)g_bGridPenIndex,
            (int)(g_nRecordOriginY + g_dwDefaultCellHeight),
            (int)(g_nRecordOriginX + g_dwDefaultCellWidth),
            (int)g_nRecordOriginY,
            (int)g_nRecordOriginX);

        (void)DispatchQueuedMessagesAndStampStartTime(
            (DWORD *)&g_dwAnimationStartTick);
        g_dwAnimationStep = 0u;
        g_bFrameSkipCounter = 0u;
        g_bFrameFlushGate = 1u;
        goto flush_batch;
    }

    if (g_dwTargetFrameIndex != 0u) {
        uint32_t dwElapsed;

        dwElapsed = (dwPumpStamp - g_dwAnimationStartTick) / g_dwAnimationStep;
        if (dwElapsed > g_dwTargetFrameIndex) {

            if (g_cbKind4FieldABytes != 0u) {
                ++g_bFrameSkipCounter;
                if (g_bFrameSkipCounter == 8u)
                    goto force_flush;
                g_bFrameFlushGate = 0u;   
                goto flush_batch;
            }
            goto force_flush;
        }

        if (dwElapsed >= g_dwTargetFrameIndex)
            goto force_flush;

        for (;;) {
            bCommand = DispatchQueuedMessagesAndStampStartTime(
                (DWORD *)&dwPumpStamp);
            if (bCommand == 0x82u) {
                NW_COMPOSE_END(0);
                return 2u;
            }
            if (bCommand == 0x83u) {
                NW_COMPOSE_END(0);
                return 3u;
            }

            dwElapsed = (dwPumpStamp - g_dwAnimationStartTick) /
                        g_dwAnimationStep;
            if (dwElapsed >= g_dwTargetFrameIndex)
                goto force_flush;
        }
    }

    g_bFrameFlushGate = 1u;
    goto flush_batch;

force_flush:

    g_bFrameSkipCounter = 0u;
    g_bFrameFlushGate = 1u;

flush_batch:

    FlushGdiDrawingBatch();
    ++g_dwAnimationStep;
    NW_COMPOSE_END(1);
    return 1u;
}
#endif /* NW_UNIT_ComposeFrameCellsFromStreamRecords */

/* ==========================================================================
 * Unit: ComputeCellAddressAndDispatchGridDelta
 * Source: grid/ComputeCellAddressAndDispatchGridDelta.c
 * ========================================================================== */
#ifdef NW_UNIT_ComputeCellAddressAndDispatchGridDelta
/*
 * Resolve the bottom-up destination cell and run one of the four grid-delta stream decoders; return the number of stream bytes consumed
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-computecelladdressanddispatchgriddelta-c
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "nwglobals.h"

extern int g_ScreenPitch;   
extern int g_ScreenRows;    
#ifdef NW_PLATFORM_SDL
extern int NwSdlRenderScale(void);
#endif

uint8_t *ApplyGridDeltaPlain(uint8_t *pStream, uint32_t rowCount,
                             uint32_t colCount, uint8_t *pDst,
                             uint32_t rewind);
uint8_t *ApplyGridDeltaSignedExt(uint8_t *pStream, uint32_t rowCount,
                                 uint32_t colCount, uint8_t *pDst,
                                 uint32_t rewind);
uint8_t *ApplyGridDeltaZeroSkip(uint8_t *pStream, uint32_t rowCount,
                                uint32_t colCount, uint8_t *pDst,
                                uint32_t rewind);
uint8_t *ApplyGridDeltaSignedLoop(uint8_t *pStream, uint32_t rowCount,
                                  uint32_t colCount, uint8_t *pDst,
                                  uint32_t rewind);

typedef uint8_t *(*NwGridDeltaApplyFn)(uint8_t *, uint32_t, uint32_t,
                                       uint8_t *, uint32_t);

static NwGridDeltaApplyFn const s_grid_delta_apply[4] = {
    ApplyGridDeltaPlain,
    ApplyGridDeltaSignedExt,
    ApplyGridDeltaZeroSkip,
    ApplyGridDeltaSignedLoop,
};

__attribute__((section(".nwdiag")))
uint32_t __stdcall ComputeCellAddressAndDispatchGridDelta(
    uint32_t rowCount, uint32_t colCount, uint32_t row, uint32_t column,
    uint32_t decoderIndex, uint8_t *pStream)
{
    uint32_t pitch = (uint32_t)g_ScreenPitch;
    uintptr_t pDst;
    uint32_t rewind;
    uint8_t *pFinal;

#ifdef NW_PLATFORM_SDL
    {
        int scale = NwSdlRenderScale();
        if (scale > 1) {
            uint8_t *base = (uint8_t *)(uintptr_t)g_ScreenBase;

            /* A repacked high-resolution startup frame emits its full-surface
             * cell in physical coordinates (1280x960 at scale 2).  In that
             * case the stream bytes are already native pixels and must not be
             * expanded a second time. */
            if (row == 0u && column == 0u &&
                rowCount == (uint32_t)g_ScreenRows &&
                colCount == (uint32_t)g_ScreenPitch) {
                pDst = (uintptr_t)base +
                       (uint32_t)((uint32_t)g_ScreenRows - 1u) * pitch;
                rewind = pitch + colCount;
                pFinal = s_grid_delta_apply[decoderIndex](
                    pStream, rowCount, colCount, (uint8_t *)pDst, rewind);
                return (uint32_t)((uintptr_t)pFinal - (uintptr_t)pStream);
            }

            if (rowCount != 0u && colCount != 0u) {
                size_t cellBytes = (size_t)rowCount * (size_t)colCount;
                static uint8_t *s_cellScratch;
                static size_t s_cellScratchBytes;
                const uint32_t logicalW = pitch / (uint32_t)scale;
                const uint32_t logicalH =
                    (uint32_t)g_ScreenRows / (uint32_t)scale;
                uint8_t rowbuf[640 * 8];
                uint8_t *tmp;
                uint32_t x;
                uint32_t y;

                if (cellBytes > s_cellScratchBytes) {
                    uint8_t *p = (uint8_t *)realloc(s_cellScratch, cellBytes);
                    if (p == NULL)
                        return 0u;
                    s_cellScratch = p;
                    s_cellScratchBytes = cellBytes;
                }
                tmp = s_cellScratch;

                /* Decode into a logical cell, then expand each logical pixel
                 * to a scale x scale block.  Reading the old block first keeps
                 * the delta semantics for partial cells. */
                for (y = 0; y < rowCount; y++) {
                    int ly = (int)(row + y);
                    uint8_t *dst = tmp +
                        (size_t)(rowCount - 1u - y) * colCount;
                    if (ly < 0 || ly >= (int)logicalH) {
                        memset(dst, 0, colCount);
                        continue;
                    }
                    for (x = 0; x < colCount; x++) {
                        int lx = (int)(column + x);
                        uint8_t value = 0;
                        if (lx >= 0 && lx < (int)logicalW) {
                            value = base[(size_t)((uint32_t)g_ScreenRows - 1u -
                                          (uint32_t)(ly * scale)) *
                                          (size_t)pitch +
                                          (size_t)(lx * scale)];
                        }
                        dst[x] = value;
                    }
                }

                pDst = (uintptr_t)(tmp +
                       (size_t)(rowCount - 1u) * (size_t)colCount);
                rewind = colCount * 2u;
                pFinal = s_grid_delta_apply[decoderIndex](
                    pStream, rowCount, colCount, (uint8_t *)pDst, rewind);

                /* Expand one logical row into a physical row buffer, then
                 * copy that row to the scale physical rows.  The previous
                 * loop called memset once per physical row per logical pixel
                 * (over a million calls for a full 640x480 cell at 4x). */
                for (y = 0; y < rowCount; y++) {
                    int ly = (int)(row + y);
                    int lx0;
                    int lx1;
                    int visible;
                    int py;
                    uint8_t *rb;
                    const uint8_t *srcrow;

                    if (ly < 0 || ly >= (int)logicalH)
                        continue;
                    lx0 = (int)column;
                    lx1 = (int)(column + colCount);
                    if (lx0 < 0)
                        lx0 = 0;
                    if (lx1 > (int)logicalW)
                        lx1 = (int)logicalW;
                    if (lx1 <= lx0)
                        continue;
                    visible = lx1 - lx0;
                    srcrow = tmp +
                        (size_t)(rowCount - 1u - y) * colCount +
                        (size_t)(lx0 - (int)column);
                    rb = rowbuf;
                    if (scale == 4) {
                        for (x = 0; x < (uint32_t)visible; x++) {
                            uint32_t pattern =
                                (uint32_t)srcrow[x] * 0x01010101u;
                            *(uint32_t *)(void *)rb = pattern;
                            rb += 4;
                        }
                    } else {
                        for (x = 0; x < (uint32_t)visible; x++) {
                            uint8_t value = srcrow[x];
                            int k;
                            for (k = 0; k < scale; k++)
                                *rb++ = value;
                        }
                    }
                    for (py = 0; py < scale; py++) {
                        int fy = ly * scale + py;
                        if (fy < 0 || fy >= (int)g_ScreenRows)
                            continue;
                        memcpy(base + (size_t)((uint32_t)g_ScreenRows -
                                              1u - (uint32_t)fy) *
                                      (size_t)pitch +
                                      (size_t)lx0 * (size_t)scale,
                               rowbuf,
                               (size_t)visible * (size_t)scale);
                    }
                }
                return (uint32_t)((uintptr_t)pFinal - (uintptr_t)pStream);
            }
        }
    }
#endif

    pDst = (uintptr_t)g_ScreenBase;
    pDst += (uint32_t)((uint32_t)g_ScreenRows - 1u - row) * pitch;
    pDst += column;

    rewind = pitch + colCount;

    pFinal = s_grid_delta_apply[decoderIndex](pStream, rowCount, colCount,
                                               (uint8_t *)pDst, rewind);

    return (uint32_t)((uintptr_t)pFinal - (uintptr_t)pStream);
}
#endif /* NW_UNIT_ComputeCellAddressAndDispatchGridDelta */

/* ==========================================================================
 * Unit: WrapGridAxesAndDispatch
 * Source: grid/WrapGridAxesAndDispatch.c
 * ========================================================================== */
#ifdef NW_UNIT_WrapGridAxesAndDispatch
/*
 * Wraps grid-axis operands and dispatches a new cell-span record.
 *
 * Module: grid
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#grid-wrapgridaxesanddispatch-c
 */

#include <stdint.h>
#include "nwglobals.h"

#define NW_DIAG __attribute__((section(".nwdiag")))

#define g_dwMinorAxisOffset \
    (*(int32_t *)(void *)(nw_data_window + 0x6220))
#define g_dwMinorAxisStride \
    (*(int32_t *)(void *)(nw_data_window + 0x61e8))
#define g_dwMajorAxisOffset \
    (*(int32_t *)(void *)(nw_data_window + 0x6224))
#define g_dwMajorAxisStride \
    (*(int32_t *)(void *)(nw_data_window + 0x61ec))

extern void (*g_pfnCellOperandHook)(uint32_t *);

extern void __stdcall AllocateGridCellSpanRecordAndDispatch(
    int nCellMajor, int nCellMinor, uint32_t dwOpcode);

int __stdcall WrapGridAxesAndDispatch(uint32_t dwCellOperandBlock);

NW_DIAG int __stdcall WrapGridAxesAndDispatch(uint32_t dwCellOperandBlock)
{
    int32_t nMinorValue;
    int32_t nMajorValue;
    int32_t nWrappedMinor;
    int32_t nWrappedMajor;

    g_pfnCellOperandHook(&dwCellOperandBlock);

    nMinorValue = (int32_t)(int8_t)((dwCellOperandBlock >> 16) & 0xffu);
    nMinorValue = (int32_t)((uint32_t)nMinorValue +
                            (uint32_t)g_dwMinorAxisOffset +
                            (uint32_t)g_dwMinorAxisStride);
    nWrappedMinor = nMinorValue % g_dwMinorAxisStride;

    nMajorValue = (int32_t)(int8_t)((dwCellOperandBlock >> 24) & 0xffu);
    nMajorValue = (int32_t)((uint32_t)nMajorValue +
                            (uint32_t)g_dwMajorAxisOffset +
                            (uint32_t)g_dwMajorAxisStride);
    nWrappedMajor = nMajorValue % g_dwMajorAxisStride;

    AllocateGridCellSpanRecordAndDispatch(nWrappedMajor, nWrappedMinor,
                                          dwCellOperandBlock);

    return (uint16_t)(dwCellOperandBlock >> 16) == 0u;
}
#endif /* NW_UNIT_WrapGridAxesAndDispatch */
