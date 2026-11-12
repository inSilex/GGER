/*
 * TextRender -- text row rendering into the display buffer.
 *
 * Module: video
 * Interface: include/TextRender.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   CopyTextRowsForwardSkipNulToDisplayBuffer                  video/CopyTextRowsForwardSkipNulToDisplayBuffer.c
 *   CopyTextRowsReverseSkipNulToDisplayBuffer                  video/CopyTextRowsReverseSkipNulToDisplayBuffer.c
 *   CopyTextRowsToDisplayBuffer                                video/CopyTextRowsToDisplayBuffer.c
 *   CopyTextRowsUnrolledSkipNulToDisplayBuffer                 video/CopyTextRowsUnrolledSkipNulToDisplayBuffer.c
 */

/* ==========================================================================
 * Unit: CopyTextRowsForwardSkipNulToDisplayBuffer
 * Source: video/CopyTextRowsForwardSkipNulToDisplayBuffer.c
 * ========================================================================== */
#ifdef NW_UNIT_CopyTextRowsForwardSkipNulToDisplayBuffer
/*
 * Copy Text Rows Forward Skip Nul To Display Buffer.
 *
 * Module: video
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#video-copytextrowsforwardskipnultodisplaybuffer-c
 */

#include <windows.h>
#include "nwglobals.h"
#include <stdint.h>
#include <stddef.h>

extern int g_ScreenPitch;    
extern int g_ScreenRows;     

void __stdcall CopyTextRowsForwardSkipNulToDisplayBuffer(
    int nDstRowLimit,   /* arg1 [EBP+0x08] - bottom destination row of the block       */
    int nDstColLimit,   /* arg2 [EBP+0x0c] - right edge of the destination block     */
    int nDstFirstRow,   /* arg3 [EBP+0x10] - top destination row of the block        */
    int nDstFirstCol,   /* arg4 [EBP+0x14] - left edge of the destination block       */
    int nSrcRowLimit,   
    int nSrcStride,     /* arg6 [EBP+0x1c] - bytes per SOURCE row                    */
    int nNegSrcFirstRow,/* arg7 [EBP+0x20] - ARRIVES NEGATED (0 - this) at the call  */
    int nSrcFirstCol,   /* arg8 [EBP+0x24] - source column origin term, added        */
    int nSrcBase)       /* arg9 [EBP+0x28] - source surface base address             */
{
    unsigned char *pDst;
    unsigned char *pSrc;
    int   nCols;          /* BYTES handled per destination row - the block's width  */
    int   nRows;          /* rows to walk; the loop is a do-while, see below       */
    int   nBudget;        /* remaining source bytes for the current row            */
    unsigned char ucChar;

    (void)nSrcRowLimit; 

    nCols = nDstColLimit - nDstFirstCol;
    nRows = nDstRowLimit - nDstFirstRow;

    g_ScratchBudget = nCols;

    pDst = (unsigned char *)(intptr_t)(g_ScreenBase
             + ((g_ScreenRows - 1 - nDstFirstRow) * g_ScreenPitch)
             + nDstFirstCol);

    pSrc = (unsigned char *)nw_guest_to_host(
             (uint32_t)((intptr_t)(uint32_t)nSrcBase
                        + (intptr_t)nSrcFirstCol
                        + (intptr_t)(1 - nNegSrcFirstRow) *
                              (intptr_t)nSrcStride
                        - 1));

    do {
        nBudget = g_ScratchBudget;

        while (nBudget != 0) {
            ucChar = (unsigned char)pSrc[0];   
            pSrc--;
            if (ucChar != 0) {
                *pDst = (unsigned char)ucChar;
            }
            pDst++;
            nBudget--;
        }

        pDst -= (ptrdiff_t)((intptr_t)g_ScreenPitch + nCols);
        pSrc += (ptrdiff_t)((intptr_t)nSrcStride + nCols);
        nRows--;
    } while (nRows > 0);
}
#endif /* NW_UNIT_CopyTextRowsForwardSkipNulToDisplayBuffer */

/* ==========================================================================
 * Unit: CopyTextRowsReverseSkipNulToDisplayBuffer
 * Source: video/CopyTextRowsReverseSkipNulToDisplayBuffer.c
 * ========================================================================== */
#ifdef NW_UNIT_CopyTextRowsReverseSkipNulToDisplayBuffer
/*
 * Copy Text Rows Reverse Skip Nul To Display Buffer.
 *
 * Module: video
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#video-copytextrowsreverseskipnultodisplaybuffer-c
 */

#include <windows.h>
#include "nwglobals.h"
#include <stdint.h>

extern int g_ScreenPitch;    
extern int g_ScreenRows;     

void __stdcall CopyTextRowsReverseSkipNulToDisplayBuffer(
    int nDstRowLimit,   /* arg1 [EBP+0x08] - last destination row of the block      */
    int nDstColLimit,   /* arg2 [EBP+0x0c] - right edge of the destination block    */
    int nDstFirstRow,   /* arg3 [EBP+0x10] - top row of the destination block       */
    int nDstFirstCol,   /* arg4 [EBP+0x14] - left edge of the destination block    */
    int nSrcRowLimit,   /* arg5 [EBP+0x18] - source row LIMIT; this variant's
                         *   distinguishing argument - see the source-origin note   */
    int nSrcStride,     /* arg6 [EBP+0x1c] - bytes per SOURCE row (row pitch)      */
    int nSrcFirstRow,   /* arg7 [EBP+0x20] - first source row                       */
    int nSrcFirstCol,   /* arg8 [EBP+0x24] - first source column                    */
    int nSrcBase)       /* arg9 [EBP+0x28] - source surface base address          */
{
    unsigned char *pDst;
    unsigned char *pSrc;
    int   nCols;      /* BYTES consumed per destination row (not just stored)  */
    int   nRows;      /* rows to walk; the outer loop is a do-while, see below   */
    int   nBudget;    /* remaining source bytes for the current row             */

    nCols = nDstColLimit - nDstFirstCol;
    nRows = nDstRowLimit - nDstFirstRow;

    g_ScratchBudget = nCols;

    pDst = (unsigned char *)(intptr_t)(g_ScreenBase
             + ((g_ScreenRows - 1 - nDstFirstRow) * g_ScreenPitch)
             + nDstFirstCol);

    pSrc = (unsigned char *)nw_guest_to_host(
             (uint32_t)((intptr_t)(nSrcRowLimit - 1 + nSrcFirstRow) *
                              (intptr_t)nSrcStride
                        + (intptr_t)nSrcStride - 1
                        + (intptr_t)nSrcFirstCol
                        + (intptr_t)(uint32_t)nSrcBase));

    do {
        nBudget = g_ScratchBudget;

        do {
            unsigned char bCell = *pSrc;
            pSrc--;
            if (bCell != 0) {
                *pDst = bCell;
                pDst++;
            } else {
                pDst++;
            }
            nBudget--;
        } while (nBudget != 0);

        pDst -= (g_ScreenPitch + nCols);
        pSrc -= (nSrcStride - nCols);
        nRows--;
    } while (nRows > 0);
}
#endif /* NW_UNIT_CopyTextRowsReverseSkipNulToDisplayBuffer */

/* ==========================================================================
 * Unit: CopyTextRowsToDisplayBuffer
 * Source: video/CopyTextRowsToDisplayBuffer.c
 * ========================================================================== */
#ifdef NW_UNIT_CopyTextRowsToDisplayBuffer
/*
 * Copies rows of text from a source character grid into the bottom-up display framebuffer, skipping NUL padding and stopping at the per-row column limit
 *
 * Module: video
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#video-copytextrowstodisplaybuffer-c
 */

#include <stddef.h>
#include "nwglobals.h"

extern int g_ScreenPitch;    
extern int g_ScreenRows;     

/* RE T 0x24: callee-clean, 9 x 4 bytes of arguments. */
void __stdcall CopyTextRowsToDisplayBuffer(int nDstRowLimit, int nDstColLimit,
                                           int nDstFirstRow, int nDstFirstCol,
                                           int nSrcRowLimit, int nSrcStride,
                                           int nSrcFirstRow, int nSrcFirstCol,
                                           int nSrcBase)
{
    char *pcDst;
    char *pcSrc;
    int   nCols;      /* characters handled per destination row   */
    int   nRows;      /* destination rows to walk, bottom-up      */
    int   nBudget;    /* remaining source bytes for the current row */
    int   iChar;

    nCols = nDstColLimit - nDstFirstCol;
    nRows = nDstRowLimit - nDstFirstRow;

    g_ScratchBudget = nCols;

    pcDst = (char *)(intptr_t)g_ScreenBase
          + ((g_ScreenRows - 1 - nDstFirstRow) * g_ScreenPitch)
          + nDstFirstCol;

    pcSrc = (char *)nw_guest_to_host(
                (uint32_t)((intptr_t)(nSrcRowLimit - 1 + nSrcFirstRow) *
                                  (intptr_t)nSrcStride
                           + (intptr_t)(uint32_t)nSrcBase
                           - (intptr_t)nSrcFirstCol));

    while (nRows > 0) {
        nBudget = g_ScratchBudget;

        /* One source byte is consumed per iteration, whatever its value. A non-NUL
         * byte is stored and advances the destination; a NUL is the padding that
         * terminates/pads the source row, so the destination slot is merely
         * stepped over and left holding whatever was already there. */
        while (nBudget != 0) {
            iChar = (int)(unsigned char)*pcSrc;
            pcSrc++;
            if (iChar != 0) {
                *pcDst = (char)iChar;
                pcDst++;
            } else {
                pcDst++;
            }
            nBudget--;
        }

        pcDst -= (g_ScreenPitch + nCols);
        pcSrc -= (nSrcStride + nCols);
        nRows--;
    }
}
#endif /* NW_UNIT_CopyTextRowsToDisplayBuffer */

/* ==========================================================================
 * Unit: CopyTextRowsUnrolledSkipNulToDisplayBuffer
 * Source: video/CopyTextRowsUnrolledSkipNulToDisplayBuffer.c
 * ========================================================================== */
#ifdef NW_UNIT_CopyTextRowsUnrolledSkipNulToDisplayBuffer
/*
 * Copies non-NUL text-grid bytes into the framebuffer.
 *
 * Module: video
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#video-copytextrowsunrolledskipnultodisplaybuffer-c
 */

#include <stdint.h>
#include "nwglobals.h"

extern int g_ScreenPitch;    
extern int g_ScreenRows;     

void __stdcall CopyTextRowsUnrolledSkipNulToDisplayBuffer(
    int nDstRowLimit,   /* arg1 [EBP+0x08] - destination row count to walk        */
    int nDstColLimit,   /* arg2 [EBP+0x0c] - right edge of the destination block    */
    int nDstFirstRow,   /* arg3 [EBP+0x10] - top row of the destination block       */
    int nDstFirstCol,   /* arg4 [EBP+0x14] - left edge of the destination block    */
    int nSrcRowLimit,   /* arg5 [EBP+0x18] - UNUSED in this variant                 */
    int nSrcStride,     /* arg6 [EBP+0x1c] - bytes per SOURCE row (row pitch)      */
    int nSrcFirstRow,   /* arg7 [EBP+0x20] - first source row, counted from nSrcBase backwards */
    int nSrcFirstCol,   /* arg8 [EBP+0x24] - left edge of the source block         */
    int nSrcBase)       /* arg9 [EBP+0x28] - source grid base address               */
{
    char *pcDst;
    char *pcSrc;
    int   nCols;        /* bytes handled per destination row                     */
    int   nRows;        /* destination rows to walk, bottom-up                   */
    int   nBudget;      /* remaining source bytes for the current row            */
    unsigned char ucChar;

    (void)nSrcRowLimit; 

    nCols = nDstColLimit - nDstFirstCol;
    nRows = nDstRowLimit - nDstFirstRow;

    g_ScratchBudget = nCols;

    pcDst = (char *)(intptr_t)(g_ScreenBase
                                + ((g_ScreenRows - 1 - nDstFirstRow) * g_ScreenPitch)
                                + nDstFirstCol);

    pcSrc = (char *)nw_guest_to_host(
                (uint32_t)((intptr_t)(-(nSrcFirstRow)) *
                                  (intptr_t)nSrcStride
                           - (intptr_t)nSrcFirstCol
                           + (intptr_t)(uint32_t)nSrcBase));

    while (nRows > 0) {
        nBudget = g_ScratchBudget;

        while (nBudget != 0) {
            ucChar = (unsigned char)*pcSrc;
            pcSrc++;
            if (ucChar != 0) {
                *pcDst = (char)ucChar;
            }
            pcDst++;
            nBudget--;
        }

        pcSrc += (nSrcStride - nCols);
        pcDst -= (g_ScreenPitch + nCols);
        nRows--;
    }
}
#endif /* NW_UNIT_CopyTextRowsUnrolledSkipNulToDisplayBuffer */
