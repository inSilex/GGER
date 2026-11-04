#include "nwarch.h"
/*
 * VideoGdi -- Win32 DIB and GDI blit helpers.
 *
 * Module: video/win32
 * Interface: include/VideoGdi.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   BlitRegionFromDcOrDib                                      video/win32/BlitRegionFromDcOrDib.c
 *   CreatePlayerDIBBackBuffer                                  video/win32/CreatePlayerDIBBackBuffer.c
 *   SetLogicalPaletteDrawGrid                                  video/win32/SetLogicalPaletteDrawGrid.c
 */

/* ==========================================================================
 * Unit: BlitRegionFromDcOrDib
 * Source: video/win32/BlitRegionFromDcOrDib.c
 * ========================================================================== */
#ifdef NW_UNIT_BlitRegionFromDcOrDib
/*
 * Copies the w*h block at (nX,nY) to the same offset on the global destination surface: BitBlt from the cached memory DC when one exists.
 *
 * Module: video/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#video-win32-blitregionfromdcordib-c
 */

#include <windows.h>
#include "nwglobals.h"


extern HDC g_hdcSrcMem;

extern BITMAPINFO g_bmiGlobal;

void BlitRegionFromDcOrDib(int nTotalHeight,
                          int nHeight,
                          int nWidth,
                          int nY,
                          int nX,
                          void *pBits)
{
    if (g_hdcSrcMem == NULL) {

        StretchDIBits(g_hdcDest,
                      nX, nY, nWidth, nHeight,                        /* dest rect */
                      nX, nTotalHeight - nY - nHeight, nWidth, nHeight, /* src rect */
                      pBits, &g_bmiGlobal, DIB_PAL_COLORS, SRCCOPY);   
    } else {
        BitBlt(g_hdcDest, nX, nY, nWidth, nHeight,
               g_hdcSrcMem, nX, nY, SRCCOPY);
    }
}
#endif /* NW_UNIT_BlitRegionFromDcOrDib */

/* ==========================================================================
 * Unit: CreatePlayerDIBBackBuffer
 * Source: video/win32/CreatePlayerDIBBackBuffer.c
 * ========================================================================== */
#ifdef NW_UNIT_CreatePlayerDIBBackBuffer
/*
 * Ensures the player window and its DIB back buffer exist, sizes and allocates the locked global scratch buffer.
 *
 * Module: video/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#video-win32-createplayerdibbackbuffer-c
 */

#include <windows.h>
extern unsigned char nw_data_window[];
#include <windef.h>

/* Arguments parked for later users, stored in this order by the prologue:
 *   (nw_data_window + 0x5efc) <- arg1, (nw_data_window + 0x5ef4) <- arg2, (nw_data_window + 0x5ef8) <- arg3.        */
#define g_dwArg1Save   (*(DWORD *)(nw_data_window + 0x5efc))
#define g_nArg2Save    (*(int   *)(nw_data_window + 0x5ef4))
#define g_pArg3Save    NW_WINDOW_PTR_REF(void, 0x5ef8)

#define g_hWndPlayer   (*(HWND  *)(nw_data_window + 0x6404))
#define g_hdcPlayer    (*(HDC   *)(nw_data_window + 0x5f00))

/* DIB section state. (nw_data_window + 0x6060) is the *pvBits out-parameter of
 * CreateDIBSection, i.e. the address of the DIB pixels; the function tests
 * it as "has the back buffer been created yet". (nw_data_window + 0x605c) is the HBITMAP
 * the pixels belong to, (nw_data_window + 0x6064) the memory DC they are selected into. */
#define g_hBitmapDIB   (*(HBITMAP  *)(nw_data_window + 0x605c))
#define g_pvDIBits     NW_WINDOW_PTR_REF(void, 0x6060)
#define g_hdcMem       (*(HDC      *)(nw_data_window + 0x6064))

#define g_bmiDIB       (*(BITMAPINFO *)(nw_data_window + 0x698))

/* Scratch buffer: (nw_data_window + 0x5f08) holds the HGLOBAL from GlobalAlloc, (nw_data_window + 0x5f0c)
 * the LPVOID from GlobalLock. Only the locked pointer is tested and used. */
#define g_hglobalScratch (*(HGLOBAL *)(nw_data_window + 0x5f08))
#define g_pvScratch     NW_WINDOW_PTR_REF(void, 0x5f0c)

typedef struct tagPlayerBufInfo {
    SIZE_T dwCount;    /* +0x00  net byte count; reduced by the header
                        *         size and by a fixed 80000 reserve          */
    void  *pvScratch;  /* +0x04  locked GlobalAlloc buffer, filled in here   */
    SIZE_T dwHeader;   /* +0x08  subtracted from dwCount, but only when the
                        *         DIB back buffer exists                    */
    void  *pvDIBits;   /* +0x0c  DIB pixel address, straight from
                        *         CreateDIBSection's pvBits out-param       */
} PLAYERBUFINFO, *PPLAYERBUFINFO;

extern void CreatePlayerMainWindow(void *pPlayerCfg);

typedef HBITMAP (WINAPI *PFN_CREATE_DIBSECTION)(HDC hdc,
                                                const BITMAPINFO *pbmi,
                                                UINT usage,
                                                void **ppvBits,
                                                HANDLE hSection,
                                                DWORD dwOffset);

/* Literal flag pushed as the first GlobalAlloc argument: GMEM_FIXED. */
#define SCRATCH_ALLOC_FLAGS  0x00000002

#define SCRATCH_RESERVE       80000

/* ------------------------------------------------------------------ *
 * Body
 * ------------------------------------------------------------------ */

BOOL WINAPI CreatePlayerDIBBackBuffer(uintptr_t dwArg1Save,
                                      void  *pPlayerCfg,
                                      PPLAYERBUFINFO pBufInfo)
{
    HMODULE hGdi32;
    FARPROC  pfnCreateDIBSection;
    BOOL     fOk;

    g_dwArg1Save = (DWORD)dwArg1Save;
    g_nArg2Save  = (int)(uintptr_t)pPlayerCfg;
    g_pArg3Save  = (void *)pBufInfo;

    /* Lazily create the player window this back buffer draws into. */
    if (g_hWndPlayer == NULL) {
        CreatePlayerMainWindow(pPlayerCfg);
    }

    /* Lazily build the 8-bit top-down DIB section, resolving CreateDIBSection
     * by hand so an ancient GDI32 simply degrades instead of crashing. */
    if (g_pvDIBits == NULL) {
        hGdi32 = GetModuleHandleA("GDI32.DLL");
        if (hGdi32 != NULL) {
            pfnCreateDIBSection = GetProcAddress(hGdi32, "CreateDIBSection");
            if (pfnCreateDIBSection != NULL) {
                g_hBitmapDIB = ((PFN_CREATE_DIBSECTION)(uintptr_t)pfnCreateDIBSection)(
                                   g_hdcPlayer,
                                   &g_bmiDIB,
                                   DIB_PAL_COLORS,   
                                   &g_pvDIBits,
                                   NULL,             /* no file mapping    */
                                   0);

                /* Only give the DIB a memory DC once it really has pixels. */
                if (g_pvDIBits != NULL) {
                    g_hdcMem = CreateCompatibleDC(g_hdcPlayer);
                    SelectObject(g_hdcMem, g_hBitmapDIB);
                }
            }
        }
    }

    /* Publish the pixel address, then net down the requested byte count:
     * drop the header allowance only when a real back buffer exists, then
     * always drop the fixed reserve. */
    pBufInfo->pvDIBits = g_pvDIBits;

    if (g_pvDIBits != NULL) {
        pBufInfo->dwCount = pBufInfo->dwCount - pBufInfo->dwHeader;
    }
    pBufInfo->dwCount = pBufInfo->dwCount - SCRATCH_RESERVE;

    /* Allocate and lock the scratch buffer once; the lock pointer doubles as
     * the "already built" flag, so the HGLOBAL is deliberately never freed. */
    if (g_pvScratch == NULL) {
        g_hglobalScratch = (HGLOBAL)GlobalAlloc(SCRATCH_ALLOC_FLAGS,
                                                (SIZE_T)pBufInfo->dwCount);
        g_pvScratch     = GlobalLock(g_hglobalScratch);
    }

    pBufInfo->pvScratch = g_pvScratch;

    /* Return value is the standard "pointer != NULL" identity, built from
     * NEG/SBB/NEG so the true result is exactly 1 or 0 in EAX. */
    fOk = (g_pvScratch != NULL);
    return fOk;
}
#endif /* NW_UNIT_CreatePlayerDIBBackBuffer */

/* ==========================================================================
 * Unit: SetLogicalPaletteDrawGrid
 * Source: video/win32/SetLogicalPaletteDrawGrid.c
 * ========================================================================== */
#ifdef NW_UNIT_SetLogicalPaletteDrawGrid
/*
 * Rebuilds the application logical palette from an RGBTRIPLE table and repaints the grid area on the memory DC
 *
 * Module: video/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#video-win32-setlogicalpalettedrawgrid-c
 */

#include <windows.h>

extern HDC      g_hdcMem;         
extern HPALETTE g_hpalPalette;    
extern int      g_cxGridClient;   
extern int      g_cyGridClient;   
extern HDC      g_hdcDib;         
extern HWND     g_hwndMain;       

extern COLORREF g_rgcrGridPenColors[];

/* Grid geometry: lines are stepped 8 pixels and the whole loop is repeated once
   per 8-pixel phase offset, so passes 0..7 together cover every phase. */
#define GRID_STEP       8
#define GRID_PHASES     8
#define SYSTEM_COLORS   10
#define HC_HIGH_COLORS  0xF6       /* GetSystemPaletteEntries start index for the high colours */

typedef struct {
    WORD         palVersion;
    WORD         palNumEntries;
    PALETTEENTRY palPalEntry[256];
} NwPalette256;

BOOL WINAPI SetLogicalPaletteDrawGrid(BOOL bUseSystemColors,
                                      char cGridPenIndex,
                                      UINT uNumPaletteEntries,
                                      const RGBTRIPLE *pRgbEntries)
{
    NwPalette256  lgpl;            /* 0x404 bytes -- see NwPalette256 above  */
    PALETTEENTRY *pEntry;
    RECT          rcBounds;
    HPEN          hpenGrid;
    HGDIOBJ       hdiOldObject;
    HBRUSH        hbrBackground;
    BYTE          bTmp;
    int           i;
    int           iPhase;
    int           iCoord;

    /* If the grid has no area yet, only the palette is rebuilt. */
    if (g_cxGridClient != 0 && g_cyGridClient != 0) {

        if (cGridPenIndex == 0) {

            rcBounds.left   = 0;
            rcBounds.top    = 0;
            rcBounds.right  = g_cxGridClient;
            rcBounds.bottom = g_cyGridClient;
            hbrBackground = GetStockObject(BLACK_BRUSH);
            FillRect(g_hdcMem, &rcBounds, hbrBackground);

        } else {
            /* Grid pass: draw the lines in the requested pen colour. */
            hpenGrid = CreatePen(PS_SOLID, 1,
                                 g_rgcrGridPenColors[(int)cGridPenIndex]);
            hdiOldObject = SelectObject(g_hdcMem, hpenGrid);

            for (iPhase = 0; iPhase < GRID_PHASES; iPhase++) {

                for (iCoord = iPhase;
                     iCoord < g_cxGridClient;
                     iCoord += GRID_STEP) {
                    MoveToEx(g_hdcMem, iCoord, 0, NULL);
                    LineTo(g_hdcMem, iCoord, g_cyGridClient);
                }

                for (iCoord = iPhase;
                     iCoord < g_cyGridClient;
                     iCoord += GRID_STEP) {
                    MoveToEx(g_hdcMem, 0, iCoord, NULL);
                    LineTo(g_hdcMem, g_cxGridClient, iCoord);
                }
            }

            SelectObject(g_hdcMem, hdiOldObject);
            DeleteObject(hpenGrid);
        }
    }

    /* Build the logical palette from the caller's RGB triples. */
    ZeroMemory(&lgpl, sizeof(lgpl));
    lgpl.palVersion      = 0x300;
    lgpl.palNumEntries   = (WORD)uNumPaletteEntries;

    pEntry = lgpl.palPalEntry;
    for (i = 0; i < (int)uNumPaletteEntries; i++) {

        pEntry->peRed   = pRgbEntries[i].rgbtBlue;   /* source byte 0 */
        pEntry->peGreen = pRgbEntries[i].rgbtGreen;  /* source byte 1 */
        pEntry->peBlue  = pRgbEntries[i].rgbtRed;    /* source byte 2 */
        pEntry->peFlags = PC_NOCOLLAPSE;
        pEntry++;
    }

    /* Optionally let Windows supply the two system colour blocks.  BOTH blocks go
     * into the palette being built: the second call's destination is
     *     40c34b lea 0x3e8(%esp),%eax / push %eax / push $0xa / push $0xf6
     * and 0x3e8(%esp) IS lgpl.palPalEntry[0xF6] -- the entries begin at
     * 0x10(%esp), and 0x10 + 4*0xF6 == 0x3E8.  Reading the high ten into a
     * separate local (as this file used to) left indices 246..255 holding the
     * caller's RGB triples instead of the system high colours. */
    if (bUseSystemColors) {
        GetSystemPaletteEntries(g_hdcMem, 0, SYSTEM_COLORS, lgpl.palPalEntry);
        GetSystemPaletteEntries(g_hdcMem, HC_HIGH_COLORS, SYSTEM_COLORS,
                                lgpl.palPalEntry + HC_HIGH_COLORS);
    }

    /* Replace the palette selected on the memory DC. */
    if (g_hpalPalette != NULL) {
        DeleteObject(g_hpalPalette);
    }
    g_hpalPalette = CreatePalette((const LOGPALETTE *)&lgpl);

    /* Mirror the palette into the DIB colour table (stored as RGBQUAD, so red
       and blue are swapped back before the write). */
    if (g_hdcDib != NULL) {
        pEntry = lgpl.palPalEntry;
        for (i = 0; i < (int)uNumPaletteEntries; i++) {
            bTmp            = pEntry->peRed;
            pEntry->peRed   = pEntry->peBlue;
            pEntry->peBlue  = bTmp;
            pEntry++;
        }
        SetDIBColorTable(g_hdcDib, 0, uNumPaletteEntries,
                         (const RGBQUAD *)lgpl.palPalEntry);
    }

    if (g_hwndMain != NULL) {
        SendMessageA(g_hwndMain, 0x30F, 0, 0);
    }

    return (g_hpalPalette != NULL);
}
#endif /* NW_UNIT_SetLogicalPaletteDrawGrid */
