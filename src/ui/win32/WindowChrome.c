#include "nwarch.h"
/*
 * WindowChrome -- player window creation, help display, and repaint.
 *
 * Module: ui/win32
 * Interface: include/WindowChrome.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   CreatePlayerMainWindow                                     ui/win32/CreatePlayerMainWindow.c
 *   DispatchWinHelpByPrefix                                    ui/win32/DispatchWinHelpByPrefix.c
 *   DisplayHelpAndDestroyMainWindow                            ui/win32/DisplayHelpAndDestroyMainWindow.c
 *   ScrollCachedMainWindowByDeltaAndRepaint                    ui/win32/ScrollCachedMainWindowByDeltaAndRepaint.c
 *   ShowMainWindowErrorBoxAfterBeep                            ui/win32/ShowMainWindowErrorBoxAfterBeep.c
 */

/* ==========================================================================
 * Unit: CreatePlayerMainWindow
 * Source: ui/win32/CreatePlayerMainWindow.c
 * ========================================================================== */
#ifdef NW_UNIT_CreatePlayerMainWindow
/*
 * Registers and creates the player window and caches its handle and DC.
 *
 * Module: ui/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#ui-win32-createplayermainwindow-c
 */

#include <windows.h>
extern unsigned char nw_data_window[];
#include <limits.h>

#define GSM_IDX_SCREEN_W      0     /* SM_CXSCREEN      */
#define GSM_IDX_SCREEN_H      1     /* SM_CYSCREEN      */
#define GSM_IDX_CAPTION       4     /* SM_CYCAPTION     */
#define GSM_IDX_FULLSCREEN_H  5     /* SM_CYFULLSCREEN  */
#define GSM_IDX_IDX6          6     /* used as the icon size -- see note above */
#define GSM_IDX_MAXIMIZED_W   0x20  /* SM_CXMAXIMIZED   */
#define GSM_IDX_MAXIMIZED_H   0x21  /* SM_CYMAXIMIZED   */

typedef struct tagPlayerConfig {
    BYTE  bUnknown00[0x0C];   /* +0x00 .. +0x0B  never read */
    int   nClientWidth;       /* +0x0C  requested client width  in pixels */
    int   nClientHeight;      /* +0x10  requested client height in pixels */
    BYTE  bUnknown14[0x10];   /* +0x14 .. +0x23  never read */
    BYTE  bThickFrameMode;    /* +0x24  0 = windowed, non-zero = thick frame */
    BYTE  bUnknown25[0x53];   /* +0x25 .. +0x77  never read */
    char  szWindowTitle[1];   /* +0x78  window title, NUL-terminated */
} PLAYERCONFIG, *PPLAYERCONFIG;

extern LRESULT CALLBACK GmwPlayerWndProc(HWND hWnd, UINT uMsg,
                                                  WPARAM wParam, LPARAM lParam);

extern HCURSOR GetActiveArrowCursor(void);

#define g_dwClientW      (*(DWORD *)(nw_data_window + 0x5f20))   /* effective client width      */
#define g_dwClientH      (*(DWORD *)(nw_data_window + 0x5f24))   /* effective client height     */
#define g_dwClientWSave  (*(DWORD *)(nw_data_window + 0x5f28))   /* pre-clamp copy of the width */
#define g_dwClientHSave  (*(DWORD *)(nw_data_window + 0x5f2c))   /* pre-clamp copy of height    */
#define g_dwFrameBorderH (*(DWORD *)(nw_data_window + 0x5f38))   /* 2x horizontal frame         */
#define g_dwFrameBorderV (*(DWORD *)(nw_data_window + 0x5f3c))   /* 2x vertical frame           */
#define g_dwScreenW      (*(DWORD *)(nw_data_window + 0x5f30))   /* SM_CXSCREEN                 */
#define g_dwScreenH      (*(DWORD *)(nw_data_window + 0x5f34))   /* SM_CYSCREEN                 */
#define g_cfgWidthSave   (*(DWORD *)(nw_data_window + 0x69c))   /* third copy of the width     */
#define g_cfgHeightSave  (*(DWORD *)(nw_data_window + 0x6a0))   /* third copy of the height    */
#define g_hInstance      (*(HINSTANCE *)(nw_data_window + 0x6400))
#define g_hWndPlayer     NW_WINDOW_PTR_REF(HWND, 0x6404)    /* the created player window   */
#define g_hdcPlayer      NW_WINDOW_PTR_REF(HDC, 0x5f00)     /* GetDC() of the player window */

/* Class name at (nw_data_window + 0x92c), verified by reading the bytes back from the image. */
static const char k_szPlayerClass[] = "GMW_PLAYER";

void CreatePlayerMainWindow(PPLAYERCONFIG pPlayerCfg)
{
    WNDCLASSA wc;
    DWORD      dwStyle;
    int        nX;
    int        nY;
    int        nSeedH;   /* the metric the horizontal border is doubled from */
    int        nSeedV;   /* the metric added to the vertical border         */

    /* Publish the requested client size. Three globals per axis, because the
     * painting and layout code read them from different places. */
    g_dwClientW     = (DWORD)pPlayerCfg->nClientWidth;
    g_dwClientH     = (DWORD)pPlayerCfg->nClientHeight;
    g_dwClientWSave = (DWORD)pPlayerCfg->nClientWidth;
    g_dwClientHSave = (DWORD)pPlayerCfg->nClientHeight;
    g_cfgWidthSave  = (DWORD)pPlayerCfg->nClientWidth;
    g_cfgHeightSave = (DWORD)pPlayerCfg->nClientHeight;

    g_dwScreenW = (DWORD)GetSystemMetrics(GSM_IDX_SCREEN_W);
    g_dwScreenH = (DWORD)GetSystemMetrics(GSM_IDX_SCREEN_H);

    /* Sentinel meaning "no explicit origin"; handed straight to CreateWindowExA. */
    nX = INT_MIN;
    nY = INT_MIN;

    g_dwFrameBorderH = 0;
    g_dwFrameBorderV = (DWORD)(GetSystemMetrics(GSM_IDX_CAPTION)
                               - GetSystemMetrics(GSM_IDX_IDX6));

    if (pPlayerCfg->bThickFrameMode == 0) {
        /* ---- windowed mode ---- */
        nSeedH = GetSystemMetrics(GSM_IDX_FULLSCREEN_H);
        g_dwFrameBorderH = (DWORD)(nSeedH * 2);

        nSeedV = GetSystemMetrics(GSM_IDX_IDX6);
        g_dwFrameBorderV += (DWORD)(nSeedV * 2);

        /* Shift the frame off-screen only for an exactly screen-filling client
         * area; a smaller one is left for Windows to place. */
        if (g_dwScreenW == g_dwClientW && g_dwScreenH == g_dwClientH) {
            nX = -(int)(g_dwFrameBorderH - (DWORD)nSeedH);
            nY = -(int)(g_dwFrameBorderV - (DWORD)nSeedV);
        }

        dwStyle = 0x020A0000u;  /* WS_BORDER|WS_VSCROLL|WS_CLIPSIBLINGS|WS_CLIPCHILDREN */
    } else {
        /* ---- thick-frame mode ---- */
        nSeedH = GetSystemMetrics(GSM_IDX_MAXIMIZED_W);
        g_dwFrameBorderH = (DWORD)(nSeedH * 2);

        nSeedV = GetSystemMetrics(GSM_IDX_MAXIMIZED_H);
        g_dwFrameBorderV += (DWORD)(nSeedV * 2);

        if ((int)g_dwScreenW <= (int)g_dwClientW) {
            g_dwClientW = g_dwScreenW;
            nX = -(int)(g_dwFrameBorderH - (DWORD)nSeedH);
        }

        if ((int)g_dwScreenH <= (int)g_dwClientH) {
            g_dwClientH = g_dwScreenH;
            nY = -(int)(g_dwFrameBorderV - (DWORD)nSeedV);
        }

        dwStyle = 0x02CF0000u;  /* WS_OVERLAPPEDWINDOW|WS_CLIPSIBLINGS|WS_CLIPCHILDREN */
    }

    ZeroMemory(&wc, sizeof(wc));
    wc.style         = 0x1020u;   /* CS_OWNDC (0x0020) | CS_DBLCLKS (0x1000) */
    wc.lpfnWndProc   = GmwPlayerWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = g_hInstance;
    wc.hIcon         = LoadIconA(g_hInstance, MAKEINTRESOURCEA(0x64));  /* icon id 100 */
    wc.hCursor       = GetActiveArrowCursor();
    wc.hbrBackground = GetStockObject(BLACK_BRUSH);                    /* literal PUSH 4 */
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = k_szPlayerClass;
    RegisterClassA(&wc);

    g_hWndPlayer = CreateWindowExA(0,
                                   k_szPlayerClass,
                                   pPlayerCfg->szWindowTitle,
                                   dwStyle,
                                   nX,
                                   nY,
                                   (int)(g_dwClientW + g_dwFrameBorderH),
                                   (int)(g_dwClientH + g_dwFrameBorderV),
                                   NULL,
                                   NULL,
                                   g_hInstance,
                                   NULL);

    g_hdcPlayer = GetDC(g_hWndPlayer);
}
#endif /* NW_UNIT_CreatePlayerMainWindow */

/* ==========================================================================
 * Unit: DispatchWinHelpByPrefix
 * Source: ui/win32/DispatchWinHelpByPrefix.c
 * ========================================================================== */
#ifdef NW_UNIT_DispatchWinHelpByPrefix
/*
 * Interprets the first character of a help identifier and forwards it to WinHelpA with the main window and the cached .hlp path
 *
 * Module: ui/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#ui-win32-dispatchwinhelpbyprefix-c
 */

#include <windows.h>
#include <stdint.h>

#define NW_DIAG __attribute__((section(".nwdiag")))

extern unsigned char nw_data_window[] __asm__("nw_data_window");
extern int ParseSignedDecimalIntegerFromString(const uint8_t *psz);

#define NW32(off) (*(volatile uint32_t *)(nw_data_window + (off)))

#define W_HELP_PATH   0x5f5cu
#define W_MAIN_HWND   0x6404u
#define W_CTYPE       0xa4du

NW_DIAG uint32_t __stdcall
DispatchWinHelpByPrefix(const char *pszToken) __asm__("_DispatchWinHelpByPrefix");

NW_DIAG uint32_t __stdcall
DispatchWinHelpByPrefix(const char *pszToken)
{
    const uint8_t *p = (const uint8_t *)pszToken;
    UINT uCommand = 0x105u;                  /* WH_CONTINUE */
    ULONG_PTR dwData = (ULONG_PTR)(uintptr_t)pszToken;

    if (p[0] == (uint8_t)'!' && p[1] == 0u) {
        dwData = 0;
        if ((GetVersion() & 0x80000000u) != 0u) {
            uCommand = 0x003u;               /* WH_CONTEXT: Win9x */
        } else {
            uCommand = 0x00bu;               /* WH_HELPINDEX: NT */
        }
    } else if (p[0] == (uint8_t)'?' && p[1] == 0u) {
        dwData = 0;
        uCommand = 0x003u;                   /* WH_CONTEXT */
    } else {

        uint8_t flags = *(const uint8_t *)((uintptr_t)nw_data_window + W_CTYPE +
                                           (intptr_t)(int8_t)p[0]);
        if ((flags & 0x04u) != 0u) {
            dwData = (ULONG_PTR)(uintptr_t)ParseSignedDecimalIntegerFromString(p);
            uCommand = 0x001u;               /* WH_HELP */
        }
    }

    return (uint32_t)WinHelpA((HWND)(uintptr_t)NW32(W_MAIN_HWND),
                              (LPCSTR)(nw_data_window + W_HELP_PATH),
                              uCommand, dwData);
}
#endif /* NW_UNIT_DispatchWinHelpByPrefix */

/* ==========================================================================
 * Unit: DisplayHelpAndDestroyMainWindow
 * Source: ui/win32/DisplayHelpAndDestroyMainWindow.c
 * ========================================================================== */
#ifdef NW_UNIT_DisplayHelpAndDestroyMainWindow
/*
 * Invokes WinHelp on the cached help window with the WH_HELP help-file path and command, then destroys that window.
 *
 * Module: ui/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#ui-win32-displayhelpanddestroymainwindow-c
 */

#include <windows.h>
#include "nwglobals.h"   /* supplies HWND and the WinHelpA prototype (winuser.h) */

#ifndef WH_HELP
#define WH_HELP 2
#endif

extern char g_pszHelpFilePath[];        

/* Shows help through the cached help window and then tears that window down.

   WinHelpA(hwnd, file, WH_HELP, 0) opens the help file for the given window;
   WH_HELP_CONTEXT-independent command 2 (WH_HELP) takes no data. The window is
   destroyed unconditionally afterwards, so a live help window never survives
   the call. Does nothing at all when no help window has been cached. */
void DisplayHelpAndDestroyMainWindow(void);
void DisplayHelpAndDestroyMainWindow(void)
{
    if (g_hwndMainWindow != NULL) {
        WinHelpA(g_hwndMainWindow, g_pszHelpFilePath, WH_HELP, 0);
        DestroyWindow(g_hwndMainWindow);
    }
}
#endif /* NW_UNIT_DisplayHelpAndDestroyMainWindow */

/* ==========================================================================
 * Unit: ScrollCachedMainWindowByDeltaAndRepaint
 * Source: ui/win32/ScrollCachedMainWindowByDeltaAndRepaint.c
 * ========================================================================== */
#ifdef NW_UNIT_ScrollCachedMainWindowByDeltaAndRepaint
/*
 * Scroll Cached Main Window By Delta And Repaint.
 *
 * Module: ui/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#ui-win32-scrollcachedmainwindowbydeltaandrepaint-c
 */

#include <windows.h>
#include "nwglobals.h"   /* ScrollWindow, UpdateWindow, HWND, NULL */
#include <stdint.h>
extern unsigned char nw_data_window[];    /* uintptr_t -- keep (nw_data_window + 0x6404) an integer until the cast */

#define g_hwndMainWindow  NW_WINDOW_PTR_REF(HWND, 0x6404)

/* Scrolls the cached main window's client area by the two deltas and forces an
   immediate repaint.  The stack offsets the two deltas are read from are shifted
   8 bytes by the two pushed zeros, so the caller's FIRST argument (nYAmount) lands
   in ScrollWindow's THIRD parameter and its SECOND (nXAmount) in ScrollWindow's
   SECOND parameter -- the two values trade positions relative to the caller's list.
   Returns 1 on every path; the no-window case is silently a no-op. */
unsigned char ScrollCachedMainWindowByDeltaAndRepaint(int nYAmount, int nXAmount);

unsigned char ScrollCachedMainWindowByDeltaAndRepaint(int nYAmount, int nXAmount)
{
    if (g_hwndMainWindow != NULL) {

        ScrollWindow(g_hwndMainWindow, nXAmount, nYAmount, NULL, NULL);

        UpdateWindow(g_hwndMainWindow);
    }

    return 1u;
}
#endif /* NW_UNIT_ScrollCachedMainWindowByDeltaAndRepaint */

/* ==========================================================================
 * Unit: ShowMainWindowErrorBoxAfterBeep
 * Source: ui/win32/ShowMainWindowErrorBoxAfterBeep.c
 * ========================================================================== */
#ifdef NW_UNIT_ShowMainWindowErrorBoxAfterBeep
/*
 * Plays the system error beep, then shows an application-modal message box (MB_ICONHAND/stop icon, default MB_OK button) owned by the game's main
 *
 * Module: ui/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#ui-win32-showmainwindowerrorboxafterbeep-c
 */

#include <windows.h>
#include "nwglobals.h"   /* HWND, LPCSTR, MessageBeep, MessageBoxA, MB_ICONHAND (winuser.h) */
#include <stdint.h>

/* Top-level main window handle. Written by CreatePlayerMainWindow from the
   CreateWindowExA result; the box uses it purely as the owner, never destroyed
   here (unlike the sibling, which DestroyWindow's it). */

void ShowMainWindowErrorBoxAfterBeep(LPCSTR lpszText, LPCSTR lpszCaption);

void ShowMainWindowErrorBoxAfterBeep(LPCSTR lpszText, LPCSTR lpszCaption)
{
    MessageBeep(MB_ICONHAND);
    MessageBoxA(g_hwndMainWindow, lpszText, lpszCaption, MB_ICONHAND);
}
#endif /* NW_UNIT_ShowMainWindowErrorBoxAfterBeep */
