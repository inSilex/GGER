#include "nwarch.h"
/*
 * WindowMessages -- Win32 message pumping and window-procedure dispatch.
 *
 * Module: input/win32
 * Interface: include/WindowMessages.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   DispatchQueuedMessagesAndStampStartTime                    input/win32/DispatchQueuedMessagesAndStampStartTime.c
 *   DispatchQueuedNonInputMessages                             input/win32/DispatchQueuedNonInputMessages.c
 *   GetActiveArrowCursor                                       input/win32/GetActiveArrowCursor.c
 *   HandleMainWindowMessage                                    input/win32/HandleMainWindowMessage.c
 *   ProcessMainWindowMessages                                  input/win32/ProcessMainWindowMessages.c
 */

/* ==========================================================================
 * Unit: DispatchQueuedMessagesAndStampStartTime
 * Source: input/win32/DispatchQueuedMessagesAndStampStartTime.c
 * ========================================================================== */
#ifdef NW_UNIT_DispatchQueuedMessagesAndStampStartTime
/*
 * Dispatch Queued Messages And Stamp Start Time.
 *
 * Module: input/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#input-win32-dispatchqueuedmessagesandstampstarttime-c
 */

#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>
#ifdef NW_PLATFORM_SDL
#include <SDL3/SDL_timer.h>
#include "NwSdlPcm.h"
extern int NwSdlPumpEventsForNestedPump(void);
#endif
extern unsigned char nw_data_window[];

#define GLOBAL_CMD_LATCH_BYTE (*(volatile BYTE *)(uintptr_t)(nw_data_window + 0x5f50))

#define GLOBAL_FATAL_LATCH (*(volatile DWORD *)(uintptr_t)(nw_data_window + 0x5f14))

/* Idle / clear code for the command latch. */
#define CMD_LATCH_CLEAR 0x0e

#define CMD_RETURN_QUIT 0x83

/* The non-pointer value this function stores into GLOBAL_FATAL_LATCH. */
#define FATAL_LATCH_SENTINEL 0x1u

/* Drains the thread queue (PM_REMOVE, no message filter, NULL hWnd so the whole thread queue),
 * translating and dispatching every message. If WM_QUIT is among them, latches the
 * fatal slot and returns 0x83 immediately. Otherwise, once the queue is empty, writes
 * timeGetTime() through pdwStartTime and returns the command latch after clearing it.
 * Only AL is written on both returns, hence the BYTE return type. */
BYTE __stdcall DispatchQueuedMessagesAndStampStartTime(DWORD *pdwStartTime)
{
    MSG  stMsg;
    BYTE bCommand;

    /* If the caller's slot is already set, leave the command latch alone; if it is still
     * zero, mark the latch idle before spending time in the pump. */
    if (*pdwStartTime == 0) {
        GLOBAL_CMD_LATCH_BYTE = CMD_LATCH_CLEAR;
    }

    if (PeekMessageA(&stMsg, (HWND)0, 0, 0, PM_REMOVE) != FALSE) {
        do {

            if (stMsg.message == WM_QUIT) {
                GLOBAL_FATAL_LATCH = FATAL_LATCH_SENTINEL;
                return CMD_RETURN_QUIT;
            }

            TranslateMessage(&stMsg);
            DispatchMessageA(&stMsg);
        } while (PeekMessageA(&stMsg, (HWND)0, 0, 0, PM_REMOVE) != FALSE);
    }

#ifdef NW_PLATFORM_SDL
    /* SDL owns the main window's WndProc, so its queue drain above never runs
     * the game's input handler.  Poll SDL's event queue here instead; this is
     * the only pump available while a synchronous intro/animation is running.
     * NwPcmSink_Pump replaces the 0x3BD waveOut completion message, which SDL
     * would dispatch to its own WndProc and drop. */
    if (NwSdlPumpEventsForNestedPump() != 0) {
        GLOBAL_FATAL_LATCH = FATAL_LATCH_SENTINEL;
        return CMD_RETURN_QUIT;
    }
    NwPcmSink_Pump();
#endif

    /* Queue drained. */
#ifdef NW_PLATFORM_SDL
    *pdwStartTime = (DWORD)SDL_GetTicks();
#else
    *pdwStartTime = timeGetTime();
#endif

    bCommand = GLOBAL_CMD_LATCH_BYTE;
    GLOBAL_CMD_LATCH_BYTE = CMD_LATCH_CLEAR;

    return bCommand;
}
#endif /* NW_UNIT_DispatchQueuedMessagesAndStampStartTime */

/* ==========================================================================
 * Unit: DispatchQueuedNonInputMessages
 * Source: input/win32/DispatchQueuedNonInputMessages.c
 * ========================================================================== */
#ifdef NW_UNIT_DispatchQueuedNonInputMessages
/*
 * Drains non-input messages from the thread queue.
 *
 * Module: input/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#input-win32-dispatchqueuednoninputmessages-c
 */

#include <windows.h>

void DispatchQueuedNonInputMessages(void)
{
    MSG stMsg;
    BOOL bGotMessage;

    bGotMessage = PeekMessageA(&stMsg, (HWND)0x0, 0, 0, PM_REMOVE);

    while (bGotMessage != FALSE) {

        if (stMsg.message != WM_KEYDOWN &&
            stMsg.message != WM_LBUTTONDOWN &&
            stMsg.message != WM_RBUTTONDOWN &&
            stMsg.message != WM_LBUTTONUP &&
            stMsg.message != WM_RBUTTONUP &&
            stMsg.message != WM_MOUSEMOVE) {
            TranslateMessage(&stMsg);
            DispatchMessageA(&stMsg);
        }

        bGotMessage = PeekMessageA(&stMsg, (HWND)0x0, 0, 0, PM_REMOVE);
    }
}
#endif /* NW_UNIT_DispatchQueuedNonInputMessages */

/* ==========================================================================
 * Unit: GetActiveArrowCursor
 * Source: input/win32/GetActiveArrowCursor.c
 * ========================================================================== */
#ifdef NW_UNIT_GetActiveArrowCursor
/*
 * Get Active Arrow Cursor.
 *
 * Module: input/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#input-win32-getactivearrowcursor-c
 */

#include <windows.h>
extern unsigned char nw_data_window[];

HCURSOR GetActiveArrowCursor(void)
{
    BYTE *pbyUiState;   /* global UI/control state block at (nw_data_window + 0x5ef4) */

    pbyUiState = NW_WINDOW_PTR_REF(BYTE, 0x5ef4);

    if (pbyUiState[0x25] == 0) {
        return NULL;                       /* no interactive window yet */
    }

    return LoadCursorA(NULL, MAKEINTRESOURCEA(0x7F00));  /* IDC_ARROW */
}
#endif /* NW_UNIT_GetActiveArrowCursor */

/* ==========================================================================
 * Unit: HandleMainWindowMessage
 * Source: input/win32/HandleMainWindowMessage.c
 * ========================================================================== */
#ifdef NW_UNIT_HandleMainWindowMessage
/*
 * Win32 window procedure of the application's main top-level window
 *
 * Module: input/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#input-win32-handlemainwindowmessage-c
 */

#include <windows.h>
#include "nwglobals.h"

extern void WINAPI BlitRectOrTileGridToPlayerSurface(char cTileGridMode,
                                                     int  nRectBottom,
                                                     int  nRectRight,
                                                     int  nRectTop,
                                                     int  nRectLeft);

extern void WINAPI PlayMciWavesCommandIfDeviceArmed(unsigned long uMciDeviceId,
                                                    unsigned long uCommand);

/* ---- PE globals ------------------------------------------------------- */
extern BYTE     g_suspendRequested;    
extern BYTE     g_closeRequested;      
extern BYTE     g_blitModeByte;        
extern BYTE     g_playfieldActive;     
extern BYTE     g_mouseBusyFlag;       
extern BYTE     g_childGeometryFixed;  
extern HDC      g_paletteDC;           
extern HPALETTE g_palette;             
extern WORD     g_clientWidth;         
extern WORD     g_clientHeight;        
extern LONG     g_minSizeX;            
extern LONG     g_minSizeY;            
extern LONG     g_windowOffsetX;       
extern LONG     g_windowOffsetY;       

/* Application-private window messages (0x30F/0x311 sit in the WM_APP range). */
#define WM_NW_PAINTFWD    0x030F  /* forward mouse traffic to the child    */
#define WM_NW_REFLECT     0x0311  /* child echoing a mouse message back   */
#define WM_NW_MOUSEEVENT  0x0210  /* synthetic move carrying a button code */
#define WM_NW_SUSPEND     0x03B9  /* clear the suspend-requested flag     */

#define NwArmUpdateAudio  PlayMciWavesCommandIfDeviceArmed(0u, 0x808u)

LRESULT WINAPI HandleMainWindowMessage(HWND hWnd, UINT uMsg,
                                       WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;

    switch (uMsg) {

    /* ---- 0x47 WM_WINDOWPOSCHANGED: repaint, then chain to the default --- */
    case WM_WINDOWPOSCHANGED:
        InvalidateRect(hWnd, NULL, FALSE);
        return DefWindowProcA(hWnd, uMsg, wParam, lParam);

    /* ---- 0x24 WM_GETMINMAXINFO: constrain max size and max track size --- */
    case WM_GETMINMAXINFO: {
        MINMAXINFO *pmmi = (MINMAXINFO *)lParam;
        pmmi->ptMaxSize.x      = g_minSizeX + g_windowOffsetX;
        pmmi->ptMaxTrackSize.x = g_minSizeX + g_windowOffsetX;
        pmmi->ptMaxSize.y      = g_minSizeY + g_windowOffsetY;
        pmmi->ptMaxTrackSize.y = g_minSizeY + g_windowOffsetY;
        return 0;
    }

    /* ---- 0x05 WM_SIZE: cache the new client extent --------------------- */
    case WM_SIZE:
        g_clientWidth  = (WORD)(lParam & 0xFFFF);          /* LOWORD  */
        g_clientHeight = (WORD)((lParam >> 16) & 0xFFFF);  /* HIWORD  */
        return 0;

    /* ---- 0x210 synthetic mouse event; LOWORD(wParam) is a button code --- */
    case WM_NW_MOUSEEVENT:
        if ((WORD)wParam == WM_LBUTTONDOWN ||
            (WORD)wParam == WM_RBUTTONDOWN) {
            NwArmUpdateAudio;
        }
        return 0;

    /* ---- 0x201 / 0x204 mouse button down: arm the update sound ---------- */
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        NwArmUpdateAudio;
        return 0;

    /* ---- 0x102 WM_CHAR: space / Enter reach the same tail -------------- */
    case WM_CHAR:
        if (wParam == 0x20 /* ' ' */ || wParam == 0x0D /* '\r' */) {
            NwArmUpdateAudio;
        }
        return 0;

    /* ---- 0x20 WM_SETCURSOR: arrow over the client area only ------------ */
    case WM_SETCURSOR:
        if ((WORD)lParam == HTCLIENT) {
            SetCursor(LoadCursorA(NULL, IDC_ARROW));
            return 0;
        }
        return DefWindowProcA(hWnd, uMsg, wParam, lParam);

    /* ---- 0x311 / 0x30F reflect-and-repaint mouse traffic ---------------- */
    case WM_NW_REFLECT:
        if (wParam == (WPARAM)hWnd) {
            return 0;               /* echo of a message we sent: ignore */
        }
        goto reflect_mouse_message;

    case WM_NW_PAINTFWD:
reflect_mouse_message:
        if (g_palette == NULL) {
            return 0;               /* no palette realised yet: skip */
        }
        /* drop the active flag unless the traffic came from the child */
        if (uMsg == WM_NW_PAINTFWD || wParam == (WPARAM)g_hwndChild) {
            g_playfieldActive = 0;
        }
        SendMessageA(g_hwndChild, uMsg, wParam, lParam);
        if (g_mouseBusyFlag || uMsg == WM_NW_REFLECT) {
            g_playfieldActive = 1;
        }
        SelectPalette(g_paletteDC, g_palette,
                      (uMsg == WM_NW_REFLECT) ? TRUE : FALSE);
        if (RealizePalette(g_paletteDC) == 0) {
            return 0;
        }
        InvalidateRect(hWnd, NULL, FALSE);
        return 1;

    /* ---- 0x0F WM_PAINT: one-shot child resize, then blit --------------- */
    case WM_PAINT:
        BeginPaint(hWnd, &ps);
        if (g_playfieldActive) {
            if (!g_childGeometryFixed) {
                int cx = g_childWidthDefault  ? g_childWidthDefault  : 0x140;
                int cy = g_childHeightDefault ? g_childHeightDefault : 0x0F0;
                g_childGeometryFixed = 1;
                MoveWindow(g_hwndChild, g_childX, g_childY, cx, cy, TRUE);
            }
            BlitRectOrTileGridToPlayerSurface((char)g_blitModeByte,
                                              (int)ps.rcPaint.bottom,
                                              (int)ps.rcPaint.right,
                                              (int)ps.rcPaint.top,
                                              (int)ps.rcPaint.left);
            g_blitModeByte = 0;      /* consumed: the mode is a one-shot */
        }
        EndPaint(hWnd, &ps);
        return 0;

    /* ---- 0x3B9 private: clear the suspend-requested flag ---------------- */
    case WM_NW_SUSPEND:
        g_suspendRequested = 0;
        return 0;

    /* ---- 0x10 WM_CLOSE: flag the close, then arm the update sound ------ */
    case WM_CLOSE:
        g_closeRequested = 1;
        NwArmUpdateAudio;
        return 0;

    default:
        return DefWindowProcA(hWnd, uMsg, wParam, lParam);
    }
}
#endif /* NW_UNIT_HandleMainWindowMessage */

/* ==========================================================================
 * Unit: ProcessMainWindowMessages
 * Source: input/win32/ProcessMainWindowMessages.c
 * ========================================================================== */
#ifdef NW_UNIT_ProcessMainWindowMessages
/*
 * Main-window message processing and exit handling.
 *
 * Module: input/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#input-win32-processmainwindowmessages-c
 */

#include <windows.h>
#include "nwglobals.h"
#include "NwSdlPcm.h"
#include <stdint.h>

extern LRESULT __stdcall HandleMainWindowMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern void     __stdcall AddTranslatedRectToExtentList(uint32_t nMaxY, uint32_t nMaxX, uint32_t nMinY, uint32_t nMinX);
extern void     __stdcall SetDefaultGridCellOffsetAndExtent(uint16_t uCellOffsetY, uint16_t uCellOffsetX);
extern void             MixActiveAudioSlotsIntoWaveOutBuffer(void);
extern void             DisplayHelpAndDestroyMainWindow(void);
extern HCURSOR          GetActiveArrowCursor(void);

extern HDC      g_hdcMem;      
extern HPALETTE g_palette;     



/* ---- globals ------------------------------------------------------------- */

typedef struct tagInputState {
    uint8_t bPad0[4];
    uint8_t bEnable;        /* +0x04: bit0 = keyboard live, bit1 = pointer live */
    uint8_t bPad1[0x3B];
    uint8_t keySlot[8];     /* +0x40..+0x47 */
    uint8_t btnSlot[4];     /* +0x48..+0x4b */
} INPUTSTATE, *PINPUTSTATE;

#define ACT_PRESS  0x81u
#define ACT_SLOT_A 0x82u
#define ACT_SLOT_B 0x83u
#define ACT_SLOT_C 0x8Fu
#define ACT_SLOT_D 0x84u
/* Button slots +0x48..+0x4b resolve, in that order, to these four codes. */
static const uint8_t kSlotAction[4] = { ACT_SLOT_A, ACT_SLOT_B, ACT_SLOT_C, ACT_SLOT_D };

/* App-defined messages that share the (wParam == hWnd) shape of WM_COMMAND. */
#define MSG_APP_COMMAND_FILTERED  0x0311u  /* swallowed when wParam == hWnd */
#define MSG_APP_COMMAND_RUN       0x030Fu
#define MSG_APP_PALETTE_UPDATE    0x03BDu

LRESULT WINAPI ProcessMainWindowMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    RECT        rcUpdate;          /* the 16 bytes of locals */
    PINPUTSTATE pIn;
    int         iSlot;
    int         bSlotMatched = 0;
    int         bBtnMatched  = 0;
    uint16_t    wX, wY;

    if (g_bForwardAllToHandler != 0) {

        return HandleMainWindowMessage(hWnd, uMsg, wParam, lParam);
    }

    pIn = (PINPUTSTATE)g_pInputState;

    switch (uMsg) {

    case WM_PAINT:                     
        /* The frame slide puts the last push, "PUSH [ESP+0x20]" issued with two
         * words already pushed, back on arg1 = hWnd; with PUSH 0x0 already in
         * place that is bErase = FALSE. */
        if (GetUpdateRect(hWnd, &rcUpdate, FALSE)) {
            /* Only a REAL invalid region is accumulated, and it is passed BY
             * VALUE: the four dword pushes read rc.left/top/right/bottom as
             * values, bottom pushed last so it lands in the first slot.  On
             * success control falls out of the switch to DefWindowProcA. */
            AddTranslatedRectToExtentList((uint32_t)rcUpdate.bottom,
                                          (uint32_t)rcUpdate.right,
                                          (uint32_t)rcUpdate.top,
                                          (uint32_t)rcUpdate.left);
        }
        return DefWindowProcA(hWnd, uMsg, wParam, lParam);

    case MSG_APP_COMMAND_FILTERED:      
        /* Notifications that originate from a child control carry
         * wParam == hWnd; those are dropped without any I/O. */
        if (wParam == (WPARAM)hWnd) {
            return 0;
        }
        /* fall through */

    case MSG_APP_COMMAND_RUN:           
        if (g_palette != 0) {
            unsigned nReal;
            SelectPalette(g_hdcMem, g_palette, FALSE);
            nReal = (unsigned)RealizePalette(g_hdcMem);
            if (nReal != 0) {
                /* Same by-value rect call, this time synthesised from the two
                 * cached WM_SIZE halves: HIWORD lands in the first slot. */
                AddTranslatedRectToExtentList(g_dwSizeHighWord,
                                              g_dwSizeLowWord, 0, 0);
                return 1;
            }
        }
        return 0;

    case WM_CHAR:                       
        if ((pIn->bEnable & 0x1) == 0) {
            return 0;
        }
        /* Character code stashed alongside the key codes. */
        g_bKeyCode = (uint8_t)wParam;
        return 0;

    case WM_KEYDOWN:                    
        if ((pIn->bEnable & 0x1) == 0) {
            return 0;
        }

        for (iSlot = 0; iSlot < 8; iSlot++) {
            if (pIn->keySlot[iSlot] == (uint8_t)wParam) {
                g_bModifierSlot = (uint8_t)iSlot;
                bSlotMatched = 1;
                break;
            }
        }
        if (bSlotMatched) {

            if (g_bModifierSlot <= 7 &&
                GetAsyncKeyState(0x10 /* VK_LSHIFT */) != 0) {
                g_bModifierSlot |= 0x8u;
            }
            /* Modifier paths also pin the live cursor as the anchor. */
            g_dwAnchorX = g_dwCursorX;
            g_dwAnchorY = g_dwCursorY;
            return 0;
        }
        /* Not a modifier: try the four button slots, which latch an action
         * code and then, only on a match, the raw wParam. */
        for (iSlot = 0; iSlot < 4; iSlot++) {
            if (pIn->btnSlot[iSlot] == (uint8_t)wParam) {
                g_bClickAction = kSlotAction[iSlot];
                bBtnMatched = 1;
                break;
            }
        }
        if (bBtnMatched) {
            g_bKeyCode = (uint8_t)wParam;
        }
        return 0;   /* unmatched: neither latch is touched */

    case WM_LBUTTONDOWN:                 
    case WM_RBUTTONDOWN:                 
        if ((pIn->bEnable & 0x2) == 0) {
            return 0;
        }
        g_bClickAction = ACT_PRESS;
        g_dwAnchorX = (uint32_t)(uint16_t)lParam;          /* LOWORD = X */
        g_dwAnchorY = (uint32_t)(uint16_t)(lParam >> 16);  /* HIWORD = Y */
        return 0;

    case WM_LBUTTONUP:                  
        if ((pIn->bEnable & 0x2) == 0) {
            return 0;
        }
        g_bClickAction = ACT_SLOT_A;
        g_bKeyCode = pIn->btnSlot[0];                     /* slot +0x48 */
        g_dwAnchorX = (uint32_t)(uint16_t)lParam;
        g_dwAnchorY = (uint32_t)(uint16_t)(lParam >> 16);
        return 0;

    case WM_RBUTTONUP:                  
        if ((pIn->bEnable & 0x2) == 0) {
            return 0;
        }
        g_bClickAction = ACT_SLOT_B;
        g_bKeyCode = pIn->btnSlot[1];                     /* slot +0x49 */
        g_dwAnchorX = (uint32_t)(uint16_t)lParam;
        g_dwAnchorY = (uint32_t)(uint16_t)(lParam >> 16);
        return 0;

    case WM_SETCURSOR:                  

        if ((uint16_t)lParam != 1 /* HTCLIENT */) {
            return DefWindowProcA(hWnd, uMsg, wParam, lParam);
        }
        SetCursor(GetActiveArrowCursor());
        return 0;

    case WM_MOUSEMOVE:                  
        if ((pIn->bEnable & 0x2) == 0) {
            return 0;
        }

        wX = (uint16_t)lParam;
        wY = (uint16_t)(lParam >> 16);
        g_dwCursorX = wX;
        g_dwCursorY = wY;
        SetDefaultGridCellOffsetAndExtent(wY, wX);
        return 0;

    case WM_GETMINMAXINFO:              
        /* lParam is a POINTER to a caller-supplied structure here (unlike every
         * other arm, where it is a packed coordinate).  The pane origin is
         * recomputed from base+delta and written twice, at +0x08/+0x0C and
         * mirrored up to +0x20/+0x24. */
        {
            uint32_t *pPane = (uint32_t *)lParam;
            pPane[8] = g_dwPaneBaseX + g_dwPaneDeltaX;  /* +0x20 */
            pPane[2] = pPane[8];                        /* +0x08 */
            pPane[9] = g_dwPaneBaseY + g_dwPaneDeltaY;  /* +0x24 */
            pPane[3] = pPane[9];                        /* +0x0C */
        }
        return 0;

    case WM_SIZE:                       
        g_dwSizeLowWord  = (uint32_t)(uint16_t)lParam;          /* LOWORD */
        g_dwSizeHighWord = (uint32_t)(uint16_t)(lParam >> 16);  /* HIWORD */
        return 0;

    case MSG_APP_PALETTE_UPDATE:        
        if (NwPcmSink_ShouldMix())
            MixActiveAudioSlotsIntoWaveOutBuffer();
        return 0;

    case WM_CLOSE:                      
        DisplayHelpAndDestroyMainWindow();
        return 0;

    case WM_DESTROY:                    
        /* The one and only PostQuitMessage site in this body. */
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

LRESULT WINAPI GmwPlayerWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    return ProcessMainWindowMessages(hWnd, uMsg, wParam, lParam);
}
#endif /* NW_UNIT_ProcessMainWindowMessages */
