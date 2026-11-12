#include "nwarch.h"
#include "nwdebug.h"
/*
 * WindowChromeSdl -- SDL3 player window creation for the removable platform
 * seam.
 *
 * Module: ui/sdl3
 * Interface: same external symbol as ui/win32/WindowChrome.c
 *
 * The Win32 backend still computes the frame metrics with GetSystemMetrics so
 * the data-window globals and the effective client size match the oracle on
 * the same host.  SDL creates the native window; the Win32 HWND is read back
 * from the SDL window properties and published in g_hWndPlayer, so the deferred
 * MCI/help paths keep a real HWND.  Input is translated by MainLoopSdl.c.
 *
 * This unit is selected for SDL desktop builds; the Win32 object is dropped from
 * the link.  Code and state live in the private SDL sections so the game's
 * fixed .text/.data window is untouched.
 */

#ifdef NW_UNIT_CreatePlayerMainWindowSdl

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_error.h>

#include "NwAppIcon.h"
#include "NwSdlAndroidControls.h"

#if defined(__ANDROID__)
#include <jni.h>
#include <SDL3/SDL_system.h>
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define NW_SDL_CODE __attribute__((section(".nwsvg")))
#define NW_SDL_BSS  __attribute__((section(".nwsvgbss")))

extern unsigned char nw_data_window[];

#if defined(NW_PS5_NATIVE_TITLE) || defined(NW_PORTABLE_PS5)
extern void platform_boot_mark(const char *stage);
#define NW_NATIVE_BOOT(stage) platform_boot_mark(stage)
#else
#define NW_NATIVE_BOOT(stage) ((void)0)
#endif

#define NW32(off) (*(uint32_t *)(nw_data_window + (off)))
#define NWPTR(off) NW_WINDOW_PTR_REF(void, (off))

#define g_hWndPlayer      NWPTR(0x6404)
#define g_hdcPlayer       NWPTR(0x5f00)
#define g_dwClientW       NW32(0x5f20)
#define g_dwClientH       NW32(0x5f24)
#define g_dwClientWSave   NW32(0x5f28)
#define g_dwClientHSave   NW32(0x5f2c)
#define g_dwScreenW       NW32(0x5f30)
#define g_dwScreenH       NW32(0x5f34)
#define g_dwFrameBorderH  NW32(0x5f38)
#define g_dwFrameBorderV  NW32(0x5f3c)
#define g_cfgWidthSave    NW32(0x69c)
#define g_cfgHeightSave   NW32(0x6a0)

extern uint16_t g_clientWidth;
extern uint16_t g_clientHeight;

/* The config struct is the shipped one; offsets are the ABI. */
typedef struct tagPlayerConfigSdl {
    unsigned char bUnknown00[0x0C];
    int           nClientWidth;
    int           nClientHeight;
    unsigned char bUnknown14[0x10];
    unsigned char bThickFrameMode;
    unsigned char bUnknown25[0x53];
    char          szWindowTitle[1];
} PLAYERCONFIGSDL, *PPLAYERCONFIGSDL;

/* Shared with VideoSdl.c and MainLoopSdl.c. */
SDL_Window *g_nwSdlWindow NW_SDL_BSS;

static SDL_Cursor *g_sdlArrowCursor NW_SDL_BSS;

/* Windowed geometry saved across a fullscreen toggle.  SDL restores most of
 * this itself on some backends, but doing it explicitly keeps the 4:3 window
 * at exactly the size and position the user left it on every backend. */
static int g_nwSdlFullscreen NW_SDL_BSS;
static int g_nwSdlWindowedValid NW_SDL_BSS;
static int g_nwSdlWindowedX NW_SDL_BSS;
static int g_nwSdlWindowedY NW_SDL_BSS;
static int g_nwSdlWindowedW NW_SDL_BSS;
static int g_nwSdlWindowedH NW_SDL_BSS;
static int g_nwSdlWindowedMaximized NW_SDL_BSS;

/* Defined below, after the 4:3 fitting helper. */
NW_SDL_CODE void NwSdlWindowViewport(int *px, int *py, int *pw, int *ph);
NW_SDL_CODE void NwSdlFitViewport4x3(int availW, int availH,
                                     int *px, int *py, int *pw, int *ph);

#define GSM_IDX_SCREEN_W      0     /* matches ui/win32/CreatePlayerMainWindow */
#define GSM_IDX_SCREEN_H      1
#define GSM_IDX_CAPTION       4
#define GSM_IDX_FULLSCREEN_H  5
#define GSM_IDX_IDX6          6
#define GSM_IDX_MAXIMIZED_W   0x20
#define GSM_IDX_MAXIMIZED_H   0x21

NW_SDL_CODE static void nw_publish_client_size(int w, int h)
{
    NW32(0x5f20) = (uint32_t)w;
    NW32(0x5f24) = (uint32_t)h;
    g_clientWidth  = (uint16_t)w;
    g_clientHeight = (uint16_t)h;
}

/* Integer render scale.  This is the size of the game's own 8bpp framebuffer:
 * 640x480 * scale.  The game keeps its logical coordinates, and the drawing
 * entry points expand destination coordinates and source pixels as they write
 * into the larger surface.  The presentation window is independent and stays
 * at 640x480 by default.  NW_RENDER_SCALE is the explicit switch; the old
 * NW_WINDOW_SCALE name is accepted as a compatibility alias for the render
 * scale.  The default is the 4x asset path; set NW_RENDER_SCALE=1 to run at
 * the original framebuffer resolution. */
NW_SDL_CODE int NwSdlRenderScale(void)
{
    static int scale;
    const char *env;
    if (scale != 0)
        return scale;
#if defined(__ANDROID__)
#ifndef NW_ANDROID_RENDER_SCALE
#define NW_ANDROID_RENDER_SCALE 1
#endif
    /* The APK ships the standard-resolution overlay by default.  A build may
     * opt into a higher scale when it also packages matching assets. */
    scale = NW_ANDROID_RENDER_SCALE;
#elif defined(NW_PORTABLE_IOS)
#ifndef NW_IOS_RENDER_SCALE
#define NW_IOS_RENDER_SCALE 1
#endif
    /* The IPA ships the standard-resolution overlay by default, matching the
     * Android package.  NW_RENDER_SCALE can still override it at runtime. */
    scale = NW_IOS_RENDER_SCALE;
#else
    scale = 4;
#endif
    env = getenv("NW_RENDER_SCALE");
    if (env == NULL || env[0] == '\0')
        env = getenv("NW_FRAMEBUFFER_SCALE");
    if (env == NULL || env[0] == '\0')
        env = getenv("NW_WINDOW_SCALE");
    if (env != NULL && env[0] != '\0') {
        int v = atoi(env);
        if (v >= 1)
            scale = v > 8 ? 8 : v;
    }
    return scale;
}

NW_SDL_CODE int NwSdlWindowResizable(void)
{
    static int value = -1;
    const char *env;
    if (value >= 0)
        return value;
    value = 1;
    env = getenv("NW_WINDOW_RESIZABLE");
    if (env != NULL && env[0] != '\0' &&
        (env[0] == '0' || env[0] == 'n' || env[0] == 'N' ||
         env[0] == 'f' || env[0] == 'F'))
        value = 0;
    return value;
}

/* Initial physical presentation window size.  This is independent of the
 * native render scale: NW_RENDER_SCALE controls the off-screen framebuffer,
 * while the window starts at 640x480 unless the caller overrides it.  The
 * window is resizable by default, with its aspect ratio locked to 4:3. */
NW_SDL_CODE static int NwSdlWindowDimension(const char *pszEnv, int nDefault)
{
    const char *env = getenv(pszEnv);
    int value;
    if (env == NULL || env[0] == '\0')
        return nDefault;
    value = atoi(env);
    if (value < 160)
        return nDefault;
    return value > 16384 ? 16384 : value;
}

/* Map a window-space pointer position back to the fixed logical game client.
 * The game is presented into the largest centered 4:3 viewport, so positions
 * in the black letterbox bars are outside the game and must not reach it.
 * Returns 1 when the position is inside that viewport.  On a normal 4:3
 * windowed presentation the viewport is the whole window and this is the same
 * mapping as before fullscreen support. */
NW_SDL_CODE int NwSdlWindowToLogical(int wx, int wy, int *lx, int *ly)
{
    int vx = 0;
    int vy = 0;
    int vw = 0;
    int vh = 0;
    int lw = (int)g_dwClientW;
    int lh = (int)g_dwClientH;
    int inside;

    NwSdlWindowViewport(&vx, &vy, &vw, &vh);
    if (lw <= 0)
        lw = 640;
    if (lh <= 0)
        lh = 480;
    if (vw <= 0)
        vw = lw;
    if (vh <= 0)
        vh = lh;

    inside = (wx >= vx && wy >= vy && wx < vx + vw && wy < vy + vh);
    if (inside) {
        if (lx != NULL)
            *lx = (int)((long)(wx - vx) * lw / vw);
        if (ly != NULL)
            *ly = (int)((long)(wy - vy) * lh / vh);
    } else {
        /* Clamp for button-release and cursor bookkeeping so dragging from
         * the game into a bar cannot leave a game button latched down. */
        if (lx != NULL) {
            if (wx < vx)
                *lx = 0;
            else if (wx >= vx + vw)
                *lx = lw - 1;
            else
                *lx = (int)((long)(wx - vx) * lw / vw);
        }
        if (ly != NULL) {
            if (wy < vy)
                *ly = 0;
            else if (wy >= vy + vh)
                *ly = lh - 1;
            else
                *ly = (int)((long)(wy - vy) * lh / vh);
        }
    }
    return inside;
}

/* Map a logical game coordinate to a window-space position.  The controller
 * cursor and pointer warp use the same 4:3 viewport as presentation and hit
 * testing, so the host pointer stays over the game image in fullscreen. */
NW_SDL_CODE void NwSdlLogicalToWindow(int lx, int ly, float *wx, float *wy)
{
    int vx = 0;
    int vy = 0;
    int vw = 0;
    int vh = 0;
    int lw = (int)g_dwClientW;
    int lh = (int)g_dwClientH;

    NwSdlWindowViewport(&vx, &vy, &vw, &vh);
    if (lw <= 0)
        lw = 640;
    if (lh <= 0)
        lh = 480;
    if (vw <= 0)
        vw = lw;
    if (vh <= 0)
        vh = lh;
    if (wx != NULL)
        *wx = (float)vx + ((float)lx + 0.5f) * (float)vw / (float)lw;
    if (wy != NULL)
        *wy = (float)vy + ((float)ly + 0.5f) * (float)vh / (float)lh;
}

/* Largest 4:3 rectangle that fits in the given area.  Both dimensions are
 * rounded to exact 4:3 so later resize checks can compare without accumulating
 * rounding drift. */
NW_SDL_CODE static void nw_largest_4x3(int availW, int availH,
                                        int *pW, int *pH)
{
    int w;
    int h;

    if (availW < 4)
        availW = 4;
    if (availH < 3)
        availH = 3;

    h = (availH / 3) * 3;
    w = (h / 3) * 4;
    if (w > availW) {
        w = (availW / 4) * 4;
        h = (w / 4) * 3;
    }
    if (w < 4)
        w = 4;
    if (h < 3)
        h = 3;
    *pW = w;
    *pH = h;
}

/* Fit a 4:3 viewport into an area and center it.  Presentation uses this with
 * surface pixels; input uses NwSdlWindowViewport() with window coordinates. */
NW_SDL_CODE void NwSdlFitViewport4x3(int availW, int availH,
                                     int *px, int *py, int *pw, int *ph)
{
    int w;
    int h;

    nw_largest_4x3(availW, availH, &w, &h);
    if (px != NULL)
        *px = (availW - w) / 2;
    if (py != NULL)
        *py = (availH - h) / 2;
    if (pw != NULL)
        *pw = w;
    if (ph != NULL)
        *ph = h;
}

/* Size used for viewport fitting, control layout, and hit-testing.  On iOS the
 * live view bounds are authoritative: SDL's window size can lag them by one
 * layout pass while presentation has already been resized to the new bounds.
 * Using the same source for both drawing and input keeps a drawn button and
 * its tap target in the same place. */
NW_SDL_CODE static void nw_window_metrics_size(int *pw, int *ph)
{
    int w = 0;
    int h = 0;

    if (g_nwSdlWindow != NULL) {
        (void)SDL_GetWindowSize(g_nwSdlWindow, &w, &h);
#if defined(NW_PORTABLE_IOS)
        /* The window surface is allocated from the view bounds in points.  If
         * high pixel density were enabled the pixel size would be in a
         * different unit than SDL's pointer coordinates, so leave it alone. */
        if (!(SDL_GetWindowFlags(g_nwSdlWindow) &
              SDL_WINDOW_HIGH_PIXEL_DENSITY)) {
            int lw = 0;
            int lh = 0;
            if (SDL_GetWindowSizeInPixels(g_nwSdlWindow, &lw, &lh) &&
                lw > 0 && lh > 0) {
                w = lw;
                h = lh;
            }
        }
#endif
    }
    if (w <= 0)
        w = (int)g_dwClientW;
    if (h <= 0)
        h = (int)g_dwClientH;
    if (w <= 0)
        w = 640;
    if (h <= 0)
        h = 480;
    if (pw != NULL)
        *pw = w;
    if (ph != NULL)
        *ph = h;
}

/* Current game viewport in window coordinates.  A normal 4:3 window maps to
 * the whole window; borderless desktop fullscreen on a wider display gets
 * centered black bars on the left and right. */
NW_SDL_CODE void NwSdlWindowViewport(int *px, int *py, int *pw, int *ph)
{
    int ww = 0;
    int wh = 0;

    nw_window_metrics_size(&ww, &wh);
    NwSdlFitViewport4x3(ww, wh, px, py, pw, ph);
}

#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
static int g_nwSdlAndroidPressed NW_SDL_BSS;
static int g_nwSdlAndroidRotationLocked NW_SDL_BSS;
static int g_nwSdlAndroidMouseRight NW_SDL_BSS;
static int g_nwSdlAndroidCardOpen NW_SDL_BSS;
static int g_nwSdlAndroidCardAnimFrom NW_SDL_BSS;
static Uint64 g_nwSdlAndroidCardAnimStart NW_SDL_BSS;
static Uint64 g_nwSdlAndroidCardDeadline NW_SDL_BSS;
static int g_nwSdlAndroidRotationLockedSynced NW_SDL_BSS;
#if defined(NW_PORTABLE_IOS)
/* The orientation that was pinned when the player last chose LOCK.  It is
 * persisted beside the game data, not inside it: the lock is a per-device UI
 * preference, so it must survive launches without being part of a save. */
static char g_nwSdlIosRotationMask[32] NW_SDL_BSS;
#endif

/* The controls are drawn in the letterbox bars whenever the bars are large
 * enough.  Only when the window is too close to 4:3 do they move to a card
 * that drops down from a small tab at the top of the window.  The tab is the
 * only thing visible while the card is closed, so play stays unobstructed.
 * Opening the card reveals a row of wide buttons across 70% of the window; it
 * closes again after a few idle seconds or when the player taps outside it. */
#define NW_SDL_CARD_SLIDE_MS 180u
#define NW_SDL_CARD_HOLD_MS  4000u
#define NW_SDL_CARD_WIDTH_PCT 70

static int nw_android_card_progress_now(void)
{
    Uint64 elapsed;
    int from;
    int to;

    to = g_nwSdlAndroidCardOpen ? NW_SDL_CARD_PROGRESS_FULL : 0;
    if (g_nwSdlAndroidCardAnimStart == 0)
        return to;
    elapsed = SDL_GetTicks() - g_nwSdlAndroidCardAnimStart;
    if (elapsed >= NW_SDL_CARD_SLIDE_MS)
        return to;
    from = g_nwSdlAndroidCardAnimFrom;
    return from + (int)((to - from) * (int)elapsed /
                        (int)NW_SDL_CARD_SLIDE_MS);
}

static void nw_android_note_interaction(void)
{
    if (g_nwSdlAndroidCardOpen)
        g_nwSdlAndroidCardDeadline = SDL_GetTicks() + NW_SDL_CARD_HOLD_MS;
}
#endif

NW_SDL_CODE static int nw_android_min(int a, int b)
{
    return a < b ? a : b;
}

#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
/* Try to place the whole control set inside the letterbox bars so the game
 * view stays fully visible.  Returns 1 for a single row in the top or bottom
 * bar, 2 for two stacked columns in the side bars, and 0 when the window is
 * too close to 4:3 and the pull-down card is needed. */
static int nw_android_bar_metrics(const SDL_Rect *safe, int ww, int wh,
                                  int pad, int gap, SDL_Rect *pButtons)
{
    int vx = 0;
    int vy = 0;
    int vw = 0;
    int vh = 0;
    int bandTop;
    int bandBottom;
    int bandHeight;
    int buttonW;
    int buttonH;
    int rowW;
    int rowH;
    int rowX;
    int rowY;
    int colH;
    int colY;
    int i;

    NwSdlWindowViewport(&vx, &vy, &vw, &vh);

    /* A horizontal row across the top or bottom bar, matching the card width
     * and button proportions. */
    rowW = ww * NW_SDL_CARD_WIDTH_PCT / 100;
    if (rowW > safe->w - 8)
        rowW = safe->w - 8;
    if (rowW >= 160) {
        buttonW = (rowW - pad * 2 -
                   gap * (NW_ANDROID_CONTROL_COUNT - 1)) /
                  NW_ANDROID_CONTROL_COUNT;
        if (buttonW >= 28) {
            buttonH = buttonW * 7 / 10;
            if (buttonH < 44)
                buttonH = 44;
            if (buttonH > 64)
                buttonH = 64;
            rowH = buttonH + pad * 2;
            rowX = safe->x + (safe->w - rowW) / 2;

            /* Prefer the bottom bar, which sits near the player's thumbs. */
            bandTop = vy + vh;
            if (bandTop < safe->y)
                bandTop = safe->y;
            bandBottom = safe->y + safe->h;
            if (bandBottom > wh)
                bandBottom = wh;
            bandHeight = bandBottom - bandTop;
            if (bandTop >= vy + vh && bandHeight >= rowH + 4) {
                rowY = bandTop + (bandHeight - rowH) / 2;
                for (i = 0; i < NW_ANDROID_CONTROL_COUNT; i++) {
                    pButtons[i].x = (Sint16)(rowX + pad +
                                             i * (buttonW + gap));
                    pButtons[i].y = (Sint16)(rowY + pad);
                    pButtons[i].w = (Sint16)buttonW;
                    pButtons[i].h = (Sint16)buttonH;
                }
                return 1;
            }

            bandTop = safe->y;
            if (bandTop < 0)
                bandTop = 0;
            bandBottom = vy;
            if (bandBottom > safe->y + safe->h)
                bandBottom = safe->y + safe->h;
            bandHeight = bandBottom - bandTop;
            if (bandBottom <= vy && bandHeight >= rowH + 4) {
                rowY = bandTop + (bandHeight - rowH) / 2;
                for (i = 0; i < NW_ANDROID_CONTROL_COUNT; i++) {
                    pButtons[i].x = (Sint16)(rowX + pad +
                                             i * (buttonW + gap));
                    pButtons[i].y = (Sint16)(rowY + pad);
                    pButtons[i].w = (Sint16)buttonW;
                    pButtons[i].h = (Sint16)buttonH;
                }
                return 1;
            }
        }
    }

    /* Two stacked buttons in each side bar when the window is wider than 4:3
     * (the usual phone landscape layout).  EXIT/KEYBOARD stay on the left,
     * LOCK/CLICK on the right, matching the old bar layout.  The columns stay
     * inside the safe area, clear of a notch or rounded corner. */
    if (vx > 0 && ww - (vx + vw) > 0) {
        int leftBarL = 0;
        int leftBarR = vx;
        int rightBarL = vx + vw;
        int rightBarR = ww;
        int leftW;
        int rightW;
        int barW;
        int leftX;
        int rightX;

        if (leftBarL < safe->x)
            leftBarL = safe->x;
        if (leftBarR > safe->x + safe->w)
            leftBarR = safe->x + safe->w;
        if (rightBarL < safe->x)
            rightBarL = safe->x;
        if (rightBarR > safe->x + safe->w)
            rightBarR = safe->x + safe->w;
        leftW = leftBarR - leftBarL;
        rightW = rightBarR - rightBarL;
        barW = leftW < rightW ? leftW : rightW;

        buttonW = barW - pad * 2;
        if (buttonW >= 40) {
            buttonH = buttonW * 7 / 10;
            if (buttonH < 44)
                buttonH = 44;
            if (buttonH > 64)
                buttonH = 64;
            colH = buttonH * 2 + gap + pad * 2;
            bandTop = safe->y;
            if (bandTop < 0)
                bandTop = 0;
            bandBottom = safe->y + safe->h;
            if (bandBottom > wh)
                bandBottom = wh;
            bandHeight = bandBottom - bandTop;
            if (bandHeight >= colH + 4) {
                colY = bandTop + (bandHeight - colH) / 2;
                leftX = leftBarL + (leftW - buttonW) / 2;
                rightX = rightBarL + (rightW - buttonW) / 2;
                pButtons[0].x = (Sint16)leftX;
                pButtons[0].y = (Sint16)(colY + pad);
                pButtons[0].w = (Sint16)buttonW;
                pButtons[0].h = (Sint16)buttonH;
                pButtons[1].x = (Sint16)leftX;
                pButtons[1].y = (Sint16)(colY + pad + buttonH + gap);
                pButtons[1].w = (Sint16)buttonW;
                pButtons[1].h = (Sint16)buttonH;
                pButtons[2].x = (Sint16)rightX;
                pButtons[2].y = (Sint16)(colY + pad);
                pButtons[2].w = (Sint16)buttonW;
                pButtons[2].h = (Sint16)buttonH;
                pButtons[3].x = (Sint16)rightX;
                pButtons[3].y = (Sint16)(colY + pad + buttonH + gap);
                pButtons[3].w = (Sint16)buttonW;
                pButtons[3].h = (Sint16)buttonH;
                return 2;
            }
        }
    }
    return 0;
}
#endif

/* Resolve the control rectangles.  The letterbox bars get first claim on the
 * controls; only when they do not fit there does the pull-down card supply
 * the tab and card rectangles.  *pInBar is non-zero for either bar layout. */
NW_SDL_CODE static int nw_android_metrics(SDL_Rect *pHandle, SDL_Rect *pCard,
                                          SDL_Rect *pButtons, int *pInBar)
{
#if !defined(__ANDROID__) && !defined(NW_PORTABLE_IOS)
    (void)pHandle;
    (void)pCard;
    (void)pButtons;
    (void)pInBar;
    return 0;
#else
    SDL_Rect safe;
    SDL_Rect scratch[NW_ANDROID_CONTROL_COUNT];
    SDL_Rect *buttons = pButtons != NULL ? pButtons : scratch;
    int ww = 0;
    int wh = 0;
    int unit;
    int cardW;
    int cardX;
    int cardY;
    int cardH;
    int handleW;
    int handleH;
    int handleX;
    int handleY;
    int pad;
    int gap;
    int avail;
    int buttonW;
    int buttonH;
    int i;

    if (pInBar != NULL)
        *pInBar = 0;
    if (g_nwSdlWindow == NULL)
        return 0;
    nw_window_metrics_size(&ww, &wh);
    if (ww <= 0 || wh <= 0)
        return 0;

    safe.x = 0;
    safe.y = 0;
    safe.w = ww;
    safe.h = wh;
    if (!SDL_GetWindowSafeArea(g_nwSdlWindow, &safe) ||
        safe.w <= 0 || safe.h <= 0) {
        safe.x = 0;
        safe.y = 0;
        safe.w = ww;
        safe.h = wh;
    } else {
        /* iPadOS can report the previous orientation's safe area for a short
         * time while a rotation settles.  Keep the rectangle inside the live
         * window so the bars and card never move outside the touchable area. */
        SDL_Rect windowRect;
        SDL_Rect clamped;

        windowRect.x = 0;
        windowRect.y = 0;
        windowRect.w = ww;
        windowRect.h = wh;
        if (SDL_GetRectIntersection(&safe, &windowRect, &clamped) &&
            clamped.w > 0 && clamped.h > 0) {
            safe = clamped;
        } else {
            safe.x = 0;
            safe.y = 0;
            safe.w = ww;
            safe.h = wh;
        }
    }

    unit = nw_android_min(ww, wh);
    cardW = ww * NW_SDL_CARD_WIDTH_PCT / 100;
    if (cardW > safe.w - 8)
        cardW = safe.w - 8;

    pad = cardW / 48;
    if (pad < 8)
        pad = 8;
    if (pad > 18)
        pad = 18;
    gap = cardW / 64;
    if (gap < 6)
        gap = 6;
    if (gap > 12)
        gap = 12;

    /* Controls that fit entirely inside a black bar never cover the game, so
     * they are drawn directly with no tab or sliding card. */
    if (nw_android_bar_metrics(&safe, ww, wh, pad, gap, buttons)) {
        if (pInBar != NULL)
            *pInBar = 1;
        return 1;
    }

    if (cardW < 160)
        return 0;
    cardX = safe.x + (safe.w - cardW) / 2;

    handleH = unit / 16;
    if (handleH < 22)
        handleH = 22;
    if (handleH > 32)
        handleH = 32;
    handleW = cardW * 28 / 100;
    if (handleW < 72)
        handleW = 72;
    if (handleW > 180)
        handleW = 180;
    if (handleW > cardW - 16)
        handleW = cardW - 16;
    if (handleW < 48)
        handleW = 48;
    handleX = safe.x + (safe.w - handleW) / 2;
    handleY = safe.y + 2;

    avail = cardW - pad * 2 - gap * (NW_ANDROID_CONTROL_COUNT - 1);
    if (avail < NW_ANDROID_CONTROL_COUNT * 24)
        return 0;
    buttonW = avail / NW_ANDROID_CONTROL_COUNT;
    buttonH = buttonW * 7 / 10;
    if (buttonH < 44)
        buttonH = 44;
    if (buttonH > 64)
        buttonH = 64;
    cardH = pad * 2 + buttonH;
    cardY = handleY + handleH;

    if (pHandle != NULL) {
        pHandle->x = (Sint16)handleX;
        pHandle->y = (Sint16)handleY;
        pHandle->w = (Sint16)handleW;
        pHandle->h = (Sint16)handleH;
    }
    if (pCard != NULL) {
        pCard->x = (Sint16)cardX;
        pCard->y = (Sint16)cardY;
        pCard->w = (Sint16)cardW;
        pCard->h = (Sint16)cardH;
    }
    if (pButtons != NULL) {
        for (i = 0; i < NW_ANDROID_CONTROL_COUNT; i++) {
            pButtons[i].x = (Sint16)(cardX + pad +
                                     i * (buttonW + gap));
            pButtons[i].y = (Sint16)(cardY + pad);
            pButtons[i].w = (Sint16)buttonW;
            pButtons[i].h = (Sint16)buttonH;
        }
    }
    return 1;
#endif
}

/* Non-zero when the controls live in the letterbox bars and no card is used. */
NW_SDL_CODE int NwSdlAndroidControlsInBar(void)
{
#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
    int inBar = 0;

    if (!nw_android_metrics(NULL, NULL, NULL, &inBar))
        return 0;
    return inBar;
#else
    return 0;
#endif
}

NW_SDL_CODE int NwSdlAndroidControlCardLayout(SDL_Rect *pHandle,
                                              SDL_Rect *pCard)
{
#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
    int inBar = 0;

    if (!nw_android_metrics(pHandle, pCard, NULL, &inBar) || inBar)
        return 0;
    return 1;
#else
    (void)pHandle;
    (void)pCard;
    return 0;
#endif
}

/* Final button rectangles.  VideoSdl.c clips them to the visible card height
 * while the card is sliding. */
NW_SDL_CODE int NwSdlAndroidControlLayout(SDL_Rect *pButtons)
{
#if !defined(__ANDROID__) && !defined(NW_PORTABLE_IOS)
    (void)pButtons;
    return 0;
#else
    return nw_android_metrics(NULL, NULL, pButtons, NULL);
#endif
}

NW_SDL_CODE int NwSdlAndroidControlCardProgress(void)
{
#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
    if (NwSdlAndroidControlsInBar())
        return 0;
    return nw_android_card_progress_now();
#else
    return 0;
#endif
}

NW_SDL_CODE int NwSdlAndroidControlCardIsOpen(void)
{
#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
    return g_nwSdlAndroidCardOpen;
#else
    return 0;
#endif
}

#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
static void nw_android_card_set_open(int open)
{
    int from;
    Uint64 now;

    if (g_nwSdlAndroidCardOpen == open) {
        if (open)
            nw_android_note_interaction();
        return;
    }
    from = nw_android_card_progress_now();
    now = SDL_GetTicks();
    g_nwSdlAndroidCardAnimFrom = from;
    g_nwSdlAndroidCardOpen = open;
    g_nwSdlAndroidCardAnimStart = now;
    if (open)
        g_nwSdlAndroidCardDeadline = now + NW_SDL_CARD_HOLD_MS;
}
#endif

NW_SDL_CODE void NwSdlAndroidControlCardToggle(void)
{
#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
    if (NwSdlAndroidControlsInBar())
        return;
    nw_android_card_set_open(g_nwSdlAndroidCardOpen ? 0 : 1);
#endif
}

NW_SDL_CODE void NwSdlAndroidControlCardClose(void)
{
#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
    if (NwSdlAndroidControlsInBar())
        return;
    nw_android_card_set_open(0);
#endif
}

/* Called once per game frame.  Returns non-zero while the card is moving or
 * when the idle deadline just closed it, so the caller can present the new
 * frame even if the game itself produced no drawing. */
NW_SDL_CODE int NwSdlAndroidControlCardTick(void)
{
#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
    Uint64 now = SDL_GetTicks();

    if (NwSdlAndroidControlsInBar()) {
        /* Rotation can move the controls into the bars while the card is
         * open; drop that state without animating. */
        if (g_nwSdlAndroidCardOpen || g_nwSdlAndroidCardAnimStart != 0) {
            g_nwSdlAndroidCardOpen = 0;
            g_nwSdlAndroidCardAnimFrom = 0;
            g_nwSdlAndroidCardAnimStart = 0;
            g_nwSdlAndroidCardDeadline = 0;
            return 1;
        }
        return 0;
    }
    if (g_nwSdlAndroidCardOpen && g_nwSdlAndroidPressed == 0 &&
        g_nwSdlAndroidCardDeadline != 0 &&
        now >= g_nwSdlAndroidCardDeadline) {
        nw_android_card_set_open(0);
        return 1;
    }
    if (g_nwSdlAndroidCardAnimStart != 0 &&
        now - g_nwSdlAndroidCardAnimStart < NW_SDL_CARD_SLIDE_MS + 60u)
        return 1;
    return 0;
#else
    return 0;
#endif
}

NW_SDL_CODE int NwSdlAndroidControlHandleHit(int x, int y)
{
#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
    SDL_Rect handle;

    if (!NwSdlAndroidControlCardLayout(&handle, NULL))
        return 0;
    return x >= handle.x && x < handle.x + handle.w &&
           y >= handle.y && y < handle.y + handle.h;
#else
    (void)x;
    (void)y;
    return 0;
#endif
}

NW_SDL_CODE int NwSdlAndroidControlCardHit(int x, int y)
{
#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
    SDL_Rect handle;
    SDL_Rect card;

    if (!NwSdlAndroidControlCardLayout(&handle, &card))
        return 0;
    if (x >= handle.x && x < handle.x + handle.w &&
        y >= handle.y && y < handle.y + handle.h)
        return 1;
    if (NwSdlAndroidControlCardProgress() <= 0)
        return 0;
    return x >= card.x && x < card.x + card.w &&
           y >= card.y && y < card.y + card.h;
#else
    (void)x;
    (void)y;
    return 0;
#endif
}

NW_SDL_CODE int NwSdlAndroidControlHit(int x, int y)
{
#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
    SDL_Rect buttons[NW_ANDROID_CONTROL_COUNT];
    int i;

    if (!NwSdlAndroidControlsInBar() &&
        NwSdlAndroidControlCardProgress() < 200)
        return 0;
    if (!NwSdlAndroidControlLayout(buttons))
        return 0;
    for (i = 0; i < NW_ANDROID_CONTROL_COUNT; i++) {
        if (x >= buttons[i].x && x < buttons[i].x + buttons[i].w &&
            y >= buttons[i].y && y < buttons[i].y + buttons[i].h)
            return i + 1;
    }
    return 0;
#else
    (void)x;
    (void)y;
    return 0;
#endif
}

NW_SDL_CODE int NwSdlAndroidControlPressed(void)
{
#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
    return g_nwSdlAndroidPressed;
#else
    return 0;
#endif
}

NW_SDL_CODE void NwSdlAndroidControlPress(int id)
{
#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
    g_nwSdlAndroidPressed = id;
    nw_android_note_interaction();
#else
    (void)id;
#endif
}

NW_SDL_CODE void NwSdlAndroidControlCancel(void)
{
#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
    g_nwSdlAndroidPressed = 0;
#endif
}

#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
/* Hide the on-screen keyboard if it is showing.  Returns 1 when it was
 * visible.  SDL_StopTextInput() only hides the Android IME while the
 * screen-keyboard hint still requests it, so Android restores text input with
 * the hint disabled afterwards; a hardware keyboard keeps delivering text. */
NW_SDL_CODE static int nw_android_hide_keyboard(void)
{
    if (g_nwSdlWindow == NULL)
        return 0;
    if (!SDL_ScreenKeyboardShown(g_nwSdlWindow))
        return 0;
    SDL_StopTextInput(g_nwSdlWindow);
#if defined(__ANDROID__)
    (void)SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD, "0");
    (void)SDL_StartTextInput(g_nwSdlWindow);
#endif
    return 1;
}
#endif

/* Dismiss the on-screen keyboard from an outside tap or click. */
NW_SDL_CODE int NwSdlAndroidHideKeyboard(void)
{
#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
    return nw_android_hide_keyboard();
#else
    return 0;
#endif
}

NW_SDL_CODE int NwSdlAndroidToggleKeyboard(void)
{
#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
    if (g_nwSdlWindow == NULL)
        return 0;
    if (nw_android_hide_keyboard())
        return 0;
#if defined(__ANDROID__)
    (void)SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD, "1");
#endif
    if (!SDL_StartTextInput(g_nwSdlWindow))
        return 0;
    return 1;
#else
    return 0;
#endif
}

/* Returns the activated control id or 0 when the release happened outside the
 * pressed control.  The caller performs the action (quit, keyboard toggle,
 * rotation lock, or click-mode toggle) so the input unit owns game semantics. */
NW_SDL_CODE int NwSdlAndroidControlRelease(int x, int y)
{
#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
    int pressed = g_nwSdlAndroidPressed;
    g_nwSdlAndroidPressed = 0;
    if (pressed != 0) {
        nw_android_note_interaction();
        if (NwSdlAndroidControlHit(x, y) == pressed)
            return pressed;
    }
    return 0;
#else
    (void)x;
    (void)y;
    return 0;
#endif
}

#if defined(__ANDROID__)
/* Ask NetWalkActivity to lock the current orientation or resume following the
 * device sensors.  SDL's Java helper cannot express this toggle: its hints only
 * choose a fixed orientation set, so the activity exposes this small method. */
NW_SDL_CODE static int nw_android_set_rotation_locked(int locked)
{
    JNIEnv *env = (JNIEnv *)SDL_GetAndroidJNIEnv();
    jobject activity;
    jclass activityClass;
    jmethodID method;
    int applied = 0;

    if (env == NULL)
        return 0;
    activity = SDL_GetAndroidActivity();
    if (activity == NULL)
        return 0;
    activityClass = (*env)->GetObjectClass(env, activity);
    if (activityClass == NULL) {
        (*env)->DeleteLocalRef(env, activity);
        return 0;
    }
    method = (*env)->GetMethodID(env, activityClass, "setRotationLocked",
                                 "(Z)V");
    if (method != NULL) {
        (*env)->CallVoidMethod(env, activity, method,
                               locked ? JNI_TRUE : JNI_FALSE);
        if (!(*env)->ExceptionCheck(env))
            applied = 1;
        else
            (*env)->ExceptionClear(env);
    } else if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
    }
    (*env)->DeleteLocalRef(env, activityClass);
    (*env)->DeleteLocalRef(env, activity);
    return applied;
}

/* Read the activity's persisted lock state once.  Returns -1 if the JNI query
 * cannot be completed, allowing the caller to retry on a later frame. */
NW_SDL_CODE static int nw_android_query_rotation_locked(void)
{
    JNIEnv *env = (JNIEnv *)SDL_GetAndroidJNIEnv();
    jobject activity;
    jclass activityClass;
    jmethodID method;
    jboolean locked;
    int result = -1;

    if (env == NULL)
        return -1;
    activity = SDL_GetAndroidActivity();
    if (activity == NULL)
        return -1;
    activityClass = (*env)->GetObjectClass(env, activity);
    if (activityClass == NULL) {
        (*env)->DeleteLocalRef(env, activity);
        return -1;
    }
    method = (*env)->GetMethodID(env, activityClass, "isRotationLocked",
                                 "()Z");
    if (method != NULL) {
        locked = (*env)->CallBooleanMethod(env, activity, method);
        if (!(*env)->ExceptionCheck(env))
            result = locked ? 1 : 0;
        else
            (*env)->ExceptionClear(env);
    } else if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
    }
    (*env)->DeleteLocalRef(env, activityClass);
    (*env)->DeleteLocalRef(env, activity);
    return result;
}

NW_SDL_CODE static void nw_android_sync_rotation_locked(void)
{
    int locked;

    if (g_nwSdlAndroidRotationLockedSynced)
        return;
    locked = nw_android_query_rotation_locked();
    if (locked >= 0) {
        g_nwSdlAndroidRotationLocked = locked;
        g_nwSdlAndroidRotationLockedSynced = 1;
    }
}
#endif

#if defined(NW_PORTABLE_IOS)
/* Persisted rotation lock, in its own file in the writable preference
 * directory.  Keeping it out of the game's save data means a new game slot or
 * a reset of high scores never changes how the device is held. */
#define NW_IOS_ROTATION_FILE "rotation.cfg"

NW_SDL_CODE static FILE *nw_ios_rotation_open(const char *pszMode)
{
    char *pszPref;
    char szPath[1024];
    FILE *pFile;

    pszPref = SDL_GetPrefPath("Gamos", "NetWalk");
    if (pszPref == NULL)
        return NULL;
    if (snprintf(szPath, sizeof(szPath), "%s%s", pszPref,
                 NW_IOS_ROTATION_FILE) <= 0) {
        SDL_free(pszPref);
        return NULL;
    }
    pFile = fopen(szPath, pszMode);
    SDL_free(pszPref);
    return pFile;
}

/* Returns 1 when the stored state says the lock is on.  The pinned
 * orientation name is copied into pszMask for diagnostics and for detecting a
 * launch in a different orientation. */
NW_SDL_CODE static int nw_ios_load_rotation_locked(char *pszMask,
                                                   size_t maskSize)
{
    char szLine[160];
    int locked = 0;
    FILE *pFile;

    if (maskSize > 0)
        pszMask[0] = '\0';
    pFile = nw_ios_rotation_open("r");
    if (pFile == NULL)
        return 0;
    while (fgets(szLine, sizeof(szLine), pFile) != NULL) {
        char *pKey = szLine;
        char *pValue = strchr(szLine, '=');
        size_t len;

        if (pValue == NULL)
            continue;
        *pValue++ = '\0';
        len = strlen(pValue);
        while (len > 0 &&
               (pValue[len - 1] == '\n' || pValue[len - 1] == '\r' ||
                pValue[len - 1] == ' ' || pValue[len - 1] == '\t'))
            pValue[--len] = '\0';
        if (strcmp(pKey, "locked") == 0) {
            locked = atoi(pValue) != 0;
        } else if (strcmp(pKey, "orientation") == 0 && maskSize > 0) {
            strncpy(pszMask, pValue, maskSize - 1);
            pszMask[maskSize - 1] = '\0';
        }
    }
    fclose(pFile);
    return locked;
}

NW_SDL_CODE static void nw_ios_save_rotation_locked(int locked,
                                                   const char *pszMask)
{
    FILE *pFile;

    pFile = nw_ios_rotation_open("w");
    if (pFile == NULL)
        return;
    fprintf(pFile,
            "# NetWalk rotation lock; kept separate from the game save\n"
            "locked=%d\n"
            "orientation=%s\n",
            locked ? 1 : 0,
            (locked && pszMask != NULL) ? pszMask : "");
    fclose(pFile);
}

/* SDL's orientation names for the current display.  The landscape names are
 * reversed relative to UIKit (see the openradar note in SDL_uikitmodes.m). */
NW_SDL_CODE static const char *nw_ios_current_orientation_mask(void)
{
    if (g_nwSdlWindow == NULL)
        return NULL;
    switch (SDL_GetCurrentDisplayOrientation(
                SDL_GetDisplayForWindow(g_nwSdlWindow))) {
    case SDL_ORIENTATION_LANDSCAPE:
        return "LandscapeRight";
    case SDL_ORIENTATION_LANDSCAPE_FLIPPED:
        return "LandscapeLeft";
    case SDL_ORIENTATION_PORTRAIT:
        return "Portrait";
    case SDL_ORIENTATION_PORTRAIT_FLIPPED:
        return "PortraitUpsideDown";
    default:
        return NULL;
    }
}

/* Fallback for entry points that did not apply the saved lock before window
 * creation.  UIKit has already chosen the launch orientation, so pinning the
 * live orientation here avoids the window-frame mismatch that an orientation
 * hint set after sizing the window would cause.  The saved orientation is left
 * untouched so the next launch can still try to restore it. */
NW_SDL_CODE static void nw_ios_sync_rotation_locked(void)
{
    char szSavedMask[32];
    const char *pszCurrent;

    if (g_nwSdlAndroidRotationLockedSynced || g_nwSdlWindow == NULL)
        return;
    if (!nw_ios_load_rotation_locked(szSavedMask, sizeof(szSavedMask))) {
        g_nwSdlAndroidRotationLockedSynced = 1;
        return;
    }
    pszCurrent = nw_ios_current_orientation_mask();
    if (pszCurrent == NULL)
        return;
    (void)SDL_SetHint(SDL_HINT_ORIENTATIONS, pszCurrent);
    strncpy(g_nwSdlIosRotationMask, pszCurrent,
            sizeof(g_nwSdlIosRotationMask) - 1);
    g_nwSdlIosRotationMask[sizeof(g_nwSdlIosRotationMask) - 1] = '\0';
    g_nwSdlAndroidRotationLocked = 1;
    g_nwSdlAndroidRotationLockedSynced = 1;
    /* Files written before the orientation was stored only have the flag.
     * Fill in the orientation UIKit actually chose, without overwriting a
     * real saved orientation. */
    if (szSavedMask[0] == '\0')
        nw_ios_save_rotation_locked(1, pszCurrent);
}

/* Restore a remembered lock from the iOS entry point, before SDL creates the
 * window.  Setting the hint this early lets UIKit launch directly in the saved
 * orientation instead of first adopting the direction the device happens to be
 * held in.  The later sync is then a no-op. */
NW_SDL_CODE void NwSdlIosApplySavedRotationLock(void)
{
    char szMask[32];

    if (!nw_ios_load_rotation_locked(szMask, sizeof(szMask)))
        return;
    if (szMask[0] == '\0')
        return;
    (void)SDL_SetHint(SDL_HINT_ORIENTATIONS, szMask);
    strncpy(g_nwSdlIosRotationMask, szMask, sizeof(g_nwSdlIosRotationMask) - 1);
    g_nwSdlIosRotationMask[sizeof(g_nwSdlIosRotationMask) - 1] = '\0';
    g_nwSdlAndroidRotationLocked = 1;
    g_nwSdlAndroidRotationLockedSynced = 1;
}
#endif

#if defined(NW_PORTABLE_IOS)
/* Implemented in platforms/ios/ios_orientation.m. */
extern void NwIosRefreshInterfaceOrientation(void);
extern void NwIosRequestOrientation(const char *pszMask);
#endif

NW_SDL_CODE int NwSdlAndroidToggleRotationLock(void)
{
#if defined(__ANDROID__)
    int locked;

    /* Make sure the native label and cache reflect the Java preference before
     * computing the next state, especially on the first frame after launch. */
    nw_android_sync_rotation_locked();
    locked = g_nwSdlAndroidRotationLocked ? 0 : 1;

    if (!nw_android_set_rotation_locked(locked))
        return g_nwSdlAndroidRotationLocked;
    g_nwSdlAndroidRotationLocked = locked;
    g_nwSdlAndroidRotationLockedSynced = 1;
    return locked;
#elif defined(NW_PORTABLE_IOS)
    /* UIKit reads SDL_HINT_ORIENTATIONS from the view controller's
     * supportedInterfaceOrientations, so pinning the current display
     * orientation to a single value is enough to stop further rotation.
     * Unlocking restores the full set from Info.plist.
     *
     * SDL deliberately maps the landscape names to the opposite UIKit
     * constants (see the openradar note in SDL_uikitmodes.m): the display
     * orientation SDL calls LANDSCAPE is UIInterfaceOrientationLandscapeRight.
     * Using SDL's own naming here without that reversal would pin the device
     * to the opposite landscape and let a 180-degree turn through.
     *
     * Both choices are written to the separate rotation file so the next
     * launch starts with the same lock state. */
    const char *mask;

    if (g_nwSdlWindow == NULL)
        return g_nwSdlAndroidRotationLocked;
    nw_ios_sync_rotation_locked();
    if (g_nwSdlAndroidRotationLocked) {
        (void)SDL_SetHint(SDL_HINT_ORIENTATIONS,
                          "Portrait PortraitUpsideDown "
                          "LandscapeLeft LandscapeRight");
        g_nwSdlAndroidRotationLocked = 0;
        g_nwSdlIosRotationMask[0] = '\0';
        nw_ios_save_rotation_locked(0, NULL);
        /* The device may have been turned while the lock was on.  Ask UIKit
         * to follow it now instead of waiting for another sensor event. */
        NwIosRefreshInterfaceOrientation();
        return 0;
    }
    mask = nw_ios_current_orientation_mask();
    if (mask != NULL) {
        (void)SDL_SetHint(SDL_HINT_ORIENTATIONS, mask);
        strncpy(g_nwSdlIosRotationMask, mask,
                sizeof(g_nwSdlIosRotationMask) - 1);
        g_nwSdlIosRotationMask[sizeof(g_nwSdlIosRotationMask) - 1] = '\0';
        g_nwSdlAndroidRotationLocked = 1;
        g_nwSdlAndroidRotationLockedSynced = 1;
        nw_ios_save_rotation_locked(1, mask);
    }
    return g_nwSdlAndroidRotationLocked;
#else
    return 0;
#endif
}

NW_SDL_CODE int NwSdlAndroidRotationLocked(void)
{
#if defined(__ANDROID__)
    nw_android_sync_rotation_locked();
    return g_nwSdlAndroidRotationLocked;
#elif defined(NW_PORTABLE_IOS)
    nw_ios_sync_rotation_locked();
    return g_nwSdlAndroidRotationLocked;
#else
    return 0;
#endif
}

/* The single click control toggles between left and right button semantics for
 * synthetic touch mouse events.  A physical mouse or pen keeps its own
 * buttons. */
NW_SDL_CODE int NwSdlAndroidMouseRight(void)
{
#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
    return g_nwSdlAndroidMouseRight;
#else
    return 0;
#endif
}

NW_SDL_CODE int NwSdlAndroidToggleMouseRight(void)
{
#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
    g_nwSdlAndroidMouseRight = g_nwSdlAndroidMouseRight ? 0 : 1;
    return g_nwSdlAndroidMouseRight;
#else
    return 0;
#endif
}

NW_SDL_CODE static void nw_center_window(SDL_Window *window, int w, int h)
{
    SDL_DisplayID display;
    SDL_Rect bounds;

    display = SDL_GetDisplayForWindow(window);
    if (display == 0 || !SDL_GetDisplayUsableBounds(display, &bounds))
        return;
    (void)SDL_SetWindowPosition(window,
                                bounds.x + (bounds.w - w) / 2,
                                bounds.y + (bounds.h - h) / 2);
}

/* Recompute the limit used by the window manager's maximize operation.  The
 * limit is the largest 4:3 rectangle on the current display, so a maximize
 * cannot turn the game canvas into a 16:9 window. */
NW_SDL_CODE void NwSdlUpdateWindowLimits(void)
{
    SDL_DisplayID display;
    SDL_Rect bounds;
    int maxW;
    int maxH;

#if defined(__ANDROID__)
    /* Android owns the window size; it is always the current surface size. */
    return;
#endif
    /* The 4:3 constraints must not apply to the fullscreen window: the whole
     * display is the window and presentation adds the letterbox bars. */
    if (g_nwSdlWindow == NULL || g_nwSdlFullscreen ||
        !NwSdlWindowResizable())
        return;
    display = SDL_GetDisplayForWindow(g_nwSdlWindow);
    if (display == 0 || !SDL_GetDisplayUsableBounds(display, &bounds))
        return;
    nw_largest_4x3(bounds.w, bounds.h, &maxW, &maxH);
    (void)SDL_SetWindowMaximumSize(g_nwSdlWindow, maxW, maxH);
}

/* Reapply the normal windowed sizing rules after a fullscreen toggle.  The
 * aspect and size hints are cleared while fullscreen so window managers cannot
 * constrain the window to 4:3 there. */
NW_SDL_CODE static void nw_apply_windowed_constraints(void)
{
    int minW;
    int minH;

    if (g_nwSdlWindow == NULL)
        return;

    if (!NwSdlWindowResizable()) {
        int w = g_nwSdlWindowedValid ? g_nwSdlWindowedW : (int)g_dwClientW;
        int h = g_nwSdlWindowedValid ? g_nwSdlWindowedH : (int)g_dwClientH;

        if (w < 1)
            w = 640;
        if (h < 1)
            h = 480;
        (void)SDL_SetWindowAspectRatio(g_nwSdlWindow, 0.0f, 0.0f);
        (void)SDL_SetWindowMinimumSize(g_nwSdlWindow, w, h);
        (void)SDL_SetWindowMaximumSize(g_nwSdlWindow, w, h);
        return;
    }

    minW = (int)g_dwClientW / 2;
    minH = (int)g_dwClientH / 2;
    if (minW < 320)
        minW = 320;
    if (minH < 240)
        minH = 240;
    (void)SDL_SetWindowAspectRatio(g_nwSdlWindow, 4.0f / 3.0f, 4.0f / 3.0f);
    (void)SDL_SetWindowMinimumSize(g_nwSdlWindow, minW, minH);
    NwSdlUpdateWindowLimits();
}

/* Called after any resize/restore/maximize event.  WM aspect hints handle the
 * normal drag path; this also covers programmatic resizes (the intro) and
 * maximize requests, which bypass the interactive sizing hints. */
NW_SDL_CODE void NwSdlEnforceWindowAspect(void)
{
    static int adjusting;
    SDL_WindowFlags flags;
    int w = 0;
    int h = 0;
    int targetW;
    int targetH;
    int delta;

#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
    /* The Android/iOS surface follows the display and (on Android) the keyboard
     * insets; the game viewport is fitted during presentation, not by
     * resizing the window. */
    return;
#endif
    if (g_nwSdlWindow == NULL || adjusting ||
        !NwSdlWindowResizable())
        return;

    flags = SDL_GetWindowFlags(g_nwSdlWindow);
    if ((flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MINIMIZED)) != 0)
        return;
    if (!SDL_GetWindowSize(g_nwSdlWindow, &w, &h) || w <= 0 || h <= 0)
        return;

    delta = w * 3 - h * 4;
    if (delta < 0)
        delta = -delta;

    /* A maximized window follows the WM's work area, which is normally not
     * 4:3.  Convert the maximize into the largest 4:3 window that fits the
     * display and center it.  This also covers WMs that ignore the maximum
     * size hint while maximizing. */
    if ((flags & SDL_WINDOW_MAXIMIZED) != 0) {
        SDL_DisplayID display = SDL_GetDisplayForWindow(g_nwSdlWindow);
        SDL_Rect bounds;

        if (display != 0 && SDL_GetDisplayUsableBounds(display, &bounds)) {
            adjusting = 1;
            nw_largest_4x3(bounds.w, bounds.h, &targetW, &targetH);
            (void)SDL_RestoreWindow(g_nwSdlWindow);
            (void)SDL_SyncWindow(g_nwSdlWindow);
            (void)SDL_SetWindowSize(g_nwSdlWindow, targetW, targetH);
            nw_center_window(g_nwSdlWindow, targetW, targetH);
            (void)SDL_SyncWindow(g_nwSdlWindow);
            adjusting = 0;
        }
        return;
    }

    if (delta <= 8)
        return;                       /* already 4:3, allow WM rounding */

    adjusting = 1;
    if (w * 3 > h * 4) {
        /* Too wide: keep the height and shrink the width. */
        targetH = (h / 3) * 3;
        targetW = (targetH / 3) * 4;
    } else {
        /* Too tall: keep the width and shrink the height. */
        targetW = (w / 4) * 4;
        targetH = (targetW / 4) * 3;
    }
    if (targetW > 0 && targetH > 0)
        (void)SDL_SetWindowSize(g_nwSdlWindow, targetW, targetH);
    adjusting = 0;
}

/* Borderless desktop fullscreen.  The window surface becomes the desktop
 * resolution; presentation keeps the game in a centered 4:3 viewport and
 * fills the unused area with black.  The window's previous size and position
 * are restored when F returns to windowed mode. */
NW_SDL_CODE int NwSdlToggleFullscreen(void)
{
    SDL_WindowFlags flags;
    int wx = 0;
    int wy = 0;
    int ww = 0;
    int wh = 0;

    if (g_nwSdlWindow == NULL)
        return 0;
#if defined(__ANDROID__)
    /* The activity is fullscreen already.  The F hotkey is also disabled on
     * Android so it remains available as ordinary text input. */
    return 0;
#endif

    if (!g_nwSdlFullscreen) {
        if (SDL_GetWindowPosition(g_nwSdlWindow, &wx, &wy) &&
            SDL_GetWindowSize(g_nwSdlWindow, &ww, &wh)) {
            g_nwSdlWindowedX = wx;
            g_nwSdlWindowedY = wy;
            g_nwSdlWindowedW = ww;
            g_nwSdlWindowedH = wh;
            flags = SDL_GetWindowFlags(g_nwSdlWindow);
            g_nwSdlWindowedMaximized =
                (flags & SDL_WINDOW_MAXIMIZED) != 0 ? 1 : 0;
            g_nwSdlWindowedValid = 1;
        }

        /* Window managers honor the 4:3 aspect and maximum-size hints even
         * for a fullscreen window, which would leave it at 4:3 instead of
         * filling a widescreen display.  Clear them before entering. */
        (void)SDL_SetWindowAspectRatio(g_nwSdlWindow, 0.0f, 0.0f);
        (void)SDL_SetWindowMinimumSize(g_nwSdlWindow, 0, 0);
        (void)SDL_SetWindowMaximumSize(g_nwSdlWindow, 0, 0);

        /* NULL selects the desktop's current video mode, which gives
         * borderless fullscreen without changing the display resolution. */
        (void)SDL_SetWindowFullscreenMode(g_nwSdlWindow, NULL);
        if (!SDL_SetWindowFullscreen(g_nwSdlWindow, true)) {
            nw_apply_windowed_constraints();
            return 0;
        }
        g_nwSdlFullscreen = 1;
        (void)SDL_SyncWindow(g_nwSdlWindow);
        return 1;
    }

    if (!SDL_SetWindowFullscreen(g_nwSdlWindow, false))
        return 0;
    g_nwSdlFullscreen = 0;

    if (g_nwSdlWindowedValid) {
        if (g_nwSdlWindowedMaximized) {
            nw_apply_windowed_constraints();
            (void)SDL_MaximizeWindow(g_nwSdlWindow);
        } else {
            (void)SDL_SetWindowSize(g_nwSdlWindow,
                                    g_nwSdlWindowedW, g_nwSdlWindowedH);
            (void)SDL_SetWindowPosition(g_nwSdlWindow,
                                        g_nwSdlWindowedX, g_nwSdlWindowedY);
            nw_apply_windowed_constraints();
            NwSdlEnforceWindowAspect();
        }
        (void)SDL_SyncWindow(g_nwSdlWindow);
    } else {
        nw_apply_windowed_constraints();
        NwSdlEnforceWindowAspect();
    }

    return 1;
}

NW_SDL_CODE int NwSdlIsFullscreen(void)
{
    return g_nwSdlFullscreen;
}

/* Borderless windows have no window-manager frame for the user to grab.  Give
 * the outer few pixels the standard resize hit-test regions so dragging an
 * edge or corner still works on X11, Wayland, and Windows. */
NW_SDL_CODE static SDL_HitTestResult SDLCALL
nw_sdl_hit_test(SDL_Window *window, const SDL_Point *area, void *data)
{
    const int border = 6;
    int w = 0;
    int h = 0;
    bool left;
    bool right;
    bool top;
    bool bottom;

    (void)data;
    if (area == NULL ||
        !SDL_GetWindowSize(window, &w, &h) ||
        area->x < 0 || area->y < 0 || area->x >= w || area->y >= h)
        return SDL_HITTEST_NORMAL;

    left = area->x < border;
    right = area->x >= w - border;
    top = area->y < border;
    bottom = area->y >= h - border;

    if (top && left)
        return SDL_HITTEST_RESIZE_TOPLEFT;
    if (top && right)
        return SDL_HITTEST_RESIZE_TOPRIGHT;
    if (bottom && left)
        return SDL_HITTEST_RESIZE_BOTTOMLEFT;
    if (bottom && right)
        return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
    if (left)
        return SDL_HITTEST_RESIZE_LEFT;
    if (right)
        return SDL_HITTEST_RESIZE_RIGHT;
    if (top)
        return SDL_HITTEST_RESIZE_TOP;
    if (bottom)
        return SDL_HITTEST_RESIZE_BOTTOM;
    return SDL_HITTEST_NORMAL;
}

NW_SDL_CODE void CreatePlayerMainWindow(void *pPlayerCfg)
{
    const PLAYERCONFIGSDL *pCfg = (const PLAYERCONFIGSDL *)pPlayerCfg;
    SDL_WindowFlags flags;
    int nSeedH;
    int nSeedV;
    int createW;
    int createH;
    int clientW;
    int clientH;
    int windowW;
    int windowH;
    int resizable = NwSdlWindowResizable();
    const char *pszTitle;

    if (g_nwSdlWindow != NULL)
        return;

    /* Publish the requested client size exactly as the Win32 backend does. */
    NW32(0x5f20) = (uint32_t)pCfg->nClientWidth;
    NW32(0x5f24) = (uint32_t)pCfg->nClientHeight;
    NW32(0x5f28) = (uint32_t)pCfg->nClientWidth;
    NW32(0x5f2c) = (uint32_t)pCfg->nClientHeight;
    NW32(0x69c)  = (uint32_t)pCfg->nClientWidth;
    NW32(0x6a0)  = (uint32_t)pCfg->nClientHeight;

    g_dwScreenW = (uint32_t)GetSystemMetrics(GSM_IDX_SCREEN_W);
    g_dwScreenH = (uint32_t)GetSystemMetrics(GSM_IDX_SCREEN_H);

    g_dwFrameBorderH = 0;
    g_dwFrameBorderV = (uint32_t)(GetSystemMetrics(GSM_IDX_CAPTION)
                                  - GetSystemMetrics(GSM_IDX_IDX6));

    if (pCfg->bThickFrameMode == 0) {
        /* Windowed mode: the Win32 style has no frame; its client is the
         * requested size. */
        nSeedH = GetSystemMetrics(GSM_IDX_FULLSCREEN_H);
        g_dwFrameBorderH = (uint32_t)(nSeedH * 2);

        nSeedV = GetSystemMetrics(GSM_IDX_IDX6);
        g_dwFrameBorderV += (uint32_t)(nSeedV * 2);

        flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN;
    } else {
        /* Thick-frame mode: keep the same metrics and clamping as the
         * oracle; the window client is the requested size. */
        nSeedH = GetSystemMetrics(GSM_IDX_MAXIMIZED_W);
        g_dwFrameBorderH = (uint32_t)(nSeedH * 2);

        nSeedV = GetSystemMetrics(GSM_IDX_MAXIMIZED_H);
        g_dwFrameBorderV += (uint32_t)(nSeedV * 2);

        if ((int)g_dwScreenW <= (int)g_dwClientW)
            g_dwClientW = g_dwScreenW;
        if ((int)g_dwScreenH <= (int)g_dwClientH)
            g_dwClientH = g_dwScreenH;

        flags = SDL_WINDOW_HIDDEN;
    }

    if (resizable)
        flags |= SDL_WINDOW_RESIZABLE;
#if defined(__ANDROID__)
    /* The Android window is the native surface, not a desktop 640x480 frame.
     * Keep it resizable so SDL's orientation helper honors the landscape hint,
     * and let the renderer fit the 4:3 game into the resulting surface. */
    flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE;
#elif defined(NW_PORTABLE_IOS)
    /* UIKit always makes the window the native screen surface.  Borderless
     * hides the status bar; presentation letterboxes the 4:3 game. */
    flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE;
#endif

    /* Keep the Win32 oracle's DPI virtualization.  This must precede the
     * first video initialization. */
    (void)SDL_SetHint("SDL_WINDOWS_DPI_AWARENESS", "unaware");
    /* Force the foreground/focus dance: the game is launched from a console
     * and must own keyboard focus exactly like the Win32 oracle window. */
    (void)SDL_SetHint(SDL_HINT_FORCE_RAISEWINDOW, "1");
    /* The shipped decoder consumes WM_KEYDOWN semantics.  SDL's raw-keyboard
     * path bypasses WM_KEYDOWN (and does not see synthetic input under Wine),
     * so use the standard window keyboard messages. */
    (void)SDL_SetHint(SDL_HINT_WINDOWS_RAW_KEYBOARD, "0");
#if defined(__ANDROID__)
    /* NetWalkActivity owns orientation.  It starts in full-sensor mode and the
     * LOCK/UNLOCK control switches between full-sensor and
     * SCREEN_ORIENTATION_LOCKED through a small JNI method; SDL's orientation
     * hints can only choose a fixed allowed set, not toggle a lock.  Return
     * must reach the game rather than dismiss the IME, and the keyboard is only
     * shown by the Keyboard control. */
    (void)SDL_SetHint(SDL_HINT_RETURN_KEY_HIDES_IME, "0");
    (void)SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD, "0");
#endif
    NW_NATIVE_BOOT("window_begin");
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        char dbg[256];
        snprintf(dbg, sizeof(dbg), "window_sdl_init_fail_%s", SDL_GetError());
        NW_NATIVE_BOOT(dbg);
        fprintf(stderr, "nw-sdl-window: SDL_InitSubSystem: %s\n", SDL_GetError());
        return;
    }
    NW_NATIVE_BOOT("window_sdl_init_ok");
    /* Gamepad input is optional: keyboard and mouse must still work on hosts
     * without a joystick backend.  Keep it a separate subsystem so a failure
     * here does not prevent the window from being created. */
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD) &&
        NW_DEBUG_GETENV("NW_DEBUG_VIDEO") != NULL) {
        fprintf(stderr, "nw-sdl-window: gamepad init: %s\n", SDL_GetError());
    }
    if (NW_DEBUG_GETENV("NW_DEBUG_VIDEO") != NULL)
        fprintf(stderr, "nw-sdl: video driver=%s\n",
                SDL_GetCurrentVideoDriver() != NULL
                    ? SDL_GetCurrentVideoDriver() : "(none)");

    pszTitle = pCfg->szWindowTitle;
    if (pszTitle == NULL || pszTitle[0] == '\0')
        pszTitle = "NetWalk";

    /* Start the presentation window at the requested physical dimensions even
     * when the native render scale is greater than one.  At NW_RENDER_SCALE=4
     * the game renders into a 2560x1920 framebuffer, but the window starts at
     * 640x480 and presentation downsamples it.  It is resizable by default, so
     * drag it to 2560x1920 (still 4:3) to inspect the native framebuffer 1:1;
     * NW_WINDOW_WIDTH / NW_WINDOW_HEIGHT choose a different initial size and
     * NW_WINDOW_RESIZABLE=0 restores the fixed 640x480 window. */
    windowW = NwSdlWindowDimension("NW_WINDOW_WIDTH", 640);
    windowH = NwSdlWindowDimension("NW_WINDOW_HEIGHT", 480);
    createW = windowW;
    createH = windowH;

    g_nwSdlWindow = SDL_CreateWindow(pszTitle, createW, createH, flags);
    if (g_nwSdlWindow == NULL) {
        char dbg[256];
        snprintf(dbg, sizeof(dbg), "window_create_fail_%s", SDL_GetError());
        NW_NATIVE_BOOT(dbg);
        fprintf(stderr, "nw-sdl-window: SDL_CreateWindow: %s\n", SDL_GetError());
        return;
    }
    NW_NATIVE_BOOT("window_created");

#if defined(NW_PORTABLE_IOS)
    /* Restore the remembered rotation lock as soon as the window exists.
     * Waiting for the first drawn frame would leave a short rotatable window
     * and could let the launch orientation change before the lock applies. */
    nw_ios_sync_rotation_locked();
    /* If the saved lock was applied before window creation, make sure UIKit
     * actually presents that orientation even when the device is currently
     * held the other way.  This runs while the window is still hidden. */
    if (g_nwSdlAndroidRotationLocked && g_nwSdlIosRotationMask[0] != '\0')
        NwIosRequestOrientation(g_nwSdlIosRotationMask);
#endif

#if !defined(__ANDROID__)
    /* Use the same generated artwork as the Android launcher, Windows
     * resource, and PS5 title icon. */
    {
        SDL_Surface *icon = SDL_CreateSurfaceFrom(
            NW_APP_ICON_WIDTH, NW_APP_ICON_HEIGHT, SDL_PIXELFORMAT_RGBA32,
            (void *)nw_app_icon_rgba, NW_APP_ICON_WIDTH * 4);
        if (icon != NULL) {
            (void)SDL_SetWindowIcon(g_nwSdlWindow, icon);
            SDL_DestroySurface(icon);
        }
    }

    /* SDL3 sizes windows by client area, so the requested 640x480 is already
     * the drawable size.  Center it in the usable work area instead of applying
     * the Win32 outer-frame arithmetic, which produced negative positions on
     * hosts whose metrics report a 640x480 desktop. */
    {
        SDL_Rect bounds;
        SDL_DisplayID display = SDL_GetPrimaryDisplay();
        if (display != 0 && SDL_GetDisplayUsableBounds(display, &bounds)) {
            int posX = bounds.x + (bounds.w - createW) / 2;
            int posY = bounds.y + (bounds.h - createH) / 2;
            if (posX < bounds.x)
                posX = bounds.x;
            if (posY < bounds.y)
                posY = bounds.y;
            (void)SDL_SetWindowPosition(g_nwSdlWindow, posX, posY);
        }
    }

    if (resizable) {
        /* The game is a fixed 4:3 canvas.  Ask the platform to constrain
         * interactive resizes to that aspect ratio; SDL applies this through
         * WM size hints on X11, WM_SIZING on Windows, and configure-time
         * constraints on Wayland. */
        (void)SDL_SetWindowAspectRatio(g_nwSdlWindow, 4.0f / 3.0f,
                                       4.0f / 3.0f);
        /* Keep the fixed logical client, but let the presentation surface be
         * any 4:3 size above a usable floor.  NwSdlVideoPresent() resamples to
         * the actual window surface, and input is mapped by window size. */
        int minW = (int)g_dwClientW / 2;
        int minH = (int)g_dwClientH / 2;
        if (minW < 320)
            minW = 320;
        if (minH < 240)
            minH = 240;
        (void)SDL_SetWindowMinimumSize(g_nwSdlWindow, minW, minH);
        if (pCfg->bThickFrameMode == 0)
            (void)SDL_SetWindowHitTest(g_nwSdlWindow, nw_sdl_hit_test, NULL);
    } else {
        (void)SDL_SetWindowMinimumSize(g_nwSdlWindow, windowW, windowH);
        (void)SDL_SetWindowMaximumSize(g_nwSdlWindow, windowW, windowH);
    }

    if (resizable) {
        NwSdlUpdateWindowLimits();
        NwSdlEnforceWindowAspect();
    }

    if (g_sdlArrowCursor == NULL)
        g_sdlArrowCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
    if (g_sdlArrowCursor != NULL)
        (void)SDL_SetCursor(g_sdlArrowCursor);

    {
        SDL_PropertiesID props = SDL_GetWindowProperties(g_nwSdlWindow);
        void *hwnd = SDL_GetPointerProperty(
            props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
        g_hWndPlayer = hwnd;
        if (hwnd != NULL)
            g_hdcPlayer = GetDC((HWND)hwnd);
    }
#endif

    SDL_ShowWindow(g_nwSdlWindow);
    NW_NATIVE_BOOT("window_shown");
    (void)SDL_RaiseWindow(g_nwSdlWindow);
    /* Text input is the portable WM_CHAR equivalent; printable keys arrive as
     * SDL_EVENT_TEXT_INPUT and are forwarded to the shipped decoder.  iOS also
     * shows the software keyboard on SDL_StartTextInput(), so open and close
     * it once to keep the keyboard hidden until the KEYBOARD control is used. */
#if defined(NW_PORTABLE_IOS)
    (void)SDL_StartTextInput(g_nwSdlWindow);
    SDL_StopTextInput(g_nwSdlWindow);
#else
    SDL_StartTextInput(g_nwSdlWindow);
#endif

    /* Seed the size globals now; the SDL_EVENT_WINDOW_RESIZED path keeps them
     * current.  The save copies remain the requested logical values: the
     * engine's layout is a fixed 640x480 canvas, and presentation/input map
     * through the real window size instead of overwriting these. */
    {
        nw_publish_client_size((int)g_dwClientW, (int)g_dwClientH);
    }
}
#endif /* NW_UNIT_CreatePlayerMainWindowSdl */
