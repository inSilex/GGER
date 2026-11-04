#include "nwarch.h"
#include "nwdebug.h"
#include "nwarch.h"
/*
 * MainLoopSdl -- SDL3 event pump for the removable platform seam.
 *
 * Module: app/sdl3
 * Interface: same external symbol as app/win32/RunMainMessageLoop.c
 *
 * Phase 3 translates SDL key, text, mouse, and resize events directly into the
 * fixed data-window input slots.  The slot matching and action codes mirror
 * input/win32/WindowMessages.c, so the game state machine and VM trace stay
 * unchanged without a synthetic Win32 message bridge.
 *
 * The loop mirrors the Win32 loop exactly: one queued event per iteration, the
 * frame step only when the queue is empty, timeGetTime() throttling and the
 * same StoreArgsAndProcessQueuedRecords arguments.
 *
 * Code and state live in the private SDL sections so the game's fixed
 * .text/.data window is untouched.
 */

#ifdef NW_UNIT_RunMainMessageLoopSdl

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_touch.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_filesystem.h>

#include "NwSdlAndroidControls.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#define NW_SDL_CODE __attribute__((section(".nwsvg")))

extern int NwSdlWindowToLogical(int wx, int wy, int *lx, int *ly);
extern void NwSdlLogicalToWindow(int lx, int ly, float *wx, float *wy);
extern int NwSdlToggleFullscreen(void);
#define NW_SDL_BSS  __attribute__((section(".nwsvgbss")))

extern unsigned char nw_data_window[] __asm__("nw_data_window");

#define NW32(off) (*(volatile uint32_t *)(nw_data_window + (off)))
#define NW8(off)  (*(volatile uint8_t  *)(nw_data_window + (off)))
#define NWPTR(off) NW_WINDOW_PTR_REF(void, (off))

/* Window slots used by the shipped message loop. */
#define W_HINSTANCE       0x6400u
#define W_SHOWCMD         0x5f10u
#define W_IDLE_OR_ERROR   0x5f14u
#define W_FRAME_INTERVAL  0x5f18u
#define W_LAST_FRAME_TIME 0x60c8u
#define W_MAIN_HWND       0x6404u
#define W_CURSOR_FLAGS    0x5efcu
#define W_WIDTH           0x5f20u
#define W_HEIGHT          0x5f24u
#define W_ANCHOR_X        0x5f48u
#define W_ANCHOR_Y        0x5f4cu
#define W_CURSOR_X        0x5f40u
#define W_CURSOR_Y        0x5f44u
#define W_CLICK_BYTE      0x5f50u
#define W_MODIFIER_BYTE   0x5f51u
#define W_KEY_BYTE        0x5f52u
#define W_APP_PATH        0x5eb4u
#define W_APP_PATH_BYTES  0x40u

#define W_FORWARD_ALL     0x60c3u
#define W_FIELD_MODE      0x1c7du
#define W_FIELD_DEPTH     0x1c70u
#define W_FIELD_BUF       0x1774u

#define NW_INPUT_CLEAR 0x0eu

#if defined(NW_PS5_NATIVE_TITLE) || defined(NW_PORTABLE_PS5)
/* Implemented by platforms/ps5/src/platform.c with the console's ImeDialog service. */
extern int pad_keyboard_open(const char *initial, char *out, size_t out_size,
                             const char *title);
#endif

extern SDL_Window *g_nwSdlWindow;

#if defined(NW_PS5_NATIVE_TITLE) || defined(NW_PORTABLE_PS5)
extern void platform_boot_mark(const char *stage);
extern void platform_log_mark(const char *stage);
#define NW_NATIVE_BOOT(stage) platform_boot_mark(stage)
/* Recurring diagnostics go to klog/stderr only; console builds no longer
 * create a log file or draw the boot trace. */
#define NW_NATIVE_LOG(stage) platform_log_mark(stage)
#else
#define NW_NATIVE_BOOT(stage) ((void)0)
#define NW_NATIVE_LOG(stage) ((void)0)
#endif

extern void NwSdlVideoPresent(void);
extern void NwSdlVideoPresentNow(void);
extern void NwSdlVideoPresentFrame(void);
extern void NwSdlVideoSetMainLoopActive(int active);
extern void NwSdlVideoTakeComposeStats(uint64_t *pNs, unsigned *pFrames);
extern void NwSdlEnforceWindowAspect(void);
extern void NwSdlUpdateWindowLimits(void);
extern void NwPcmSink_Pump(void);
extern void NwAssetTakeLoadStats(uint64_t *pNs, unsigned *pCalls,
                                 unsigned *pMisses);
extern void NwAssetToggleHighRes(void);
extern void NwBlitTakeStats(uint64_t *pNs, unsigned *pCalls);

extern char *__cdecl CopyNulTerminatedString(char *pszDst, const char *pszSrc);
extern uint32_t __stdcall
InitializeAppSubsystemsFromSavedPath(const char *pszPath);
extern void __stdcall BuildHlpSiblingPath(const char *pszSrcPath);
extern void FlushDeferredErrorString(void);
extern void DisplayHelpAndDestroyMainWindow(void);
extern void ShutdownAndReleaseResources(void);
extern uint32_t __stdcall
StoreArgsAndProcessQueuedRecords(uint32_t bPointerInsideClient,
                                 uint32_t bKeyCode,
                                 uint32_t bModifierSlot,
                                 uint32_t bClickAction,
                                 uint32_t dwAnchorY,
                                 uint32_t dwAnchorX,
                                 uint32_t dwCursorY,
                                 uint32_t dwCursorX,
                                 uint32_t dwSizeHighWord,
                                 uint32_t dwSizeLowWord);

static int g_bSdlMinimized NW_SDL_BSS;
/* H is the asset-mode hotkey.  SDL still emits SDL_EVENT_TEXT_INPUT for the
 * printable character, so suppress that one event while the key is down. */
static int g_nwAssetHSuppressText NW_SDL_BSS;
/* F is the PC fullscreen hotkey.  Suppress its text event the same way. */
static int g_nwFullscreenFSuppressText NW_SDL_BSS;
/* Real mouse buttons only become game clicks when the press starts inside the
 * 4:3 viewport.  A release in a letterbox bar must not synthesize a click. */
static int g_nwMouseLeftDownInside NW_SDL_BSS;
static int g_nwMouseRightDownInside NW_SDL_BSS;

/* ---- pointer emulation for gamepads and their touchpads ------------------ */
#define NW_MAX_POINTER_PADS 8
#define NW_PAD_DEADZONE     7500
#define NW_PAD_SPEED        700.0

typedef struct {
    SDL_JoystickID id;
    SDL_Gamepad   *gamepad;
    SDL_Joystick  *joystick;
    int            bR3Held;      /* edge state for the asset-mode toggle */
    int            bL3Held;      /* edge state for the PS5 system keyboard */
    int            bSelectHeld;  /* edge state for the Select/Back keyboard key */
} NW_SDL_POINTER_PAD;

static NW_SDL_POINTER_PAD g_nwPointerPads[NW_MAX_POINTER_PADS] NW_SDL_BSS;
static int    g_nwPointerPadCount NW_SDL_BSS;
static int    g_nwPointerPadsScanned NW_SDL_BSS;
static int    g_nwPointerCursorValid NW_SDL_BSS;
static int    g_nwPointerSentX NW_SDL_BSS;
static int    g_nwPointerSentY NW_SDL_BSS;
static double g_nwPointerX NW_SDL_BSS;
static double g_nwPointerY NW_SDL_BSS;
static Uint64 g_nwPointerLastNS NW_SDL_BSS;
static Uint64 g_nwPointerLastPadNS NW_SDL_BSS;
static Uint64 g_nwPointerLastWarpNS NW_SDL_BSS;
static Uint64 g_nwPointerLastPresentNS NW_SDL_BSS;
static int    g_nwPointerSoftwareCursor NW_SDL_BSS;
static int    g_nwPointerLeftHeld NW_SDL_BSS;
static int    g_nwPointerRightHeld NW_SDL_BSS;

#if defined(NW_PS5_NATIVE_TITLE) || defined(NW_PORTABLE_PS5)
/* Text confirmed by the system keyboard is fed to the game's typed-token
 * field one byte per frame, exactly as individual key presses arrive. */
#define NW_TEXT_QUEUE_CAP 128
static unsigned char g_nwTextQueue[NW_TEXT_QUEUE_CAP] NW_SDL_BSS;
static int g_nwTextHead NW_SDL_BSS;
static int g_nwTextTail NW_SDL_BSS;
static int g_nwTextRequireField NW_SDL_BSS;
#endif

/* ---- opt-in profiler ----------------------------------------------------
 *
 * NW_DEBUG_PROFILE=1 writes nwprof.csv (or NW_DEBUG_PROFILE=<path>) with one F row per rendered
 * frame and one I row per input event.  Times are microseconds from
 * SDL_GetPerformanceCounter(), so this distinguishes game logic from video
 * presentation instead of relying on the 1 ms GetTickCount()/SDL_GetTicks()
 * frame pacing clock.  Without NW_DEBUG_PROFILE the only cost is one cached getenv
 * check per frame.
 */
#define NWP_EVENT          0
#define NWP_STEP           1
#define NWP_PRESENT        2
#define NWP_CONV           3
#define NWP_UPDATE         4
#define NWP_AUDIO          5
#define NWP_COUNT          6

static uint64_t g_nwProfTicks[NWP_COUNT] NW_SDL_BSS;
static uint64_t g_nwProfFreq NW_SDL_BSS;
static uint64_t g_nwProfPresents NW_SDL_BSS;
static uint64_t g_nwProfEvents NW_SDL_BSS;
static FILE    *g_nwProfFile NW_SDL_BSS;
static int      g_nwProfState NW_SDL_BSS;

/* Optional frame-pacing summary for console klog.  The NW_DEBUG_PROFILE CSV
 * is per-frame; this is one line per 60 rendered frames and distinguishes a
 * slow game step from a slow presentation path.  The same counters feed a
 * one-shot line whenever a frame interval overshoots its target, which catches
 * intermittent stalls that an average would hide. */
static uint64_t g_nwPerfFreq NW_SDL_BSS;
static uint64_t g_nwPerfWindowStart NW_SDL_BSS;
static uint64_t g_nwPerfFrames NW_SDL_BSS;
static uint64_t g_nwPerfStepTicks NW_SDL_BSS;
static uint64_t g_nwPerfPresentTicks NW_SDL_BSS;
static uint64_t g_nwPerfPumpTicks NW_SDL_BSS;
static uint64_t g_nwPerfPumpMaxTicks NW_SDL_BSS;
static uint64_t g_nwPerfPumpFrameTicks NW_SDL_BSS;
static uint64_t g_nwPerfPumpFrameMaxTicks NW_SDL_BSS;
static uint64_t g_nwPerfAssetFrameTicks NW_SDL_BSS;
static uint64_t g_nwPerfAssetFrameCalls NW_SDL_BSS;
static uint64_t g_nwPerfAssetFrameMisses NW_SDL_BSS;
static uint64_t g_nwPerfComposeFrameTicks NW_SDL_BSS;
static uint64_t g_nwPerfComposeFrameFrames NW_SDL_BSS;
static uint64_t g_nwPerfAssetTicks NW_SDL_BSS;
static uint64_t g_nwPerfAssetMaxTicks NW_SDL_BSS;
static uint64_t g_nwPerfAssetCalls NW_SDL_BSS;
static uint64_t g_nwPerfAssetMisses NW_SDL_BSS;
static uint64_t g_nwPerfLastFrameEnd NW_SDL_BSS;

NW_SDL_CODE static int nw_prof_on(void)
{
    if (g_nwProfState == 0) {
        const char *psz = NW_DEBUG_GETENV("NW_DEBUG_PROFILE");
        g_nwProfState = -1;
        if (psz != NULL && psz[0] != '\0') {
            char szPath[1024];

            if (strcmp(psz, "1") != 0) {
                g_nwProfFile = fopen(psz, "wb");
            } else {
                /* Explorer/shortcut launches often have an unexpected working
                 * directory.  Try the cwd first, then the module directory,
                 * then the per-user temp directory. */
                const char *base;
                const char *tmp;

                g_nwProfFile = fopen("nwprof.csv", "wb");
                if (g_nwProfFile == NULL) {
                    base = SDL_GetBasePath();
                    if (base != NULL) {
                        snprintf(szPath, sizeof szPath, "%snwprof.csv", base);
                        g_nwProfFile = fopen(szPath, "wb");
                        SDL_free((void *)base);
                    }
                }
                if (g_nwProfFile == NULL) {
                    tmp = getenv("TEMP");
                    if (tmp == NULL || tmp[0] == '\0')
                        tmp = getenv("TMPDIR");
                    if (tmp != NULL && tmp[0] != '\0') {
                        snprintf(szPath, sizeof szPath, "%s/nwprof.csv", tmp);
                        g_nwProfFile = fopen(szPath, "wb");
                    }
                }
                if (g_nwProfFile == NULL)
                    fprintf(stderr, "nwprof: cannot create nwprof.csv\n");
            }
            if (g_nwProfFile != NULL) {
                setvbuf(g_nwProfFile, NULL, _IOFBF, 1u << 16);
                fprintf(g_nwProfFile,
                        "F,frame,t_us,gap_us,dur_us,evt_us,step_us,"
                        "step_pres_us,final_pres_us,present_us,conv_us,"
                        "update_us,audio_us,events,presents,click,key,res\n");
                fflush(g_nwProfFile);
                g_nwProfState = 1;
            }
        }
        if (g_nwProfFreq == 0)
            g_nwProfFreq = (uint64_t)SDL_GetPerformanceFrequency();
    }
    return g_nwProfState == 1;
}

NW_SDL_CODE int NwProfOn(void)
{
    return nw_prof_on();
}

NW_SDL_CODE uint64_t NwProfNow(void)
{
    if (g_nwProfFreq == 0)
        g_nwProfFreq = (uint64_t)SDL_GetPerformanceFrequency();
    return (uint64_t)SDL_GetPerformanceCounter();
}

NW_SDL_CODE static unsigned long nw_prof_us(uint64_t ticks)
{
    if (g_nwProfFreq == 0)
        return 0;
    return (unsigned long)((ticks * 1000000u) / g_nwProfFreq);
}

NW_SDL_CODE void NwProfAdd(int slot, uint64_t start, uint64_t end)
{
    if (slot < 0 || slot >= NWP_COUNT || !nw_prof_on())
        return;
    if (end > start)
        g_nwProfTicks[slot] += end - start;
}

NW_SDL_CODE uint64_t NwProfTotal(int slot)
{
    if (slot < 0 || slot >= NWP_COUNT)
        return 0;
    return g_nwProfTicks[slot];
}

NW_SDL_CODE void NwProfBump(int slot)
{
    if (slot < 0 || slot >= NWP_COUNT || !nw_prof_on())
        return;
    if (slot == NWP_PRESENT)
        g_nwProfPresents++;
}

NW_SDL_CODE void NwProfEvent(uint32_t kind, uint32_t a, uint32_t b)
{
    if (!nw_prof_on() || g_nwProfFile == NULL)
        return;
    fprintf(g_nwProfFile, "I,%lu,%u,%u,%u\n",
            nw_prof_us(NwProfNow()), kind, a, b);
    fflush(g_nwProfFile);
    g_nwProfEvents++;
}

/* The shipped input decoder keeps its working set in the input-state block;
 * these are the same offsets as input/win32/WindowMessages.c.  Phase 3 writes
 * the fixed data-window slots directly instead of manufacturing window
 * messages for the Win32 decoder. */
typedef struct tagNwSdlInputState {
    unsigned char bPad0[4];
    unsigned char bEnable;        /* +0x04: bit0 keyboard, bit1 pointer */
    unsigned char bPad1[0x3B];
    unsigned char keySlot[8];     /* +0x40 */
    unsigned char btnSlot[4];     /* +0x48 */
} NW_SDL_INPUTSTATE;

#define NW_ACT_PRESS   0x81u
#define NW_ACT_SLOT_A  0x82u
#define NW_ACT_SLOT_B  0x83u
#define NW_ACT_SLOT_C  0x8Fu
#define NW_ACT_SLOT_D  0x84u

static const unsigned char nw_slot_action[4] = {
    NW_ACT_SLOT_A, NW_ACT_SLOT_B, NW_ACT_SLOT_C, NW_ACT_SLOT_D
};

extern void __stdcall
SetDefaultGridCellOffsetAndExtent(uint16_t uCellOffsetY, uint16_t uCellOffsetX);
extern void __stdcall
PlayMciWavesCommandIfDeviceArmed(unsigned long uMciDeviceId,
                                 unsigned long uCommand);
extern unsigned char g_closeRequested;
extern uint16_t g_clientWidth;
extern uint16_t g_clientHeight;

NW_SDL_CODE static NW_SDL_INPUTSTATE *nw_input_state(void)
{
    return (NW_SDL_INPUTSTATE *)NWPTR(0x5efc);
}

NW_SDL_CODE static int nw_forward_all(void)
{
    return NW8(W_FORWARD_ALL) != 0u;
}

/* WM_CHAR: store the character byte when the decoder is keyboard-live. */
NW_SDL_CODE static void nw_input_char(unsigned ch)
{
    NW_SDL_INPUTSTATE *pIn;

    NwProfEvent(1u, (uint32_t)ch, 0u);

    if (nw_forward_all()) {
        if (ch == 0x20u || ch == 0x0Du)
            PlayMciWavesCommandIfDeviceArmed(0u, 0x808u);
        return;
    }
    pIn = nw_input_state();
    if (pIn == NULL || (pIn->bEnable & 0x01u) == 0u)
        return;
    NW8(W_KEY_BYTE) = (uint8_t)ch;
}

/* WM_KEYDOWN: modifier slots latch a slot index (plus the left-shift bit);
 * button slots latch an action code and the raw virtual key. */
NW_SDL_CODE static void nw_input_keydown(UINT vk, int bLeftShift)
{
    NW_SDL_INPUTSTATE *pIn;
    int i;

    NwProfEvent(2u, (uint32_t)vk, (uint32_t)bLeftShift);

    if (nw_forward_all())
        return;

    pIn = nw_input_state();
    if (pIn == NULL || (pIn->bEnable & 0x01u) == 0u)
        return;

    for (i = 0; i < 8; i++) {
        if (pIn->keySlot[i] == (unsigned char)vk) {
            NW8(W_MODIFIER_BYTE) = (uint8_t)i;
            if (bLeftShift)
                NW8(W_MODIFIER_BYTE) |= 0x08u;
            NW32(W_ANCHOR_X) = NW32(W_CURSOR_X);
            NW32(W_ANCHOR_Y) = NW32(W_CURSOR_Y);
            return;
        }
    }
    for (i = 0; i < 4; i++) {
        if (pIn->btnSlot[i] == (unsigned char)vk) {
            NW8(W_CLICK_BYTE) = nw_slot_action[i];
            NW8(W_KEY_BYTE) = (uint8_t)vk;
            return;
        }
    }
}

/* WM_LBUTTONDOWN / WM_RBUTTONDOWN */
NW_SDL_CODE static void nw_input_mouse_down(int nX, int nY)
{
    NW_SDL_INPUTSTATE *pIn;

    NwProfEvent(3u, (uint32_t)nX, (uint32_t)nY);

    if (nw_forward_all()) {
        PlayMciWavesCommandIfDeviceArmed(0u, 0x808u);
        return;
    }
    pIn = nw_input_state();
    if (pIn == NULL || (pIn->bEnable & 0x02u) == 0u)
        return;
    NW8(W_CLICK_BYTE) = NW_ACT_PRESS;
    NW32(W_ANCHOR_X) = (uint32_t)nX;
    NW32(W_ANCHOR_Y) = (uint32_t)nY;
}

/* WM_LBUTTONUP / WM_RBUTTONUP */
NW_SDL_CODE static void nw_input_mouse_up(int nX, int nY, int bRight)
{
    NW_SDL_INPUTSTATE *pIn;

    NwProfEvent(4u, (uint32_t)nX,
                (uint32_t)nY | (bRight ? 0x10000u : 0u));

    if (nw_forward_all())
        return;
    pIn = nw_input_state();
    if (pIn == NULL || (pIn->bEnable & 0x02u) == 0u)
        return;
    NW8(W_CLICK_BYTE) = bRight ? NW_ACT_SLOT_B : NW_ACT_SLOT_A;
    NW8(W_KEY_BYTE) = pIn->btnSlot[bRight ? 1 : 0];
    NW32(W_ANCHOR_X) = (uint32_t)nX;
    NW32(W_ANCHOR_Y) = (uint32_t)nY;
}

/* WM_MOUSEMOVE */
NW_SDL_CODE static void nw_input_mouse_move(int nX, int nY)
{
    NW_SDL_INPUTSTATE *pIn;

    if (nw_forward_all())
        return;
    pIn = nw_input_state();
    if (pIn == NULL || (pIn->bEnable & 0x02u) == 0u)
        return;
    NW32(W_CURSOR_X) = (uint32_t)nX;
    NW32(W_CURSOR_Y) = (uint32_t)nY;
    SetDefaultGridCellOffsetAndExtent((uint16_t)nY, (uint16_t)nX);
}

/* ---- gamepad / touchpad pointer bridge -------------------------------- */

NW_SDL_CODE static int nw_pointer_width(void)
{
    return g_clientWidth > 0 ? (int)g_clientWidth : 640;
}

NW_SDL_CODE static int nw_pointer_height(void)
{
    return g_clientHeight > 0 ? (int)g_clientHeight : 480;
}

/* The host cursor can be hidden by the compositor while only a controller is
 * in use.  While the controller owns the pointer, hide the system arrow and
 * let VideoSdl draw a scaled crosshair at the logical cursor position.  The
 * mode expires after a few seconds without controller input, and any real
 * mouse event switches back immediately. */
NW_SDL_CODE static void nw_pointer_refresh_cursor_mode(void)
{
    Uint64 now = SDL_GetTicksNS();
    int active = (g_nwPointerLastPadNS != 0 &&
                  (now - g_nwPointerLastPadNS) < SDL_SECONDS_TO_NS(3));

    if (active == g_nwPointerSoftwareCursor)
        return;
    g_nwPointerSoftwareCursor = active;
    if (active) {
        (void)SDL_HideCursor();
    } else {
        (void)SDL_ShowCursor();
        /* Erase the crosshair from the last presented frame immediately; the
         * game may not redraw while the pointer is idle. */
        NwSdlVideoPresentNow();
    }
}

NW_SDL_CODE static void nw_pointer_mark_pad(void)
{
    g_nwPointerLastPadNS = SDL_GetTicksNS();
    nw_pointer_refresh_cursor_mode();
}

NW_SDL_CODE static void nw_pointer_mark_mouse(void)
{
    g_nwPointerLastPadNS = 0;
    nw_pointer_refresh_cursor_mode();
}

/* Read by the SDL presentation path. */
NW_SDL_CODE int NwSdlJoystickCursorActive(void)
{
    return g_nwPointerSoftwareCursor;
}

NW_SDL_CODE void NwSdlJoystickCursorPosition(int *px, int *py)
{
    if (px != NULL)
        *px = (int)(g_nwPointerX + 0.5);
    if (py != NULL)
        *py = (int)(g_nwPointerY + 0.5);
}

/* Warping the host pointer generates a mouse-motion event on most desktops.
 * That echo must not feed the integer host position back into the virtual
 * cursor, or continuous stick movement would be quantized to at least one
 * pixel per poll and become much faster than NW_PAD_SPEED. */
NW_SDL_CODE static int nw_pointer_warp_echo(void)
{
    if (g_nwPointerLastWarpNS == 0)
        return 0;
    return (SDL_GetTicksNS() - g_nwPointerLastWarpNS) < SDL_MS_TO_NS(100);
}

/* Keep the game's logical cursor in range and mirror it to the host pointer.
 * The software cursor is what the game actually draws and hit-tests; warping
 * the system pointer keeps SDL's focused-window pointer state consistent, so
 * the pointer-inside-client checks in the main loop keep working.  Continuous
 * stick movement warps at display rate instead of every 1 ms poll. */
NW_SDL_CODE static void nw_pointer_warp(int x, int y, int bForce)
{
    Uint64 now;
    float wx = 0.0f;
    float wy = 0.0f;

    if (g_nwSdlWindow == NULL)
        return;

    now = SDL_GetTicksNS();
    if (!bForce && g_nwPointerLastWarpNS != 0 &&
        (now - g_nwPointerLastWarpNS) < SDL_MS_TO_NS(16))
        return;

    NwSdlLogicalToWindow(x, y, &wx, &wy);
    SDL_WarpMouseInWindow(g_nwSdlWindow, wx, wy);
    g_nwPointerLastWarpNS = SDL_GetTicksNS();
}

/* The game only redraws when its own state changes, so a software cursor that
 * merely waits for the next game present can freeze while it moves inside one
 * cell.  Present at display rate while controller input is moving it. */
NW_SDL_CODE static void nw_pointer_present(void)
{
    Uint64 now;

    if (!g_nwPointerSoftwareCursor)
        return;
    now = SDL_GetTicksNS();
    if (g_nwPointerLastPresentNS != 0 &&
        (now - g_nwPointerLastPresentNS) < SDL_MS_TO_NS(16))
        return;
    g_nwPointerLastPresentNS = now;
    NwSdlVideoPresent();
}

NW_SDL_CODE static void nw_pointer_set(double x, double y)
{
    int lw = nw_pointer_width();
    int lh = nw_pointer_height();
    int ix;
    int iy;

    if (x < 0.0)
        x = 0.0;
    if (y < 0.0)
        y = 0.0;
    if (x > (double)(lw - 1))
        x = (double)(lw - 1);
    if (y > (double)(lh - 1))
        y = (double)(lh - 1);

    g_nwPointerX = x;
    g_nwPointerY = y;
    g_nwPointerCursorValid = 1;

    ix = (int)(x + 0.5);
    iy = (int)(y + 0.5);
    if (ix == g_nwPointerSentX && iy == g_nwPointerSentY)
        return;

    g_nwPointerSentX = ix;
    g_nwPointerSentY = iy;
    nw_input_mouse_move(ix, iy);
}

/* Real mouse traffic takes precedence: keep the virtual cursor where the user
 * left the pointer before a stick or touchpad takes over. */
NW_SDL_CODE static void nw_pointer_sync_mouse(int x, int y)
{
    g_nwPointerX = (double)x;
    g_nwPointerY = (double)y;
    g_nwPointerSentX = x;
    g_nwPointerSentY = y;
    g_nwPointerCursorValid = 1;
}

NW_SDL_CODE static void nw_pointer_seed(void)
{
    int x;
    int y;

    if (g_nwPointerCursorValid)
        return;
    x = (int)NW32(W_CURSOR_X);
    y = (int)NW32(W_CURSOR_Y);
    if (x == 0 && y == 0) {
        x = nw_pointer_width() / 2;
        y = nw_pointer_height() / 2;
    }
    nw_pointer_sync_mouse(x, y);
}

NW_SDL_CODE static void nw_pointer_click(int bRight, int bDown)
{
    int x;
    int y;

    nw_pointer_seed();
    x = (int)(g_nwPointerX + 0.5);
    y = (int)(g_nwPointerY + 0.5);
    nw_pointer_mark_pad();
    nw_pointer_set(g_nwPointerX, g_nwPointerY);
    nw_pointer_warp(x, y, 1);
    nw_pointer_present();
    if (bDown)
        nw_input_mouse_down(x, y);
    else
        nw_input_mouse_up(x, y, bRight);
}

NW_SDL_CODE static void nw_pointer_update_buttons(int bLeft, int bRight)
{
    if (bLeft != g_nwPointerLeftHeld) {
        g_nwPointerLeftHeld = bLeft;
        nw_pointer_click(0, bLeft);
    }
    if (bRight != g_nwPointerRightHeld) {
        g_nwPointerRightHeld = bRight;
        nw_pointer_click(1, bRight);
    }
}

NW_SDL_CODE static int nw_pointer_find(SDL_JoystickID id)
{
    int i;

    for (i = 0; i < g_nwPointerPadCount; i++) {
        if (g_nwPointerPads[i].id == id)
            return i;
    }
    return -1;
}

NW_SDL_CODE static void nw_pointer_open_gamepad(SDL_JoystickID id)
{
    SDL_Gamepad *gamepad;

    if (g_nwPointerPadCount >= NW_MAX_POINTER_PADS ||
        nw_pointer_find(id) >= 0)
        return;
    gamepad = SDL_OpenGamepad(id);
    if (gamepad == NULL)
        return;
    g_nwPointerPads[g_nwPointerPadCount].id = id;
    g_nwPointerPads[g_nwPointerPadCount].gamepad = gamepad;
    g_nwPointerPads[g_nwPointerPadCount].joystick = NULL;
    g_nwPointerPads[g_nwPointerPadCount].bR3Held = 0;
    g_nwPointerPads[g_nwPointerPadCount].bL3Held = 0;
    g_nwPointerPads[g_nwPointerPadCount].bSelectHeld = 0;
    g_nwPointerPadCount++;
}

NW_SDL_CODE static void nw_pointer_open_joystick(SDL_JoystickID id)
{
    SDL_Joystick *joystick;

    if (g_nwPointerPadCount >= NW_MAX_POINTER_PADS ||
        nw_pointer_find(id) >= 0)
        return;
    joystick = SDL_OpenJoystick(id);
    if (joystick == NULL)
        return;
    g_nwPointerPads[g_nwPointerPadCount].id = id;
    g_nwPointerPads[g_nwPointerPadCount].gamepad = NULL;
    g_nwPointerPads[g_nwPointerPadCount].joystick = joystick;
    g_nwPointerPads[g_nwPointerPadCount].bR3Held = 0;
    g_nwPointerPads[g_nwPointerPadCount].bL3Held = 0;
    g_nwPointerPads[g_nwPointerPadCount].bSelectHeld = 0;
    g_nwPointerPadCount++;
}

NW_SDL_CODE static void nw_pointer_close(SDL_JoystickID id)
{
    int i = nw_pointer_find(id);

    if (i < 0)
        return;
    if (g_nwPointerPads[i].gamepad != NULL)
        SDL_CloseGamepad(g_nwPointerPads[i].gamepad);
    if (g_nwPointerPads[i].joystick != NULL)
        SDL_CloseJoystick(g_nwPointerPads[i].joystick);
    g_nwPointerPads[i] = g_nwPointerPads[g_nwPointerPadCount - 1];
    g_nwPointerPadCount--;
}

NW_SDL_CODE static void nw_pointer_scan(void)
{
    SDL_JoystickID *ids;
    int count = 0;
    int i;

    if (g_nwPointerPadsScanned || g_nwSdlWindow == NULL)
        return;
    g_nwPointerPadsScanned = 1;
    if (!SDL_WasInit(SDL_INIT_GAMEPAD))
        return;

    ids = SDL_GetGamepads(&count);
    for (i = 0; i < count; i++)
        nw_pointer_open_gamepad(ids[i]);
    SDL_free(ids);

    count = 0;
    ids = SDL_GetJoysticks(&count);
    for (i = 0; i < count; i++) {
        if (!SDL_IsGamepad(ids[i]))
            nw_pointer_open_joystick(ids[i]);
    }
    SDL_free(ids);
}

NW_SDL_CODE static void nw_pointer_touchpad(float x, float y)
{
    int lw = nw_pointer_width();
    int lh = nw_pointer_height();
    int ix;
    int iy;

    if (x < 0.0f)
        x = 0.0f;
    if (x > 1.0f)
        x = 1.0f;
    if (y < 0.0f)
        y = 0.0f;
    if (y > 1.0f)
        y = 1.0f;
    ix = (int)((double)x * (double)(lw - 1) + 0.5);
    iy = (int)((double)y * (double)(lh - 1) + 0.5);
    nw_pointer_mark_pad();
    nw_pointer_set((double)ix, (double)iy);
    nw_pointer_warp(ix, iy, 0);
    nw_pointer_present();
}

#if defined(NW_PS5_NATIVE_TITLE) || defined(NW_PORTABLE_PS5)
NW_SDL_CODE static void nw_text_queue_clear(void)
{
    g_nwTextHead = 0;
    g_nwTextTail = 0;
}

NW_SDL_CODE static void nw_text_queue_push(unsigned char ch)
{
    const int next = (g_nwTextTail + 1) % NW_TEXT_QUEUE_CAP;

    if (next == g_nwTextHead)
        return;
    g_nwTextQueue[g_nwTextTail] = ch;
    g_nwTextTail = next;
}

/* VM opcode 0x0f is the engine's typed-token field: it raises g_bFieldMode
 * until the 0x08/0x0d commit commands finish.  Some screens (notably the
 * high-score name prompt) enter their input state through a different VM
 * path, so L3 always opens the system keyboard.  When a typed-token field is
 * active we replace its complete contents; otherwise the confirmed keys are
 * still delivered through the normal WM_CHAR path.  One byte is consumed per
 * game frame so the VM sees the same sequence as individual key presses. */
NW_SDL_CODE static void nw_text_queue_pump(void)
{
    if (g_nwTextHead == g_nwTextTail)
        return;
    if (g_nwTextRequireField && NW8(W_FIELD_MODE) == 0u) {
        nw_text_queue_clear();
        g_nwTextRequireField = 0;
        return;
    }
    /* Do not overwrite a character delivered by a real key in this frame. */
    if (NW8(W_KEY_BYTE) != NW_INPUT_CLEAR)
        return;
    NW8(W_KEY_BYTE) = g_nwTextQueue[g_nwTextHead];
    g_nwTextHead = (g_nwTextHead + 1) % NW_TEXT_QUEUE_CAP;
    if (g_nwTextHead == g_nwTextTail)
        g_nwTextRequireField = 0;
}

NW_SDL_CODE static void nw_keyboard_toggle(void)
{
    char initial[33];
    char result[64];
    int bField;
    int depth;
    int opened;
    int i;

    bField = NW8(W_FIELD_MODE) != 0u;
    depth = bField ? (int)NW32(W_FIELD_DEPTH) : 0;
    if (depth < 0)
        depth = 0;
    if (depth > 32)
        depth = 32;
    NW_NATIVE_LOG("ps5_ime_request");
    for (i = 0; i < depth; i++) {
        const unsigned char ch = NW8(W_FIELD_BUF + (uint32_t)i);
        initial[i] = (ch >= 0x20u && ch < 0x7fu) ? (char)ch : ' ';
    }
    initial[depth] = '\0';

    opened = pad_keyboard_open(initial, result, sizeof(result), "ENTER NAME");
    if (opened < 0) {
        NW_NATIVE_LOG("ps5_ime_unavailable");
        return;
    }
    if (opened == 0) {
        NW_NATIVE_LOG("ps5_ime_cancelled");
        return;
    }
    NW_NATIVE_LOG("ps5_ime_confirmed");

    nw_text_queue_clear();
    g_nwTextRequireField = bField;
    if (bField) {
        /* The dialog edits the complete field, so first remove whatever was
         * already in it (VM 0x08 releases one typed-token character), then
         * feed the replacement and commit it with Return. */
        for (i = 0; i < depth; i++)
            nw_text_queue_push(0x08u);
    }
    /* The high-score table stores ten bytes per name.  Keep the direct name
     * path inside that field so a long system-keyboard entry cannot run into
     * the next record; typed-token fields keep their own, larger limit. */
    {
        const int limit = bField ? 63 : 10;
        for (i = 0; result[i] != '\0' && i < limit; i++) {
            const unsigned char ch = (unsigned char)result[i];
            if (ch >= 0x20u && ch < 0x7fu)
                nw_text_queue_push(ch);
        }
    }
    nw_text_queue_push('\r');
}
#endif

/* Polled gamepad state: left/right stick or d-pad moves the cursor; south
 * (A/Cross) and the touchpad button are the left button, east (B/Circle) is
 * the right button.  Raw joysticks use axes 0/1 and buttons 0/1. */
NW_SDL_CODE static void nw_pointer_update(void)
{
    Uint64 now;
    double dt;
    int bestX = 0;
    int bestY = 0;
    int bestMag = 0;
    int bLeft = 0;
    int bRight = 0;
    int i;

    nw_pointer_scan();
    nw_pointer_refresh_cursor_mode();
    now = SDL_GetTicksNS();
    dt = 0.0;
    if (g_nwPointerLastNS != 0)
        dt = (double)(now - g_nwPointerLastNS) / 1000000000.0;
    g_nwPointerLastNS = now;
    if (dt > 0.1)
        dt = 0.1;

    for (i = 0; i < g_nwPointerPadCount; i++) {
        NW_SDL_POINTER_PAD *pad = &g_nwPointerPads[i];
        int x = 0;
        int y = 0;
        int bR3 = 0;
        int bL3 = 0;
        int bSelect = 0;
        int mag;

        if (pad->gamepad != NULL) {
            Sint16 lx = SDL_GetGamepadAxis(pad->gamepad,
                                           SDL_GAMEPAD_AXIS_LEFTX);
            Sint16 ly = SDL_GetGamepadAxis(pad->gamepad,
                                           SDL_GAMEPAD_AXIS_LEFTY);
            int adx;
            int ady;

            x = (int)lx;
            y = (int)ly;
            if (x > -NW_PAD_DEADZONE && x < NW_PAD_DEADZONE &&
                y > -NW_PAD_DEADZONE && y < NW_PAD_DEADZONE) {
                Sint16 rx = SDL_GetGamepadAxis(pad->gamepad,
                                               SDL_GAMEPAD_AXIS_RIGHTX);
                Sint16 ry = SDL_GetGamepadAxis(pad->gamepad,
                                               SDL_GAMEPAD_AXIS_RIGHTY);
                if ((rx < -NW_PAD_DEADZONE || rx > NW_PAD_DEADZONE) ||
                    (ry < -NW_PAD_DEADZONE || ry > NW_PAD_DEADZONE)) {
                    x = (int)rx;
                    y = (int)ry;
                }
            }
            adx = (SDL_GetGamepadButton(pad->gamepad,
                                        SDL_GAMEPAD_BUTTON_DPAD_RIGHT) ? 32767 : 0) -
                  (SDL_GetGamepadButton(pad->gamepad,
                                        SDL_GAMEPAD_BUTTON_DPAD_LEFT) ? 32767 : 0);
            ady = (SDL_GetGamepadButton(pad->gamepad,
                                        SDL_GAMEPAD_BUTTON_DPAD_DOWN) ? 32767 : 0) -
                  (SDL_GetGamepadButton(pad->gamepad,
                                        SDL_GAMEPAD_BUTTON_DPAD_UP) ? 32767 : 0);
            x += adx;
            y += ady;
            if (x > 32767) x = 32767;
            if (x < -32768) x = -32768;
            if (y > 32767) y = 32767;
            if (y < -32768) y = -32768;

            if (SDL_GetGamepadButton(pad->gamepad,
                                     SDL_GAMEPAD_BUTTON_SOUTH) ||
                SDL_GetGamepadButton(pad->gamepad,
                                     SDL_GAMEPAD_BUTTON_TOUCHPAD))
                bLeft = 1;
            if (SDL_GetGamepadButton(pad->gamepad,
                                     SDL_GAMEPAD_BUTTON_EAST))
                bRight = 1;
            bR3 = SDL_GetGamepadButton(pad->gamepad,
                                       SDL_GAMEPAD_BUTTON_RIGHT_STICK) ? 1 : 0;
            bL3 = SDL_GetGamepadButton(pad->gamepad,
                                       SDL_GAMEPAD_BUTTON_LEFT_STICK) ? 1 : 0;
            /* "Select" in SDL is BACK.  The PS5 driver maps both BACK and
             * START to the Options button, so either semantic name opens the
             * keyboard; a standard controller's Select button works too. */
            bSelect = (SDL_GetGamepadButton(pad->gamepad,
                                            SDL_GAMEPAD_BUTTON_BACK) ||
                       SDL_GetGamepadButton(pad->gamepad,
                                            SDL_GAMEPAD_BUTTON_START)) ? 1 : 0;
        } else if (pad->joystick != NULL) {
            if (SDL_GetNumJoystickAxes(pad->joystick) > 0)
                x = (int)SDL_GetJoystickAxis(pad->joystick, 0);
            if (SDL_GetNumJoystickAxes(pad->joystick) > 1)
                y = (int)SDL_GetJoystickAxis(pad->joystick, 1);

            if (SDL_GetNumJoystickButtons(pad->joystick) > 0 &&
                SDL_GetJoystickButton(pad->joystick, 0))
                bLeft = 1;
            if (SDL_GetNumJoystickButtons(pad->joystick) > 1 &&
                SDL_GetJoystickButton(pad->joystick, 1))
                bRight = 1;
            /* Raw joysticks have no semantic button map.  The PS5 pad's
             * table places R3 at raw button 7 and L3 at raw button 6. */
            if (SDL_GetNumJoystickButtons(pad->joystick) > 7 &&
                SDL_GetJoystickButton(pad->joystick, 7))
                bR3 = 1;
            if (SDL_GetNumJoystickButtons(pad->joystick) > 6 &&
                SDL_GetJoystickButton(pad->joystick, 6))
                bL3 = 1;
            /* Raw button 8 is Options in the PS5 driver's table, the same
             * physical button that carries BACK/START above. */
            if (SDL_GetNumJoystickButtons(pad->joystick) > 8 &&
                SDL_GetJoystickButton(pad->joystick, 8))
                bSelect = 1;
        }

        if (bR3 && !pad->bR3Held)
            NwAssetToggleHighRes();
        pad->bR3Held = bR3;

#if defined(NW_PS5_NATIVE_TITLE) || defined(NW_PORTABLE_PS5)
        if ((bL3 && !pad->bL3Held) || (bSelect && !pad->bSelectHeld))
            nw_keyboard_toggle();
#endif
        pad->bL3Held = bL3;
        pad->bSelectHeld = bSelect;

        mag = (x < 0 ? -x : x) + (y < 0 ? -y : y);
        if (mag > bestMag) {
            bestMag = mag;
            bestX = x;
            bestY = y;
        }
    }

    if (bestMag >= NW_PAD_DEADZONE) {
        nw_pointer_seed();
        nw_pointer_mark_pad();
        nw_pointer_set(g_nwPointerX +
                           (double)bestX * NW_PAD_SPEED * dt / 32767.0,
                       g_nwPointerY +
                           (double)bestY * NW_PAD_SPEED * dt / 32767.0);
        nw_pointer_warp(g_nwPointerSentX, g_nwPointerSentY, 0);
        nw_pointer_present();
    }
    nw_pointer_update_buttons(bLeft, bRight);
    nw_pointer_refresh_cursor_mode();
}

/* WM_SIZE; the forwarded-handler state writes the UI client extents, the
 * normal state writes the frame-step extents, exactly like the Win32 split. */
NW_SDL_CODE static void nw_input_size(int nW, int nH)
{
    /* The app's logical client is fixed.  A window resize changes only the
     * presentation surface; hit testing maps back through
     * NwSdlWindowToLogical(), so do not feed physical window pixels into the
     * game's layout globals. */
    (void)nW;
    (void)nH;
    if (g_clientWidth == 0)
        g_clientWidth = 640;
    if (g_clientHeight == 0)
        g_clientHeight = 480;
    if (nw_forward_all())
        return;
    NW32(W_WIDTH) = (uint32_t)g_clientWidth;
    NW32(W_HEIGHT) = (uint32_t)g_clientHeight;
}

/* SDL keycode -> Win32 virtual-key code, the value the shipped decoder reads
 * from WM_KEYDOWN.wParam.  Letters map to their uppercase ASCII virtual key;
 * digits and the named keys map to the VK_* constants. */
NW_SDL_CODE static UINT nw_vk_from_sdl(SDL_Keycode key, SDL_Scancode scancode)
{
    (void)scancode;

    if (key >= SDLK_A && key <= SDLK_Z)
        return (UINT)('A' + (key - SDLK_A));
    if (key >= SDLK_0 && key <= SDLK_9)
        return (UINT)('0' + (key - SDLK_0));
    if (key >= SDLK_F1 && key <= SDLK_F12)
        return (UINT)(VK_F1 + (key - SDLK_F1));

    switch (key) {
    case SDLK_RETURN:      return VK_RETURN;
    case SDLK_ESCAPE:      return VK_ESCAPE;
    case SDLK_BACKSPACE:   return VK_BACK;
    case SDLK_TAB:         return VK_TAB;
    case SDLK_SPACE:       return VK_SPACE;
    case SDLK_DELETE:      return VK_DELETE;
    case SDLK_INSERT:      return VK_INSERT;
    case SDLK_HOME:        return VK_HOME;
    case SDLK_END:         return VK_END;
    case SDLK_PAGEUP:      return VK_PRIOR;
    case SDLK_PAGEDOWN:    return VK_NEXT;
    case SDLK_LEFT:        return VK_LEFT;
    case SDLK_RIGHT:       return VK_RIGHT;
    case SDLK_UP:          return VK_UP;
    case SDLK_DOWN:        return VK_DOWN;
    case SDLK_LSHIFT:
    case SDLK_RSHIFT:      return VK_SHIFT;
    case SDLK_LCTRL:
    case SDLK_RCTRL:       return VK_CONTROL;
    case SDLK_LALT:
    case SDLK_RALT:        return VK_MENU;
    case SDLK_CAPSLOCK:    return VK_CAPITAL;
    case SDLK_NUMLOCKCLEAR:return VK_NUMLOCK;
    case SDLK_SCROLLLOCK:  return VK_SCROLL;
    case SDLK_PRINTSCREEN: return VK_SNAPSHOT;
    case SDLK_PAUSE:       return VK_PAUSE;
    case SDLK_KP_0:        return VK_NUMPAD0;
    case SDLK_KP_1:        return VK_NUMPAD1;
    case SDLK_KP_2:        return VK_NUMPAD2;
    case SDLK_KP_3:        return VK_NUMPAD3;
    case SDLK_KP_4:        return VK_NUMPAD4;
    case SDLK_KP_5:        return VK_NUMPAD5;
    case SDLK_KP_6:        return VK_NUMPAD6;
    case SDLK_KP_7:        return VK_NUMPAD7;
    case SDLK_KP_8:        return VK_NUMPAD8;
    case SDLK_KP_9:        return VK_NUMPAD9;
    case SDLK_KP_MULTIPLY: return VK_MULTIPLY;
    case SDLK_KP_PLUS:     return VK_ADD;
    case SDLK_KP_MINUS:    return VK_SUBTRACT;
    case SDLK_KP_PERIOD:   return VK_DECIMAL;
    case SDLK_KP_DIVIDE:   return VK_DIVIDE;
    case SDLK_COMMA:       return VK_OEM_COMMA;
    case SDLK_PERIOD:      return VK_OEM_PERIOD;
    case SDLK_MINUS:       return VK_OEM_MINUS;
    case SDLK_EQUALS:      return VK_OEM_PLUS;
    case SDLK_SEMICOLON:   return VK_OEM_1;
    case SDLK_SLASH:       return VK_OEM_2;
    case SDLK_GRAVE:       return VK_OEM_3;
    case SDLK_LEFTBRACKET: return VK_OEM_4;
    case SDLK_BACKSLASH:   return VK_OEM_5;
    case SDLK_RIGHTBRACKET:return VK_OEM_6;
    case SDLK_APOSTROPHE:  return VK_OEM_7;
    default:               return 0;
    }
}

NW_SDL_CODE static void nw_input_text(const char *pszText)
{
    const unsigned char *p = (const unsigned char *)pszText;

    while (p != NULL && *p != '\0') {
        uint32_t code = *p++;

        if (code >= 0x80u) {
            int extra = 0;
            if ((code & 0xe0u) == 0xc0u) { extra = 1; code &= 0x1fu; }
            else if ((code & 0xf0u) == 0xe0u) { extra = 2; code &= 0x0fu; }
            else if ((code & 0xf8u) == 0xf0u) { extra = 3; code &= 0x07u; }
            else { continue; }
            while (extra-- > 0 && (*p & 0xc0u) == 0x80u)
                code = (code << 6) | (uint32_t)(*p++ & 0x3fu);
        }

        /* Android IMEs commit Enter as a newline, while the shipped decoder
         * uses the Win32 carriage-return code.  Collapse CRLF and translate
         * both forms so the soft keyboard's Enter reaches the game. */
        if (code == '\r' && *p == '\n')
            p++;
        if (code == '\n' || code == '\r')
            code = '\r';

        /* The shipped decoder stores the character in a byte. */
        nw_input_char(code & 0xffu);
    }
}

#if defined(__ANDROID__)
/* SDL turns a touch contact into a synthetic left-button event.  Route that
 * event through the selected click mode while leaving a real mouse or pen
 * untouched. */
NW_SDL_CODE static Uint8 nw_android_touch_button(
    const SDL_MouseButtonEvent *pButton)
{
    if (pButton->button == SDL_BUTTON_LEFT &&
        pButton->which == SDL_TOUCH_MOUSEID &&
        NwSdlAndroidMouseRight())
        return SDL_BUTTON_RIGHT;
    return pButton->button;
}
#endif

/* Returns 1 when the process should shut down immediately, 0 to keep going. */
NW_SDL_CODE static int nw_handle_sdl_event(const SDL_Event *pEvent)
{
    switch (pEvent->type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_DESTROYED:
        return 1;

    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        if (nw_forward_all()) {
            g_closeRequested = 1;
            PlayMciWavesCommandIfDeviceArmed(0u, 0x808u);
            return 0;
        }
        DisplayHelpAndDestroyMainWindow();
        return 1;

    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        NwSdlEnforceWindowAspect();
        nw_input_size((int)g_clientWidth, (int)g_clientHeight);
        NwSdlVideoPresent();
        return 0;

    case SDL_EVENT_WINDOW_MAXIMIZED:
        NwSdlEnforceWindowAspect();
        NwSdlVideoPresent();
        return 0;

    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
        NwSdlUpdateWindowLimits();
        NwSdlEnforceWindowAspect();
        NwSdlVideoPresent();
        return 0;

    case SDL_EVENT_WINDOW_MINIMIZED:
        g_bSdlMinimized = 1;
        return 0;

    case SDL_EVENT_WINDOW_EXPOSED:
    case SDL_EVENT_WINDOW_SHOWN:
        NwSdlVideoPresent();
        return 0;

    case SDL_EVENT_WINDOW_RESTORED:
        g_bSdlMinimized = 0;
        NwSdlEnforceWindowAspect();
        NwSdlVideoPresent();
        return 0;

    case SDL_EVENT_GAMEPAD_ADDED:
        nw_pointer_open_gamepad(pEvent->gdevice.which);
        return 0;

    case SDL_EVENT_GAMEPAD_REMOVED:
        nw_pointer_close(pEvent->gdevice.which);
        return 0;

    case SDL_EVENT_JOYSTICK_ADDED:
        if (!SDL_IsGamepad(pEvent->jdevice.which))
            nw_pointer_open_joystick(pEvent->jdevice.which);
        return 0;

    case SDL_EVENT_JOYSTICK_REMOVED:
        nw_pointer_close(pEvent->jdevice.which);
        return 0;

    case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
    case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
        nw_pointer_touchpad(pEvent->gtouchpad.x, pEvent->gtouchpad.y);
        return 0;

    case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
        return 0;

    case SDL_EVENT_MOUSE_MOTION:
#if defined(__ANDROID__)
        if (NwSdlAndroidControlPressed() != 0)
            return 0;
#endif
        if (!nw_pointer_warp_echo()) {
            int lx = 0;
            int ly = 0;
            int bInside = NwSdlWindowToLogical((int)pEvent->motion.x,
                                               (int)pEvent->motion.y,
                                               &lx, &ly);
            nw_pointer_mark_mouse();
            nw_pointer_sync_mouse(lx, ly);
            if (bInside)
                nw_input_mouse_move(lx, ly);
        }
        return 0;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
#if defined(__ANDROID__)
        /* The Android-only controls live in the letterbox bars.  Check them
         * before the viewport mapping, and consume synthetic touch mouse
         * events so a button tap never reaches the game. */
        if (pEvent->button.button == SDL_BUTTON_LEFT) {
            int control = NwSdlAndroidControlHit((int)pEvent->button.x,
                                                (int)pEvent->button.y);
            if (control != 0) {
                NwSdlAndroidControlPress(control);
                NwSdlVideoPresentNow();
                return 0;
            }
        }
#endif
        {
            int button = (int)pEvent->button.button;
            int lx = 0;
            int ly = 0;
            int bRight;
            int bInside;

#if defined(__ANDROID__)
            button = nw_android_touch_button(&pEvent->button);
#endif
            if (button == SDL_BUTTON_LEFT || button == SDL_BUTTON_RIGHT) {
                bRight = button == SDL_BUTTON_RIGHT;
                bInside = NwSdlWindowToLogical((int)pEvent->button.x,
                                               (int)pEvent->button.y,
                                               &lx, &ly);
                nw_pointer_mark_mouse();
                if (!bInside) {
                    if (bRight)
                        g_nwMouseRightDownInside = 0;
                    else
                        g_nwMouseLeftDownInside = 0;
                    return 0;
                }
                if (bRight)
                    g_nwMouseRightDownInside = 1;
                else
                    g_nwMouseLeftDownInside = 1;
                nw_pointer_sync_mouse(lx, ly);
                nw_input_mouse_down(lx, ly);
            }
        }
        return 0;

    case SDL_EVENT_MOUSE_BUTTON_UP:
#if defined(__ANDROID__)
        if (pEvent->button.button == SDL_BUTTON_LEFT &&
            NwSdlAndroidControlPressed() != 0) {
            int action = NwSdlAndroidControlRelease((int)pEvent->button.x,
                                                    (int)pEvent->button.y);

            if (action == NW_ANDROID_CONTROL_EXIT)
                return 1;      /* Exit control */
            if (action == NW_ANDROID_CONTROL_KEYBOARD) {
                (void)NwSdlAndroidToggleKeyboard();
            } else if (action == NW_ANDROID_CONTROL_LOCK) {
                (void)NwSdlAndroidToggleRotationLock();
            } else if (action == NW_ANDROID_CONTROL_CLICK) {
                (void)NwSdlAndroidToggleMouseRight();
            }
            NwSdlVideoPresentNow();
            return 0;
        }
#endif
        {
            int button = (int)pEvent->button.button;
            int lx = 0;
            int ly = 0;
            int bRight;
            int bDownInside;

#if defined(__ANDROID__)
            button = nw_android_touch_button(&pEvent->button);
#endif
            if (button == SDL_BUTTON_LEFT || button == SDL_BUTTON_RIGHT) {
                bRight = button == SDL_BUTTON_RIGHT;
                bDownInside = bRight ? g_nwMouseRightDownInside
                                     : g_nwMouseLeftDownInside;

                /* Still map and clamp the release even outside the viewport:
                 * this ends a drag that began in the game without leaving a
                 * game button latched.  A press that began in a black bar is
                 * ignored. */
                (void)NwSdlWindowToLogical((int)pEvent->button.x,
                                           (int)pEvent->button.y, &lx, &ly);
                if (bRight)
                    g_nwMouseRightDownInside = 0;
                else
                    g_nwMouseLeftDownInside = 0;
                if (bDownInside) {
                    nw_pointer_mark_mouse();
                    nw_pointer_sync_mouse(lx, ly);
                    nw_input_mouse_up(lx, ly, bRight);
                }
            }
        }
        return 0;

#if defined(__ANDROID__)
    case SDL_EVENT_FINGER_CANCELED:
        if (NwSdlAndroidControlPressed() != 0) {
            NwSdlAndroidControlCancel();
            NwSdlVideoPresentNow();
            return 0;
        }
        return 0;
#endif

    case SDL_EVENT_KEY_DOWN: {
        UINT vk = nw_vk_from_sdl(pEvent->key.key, pEvent->key.scancode);
        int bLeftShift = (SDL_GetModState() & SDL_KMOD_LSHIFT) != 0;

        if (vk == 0)
            return 0;

#if !defined(NW_PORTABLE_PS5) && !defined(NW_PS5_NATIVE_TITLE) && \
    !defined(__ANDROID__)
        /* PC fullscreen hotkey.  The typed-token field uses F as game input,
         * so leave it alone while that mode is active. */
        if (vk == 'F') {
            if (NW8(W_FIELD_MODE) == 0u) {
                g_nwFullscreenFSuppressText = 1;
                if (!pEvent->key.repeat) {
                    if (NwSdlToggleFullscreen())
                        NwSdlVideoPresentNow();
                }
                return 0;
            }
            g_nwFullscreenFSuppressText = 0;
        }
#endif

#if !defined(__ANDROID__)
        /* Global asset-mode hotkey.  The typed-token field uses H as game
         * input, so leave it alone while that mode is active.  Android ships
         * the standard overlay, so H remains ordinary text there. */
        if (vk == 'H') {
            if (NW8(W_FIELD_MODE) == 0u) {
                g_nwAssetHSuppressText = 1;
                if (!pEvent->key.repeat)
                    NwAssetToggleHighRes();
                return 0;
            }
            g_nwAssetHSuppressText = 0;
        }
#endif

        nw_input_keydown(vk, bLeftShift);

        /* Control keys do not produce SDL_EVENT_TEXT_INPUT. */
        if (vk == VK_RETURN)
            nw_input_char('\r');
        else if (vk == VK_BACK)
            nw_input_char('\b');
        else if (vk == VK_TAB)
            nw_input_char('\t');
        return 0;
    }

    case SDL_EVENT_KEY_UP: {
        UINT vk = nw_vk_from_sdl(pEvent->key.key, pEvent->key.scancode);
        if (vk == 'H')
            g_nwAssetHSuppressText = 0;
        if (vk == 'F')
            g_nwFullscreenFSuppressText = 0;
        return 0;
    }

    case SDL_EVENT_TEXT_INPUT:
        if (g_nwFullscreenFSuppressText) {
            const char *p = pEvent->text.text;
            int bOnlyF = (p != NULL && p[0] != '\0');

            while (p != NULL && *p != '\0') {
                if (*p != 'f' && *p != 'F') {
                    bOnlyF = 0;
                    break;
                }
                p++;
            }
            if (bOnlyF)
                return 0;
            g_nwFullscreenFSuppressText = 0;
        }
        if (g_nwAssetHSuppressText) {
            const char *p = pEvent->text.text;
            int bOnlyH = (p != NULL && p[0] != '\0');

            while (p != NULL && *p != '\0') {
                if (*p != 'h' && *p != 'H') {
                    bOnlyH = 0;
                    break;
                }
                p++;
            }
            if (bOnlyH)
                return 0;
            g_nwAssetHSuppressText = 0;
        }
        nw_input_text(pEvent->text.text);
        return 0;

    default:
        return 0;
    }
}

/* Drains the SDL event queue from inside the frame-step pump.  The Win32 build
 * processes keyboard/mouse messages in DispatchQueuedMessagesAndStampStartTime
 * (PeekMessage + DispatchMessage -> GmwPlayerWndProc); on SDL the equivalent
 * messages belong to SDL's WndProc and are surfaced as SDL events instead.
 * Long synchronous game sequences (the startup splash/intro, record-load
 * animations) call the frame-step pump without returning to RunMainMessageLoop,
 * so without this the whole intro ignores input.  Returns 1 when a quit/close
 * event asks the caller to unwind. */
NW_SDL_CODE int NwSdlPumpEventsForNestedPump(void)
{
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        if (nw_handle_sdl_event(&event) != 0)
            return 1;
    }
    nw_pointer_update();
    return 0;
}

NW_SDL_CODE int __stdcall
RunMainMessageLoop(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
    __asm__("_RunMainMessageLoop");

NW_SDL_CODE int __stdcall
RunMainMessageLoop(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    char szPath[0x100];
    DWORD dwNow;
    uint32_t bPointerInsideClient;
    uint64_t nwLastEventTotal = 0;
    uint64_t nwLastPresentTotal = 0;
    uint64_t nwLastConvTotal = 0;
    uint64_t nwLastUpdateTotal = 0;
    uint64_t nwLastAudioTotal = 0;
    uint64_t nwLastFrameStartUs = 0;
    uint64_t nwLastEvents = 0;
    uint64_t nwLastPresents = 0;
    unsigned long nwFrameIndex = 0;
    int nwProf;

    (void)hPrevInstance;
    (void)lpCmdLine;

    (void)GetModuleFileNameA(hInstance, szPath, (DWORD)sizeof szPath);

    NW_WINDOW_PTR_REF(HINSTANCE, W_HINSTANCE) = hInstance;
    NW32(W_SHOWCMD) = (uint32_t)nCmdShow;

    NW32(0x5f00u) = 0u;
    NW32(0x5f04u) = 0u;
    NW32(0x5f08u) = 0u;
    NW32(0x5f0cu) = 0u;
    NW32(W_IDLE_OR_ERROR) = 0u;
    NW32(W_FRAME_INTERVAL) = 0u;
    NW32(0x605cu) = 0u;
    NW32(0x6060u) = 0u;
    NW32(0x6064u) = 0u;
    NW32(0x60bcu) = 0u;
    NW32(W_MAIN_HWND) = 0u;
    NW32(W_ANCHOR_Y) = 0u;
    NW32(W_ANCHOR_X) = 0u;
    NW32(W_CURSOR_Y) = 0u;
    NW32(W_CURSOR_X) = 0u;
    NW32(W_HEIGHT) = 0u;
    NW32(W_WIDTH) = 0u;

    NW32(0x5f58u) = 0xffffffffu;
    NW32(0x5f54u) = 0xffffffffu;
    NW8(W_KEY_BYTE) = NW_INPUT_CLEAR;
    NW8(W_MODIFIER_BYTE) = NW_INPUT_CLEAR;
    NW8(W_CLICK_BYTE) = NW_INPUT_CLEAR;

    /* The shipped app-path slot is only 0x40 bytes.  Truncate the copy that
     * legacy movie/save helpers read, but initialize from the full module path
     * so overlay lookup and long install directories keep working. */
    (void)strncpy((char *)(nw_data_window + W_APP_PATH), szPath,
                  W_APP_PATH_BYTES - 1u);
    ((char *)(nw_data_window + W_APP_PATH))[W_APP_PATH_BYTES - 1u] = '\0';
    NW_NATIVE_BOOT("loop_begin");
    if (InitializeAppSubsystemsFromSavedPath(szPath) == 0u) {
        NW_NATIVE_BOOT("loop_init_fail");
        FlushDeferredErrorString();
        DisplayHelpAndDestroyMainWindow();
        ShutdownAndReleaseResources();
        return 0;
    }
    NW_NATIVE_BOOT("loop_init_ok");

    BuildHlpSiblingPath(szPath);
    if (g_nwSdlWindow != NULL)
        (void)SDL_RaiseWindow(g_nwSdlWindow);
    NW_NATIVE_BOOT("loop_enter");
    NwSdlVideoSetMainLoopActive(1);

    g_nwPerfLastFrameEnd = 0;
    g_nwPerfWindowStart = 0;
    g_nwPerfFrames = 0;
    g_nwPerfStepTicks = 0;
    g_nwPerfPresentTicks = 0;
    g_nwPerfPumpTicks = 0;
    g_nwPerfPumpMaxTicks = 0;
    g_nwPerfPumpFrameTicks = 0;
    g_nwPerfPumpFrameMaxTicks = 0;
    g_nwPerfAssetFrameTicks = 0;
    g_nwPerfAssetFrameCalls = 0;
    g_nwPerfAssetFrameMisses = 0;
    g_nwPerfComposeFrameTicks = 0;
    g_nwPerfComposeFrameFrames = 0;
    g_nwPerfAssetTicks = 0;
    g_nwPerfAssetMaxTicks = 0;
    g_nwPerfAssetCalls = 0;
    g_nwPerfAssetMisses = 0;
    /* Drop the pre-loop intro/preload samples; the first gameplay frame
     * should report only its own work. */
    NwAssetTakeLoadStats(NULL, NULL, NULL);
    NwSdlVideoTakeComposeStats(NULL, NULL);

    if (g_nwPerfFreq == 0)
        g_nwPerfFreq = (uint64_t)SDL_GetPerformanceFrequency();
    nwProf = nw_prof_on();
    if (nwProf) {
        /* Startup rendering happened before the menu; start the sample here. */
        memset(g_nwProfTicks, 0, sizeof g_nwProfTicks);
        g_nwProfPresents = 0;
        g_nwProfEvents = 0;
        NwProfEvent(9u, 0u, 0u);   /* main loop entered / sample starts */
    }

    for (;;) {
        int nwFrameProf = nwProf;
        uint64_t nwIterStart = NwProfNow();
        uint64_t nwTmp;
        SDL_Event event;
        static int nwBootLoopMark;

        if (nwBootLoopMark == 0) {
            nwBootLoopMark = 1;
            NW_NATIVE_BOOT("loop_tick");
        }

        /* Keep the SFX stream fed even while SDL is handing us a burst of
         * events (the Win32 loop drains waveOut completion messages on every
         * iteration for the same reason). */
#if defined(NW_PS5_NATIVE_TITLE) || defined(NW_PORTABLE_PS5)
        {
            const uint64_t nwPumpStart = NwProfNow();
#endif
        if (nwFrameProf) {
            nwTmp = NwProfNow();
            NwPcmSink_Pump();
            NwProfAdd(NWP_AUDIO, nwTmp, NwProfNow());
        } else {
            NwPcmSink_Pump();
        }
#if defined(NW_PS5_NATIVE_TITLE) || defined(NW_PORTABLE_PS5)
            const uint64_t nwPumpDelta = NwProfNow() - nwPumpStart;
            g_nwPerfPumpFrameTicks += nwPumpDelta;
            if (nwPumpDelta > g_nwPerfPumpFrameMaxTicks)
                g_nwPerfPumpFrameMaxTicks = nwPumpDelta;
        }
#endif

        {
            uint64_t nwEventStart = nwFrameProf ? NwProfNow() : 0;
            int nwHasEvent = SDL_PollEvent(&event);

            if (nwHasEvent) {
                int nwQuit = nw_handle_sdl_event(&event);
                if (nwFrameProf)
                    NwProfAdd(NWP_EVENT, nwEventStart, NwProfNow());
                if (nwQuit != 0) {
                    NW_NATIVE_BOOT("loop_quit");
                    ShutdownAndReleaseResources();
                    return 0;
                }
                continue;
            }
            if (nwFrameProf)
                NwProfAdd(NWP_EVENT, nwEventStart, NwProfNow());
        }

        if (NW32(W_IDLE_OR_ERROR) == 1u) {
            NW_NATIVE_BOOT("loop_idle_exit");
            ShutdownAndReleaseResources();
            return 0;
        }
        if (NW32(W_IDLE_OR_ERROR) != 0u) {
            FlushDeferredErrorString();
            DisplayHelpAndDestroyMainWindow();
            continue;
        }

        if (g_bSdlMinimized != 0) {
            SDL_Delay(1);
            continue;
        }

        nw_pointer_update();

        dwNow = (DWORD)SDL_GetTicks();
        if ((uint32_t)(NW32(W_LAST_FRAME_TIME) + NW32(W_FRAME_INTERVAL)) >= dwNow) {
            /* The Win32 oracle can spin in PeekMessage because the Windows
             * scheduler preempts it.  The portable loop owns the whole CPU
             * while waiting for the next frame, which starves the X/Wayland
             * compositor on WSLg and makes menus and gameplay look slow even
             * though step_us is tiny.  Yield for one tick; events still wake
             * the loop immediately. */
            SDL_Delay(1);
            continue;
        }
        if (nwBootLoopMark == 1) {
            nwBootLoopMark = 2;
            NW_NATIVE_BOOT("loop_frame");
        }
        NW32(W_LAST_FRAME_TIME) = dwNow;
#if defined(NW_PS5_NATIVE_TITLE) || defined(NW_PORTABLE_PS5)
        nw_text_queue_pump();
#endif
        bPointerInsideClient = 1u;

        {
            uint32_t nwClickByte = NW32(W_CLICK_BYTE);
            uint32_t nwKeyByte = NW32(W_KEY_BYTE);
            uint64_t nwStepStart = NwProfNow();
            uint64_t nwStepPresent0 = nwFrameProf
                ? NwProfTotal(NWP_PRESENT) : 0;
            uint32_t nwStepResult;

        if ((((const uint8_t *)NW_WINDOW_PTR_VALUE(W_CURSOR_FLAGS))[4] & 0x02u) != 0u) {
            float gx = 0.0f;
            float gy = 0.0f;
            int wx = 0;
            int wy = 0;

            (void)SDL_GetGlobalMouseState(&gx, &gy);
            if (g_nwSdlWindow != NULL)
                (void)SDL_GetWindowPosition(g_nwSdlWindow, &wx, &wy);
            {
                int lx = 0;
                int ly = 0;
                if (!NwSdlWindowToLogical((int)gx - wx,
                                          (int)gy - wy, &lx, &ly))
                    bPointerInsideClient = 0u;
            }
        }

        nwStepResult = StoreArgsAndProcessQueuedRecords(
                bPointerInsideClient,
                NW32(W_KEY_BYTE),
                NW32(W_MODIFIER_BYTE),
                NW32(W_CLICK_BYTE),
                NW32(W_ANCHOR_Y),
                NW32(W_ANCHOR_X),
                NW32(W_CURSOR_Y),
                NW32(W_CURSOR_X),
                NW32(W_HEIGHT),
                NW32(W_WIDTH));

#if defined(NW_PS5_NATIVE_TITLE) || defined(NW_PORTABLE_PS5)
        {
            uint64_t nwAssetNs = 0;
            uint64_t nwComposeNs = 0;
            unsigned nwAssetCalls = 0;
            unsigned nwAssetMisses = 0;
            unsigned nwComposeFrames = 0;

            NwAssetTakeLoadStats(&nwAssetNs, &nwAssetCalls, &nwAssetMisses);
            NwSdlVideoTakeComposeStats(&nwComposeNs, &nwComposeFrames);
            g_nwPerfAssetFrameTicks += nwAssetNs;
            g_nwPerfAssetFrameCalls += nwAssetCalls;
            g_nwPerfAssetFrameMisses += nwAssetMisses;
            g_nwPerfComposeFrameTicks += nwComposeNs;
            g_nwPerfComposeFrameFrames += nwComposeFrames;
        }
#endif

        if (nwStepResult != 0u) {
            uint64_t nwStepEnd = NwProfNow();
            uint64_t nwStepPresent1 = nwFrameProf
                ? NwProfTotal(NWP_PRESENT) : 0;
            uint64_t nwFinalStart = NwProfNow();
            uint64_t nwBlitNs = 0;
            unsigned nwBlitCalls = 0;

            NwBlitTakeStats(&nwBlitNs, &nwBlitCalls);

            /* Some screens (including the main menu) draw directly into the
             * 8bpp framebuffer without a per-rect blit.  A frame-rate present
             * keeps the window surface in sync with that path.  This is the
             * one present per frame; batch presents are suppressed. */
            NwSdlVideoPresentFrame();

            {
                const uint64_t nwPerfEnd = NwProfNow();
                const uint64_t nwPerfPresent = nwPerfEnd - nwFinalStart;

#if defined(NW_PS5_NATIVE_TITLE) || defined(NW_PORTABLE_PS5)
                g_nwPerfPumpTicks += g_nwPerfPumpFrameTicks;
                if (g_nwPerfPumpFrameMaxTicks > g_nwPerfPumpMaxTicks)
                    g_nwPerfPumpMaxTicks = g_nwPerfPumpFrameMaxTicks;
                g_nwPerfAssetTicks += g_nwPerfAssetFrameTicks;
                if (g_nwPerfAssetFrameTicks > g_nwPerfAssetMaxTicks)
                    g_nwPerfAssetMaxTicks = g_nwPerfAssetFrameTicks;
                g_nwPerfAssetCalls += g_nwPerfAssetFrameCalls;
                g_nwPerfAssetMisses += g_nwPerfAssetFrameMisses;

                /* A stall line is more useful than a 60-frame average when the
                 * hitch is intermittent.  Use the game's current frame target:
                 * a 16 ms target logs a missed 60 Hz vblank, while a 71 ms
                 * startup-animation target only logs a severe overrun. */
                if (NwProfOn() && g_nwPerfLastFrameEnd != 0 && g_nwPerfFreq != 0) {
                    const uint64_t interval = nwPerfEnd - g_nwPerfLastFrameEnd;
                    const uint32_t target_ms = NW32(W_FRAME_INTERVAL);
                    uint32_t threshold_ms;

                    if (target_ms == 0)
                        threshold_ms = 40u;
                    else if (target_ms <= 40u)
                        threshold_ms = target_ms + 10u;
                    else
                        threshold_ms = target_ms * 2u + 20u;
                    if (threshold_ms < 25u)
                        threshold_ms = 25u;
                    if (interval > (uint64_t)threshold_ms * g_nwPerfFreq / 1000u) {
                        char stage[256];
                        const unsigned interval_us = (unsigned)
                            ((interval * 1000000u) / g_nwPerfFreq);
                        const unsigned step_us = (unsigned)
                            (((nwStepEnd - nwStepStart) * 1000000u) / g_nwPerfFreq);
                        const unsigned present_us = (unsigned)
                            ((nwPerfPresent * 1000000u) / g_nwPerfFreq);
                        const unsigned pump_us = (unsigned)
                            ((g_nwPerfPumpFrameTicks * 1000000u) / g_nwPerfFreq);
                        const unsigned pump_max_us = (unsigned)
                            ((g_nwPerfPumpFrameMaxTicks * 1000000u) / g_nwPerfFreq);
                        const unsigned asset_us = (unsigned)
                            ((g_nwPerfAssetFrameTicks * 1000000u) / g_nwPerfFreq);
                        const unsigned comp_us = (unsigned)
                            ((g_nwPerfComposeFrameTicks * 1000000u) / g_nwPerfFreq);
                        const unsigned blit_us = (unsigned)
                            ((nwBlitNs * 1000000u) / g_nwPerfFreq);
                        const unsigned blit_calls = (unsigned)nwBlitCalls;
                        const unsigned step_other_us =
                            step_us > blit_us ? step_us - blit_us : 0u;
                        unsigned other_us = 0;
                        const unsigned accounted_us =
                            step_us + present_us + pump_us;

                        if (interval_us > accounted_us)
                            other_us = interval_us - accounted_us;

                        snprintf(stage, sizeof stage,
                                 "stall_int_%u_step_%u_pres_%u_pump_%u_pmax_%u_other_%u_asset_%u_ac_%u_am_%u_comp_%u_cf_%u_blit_%u_bc_%u_so_%u_t%u",
                                 interval_us, step_us, present_us, pump_us,
                                 pump_max_us, other_us, asset_us,
                                 (unsigned)g_nwPerfAssetFrameCalls,
                                 (unsigned)g_nwPerfAssetFrameMisses, comp_us,
                                 (unsigned)g_nwPerfComposeFrameFrames,
                                 blit_us, blit_calls, step_other_us,
                                 (unsigned)target_ms);
                        NW_NATIVE_LOG(stage);
                    }
                }
                g_nwPerfLastFrameEnd = nwPerfEnd;
                g_nwPerfPumpFrameTicks = 0;
                g_nwPerfPumpFrameMaxTicks = 0;
                g_nwPerfAssetFrameTicks = 0;
                g_nwPerfAssetFrameCalls = 0;
                g_nwPerfAssetFrameMisses = 0;
                g_nwPerfComposeFrameTicks = 0;
                g_nwPerfComposeFrameFrames = 0;
#endif

                if (g_nwPerfFrames == 0)
                    g_nwPerfWindowStart = nwIterStart;
                g_nwPerfFrames++;
                g_nwPerfStepTicks += nwStepEnd - nwStepStart;
                g_nwPerfPresentTicks += nwPerfPresent;
                if (NwProfOn() && g_nwPerfFrames >= 60 && g_nwPerfFreq != 0) {
                    const uint64_t elapsed = nwPerfEnd - g_nwPerfWindowStart;
                    if (elapsed != 0) {
                        char stage[160];
                        const unsigned fps = (unsigned)
                            ((g_nwPerfFrames * g_nwPerfFreq) / elapsed);
                        const unsigned frame_us = (unsigned)
                            ((elapsed * 1000000u) /
                             (g_nwPerfFreq * g_nwPerfFrames));
                        const unsigned step_us = (unsigned)
                            ((g_nwPerfStepTicks * 1000000u) /
                             (g_nwPerfFreq * g_nwPerfFrames));
                        const unsigned present_us = (unsigned)
                            ((g_nwPerfPresentTicks * 1000000u) /
                             (g_nwPerfFreq * g_nwPerfFrames));
                        const unsigned pump_us = (unsigned)
                            ((g_nwPerfPumpTicks * 1000000u) /
                             (g_nwPerfFreq * g_nwPerfFrames));
                        const unsigned pump_max_us = (unsigned)
                            ((g_nwPerfPumpMaxTicks * 1000000u) / g_nwPerfFreq);
                        const unsigned asset_us = (unsigned)
                            ((g_nwPerfAssetTicks * 1000000u) /
                             (g_nwPerfFreq * g_nwPerfFrames));
                        const unsigned asset_max_us = (unsigned)
                            ((g_nwPerfAssetMaxTicks * 1000000u) / g_nwPerfFreq);
                        const unsigned asset_calls = (unsigned)
                            (g_nwPerfAssetCalls / g_nwPerfFrames);
                        const unsigned asset_misses = (unsigned)
                            (g_nwPerfAssetMisses / g_nwPerfFrames);

                        snprintf(stage, sizeof stage,
                                 "perf_fps_%u_frame_%u_step_%u_present_%u_pump_%u_pmax_%u_asset_%u_amax_%u_ac_%u_am_%u",
                                 fps, frame_us, step_us, present_us,
                                 pump_us, pump_max_us, asset_us,
                                 asset_max_us, asset_calls, asset_misses);
                        NW_NATIVE_LOG(stage);
                    }
                    g_nwPerfWindowStart = nwPerfEnd;
                    g_nwPerfFrames = 0;
                    g_nwPerfStepTicks = 0;
                    g_nwPerfPresentTicks = 0;
                    g_nwPerfPumpTicks = 0;
                    g_nwPerfPumpMaxTicks = 0;
                    g_nwPerfAssetTicks = 0;
                    g_nwPerfAssetMaxTicks = 0;
                    g_nwPerfAssetCalls = 0;
                    g_nwPerfAssetMisses = 0;
                }
            }

            if (nwFrameProf) {
                uint64_t nwEnd = NwProfNow();
                uint64_t nwPresentTotal = NwProfTotal(NWP_PRESENT);
                uint64_t nwConvTotal = NwProfTotal(NWP_CONV);
                uint64_t nwUpdateTotal = NwProfTotal(NWP_UPDATE);
                uint64_t nwEventTotal = NwProfTotal(NWP_EVENT);
                uint64_t nwAudioTotal = NwProfTotal(NWP_AUDIO);
                unsigned long t = nw_prof_us(nwIterStart);

                fprintf(g_nwProfFile,
                        "F,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,"
                        "%lu,%lu,%lu,%lu,%lu\n",
                        nwFrameIndex,
                        t,
                        nwFrameIndex == 0 ? 0
                            : t - (unsigned long)nwLastFrameStartUs,
                        nw_prof_us(nwEnd - nwIterStart),
                        nw_prof_us(nwEventTotal - nwLastEventTotal),
                        nw_prof_us(nwStepEnd - nwStepStart),
                        nw_prof_us(nwStepPresent1 - nwStepPresent0),
                        nw_prof_us(nwPresentTotal - nwStepPresent1),
                        nw_prof_us(nwPresentTotal - nwLastPresentTotal),
                        nw_prof_us(nwConvTotal - nwLastConvTotal),
                        nw_prof_us(nwUpdateTotal - nwLastUpdateTotal),
                        nw_prof_us(nwAudioTotal - nwLastAudioTotal),
                        (unsigned long)(g_nwProfEvents - nwLastEvents),
                        (unsigned long)(g_nwProfPresents - nwLastPresents),
                        (unsigned long)nwClickByte,
                        (unsigned long)nwKeyByte,
                        (unsigned long)nwStepResult);
                fflush(g_nwProfFile);

                nwFrameIndex++;
                nwLastFrameStartUs = t;
                nwLastEventTotal = nwEventTotal;
                nwLastPresentTotal = nwPresentTotal;
                nwLastConvTotal = nwConvTotal;
                nwLastUpdateTotal = nwUpdateTotal;
                nwLastAudioTotal = nwAudioTotal;
                nwLastEvents = g_nwProfEvents;
                nwLastPresents = g_nwProfPresents;
            }

            NW8(W_KEY_BYTE) = NW_INPUT_CLEAR;
            NW8(W_MODIFIER_BYTE) = NW_INPUT_CLEAR;
            NW8(W_CLICK_BYTE) = NW_INPUT_CLEAR;
            continue;
        }
        } /* profiler capture block */

        DisplayHelpAndDestroyMainWindow();
        ShutdownAndReleaseResources();
        return 0;
    }
}
#endif /* NW_UNIT_RunMainMessageLoopSdl */
