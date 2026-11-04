#include "nwarch.h"
/*
 * Blit -- sprite, row, and region blitting.
 *
 * Module: video
 * Interface: include/Blit.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   AddRectToExtentList                                        video/AddRectToExtentList.c
 *   AddTranslatedRectToExtentList                              video/AddTranslatedRectToExtentList.c
 *   BlitPlayerRegionOrTileGrid                                 video/BlitPlayerRegionOrTileGrid.c
 *   BlitRectOrTileGridToPlayerSurface                          video/BlitRectOrTileGridToPlayerSurface.c
 *   CopyRowsBulkWithTailToDisplayBuffer                        video/CopyRowsBulkWithTailToDisplayBuffer.c
 *   CopySpriteRowsToDisplayBuffer                              video/CopySpriteRowsToDisplayBuffer.c
 *   FillActiveSurfaceWithReplicatedIndexDword                  video/FillActiveSurfaceWithReplicatedIndexDword.c
 *   FlushGdiDrawingBatch                                       video/FlushGdiDrawingBatch.c
 */

/* ==========================================================================
 * Unit: AddRectToExtentList
 * Source: video/AddRectToExtentList.c
 * ========================================================================== */
#ifdef NW_UNIT_AddRectToExtentList
/*
 * Add Rect To Extent List.
 *
 * Module: video
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#video-addrecttoextentlist-c
 */

#include <stdint.h>

/*
 * Extent-list layout (5 words = 0x14 bytes, allocated through the custom heap):
 *   +0x00  nMinX   \  bounding box, signed; compared with JG/JL only, so the
 *   +0x04  nMinY    \ coordinates are genuinely signed 32-bit
 *   +0x08  nMaxX    /
 *   +0x0c  nMaxY   /
 *   +0x10  pNext   next node, NULL terminates the singly-linked list.
 *
 * The argument order is the binary's, not a natural reading order:
 * (nMaxY, nMaxX, nMinY, nMinX) — a node is filled as
 *   node->nMinX = arg4, node->nMinY = arg3, node->nMaxX = arg2, node->nMaxY = arg1.
 */

typedef struct RectNode {
    int             nMinX;
    int             nMinY;
    int             nMaxX;
    int             nMaxY;
    uint32_t         pNext;    /* +0x10 guest pointer to the next node */
} RectNode;

#define RECT_NODE_BYTES 0x14   /* 5 words: 4 coordinates + next pointer */

/* Module-owned global: head of the extent list. */
extern RectNode *g_pRectListHead;   

extern uint32_t *__stdcall AllocHeapBlock(uint32_t dwSizeBytes);                    
extern void __stdcall FreeHeapBlock(uint8_t *pData);                          

void AddRectToExtentList(int nMaxY, int nMaxX, int nMinY, int nMinX)
{
    RectNode *pScan;        /* node currently examined by the search          */
    RectNode *pFound;       /* node that contains the rectangle, or new node   */
    RectNode *pCur;         /* coalesce pass: current node                    */
    RectNode *pNext;        /* coalesce pass: follower, absorbed when merged  */
    RectNode *pPrev;        /* coalesce pass: predecessor of pCur             */
    int       nMoved;       /* coordinates that actually changed on growth    */

    if (g_pRectListHead == NULL) {
        /* Empty list: the rectangle becomes the head node. */
        pFound = (RectNode *)(uintptr_t)AllocHeapBlock(RECT_NODE_BYTES);
        g_pRectListHead = pFound;
        goto FillNewNode;
    }

    /*
     * Walk to the first node that contains the new rectangle, i.e. whose
     * bounding box satisfies newMin <= nodeMax on both axes. The binary tests
     * the head node and the tail-following nodes with the same four
     * comparisons, so one loop covers both; appending happens when the walk
     * runs off the end.
     */
    pScan = g_pRectListHead;
    for (;;) {
        if ((nMaxX >= pScan->nMinX) && (nMaxY >= pScan->nMinY) &&
            (nMinX <= pScan->nMaxX) && (nMinY <= pScan->nMaxY)) {
            pFound = pScan;
            break;
        }
        pNext = (RectNode *)(uintptr_t)pScan->pNext;
        if (pNext == NULL) {
            pFound = (RectNode *)(uintptr_t)AllocHeapBlock(RECT_NODE_BYTES);
            pScan->pNext = (uint32_t)(uintptr_t)pFound;
            goto FillNewNode;
        }
        pScan = pNext;
    }

    /* Contained: grow the node to the union of itself and the rectangle. */
    nMoved = 0;
    if (nMinX < pFound->nMinX) { pFound->nMinX = nMinX; nMoved++; }
    if (nMaxX > pFound->nMaxX) { pFound->nMaxX = nMaxX; nMoved++; }
    if (nMinY < pFound->nMinY) { pFound->nMinY = nMinY; nMoved++; }
    if (nMaxY > pFound->nMaxY) { pFound->nMaxY = nMaxY; nMoved++; }
    if (nMoved == 0) {
        return;   /* already contained, nothing changed */
    }

    /*
     * Growth may have made this node overlap a neighbour. Repeatedly rescan
     * consecutive pairs, union the earlier node into the later one, unlink and
     * free the earlier node, then start the scan over from the head (the list
     * may have shrunk, so the outer loop restarts rather than resuming).
     * Overlap is inclusive: merely touching boxes coalesce.
     */
CoalesceRestart:
    for (pCur = g_pRectListHead, pPrev = NULL; pCur != NULL;
         pPrev = pCur, pCur = (RectNode *)(uintptr_t)pCur->pNext) {

        for (pNext = (RectNode *)(uintptr_t)pCur->pNext; pNext != NULL;
             pNext = (RectNode *)(uintptr_t)pNext->pNext) {
            if ((pNext->nMinX <= pCur->nMaxX) && (pNext->nMinY <= pCur->nMaxY) &&
                (pCur->nMinX <= pNext->nMaxX) && (pCur->nMinY <= pNext->nMaxY)) {
                if (pCur->nMinX < pNext->nMinX) { pNext->nMinX = pCur->nMinX; }
                if (pCur->nMaxX > pNext->nMaxX) { pNext->nMaxX = pCur->nMaxX; }
                if (pCur->nMinY < pNext->nMinY) { pNext->nMinY = pCur->nMinY; }
                if (pCur->nMaxY > pNext->nMaxY) { pNext->nMaxY = pCur->nMaxY; }

                if (pPrev != NULL) {
                    pPrev->pNext = pCur->pNext;   /* unlink the absorbed node */
                } else {
                    g_pRectListHead = (RectNode *)(uintptr_t)pCur->pNext;
                }
                FreeHeapBlock((uint8_t *)pCur);
                goto CoalesceRestart;
            }
        }
    }

    return;

FillNewNode:
    pFound->nMinX = nMinX;
    pFound->nMinY = nMinY;
    pFound->nMaxX = nMaxX;
    pFound->nMaxY = nMaxY;
    pFound->pNext = 0u;
}
#endif /* NW_UNIT_AddRectToExtentList */

/* ==========================================================================
 * Unit: AddTranslatedRectToExtentList
 * Source: video/AddTranslatedRectToExtentList.c
 * ========================================================================== */
#ifdef NW_UNIT_AddTranslatedRectToExtentList
/*
 * Add Translated Rect To Extent List.
 *
 * Module: video
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#video-addtranslatedrecttoextentlist-c
 */

#include <stdint.h>
#include "nwglobals.h"

/* Module globals: translation applied to X / Y before the list is updated.
   Read and written as signed 32-bit values elsewhere in the extent code. */

/* Extent-list manager; sole callee. Argument order is (nMaxY, nMaxX, nMinY, nMinX). */
extern void AddRectToExtentList(int nMaxY, int nMaxX, int nMinY, int nMinX); 

void __stdcall AddTranslatedRectToExtentList(int nMaxY, int nMaxX, int nMinY, int nMinX)
{
    AddRectToExtentList(nMaxY + g_nExtentOffsetY,
                        nMaxX + g_nExtentOffsetX,
                        nMinY + g_nExtentOffsetY,
                        nMinX + g_nExtentOffsetX);
}
#endif /* NW_UNIT_AddTranslatedRectToExtentList */

/* ==========================================================================
 * Unit: BlitPlayerRegionOrTileGrid
 * Source: video/BlitPlayerRegionOrTileGrid.c
 * ========================================================================== */
#ifdef NW_UNIT_BlitPlayerRegionOrTileGrid
/*
 * Blit Player Region Or Tile Grid.
 *
 * Module: video
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#video-blitplayerregionortilegrid-c
 */

#include <windows.h>
extern unsigned char nw_data_window[];

#define g_dwClientW   (*(DWORD *)(nw_data_window + 0x5f20))

#define g_dwClientH   (*(DWORD *)(nw_data_window + 0x5f24))

#define g_adwTileCol   ((const DWORD *)(nw_data_window + 0x964))
#define g_adwTileRow   ((const DWORD *)(nw_data_window + 0x9a4))

#define TILE_SIZE          16
/* Lattice pitch between repeats of one stamp: ADD reg,0x40 on both axes. */
#define TILE_PITCH         0x40
/* Number of stamp slots in each origin table (outer loop bound, PUSH/CMP 0x10). */
#define TILE_SLOT_COUNT    16

extern void BlitRegionFromDcOrDib(int nSourceHeight,
                                  int nRegionHeight,
                                  int nRegionWidth,
                                  int nRegionY,
                                  int nRegionX,
                                  void *pSrcBits);

BOOL WINAPI BlitPlayerRegionOrTileGrid(unsigned char bTileGridMode,
                                       int           nSourceHeight,
                                       unsigned int  nReserved,
                                       int           nRegionHeight,
                                       int           nRegionWidth,
                                       int           nRegionY,
                                       int           nRegionX,
                                       void         *pSrcBits)
{
    int nTileIndex;
    int nCol;
    int nRow;

    /* Forwarded by the caller and never consumed here; see the note at
     * (nw_data_window + 0x1ccc). Referenced once so -Wextra stays quiet. */
    (void)nReserved;

    if (g_dwClientW == 0 || g_dwClientH == 0) {
        return TRUE;
    }

    if (bTileGridMode == 0) {
        /* Single region: the caller's own rectangle, passed straight through.
         * The sole caller computes the extents as (source height - nRegionY)
         * and (source width - nRegionX), i.e. the part of the source that
         * remains below/right of the origin. */
        BlitRegionFromDcOrDib(nSourceHeight, nRegionHeight, nRegionWidth,
                              nRegionY, nRegionX, pSrcBits);
        return TRUE;
    }

    /* Tile mode: walk all 16 stamp slots. For slot i the column origin comes
     * from g_adwTileCol[i] and the row origin from g_adwTileRow[i]; both origin
     * pointers advance by one dword per slot (ADD [stack],EAX with EAX=4).
     * Each axis then repeats its stamp every TILE_PITCH pixels until it runs
     * past the client extent. GdiFlush() follows EVERY individual stamp so the
     * flush is per-stamp, not per-slot. */
    for (nTileIndex = 0; nTileIndex < TILE_SLOT_COUNT; nTileIndex++) {
        nCol = (int)g_adwTileCol[nTileIndex];
        while (nCol < (int)g_dwClientW) {
            nRow = (int)g_adwTileRow[nTileIndex];
            while (nRow < (int)g_dwClientH) {
                BlitRegionFromDcOrDib(nSourceHeight, TILE_SIZE, TILE_SIZE,
                                      nRow, nCol, pSrcBits);
                GdiFlush();
                nRow += TILE_PITCH;
            }
            nCol += TILE_PITCH;
        }
    }
    return TRUE;
}
#endif /* NW_UNIT_BlitPlayerRegionOrTileGrid */

/* ==========================================================================
 * Unit: BlitRectOrTileGridToPlayerSurface
 * Source: video/BlitRectOrTileGridToPlayerSurface.c
 * ========================================================================== */
#ifdef NW_UNIT_BlitRectOrTileGridToPlayerSurface
/*
 * Turns one caller's dirty rectangle into a blit against the current source surface.
 *
 * Module: video
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#video-blitrectortilegridtoplayersurface-c
 */

#include <windows.h>
extern unsigned char nw_data_window[];

#ifdef NW_PLATFORM_SDL
extern void NwSdlVideoPresent(void);
#endif

#define g_dwSourceHeight  (*(DWORD *)(nw_data_window + 0x1cd0))

#define g_dwSourceWidth   (*(DWORD *)(nw_data_window + 0x1ccc))

#define g_pSourceBits     NW_WINDOW_PTR_REF(void, 0x1cc8)

extern void __stdcall BlitPlayerRegionOrTileGrid(unsigned char bTileGridMode,
                                       int           nSourceHeight,
                                       unsigned int  nReserved,
                                       int           nRegionHeight,
                                       int           nRegionWidth,
                                       int           nRegionY,
                                       int           nRegionX,
                                       void         *pSrcBits);

void WINAPI BlitRectOrTileGridToPlayerSurface(char cTileGridMode,
                                              int  nRectBottom,
                                              int  nRectRight,
                                              int  nRectTop,
                                              int  nRectLeft)
{
    BlitPlayerRegionOrTileGrid(cTileGridMode,
                               g_dwSourceHeight,
                               g_dwSourceWidth,
                               nRectBottom - nRectTop,
                               nRectRight - nRectLeft,
                               nRectTop,
                               nRectLeft,
                               g_pSourceBits);

#ifdef NW_PLATFORM_SDL
    NwSdlVideoPresent();
#endif
}
#endif /* NW_UNIT_BlitRectOrTileGridToPlayerSurface */

/* ==========================================================================
 * Unit: CopyRowsBulkWithTailToDisplayBuffer
 * Source: video/CopyRowsBulkWithTailToDisplayBuffer.c
 * ========================================================================== */
#ifdef NW_UNIT_CopyRowsBulkWithTailToDisplayBuffer
/*
 * Blits a rectangular block of raw bytes into the bottom-up display framebuffer as floor(width/4) dwords plus a 0/1/2/3-byte tail selected by width...
 *
 * Module: video
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#video-copyrowsbulkwithtailtodisplaybuffer-c
 */

#include <windows.h>
#include "nwglobals.h"
#include <stdint.h>
#include <string.h>

extern int g_ScreenPitch;    
extern int g_ScreenRows;     

/* The VM's source surface is the fixed 640x480 screen background.  A
 * high-resolution pack may carry the same image at an integer multiple;
 * these maps implement nearest-neighbour sampling for a pack scale that does
 * not match NW_RENDER_SCALE. */
#define NW_BG_SRC_W      640
#define NW_BG_SRC_H      480
#define NW_BG_MAX_SCALE  8
#ifdef NW_PLATFORM_SDL
/* Only the SDL high-resolution path reads these maps.  Keeping their 35 KiB
 * out of the Win32 oracle's .bss preserves the pinned portable section
 * layout; that build has no render scale and never references them. */
static int g_nwBgMapX[NW_BG_SRC_W * NW_BG_MAX_SCALE];
static int g_nwBgMapY[NW_BG_SRC_H * NW_BG_MAX_SCALE];
static int g_nwBgMapScale;
static int g_nwBgMapW;
static int g_nwBgMapH;
static int g_nwBgMapReady;
#endif

void __stdcall CopyRowsBulkWithTailToDisplayBuffer(
    int nDstRowLimit,   /* arg1 [EBP+0x08] - last destination row of the block      */
    int nDstColLimit,   /* arg2 [EBP+0x0c] - right edge of the destination block    */
    int nDstFirstRow,   /* arg3 [EBP+0x10] - top row of the destination block       */
    int nDstFirstCol,   /* arg4 [EBP+0x14] - left edge of the destination block    */
    int nSrcRowLimit,   /* arg5 [EBP+0x18] - DEAD in this variant, never read      */
    int nSrcStride,     /* arg6 [EBP+0x1c] - bytes per SOURCE row (row pitch)      */
    int nSrcFirstRow,   /* arg7 [EBP+0x20] - first source row (caller negates it)  */
    int nSrcFirstCol,   /* arg8 [EBP+0x24] - first source column (caller negates it)*/
    int nSrcBase)       /* arg9 [EBP+0x28] - source surface base address          */
{
    unsigned char *pDst;
    unsigned char *pSrc;
    int   nCols;        /* BYTES copied per row - the block's width                */
    int   nRows;        /* rows to walk; the loop is a do-while, see below         */
    unsigned int uDwords; /* nCols >> 2 - REP MOVSD trip count per row             */
    int   nTail;        /* nCols & 3 - 0, 1, 2 or 3 leftover bytes after the dwords */
    int   i;

    (void)nSrcRowLimit; 

    nCols = nDstColLimit - nDstFirstCol;
    nRows = nDstRowLimit - nDstFirstRow;

#ifdef NW_PLATFORM_SDL
    {
        extern int NwSdlRenderScale(void);
        int scale = NwSdlRenderScale();
        if (scale > 1) {
            int logicalW = g_ScreenPitch / scale;
            int logicalH = g_ScreenRows / scale;
            int x0 = nDstFirstCol;
            int x1 = nDstColLimit;
            int r;
            int py;
            int x;

            if (x0 < 0)
                x0 = 0;
            if (x1 > logicalW)
                x1 = logicalW;
            if (x1 <= x0)
                return;

            pSrc = (unsigned char *)(intptr_t)(nSrcBase
                     - (nSrcFirstRow * nSrcStride)
                     - nSrcFirstCol);

            /* The screen background is a 640x480 indexed surface and is
             * blitted here before sprites are overlaid.  If the overlay pack
             * carries an upscaled copy, sample it directly instead of
             * enlarging the 640x480 pixels. */
            {
                extern int NwAssetGetHighResBackground(
                    const unsigned char **ppPixels, int *pw, int *ph);
                const unsigned char *pHi = NULL;
                const unsigned char *pBg = NULL;
                int hiW = 0;
                int hiH = 0;
                int srcW = 0;
                int srcH = 0;
                int srcY0 = -nSrcFirstRow;
                int srcX0 = -nSrcFirstCol;
                int sx0;
                int sx1;

                if (g_pColorTableBlock != NULL) {
                    pBg  = g_pColorTableBlock + 0x18;
                    srcW = *(const int *)(const void *)(g_pColorTableBlock + 8);
                    srcH = *(const int *)(const void *)(g_pColorTableBlock + 12);
                }
                sx0 = srcX0 + x0 - nDstFirstCol;
                sx1 = srcX0 + x1 - nDstFirstCol;
                if (pBg != NULL &&
                    (const unsigned char *)(intptr_t)nSrcBase == pBg &&
                    srcW == NW_BG_SRC_W && srcH == NW_BG_SRC_H &&
                    nSrcStride == srcW &&
                    srcY0 >= 0 && srcY0 + nRows <= srcH &&
                    sx0 >= 0 && sx1 <= srcW &&
                    NwAssetGetHighResBackground(&pHi, &hiW, &hiH) &&
                    hiW >= srcW && hiH >= srcH) {
                    int exact = (hiW == srcW * scale &&
                                 hiH == srcH * scale);

                    if (!g_nwBgMapReady ||
                        g_nwBgMapScale != scale ||
                        g_nwBgMapW != hiW || g_nwBgMapH != hiH) {
                        int i;
                        int denomX = srcW * scale;
                        int denomY = srcH * scale;
                        for (i = 0; i < denomX; i++)
                            g_nwBgMapX[i] = (int)(((long long)i * hiW) /
                                                  denomX);
                        for (i = 0; i < denomY; i++)
                            g_nwBgMapY[i] = (int)(((long long)i * hiH) /
                                                  denomY);
                        g_nwBgMapScale = scale;
                        g_nwBgMapW = hiW;
                        g_nwBgMapH = hiH;
                        g_nwBgMapReady = 1;
                    }

                    for (r = 0; r < nRows; r++) {
                        int sy = srcY0 + r;
                        int ly = nDstFirstRow + r;
                        if (ly < 0 || ly >= logicalH)
                            continue;
                        for (py = 0; py < scale; py++) {
                            int fy = ly * scale + py;
                            unsigned char *d;
                            if (fy < 0 || fy >= g_ScreenRows)
                                continue;
                            d = (unsigned char *)(intptr_t)(g_ScreenBase
                                + (g_ScreenRows - 1 - fy) * g_ScreenPitch)
                                + (size_t)x0 * (size_t)scale;
                            if (exact) {
                                int hy = sy * scale + py;
                                int hx = sx0 * scale;
                                memcpy(d,
                                       pHi + (size_t)hy * (size_t)hiW +
                                           (size_t)hx,
                                       (size_t)(x1 - x0) * (size_t)scale);
                            } else {
                                int hy = g_nwBgMapY[sy * scale + py];
                                const unsigned char *hrow =
                                    pHi + (size_t)hy * (size_t)hiW;
                                for (x = x0; x < x1; x++) {
                                    int sx = srcX0 + x - nDstFirstCol;
                                    int mx = sx * scale;
                                    int px;
                                    for (px = 0; px < scale; px++)
                                        d[(size_t)(x - x0) *
                                              (size_t)scale +
                                          (size_t)px] =
                                            hrow[g_nwBgMapX[mx + px]];
                                }
                            }
                        }
                    }
                    return;
                }
            }

            for (r = 0; r < nRows; r++) {
                const unsigned char *srow = pSrc + (size_t)r * (size_t)nSrcStride;
                int ly = nDstFirstRow + r;
                int visible;
                int done;
                unsigned char rowbuf[NW_BG_SRC_W * NW_BG_MAX_SCALE];

                if (ly < 0 || ly >= logicalH)
                    continue;
                visible = x1 - x0;
                if (visible <= 0)
                    continue;
                if (scale > NW_BG_MAX_SCALE) {
                    /* The row buffer is sized for the supported pack scales. */
                    for (py = 0; py < scale; py++) {
                        int fy = ly * scale + py;
                        unsigned char *d;
                        if (fy < 0 || fy >= g_ScreenRows)
                            continue;
                        d = (unsigned char *)(intptr_t)(g_ScreenBase
                            + (g_ScreenRows - 1 - fy) * g_ScreenPitch)
                            + (size_t)x0 * (size_t)scale;
                        for (x = x0; x < x1; x++) {
                            unsigned char v = srow[x - nDstFirstCol];
                            memset(d + (size_t)(x - x0) * (size_t)scale, v,
                                   (size_t)scale);
                        }
                    }
                    continue;
                }
                /* Build one logical row at the physical width, then copy it
                 * to each of the scale destination rows.  The old inner loop
                 * issued one memset per logical pixel per destination row;
                 * a full-screen background blit at 4x made over a million
                 * tiny calls per frame. */
                for (done = 0; done < visible; done += NW_BG_SRC_W) {
                    int chunk = visible - done;
                    unsigned char *rb = rowbuf;
                    const unsigned char *s =
                        srow + (x0 - nDstFirstCol) + done;
                    int k;

                    if (chunk > NW_BG_SRC_W)
                        chunk = NW_BG_SRC_W;
                    if (scale == 4) {
                        for (k = 0; k < chunk; k++) {
                            uint32_t pattern =
                                (uint32_t)s[k] * 0x01010101u;
                            *(uint32_t *)(void *)rb = pattern;
                            rb += 4;
                        }
                    } else {
                        for (k = 0; k < chunk; k++) {
                            unsigned char v = s[k];
                            int px;
                            for (px = 0; px < scale; px++)
                                *rb++ = v;
                        }
                    }
                    for (py = 0; py < scale; py++) {
                        int fy = ly * scale + py;
                        unsigned char *d;
                        if (fy < 0 || fy >= g_ScreenRows)
                            continue;
                        d = (unsigned char *)(intptr_t)(g_ScreenBase
                            + (g_ScreenRows - 1 - fy) * g_ScreenPitch)
                            + (size_t)(x0 + done) * (size_t)scale;
                        memcpy(d, rowbuf, (size_t)chunk * (size_t)scale);
                    }
                }
            }
            return;
        }
    }
#endif

    pDst = (unsigned char *)(intptr_t)(g_ScreenBase
             + ((g_ScreenRows - 1 - nDstFirstRow) * g_ScreenPitch)
             + nDstFirstCol);

    pSrc = (unsigned char *)(intptr_t)(nSrcBase
             - (nSrcFirstRow * nSrcStride)
             - nSrcFirstCol);

    uDwords = (unsigned int)nCols >> 2;
    nTail  = (int)((unsigned int)nCols & 3u);

    do {
        /* One MOVSD.REP iteration copies one dword and steps BOTH pointers by four.
         * There is NO alignment fixup on either side - nothing in this body ever masks a
         * pointer or compares a pointer to 4, so an odd source or destination address is
         * copied unchanged and no unaligned fallback branch exists. */
        for (i = 0; i < (int)uDwords; i++) {
            uint32_t dwCell = *(const uint32_t *)(const void *)(pSrc + (4 * i));
            *(uint32_t *)(void *)(pDst + (4 * i)) = dwCell;
        }

        switch (nTail) {
        case 1:
            pDst[(size_t)uDwords * 4u] = pSrc[(size_t)uDwords * 4u];
            break;
        case 2:
            {
                uint16_t wCell = *(const uint16_t *)(const void *)(pSrc + ((size_t)uDwords * 4u));
                *(uint16_t *)(void *)(pDst + ((size_t)uDwords * 4u)) = wCell;
            }
            break;
        case 3:
            {
                uint16_t wCell = *(const uint16_t *)(const void *)(pSrc + ((size_t)uDwords * 4u));
                *(uint16_t *)(void *)(pDst + ((size_t)uDwords * 4u)) = wCell;
                pDst[((size_t)uDwords * 4u) + 2u] = pSrc[((size_t)uDwords * 4u) + 2u];
            }
            break;
        default:
            break;
        }

        pDst -= g_ScreenPitch;
        pSrc += nSrcStride;
        nRows--;
    } while (nRows > 0);
}
#endif /* NW_UNIT_CopyRowsBulkWithTailToDisplayBuffer */

/* ==========================================================================
 * Unit: CopySpriteRowsToDisplayBuffer
 * Source: video/CopySpriteRowsToDisplayBuffer.c
 * ========================================================================== */
#ifdef NW_UNIT_CopySpriteRowsToDisplayBuffer
/*
 * Copy Sprite Rows To Display Buffer.
 *
 * Module: video
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#video-copyspriterowstodisplaybuffer-c
 */

#include <stdint.h>
#include <string.h>
#include "nwglobals.h"

#ifdef NW_PLATFORM_SDL
#include <SDL3/SDL_timer.h>

/* Wall time spent in the game's draw path.  The main loop logs this beside
 * the high-resolution asset time so a first-frame hitch can be attributed to
 * drawing rather than to VM or stream work. */
static uint64_t g_nwBlitNs;
static unsigned g_nwBlitCalls;

void NwBlitTakeStats(uint64_t *pNs, unsigned *pCalls)
{
    if (pNs != NULL)
        *pNs = g_nwBlitNs;
    if (pCalls != NULL)
        *pCalls = g_nwBlitCalls;
    g_nwBlitNs = 0;
    g_nwBlitCalls = 0;
}
#endif

#ifdef NW_PLATFORM_SDL
extern int NwSdlRenderScale(void);
extern int NwAssetGetHighResSprite(uint32_t dwOffset,
                                   const unsigned char *pbPixels,
                                   int w, int h, int bMatchByCrc,
                                   const unsigned char **pp,
                                   int *pw, int *ph);
extern int NwAssetImageRecordIsDisk(const void *pRecord);

/* Copy one sprite row, leaving transparent (index 0) destination pixels
 * untouched.  Whole words are copied with a single store when they contain no
 * zero bytes; that is the common case for opaque sprite rows and for the large
 * frame images used by cutscenes.  The zero-byte test is conservative, so a
 * false positive only falls back to the byte loop. */
static void nw_copy_sprite_row(unsigned char *d, const unsigned char *s,
                               int count)
{
    int i = 0;

    while (i + 8 <= count) {
        uint64_t v;
        memcpy(&v, s + i, 8);
        if (v == 0) {
            i += 8;
            continue;
        }
        if (((v - 0x0101010101010101ULL) & ~v &
             0x8080808080808080ULL) == 0) {
            memcpy(d + i, &v, 8);
        } else {
            int j;
            for (j = 0; j < 8; j++) {
                unsigned char c = s[i + j];
                if (c != 0)
                    d[i + j] = c;
            }
        }
        i += 8;
    }
    for (; i < count; i++) {
        unsigned char c = s[i];
        if (c != 0)
            d[i] = c;
    }
}

/* Blit one sprite at the native framebuffer resolution.  The destination is
 * still described in the VM's 640x480 logical coordinates, but every logical
 * pixel expands to a scale x scale block.  Source coordinates are derived
 * from the *output* pixel, not the logical pixel, so a high-resolution image
 * (for example a 4x pack at a 4x render scale) contributes every source pixel
 * instead of being reduced to 640x480 and expanded again. */
static void nw_blit_sprite_scaled(const unsigned char *src, int srcStride,
                                  int srcW, int srcH,
                                  int dstW, int dstH, int flags,
                                  int sceneX, int sceneY,
                                  int clipLeft, int clipRight,
                                  int clipTop, int clipBottom)
{
    extern int g_ScreenPitch;
    extern int g_ScreenRows;
    int scale = NwSdlRenderScale();
    int logicalW = g_ScreenPitch / scale;
    int logicalH = g_ScreenRows / scale;
    int outW = dstW * scale;
    int outH = dstH * scale;
    int x0 = sceneX;
    int x1 = sceneX + dstW;
    int y0 = sceneY;
    int y1 = sceneY + dstH;
    int mode = (flags >> 3) & 3;
    int flipX = (mode == 1 || mode == 3);
    int bottomUp = (mode >= 2);
    int exactX = (srcW == outW);
    int exactY = (srcH == outH);
    int dy;
    unsigned char *base = (unsigned char *)(intptr_t)g_ScreenBase;

    if (src == NULL || srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0)
        return;
    if (x0 < clipLeft) x0 = clipLeft;       /* v_left: x origin */
    if (x1 > clipTop) x1 = clipTop;         /* v_top: x extent */
    if (y0 < clipRight) y0 = clipRight;     /* v_right: y origin */
    if (y1 > clipBottom) y1 = clipBottom;   /* v_bot: y extent */
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > logicalW) x1 = logicalW;
    if (y1 > logicalH) y1 = logicalH;
    if (x1 <= x0 || y1 <= y0)
        return;

    /* A 4x pack at a 4x render scale supplies exactly outW x outH pixels.
     * In that case every output row is a direct slice of one source row, so
     * the general per-pixel source-coordinate arithmetic is unnecessary.
     * Transparency (index 0) still has to be preserved. */
    if (exactX && exactY) {
        for (dy = y0; dy < y1; dy++) {
            const int ly = dy - sceneY;
            int py;
            for (py = 0; py < scale; py++) {
                const int outLocalY = ly * scale + py;
                const int srcLocalY = bottomUp ? (outH - 1 - outLocalY)
                                               : outLocalY;
                const int fy = dy * scale + py;
                const unsigned char *srow;
                unsigned char *d;
                int count;
                int i;

                if (srcLocalY < 0 || srcLocalY >= srcH ||
                    fy < 0 || fy >= g_ScreenRows)
                    continue;
                srow = src + (size_t)srcLocalY * (size_t)srcStride;
                d = base + (size_t)(g_ScreenRows - 1 - fy) *
                            (size_t)g_ScreenPitch +
                    (size_t)x0 * (size_t)scale;
                count = (x1 - x0) * scale;
                if (flipX) {
                    int sx = outW - 1 - (x0 - sceneX) * scale;
                    for (i = 0; i < count; i++) {
                        const unsigned char v = srow[sx - i];
                        if (v != 0)
                            d[i] = v;
                    }
                } else {
                    const int sx = (x0 - sceneX) * scale;
                    nw_copy_sprite_row(d, srow + sx, count);
                }
            }
        }
        return;
    }

    {
        static int s_spriteXMap[4096];
        static int s_spriteXMapSrcW;
        static int s_spriteXMapOutW;
        static int s_spriteXMapValid;
        static unsigned char s_spriteRow[4096];
        int count = (x1 - x0) * scale;
        int useRow = count > 0 && count <= (int)sizeof(s_spriteRow);
        int useXMap = !exactX && outW > 0 &&
                      outW <= (int)(sizeof(s_spriteXMap) /
                                    sizeof(s_spriteXMap[0]));
        int mapi;

        if (useXMap &&
            (!s_spriteXMapValid || s_spriteXMapSrcW != srcW ||
             s_spriteXMapOutW != outW)) {
            for (mapi = 0; mapi < outW; mapi++)
                s_spriteXMap[mapi] =
                    (int)((long long)mapi * srcW / outW);
            s_spriteXMapSrcW = srcW;
            s_spriteXMapOutW = outW;
            s_spriteXMapValid = 1;
        }

        for (dy = y0; dy < y1; dy++) {
            int ly = dy - sceneY;
            int py;
            for (py = 0; py < scale; py++) {
                int outLocalY = ly * scale + py;
                int srcLocalY = bottomUp ? (outH - 1 - outLocalY)
                                         : outLocalY;
                int syi = exactY ? srcLocalY
                                 : (int)((long long)srcLocalY * srcH / outH);
                int fy = dy * scale + py;
                const unsigned char *srow;
                unsigned char *drow;

                if (syi < 0 || syi >= srcH || fy < 0 || fy >= g_ScreenRows)
                    continue;
                srow = src + (size_t)syi * (size_t)srcStride;
                drow = base + (size_t)(g_ScreenRows - 1 - fy) *
                              (size_t)g_ScreenPitch;
                if (exactX && !flipX) {
                    const int sx = (x0 - sceneX) * scale;
                    nw_copy_sprite_row(drow + (size_t)x0 * (size_t)scale,
                                       srow + sx, count);
                    continue;
                }
                if (useRow && useXMap) {
                    const int baseX = (x0 - sceneX) * scale;
                    int i;
                    for (i = 0; i < count; i++) {
                        int outLocalX = baseX + i;
                        int srcLocalX = flipX ? (outW - 1 - outLocalX)
                                              : outLocalX;
                        int sxi = (srcLocalX >= 0 && srcLocalX < outW)
                                      ? s_spriteXMap[srcLocalX] : -1;
                        s_spriteRow[i] = (sxi >= 0 && sxi < srcW)
                                             ? srow[sxi] : 0;
                    }
                    nw_copy_sprite_row(drow + (size_t)x0 * (size_t)scale,
                                       s_spriteRow, count);
                    continue;
                }
                {
                    int dx;
                    for (dx = x0; dx < x1; dx++) {
                        int lx = dx - sceneX;
                        int px;
                        unsigned char *d = drow + (size_t)(dx * scale);
                        for (px = 0; px < scale; px++) {
                            int outLocalX = lx * scale + px;
                            int srcLocalX = flipX
                                                ? (outW - 1 - outLocalX)
                                                : outLocalX;
                            int sxi = exactX ? srcLocalX
                                             : (int)((long long)srcLocalX *
                                                     srcW / outW);
                            unsigned char value;
                            if (sxi < 0 || sxi >= srcW)
                                continue;
                            value = srow[sxi];
                            if (value != 0)
                                d[px] = value;
                        }
                    }
                }
            }
        }
    }
}
#endif

/* ---- parent-resolved callee signatures (do not redefine) ---- */
extern void __stdcall FreeHeapBlock(void *p);                
extern void   FlushGdiDrawingBatch(void);            
extern void AddRectToExtentList(int nMaxY, int nMaxX, int nMinY, int nMinX);

extern void __stdcall CopyRowsBulkWithTailToDisplayBuffer(
    int nDstRowLimit, int nDstColLimit, int nDstFirstRow, int nDstFirstCol,
    int nSrcRowLimit, int nSrcStride, int nSrcFirstRow, int nSrcFirstCol,
    int nSrcBase);                                   
extern void * __stdcall LoadDiskSpriteImageFromStream(void *stream_base); 
extern void __stdcall CopyTextRowsReverseSkipNulToDisplayBuffer(
    int nDstRowLimit, int nDstColLimit, int nDstFirstRow, int nDstFirstCol,
    int nSrcRowLimit, int nSrcStride, int nSrcFirstRow, int nSrcFirstCol,
    int nSrcBase);                                   
extern void __stdcall CopyTextRowsForwardSkipNulToDisplayBuffer(
    int nDstRowLimit, int nDstColLimit, int nDstFirstRow, int nDstFirstCol,
    int nSrcRowLimit, int nSrcStride, int nNegSrcFirstRow, int nSrcFirstCol,
    int nSrcBase);                                   
extern void __stdcall CopyTextRowsToDisplayBuffer(
    int nDstRowLimit, int nDstColLimit, int nDstFirstRow, int nDstFirstCol,
    int nSrcRowLimit, int nSrcStride, int nSrcFirstRow, int nSrcFirstCol,
    int nSrcBase);                                   
extern void __stdcall CopyTextRowsUnrolledSkipNulToDisplayBuffer(
    int nDstRowLimit, int nDstColLimit, int nDstFirstRow, int nDstFirstCol,
    int nSrcRowLimit, int nSrcStride, int nSrcFirstRow, int nSrcFirstCol,
    int nSrcBase);                                   
extern void __stdcall BlitRectOrTileGridToPlayerSurface(
    char cTileGridMode, int nRectBottom, int nRectRight, int nRectTop,
    int nRectLeft);                                  

typedef void (__stdcall *NwRowCopyFn)(int, int, int, int, int, int, int, int, int);
static NwRowCopyFn const nw_rowCopyByFlagsBits[4] = {
    CopyTextRowsUnrolledSkipNulToDisplayBuffer,   
    CopyTextRowsForwardSkipNulToDisplayBuffer,    
    CopyTextRowsToDisplayBuffer,                  
    CopyTextRowsReverseSkipNulToDisplayBuffer,    
};

#ifdef NWDEBUG_ENABLED
extern unsigned long g_pSlotsTop;
extern void nwtrace_hex(const char *, unsigned long);
#endif

static int NwCopySpriteRowsToDisplayBufferImpl(void)
{
#ifdef NWDEBUG_ENABLED
    nwtrace_hex("rend_top", (unsigned long)g_pSlotsTop);
#endif

    uint32_t sprite_scratch[0x1040 / sizeof(uint32_t)];  

    void   *obj;                                /* [EBP-0x104c] snapshot of node    */
    uint32_t sprite_count = 0;                  /* [EBP-0x1044] gathered count     */
    void  **sprite_recs = (void **)sprite_scratch; /* [EBP-0x1040 + i*4]; the scratch area above */

    obj = g_display_object;
    if (obj == NULL)
        return 1;

    /* ---- gather records from the sprite list (-0x10 stride) ------------- */
    for (void *p = g_sprite_list_top;           
         (char *)p > (char *)g_sprite_list_bot; 
         p = (void *)((char *)p - 0x10)) {
        uint8_t b0 = *(const uint8_t *)p;       /* record signature test          */
        if ((b0 & 0x83) == 0x81) sprite_recs[sprite_count++] = p;   /* [EBP+i*4-0x1040] */
    }

    if (sprite_count > 1u) {
        for (uint32_t lim = 1; lim <= sprite_count; ++lim) {
            for (uint32_t j = lim; j < sprite_count; ++j) {
                void **ppAnchor = &sprite_recs[lim - 1];
                void **ppCur    = &sprite_recs[j];
                uint8_t kAnchor = *(const uint8_t *)((char *)*ppAnchor + 3);
                uint8_t kCur    = *(const uint8_t *)((char *)*ppCur    + 3);
                if (kAnchor < kCur) {            
                    void *t = *ppAnchor;
                    *ppAnchor = *ppCur;
                    *ppCur    = t;
                }
            }
        }
    }

    if (g_view_flag_7d == 0 && g_default_recsel != 0) {
        sprite_recs[sprite_count] = (void *)(nw_data_window + 0x74);  
        ++sprite_count;                          
    }

    void *next_node;                           /* [EBP-0x40] chain successor */

retry_object:                                  /* re-entry from the tail chain loop */
    /* The extent/display list is guest memory; pNext is one 32-bit word. */
    next_node = (void *)(uintptr_t)
                (*(const uint32_t *)((const char *)obj + 0x10));

    /* ---- view rect from object fields, relative to coord bases ---------- */
    int32_t v_left  = *(const int32_t *)(char *)obj           - g_coord_base_x; /* obj+0x0 */
    if (v_left < 0) v_left = 0;                                   /* [EBP-0x3c]          */
    int32_t v_top   = *(const int32_t *)((char *)obj + 8)        - g_coord_base_x; /* obj+8   */
    if (v_top > g_view_clamp_a) v_top = g_view_clamp_a;          
    int32_t v_right = *(const int32_t *)((char *)obj + 4)        - g_coord_base_y; /* obj+4   */
    if (v_right < 0) v_right = 0;                                 /* [EBP-0x34]          */
    int32_t v_bot   = *(const int32_t *)((char *)obj + 0xC)      - g_coord_base_y; /* obj+0xC */
    if (v_bot > g_view_clamp_b) v_bot = g_view_clamp_b;          

    int degenerate = (v_top <= v_left) || (v_bot <= v_right);     
    if (!degenerate) {

        CopyRowsBulkWithTailToDisplayBuffer(
            /* nDstRowLimit  */ v_bot,
            /* nDstColLimit  */ v_top,
            /* nDstFirstRow  */ v_right,
            /* nDstFirstCol  */ v_left,
            /* nSrcRowLimit  */ *(const int32_t *)(g_pColorTableBlock + 0xc),
            /* nSrcStride    */ *(const int32_t *)(g_pColorTableBlock + 8),
            /* nSrcFirstRow  */ -(v_right + g_coord_base_y),
            /* nSrcFirstCol  */ -(v_left  + g_coord_base_x),
            /* nSrcBase      */ (int)(intptr_t)(g_pColorTableBlock + 0x18));

        /* ---- per-sprite dispatch loop ------------------------------------- */
        uint32_t idx = 0;                        /* [EBP-0x1048] counter          */
        while (idx < sprite_count) {             
            void *rec = sprite_recs[idx++];      /* [EBP + i*4 - 0x1040]         */
            int32_t scene_x = (int16_t)*(const uint16_t *)((char *)rec + 8)   - g_coord_base_x;
            int32_t scene_y = (int16_t)*(const uint16_t *)((char *)rec + 0xA) - g_coord_base_y;
            uint8_t flags = *(const uint8_t *)rec;    /* THE DISPATCH SELECTOR    */

            /* Both records are guest-width: +0xc is the image-descriptor
             * pointer and the descriptor's first word points at the image. */
            void *imgdesc = (void *)(uintptr_t)
                            (*(const uint32_t *)((const char *)rec + 0xC));

            const void *imgrec = (const void *)(uintptr_t)
                                 (*(const uint32_t *)imgdesc);

            if ((uintptr_t)imgrec < 0x10000u || (uintptr_t)imgrec >= 0x80000000u) {
                continue;
            }
            int32_t src_w = (int16_t)*(const uint16_t *)(const char *)imgrec;         /* image+0x0 */
            int32_t src_h = (int16_t)*((const uint16_t *)(const char *)imgrec + 1);   /* image+0x2 */

            if ((flags & 0x40) != 0) {           /* bit6: extra offset adjust    */
                scene_x -= (int16_t)*(const uint16_t *)((char *)imgdesc + 4);
                scene_y -= (int16_t)*(const uint16_t *)((char *)imgdesc + 6);
            }

            int32_t dst_x = src_w + scene_x;      /* [EBP-0x24] unclamped dest x */
            int32_t dst_y = src_h + scene_y;      /* [EBP-0x1c] unclamped dest y */

            if (scene_x >= v_top ||               /* off-view bounds tests:      */
                scene_y >= v_bot ||               
                dst_x <= v_left ||                
                dst_y <= v_right) {               
                continue;
            }

            void *img = LoadDiskSpriteImageFromStream((void *)imgrec);
            int32_t rel_x        = scene_x - v_left;              /* [EBP-0x14] */
            int32_t rel_y        = scene_y - v_right;
            int32_t nDstRowLimit = (dst_y   < v_bot) ? dst_y   : v_bot;
            int32_t nDstColLimit = (dst_x   < v_top) ? dst_x   : v_top;
            int32_t nDstFirstRow = (rel_y  >= 0)     ? scene_y : v_right;
            int32_t nDstFirstCol = (rel_x  >= 0)     ? scene_x : v_left;
            int32_t nSrcFirstRow = (rel_y  >= 0)     ? 0      : rel_y;
            int32_t nSrcFirstCol = (rel_x  >= 0)     ? 0      : rel_x;

#ifdef NW_PLATFORM_SDL
            if (NwSdlRenderScale() > 1) {
                const unsigned char *src = (const unsigned char *)img;
                const unsigned char *hi = NULL;
                int srcW = (int)src_w;
                int srcH = (int)src_h;
                int srcStride = (int)src_w;
                int hiW = 0;
                int hiH = 0;
                /* Disk-backed images carry a stream offset at +8.  Inline
                 * images start with their geometry and store pixels at +4;
                 * the dword at +8 is pixel data, so match those by CRC. */
                int bMatchByCrc = !NwAssetImageRecordIsDisk(imgrec);
                uint32_t assetOffset = bMatchByCrc ? 0u
                    : *(const uint32_t *)((const char *)imgrec + 8);

                if (NwAssetGetHighResSprite(assetOffset, src, (int)src_w,
                                            (int)src_h, bMatchByCrc,
                                            &hi, &hiW, &hiH) &&
                    hi != NULL && hiW > 0 && hiH > 0) {
                    src = hi;
                    srcW = hiW;
                    srcH = hiH;
                    srcStride = hiW;
                }
                nw_blit_sprite_scaled(src, srcStride, srcW, srcH,
                                      (int)src_w, (int)src_h, flags,
                                      (int)scene_x, (int)scene_y,
                                      (int)v_left, (int)v_right,
                                      (int)v_top, (int)v_bot);
                continue;
            }
#endif

            nw_rowCopyByFlagsBits[(flags >> 3) & 3](
                nDstRowLimit, nDstColLimit, nDstFirstRow, nDstFirstCol,
                src_h, src_w, nSrcFirstRow, nSrcFirstCol, (int)(intptr_t)img);
        }

        BlitRectOrTileGridToPlayerSurface((char)g_blit_mode_byte, v_bot, v_top, v_right, v_left);
    }

    /* ---- teardown: free node, follow chain ------------------------------ */
    FreeHeapBlock(obj);                           
    obj = next_node;                              /* next via field +0x10         */
    g_display_object = obj;                       
    if (obj != NULL) goto retry_object;           /* more nodes in chain          */

    FlushGdiDrawingBatch();                       
    g_display_object = NULL;                      
    g_blit_mode_byte = 0;                         

    return 1;                                     /* AL = 1 on all paths          */
}

int CopySpriteRowsToDisplayBuffer(void)
{
#ifdef NW_PLATFORM_SDL
    uint64_t start = SDL_GetTicksNS();
    int result = NwCopySpriteRowsToDisplayBufferImpl();
    g_nwBlitNs += SDL_GetTicksNS() - start;
    g_nwBlitCalls++;
    return result;
#else
    return NwCopySpriteRowsToDisplayBufferImpl();
#endif
}

/*
 * Redraw the current screen through the selected asset source.
 *
 * A static screen normally has an empty extent list: the game only draws the
 * rectangles that changed, and the idle tick calls CopySpriteRows with a NULL
 * head.  Queue one full-frame extent and consume it immediately so an asset
 * switch is visible without waiting for the game to repaint something.  The
 * list is the renderer's normal input, and the sprite list and background
 * remain the current frame's data, so this does not alter game state.
 */
int NwBlitRedrawFullFrame(void)
{
    /* nw_portableize.py turns a local "extern int g_ScreenPitch" into a
     * function-local typedef that does not survive to the next function.
     * Read the recovered surface fields directly so this unit is independent
     * of that transform, matching the VM's 32-bit layout. */
    const int32_t screenPitch =
        *(const int32_t *)(const void *)(nw_data_window + 0x1ccc);
    const int32_t screenRows =
        *(const int32_t *)(const void *)(nw_data_window + 0x1cd0);

    if (g_pColorTableBlock == NULL ||
        screenPitch <= 0 || screenRows <= 0 ||
        g_view_clamp_a <= 0 || g_view_clamp_b <= 0)
        return 0;

    AddRectToExtentList((int)g_view_clamp_b, (int)g_view_clamp_a, 0, 0);
    return CopySpriteRowsToDisplayBuffer();
}
#endif /* NW_UNIT_CopySpriteRowsToDisplayBuffer */

/* ==========================================================================
 * Unit: FillActiveSurfaceWithReplicatedIndexDword
 * Source: video/FillActiveSurfaceWithReplicatedIndexDword.c
 * ========================================================================== */
#ifdef NW_UNIT_FillActiveSurfaceWithReplicatedIndexDword
/*
 * Fills the whole active framebuffer with one palette index, replicated into all four bytes of every dword, for (pitch * height) / 4 dwords
 *
 * Module: video
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#video-fillactivesurfacewithreplicatedindexdword-c
 */

#include <windows.h>
#include <stdint.h>
extern unsigned char nw_data_window[];

#define g_dwSurfaceHeight  (*(DWORD *)(nw_data_window + 0x1cd0))

#define g_dwSurfaceWidth   (*(DWORD *)(nw_data_window + 0x1ccc))

#define g_pSurfaceBits    NW_WINDOW_PTR_REF(void, 0x1cc8)

void __stdcall FillActiveSurfaceWithReplicatedIndexDword(BYTE bIndex)
{

    DWORD dwPixelDword = ((DWORD)(UINT8)bIndex) * 0x01010101u;

    /* width * height as a 32-bit product, then a LOGICAL >> 2. */
    DWORD dwDwordCount = (g_dwSurfaceWidth * g_dwSurfaceHeight) >> 2;

    BYTE *pDest = (BYTE *)(intptr_t)g_pSurfaceBits;

    DWORD *pdwDest = (DWORD *)(intptr_t)pDest;
    DWORD i;
    for (i = 0; i < dwDwordCount; i++) {
        *pdwDest = dwPixelDword;
        pdwDest++;
    }
}
#endif /* NW_UNIT_FillActiveSurfaceWithReplicatedIndexDword */

/* ==========================================================================
 * Unit: FlushGdiDrawingBatch
 * Source: video/FlushGdiDrawingBatch.c
 * ========================================================================== */
#ifdef NW_UNIT_FlushGdiDrawingBatch
/*
 * Flush Gdi Drawing Batch.
 *
 * Module: video
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#video-flushgdidrawingbatch-c
 */

#include <windows.h>

void FlushGdiDrawingBatch(void)
{
    GdiFlush();
}
#endif /* NW_UNIT_FlushGdiDrawingBatch */
