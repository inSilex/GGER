#include "nwarch.h"
#include "nwdebug.h"
/*
 * VideoSdl -- SDL3 framebuffer, palette, and presentation backend.
 *
 * Module: video/sdl3
 * Interface: include/VideoGdi.h (same symbols as the Win32 backend)
 *
 * This file implements the SDL3 desktop video path.
 * three GDI entry points the game calls, with no windows.h dependency:
 *
 *   CreatePlayerDIBBackBuffer   8bpp framebuffer instead of CreateDIBSection
 *   BlitRegionFromDcOrDib       copy into the framebuffer; no GDI blit
 *   SetLogicalPaletteDrawGrid   256-entry ARGB LUT plus grid drawing
 *
 * The game keeps its original 8bpp, bottom-up layout.  Presentation expands
 * the framebuffer through the LUT into the SDL window surface owned by
 * ui/sdl3/WindowChromeSdl.c, then presents it with SDL_UpdateWindowSurface.
 *
 * All code and state live in the private SDL sections (.nwsvg / .nwsvgdata /
 * .nwsvgbss) so the game's fixed .text/.data window is untouched.
 */

#ifdef NW_UNIT_VideoSdl

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_timer.h>

#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
#include "NwSdlAndroidControls.h"
#include "NwAndroidIcons.h"
#endif

/* ---- minimal Win32-compatible vocabulary (no windows.h) ------------------ */

typedef int           BOOL;
typedef unsigned int  UINT;
typedef uint32_t      DWORD;

#define WINAPI __stdcall

typedef struct {
    unsigned char rgbtBlue;
    unsigned char rgbtGreen;
    unsigned char rgbtRed;
} RGBTRIPLE;

/*
 * The game passes a pointer to the recovered 16-byte arena at
 * nw_data_window+0x6314.  Its fields are 32-bit guest values, and the layout
 * is shared with the Win32 backend:
 *
 *   +0  dwCount / bytes remaining
 *   +4  pvScratch / bump cursor
 *   +8  dwHeader / buffer bytes
 *   +c  pvDIBits / cached base
 *
 * Using size_t or a native pointer here makes the structure 32 bytes wide on
 * a 64-bit host, so writing pvScratch would overwrite the buffer size with a
 * host malloc address.  Keep the structure in guest width.
 */
typedef struct {
    uint32_t dwCount;
    uint32_t pvScratch;
    uint32_t dwHeader;
    uint32_t pvDIBits;
} PLAYERBUFINFO, *PPLAYERBUFINFO;

#define NW_DIAG_CODE __attribute__((section(".nwsvg")))
#define NW_DIAG_BSS  __attribute__((section(".nwsvgbss")))
#define NW_DIAG_DATA __attribute__((section(".nwsvgdata")))

/* Profiler slots; implemented by app/sdl3/MainLoopSdl.c. */
#define NWP_PRESENT 2
#define NWP_CONV    3
#define NWP_UPDATE  4
extern int      NwProfOn(void);
extern uint64_t NwProfNow(void);
extern void     NwProfAdd(int slot, uint64_t start, uint64_t end);
extern void     NwProfBump(int slot);

extern unsigned char nw_data_window[];

extern void CreatePlayerMainWindow(void *pPlayerCfg);

#if defined(NW_PORTABLE_PS5) || defined(NW_PS5_NATIVE_TITLE)
/* The PS5 libc heap can return addresses above 4 GiB, while the recovered
 * engine keeps pointers in 32-bit fields.  Large buffers that feed those
 * fields must therefore come from the low-address mmap path.  The native
 * title also has a small libc heap. */
extern void *NwHostAllocLow(size_t size);
extern void platform_boot_mark(const char *stage);
extern void platform_log_mark(const char *stage);
#define NW_PS5_BOOT(stage) platform_boot_mark(stage)
#define NW_PS5_LOG(stage) platform_log_mark(stage)
#elif defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
/* Android and iOS arm64 also use the guest-window allocator, but have no
 * boot trace. */
extern void *NwHostAllocLow(size_t size);
#define NW_PS5_BOOT(stage) ((void)0)
#define NW_PS5_LOG(stage) ((void)0)
#else
#define NW_PS5_BOOT(stage) ((void)0)
#define NW_PS5_LOG(stage) ((void)0)
#endif

#if defined(NW_PORTABLE_PS5) || defined(NW_PS5_NATIVE_TITLE)
/* One-shot presentation marks: tell us whether the main loop reaches the
 * first present, whether SDL gives us a window surface, and whether the
 * surface update succeeds.  Repeating them every frame would flood the
 * boot log. */
NW_DIAG_CODE static void nw_mark_present_first(void)
{
    static int done;
    if (!done) {
        done = 1;
        platform_boot_mark("present_first");
    }
}

NW_DIAG_CODE static void nw_mark_present_ok(void)
{
    static int done;
    if (!done) {
        done = 1;
        platform_boot_mark("present_ok");
    }
}

NW_DIAG_CODE static void nw_mark_present_surface_fail(void)
{
    static int done;
    if (!done) {
        done = 1;
        platform_boot_mark("present_surface_fail");
    }
}

NW_DIAG_CODE static void nw_mark_present_update_fail(void)
{
    static int done;
    if (!done) {
        done = 1;
        platform_boot_mark("present_update_fail");
    }
}
#else
#define nw_mark_present_first() ((void)0)
#define nw_mark_present_ok() ((void)0)
#define nw_mark_present_surface_fail() ((void)0)
#define nw_mark_present_update_fail() ((void)0)
#endif

/* Owned by ui/sdl3/WindowChromeSdl.c.  The window is created before the first
 * CreatePlayerDIBBackBuffer call, so it is normally already present here. */
extern SDL_Window *g_nwSdlWindow;
extern int      NwSdlRenderScale(void);
extern void     NwSdlFitViewport4x3(int availW, int availH,
                                    int *px, int *py, int *pw, int *ph);
extern int      NwSdlJoystickCursorActive(void);
extern void     NwSdlJoystickCursorPosition(int *px, int *py);
extern int      NwSdlAndroidControlPressed(void);

extern int      g_cxGridClient;
extern int      g_cyGridClient;
extern uint32_t g_rgcrGridPenColors[];

/* Data-window fields.  Offsets match src/video/win32/VideoGdi.c and
 * src/ui/win32/WindowChrome.c. */
#define NW32(off) (*(uint32_t *)(nw_data_window + (off)))
#define NWPTR(off) NW_WINDOW_PTR_REF(void, (off))

#define g_hWndPlayer      NWPTR(0x6404)
#define g_hdcMem          NWPTR(0x6064)
#define g_pvDIBits        NWPTR(0x6060)
#define g_pvScratch       NWPTR(0x5f0c)
#define g_dwClientW       NW32(0x5f20)
#define g_dwClientH       NW32(0x5f24)
#define g_dwSourceWidth   NW32(0x1ccc)

#define NW_DIB_OFFSET     0x698
#define NW_SCRATCH_RESERVE 80000u

/* ---- state --------------------------------------------------------------- */

typedef struct {
    SDL_Window   *win;
    SDL_Surface  *surface;
#if defined(__ANDROID__)
    /* Android has no software window surface.  The converted ARGB frame is
     * uploaded to this streaming texture and scaled to the activity surface. */
    SDL_Renderer *renderer;
    SDL_Texture  *texture;
#endif
    unsigned char *fb;        /* 8bpp, bottom-up, pitch bytes per row */
    int            w;
    int            h;
    int            pitch;
    int            ready;     /* presentation target usable */
    uint32_t       lut[256];  /* 0xAARRGGBB */
} NwSdlVideoState;

static NwSdlVideoState g_v NW_DIAG_BSS;

#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
static void nw_draw_android_controls(SDL_Surface *dst);
#endif

/* ---- helpers ------------------------------------------------------------- */

NW_DIAG_CODE static int nw_dib_width(void)
{
    int w = *(int *)(nw_data_window + NW_DIB_OFFSET + 4);
    if (w <= 0)
        w = (int)g_dwClientW;
    return w;
}

NW_DIAG_CODE static int nw_dib_height(void)
{
    int h = *(int *)(nw_data_window + NW_DIB_OFFSET + 8);
    if (h < 0)
        h = -h;
    if (h <= 0)
        h = (int)g_dwClientH;
    return h;
}

NW_DIAG_CODE static int nw_nearest_index(unsigned r, unsigned g, unsigned b)
{
    int best = 0;
    uint32_t bestd = 0xffffffffu;
    int i;

    for (i = 0; i < 256; i++) {
        unsigned pr = (g_v.lut[i] >> 16) & 0xffu;
        unsigned pg = (g_v.lut[i] >> 8) & 0xffu;
        unsigned pb = g_v.lut[i] & 0xffu;
        unsigned dr = r > pr ? r - pr : pr - r;
        unsigned dg = g > pg ? g - pg : pg - g;
        unsigned db = b > pb ? b - pb : pb - b;
        uint32_t d = dr * dr + dg * dg + db * db;
        if (d < bestd) {
            bestd = d;
            best = i;
        }
    }
    return best;
}

NW_DIAG_CODE static int nw_video_init(int w, int h)
{
    if (g_v.ready)
        return 1;

    NW_PS5_BOOT("video_init_begin");
    /* The SDL window backend creates the window before the first DIB request.
     * There is no HWND wrapping or second window here. */
    if (g_nwSdlWindow == NULL)
    {
        NW_PS5_BOOT("video_init_no_window");
        fprintf(stderr, "nw-sdl: player window is not available\n");
        return 0;
    }
    g_v.win = g_nwSdlWindow;

#if defined(__ANDROID__)
    g_v.surface = NULL;
    /* A textured-quad renderer is all this target needs.  Prefer GLES2 for
     * broad device support; fall back to SDL's default (GPU/Vulkan) choice. */
    g_v.renderer = SDL_CreateRenderer(g_v.win, "opengles2");
    if (g_v.renderer == NULL)
        g_v.renderer = SDL_CreateRenderer(g_v.win, NULL);
    if (g_v.renderer == NULL)
    {
        char dbg[256];
        snprintf(dbg, sizeof(dbg), "video_init_renderer_fail_%s",
                 SDL_GetError());
        NW_PS5_BOOT(dbg);
        fprintf(stderr, "nw-sdl: SDL_CreateRenderer: %s\n", SDL_GetError());
        return 0;
    }
    (void)SDL_SetRenderVSync(g_v.renderer, 1);
#else
    g_v.surface = SDL_GetWindowSurface(g_v.win);
    if (g_v.surface == NULL)
    {
        char dbg[256];
        snprintf(dbg, sizeof(dbg), "video_init_surface_fail_%s", SDL_GetError());
        NW_PS5_BOOT(dbg);
        fprintf(stderr, "nw-sdl: SDL_GetWindowSurface: %s\n", SDL_GetError());
        return 0;
    }
#endif

    g_v.pitch = (w + 3) & ~3;
#if defined(NW_PORTABLE_PS5) || defined(NW_PS5_NATIVE_TITLE) || \
    defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
    /* Keep the 32-bit guest mirror valid and, on the native title, stay off
     * the small libc heap. */
    {
        int low = 1;
        g_v.fb = (unsigned char *)NwHostAllocLow((size_t)g_v.pitch * (size_t)h);
        if (g_v.fb == NULL) {
            low = 0;
            g_v.fb = (unsigned char *)SDL_aligned_alloc(16,
                                                        (size_t)g_v.pitch * (size_t)h);
        }
        if (g_v.fb != NULL) {
            char dbg[96];
            snprintf(dbg, sizeof(dbg), "fb_%s_%lx", low ? "low" : "high",
                     (unsigned long)(uintptr_t)g_v.fb);
            NW_PS5_BOOT(dbg);
        }
    }
#else
    g_v.fb = (unsigned char *)SDL_aligned_alloc(16,
                                                (size_t)g_v.pitch * (size_t)h);
#endif
    if (g_v.fb == NULL)
    {
        char dbg[96];
        snprintf(dbg, sizeof(dbg), "video_init_fb_fail_%dx%d", w, h);
        NW_PS5_BOOT(dbg);
        fprintf(stderr, "nw-sdl: framebuffer alloc failed (%dx%d)\n", w, h);
        return 0;
    }

    memset(g_v.fb, 0, (size_t)g_v.pitch * (size_t)h);
    g_v.w = w;
    g_v.h = h;
    g_v.ready = 1;
    NW_PS5_BOOT("video_init_ok");
    return 1;
}

/* Opt-in framebuffer snapshot used by the headless/scale regression tests.
 * NW_DEBUG_SCREENSHOT names a PPM path; NW_DEBUG_SCREENSHOT_EVERY throttles writes (default 60). */
NW_DIAG_CODE static void nw_debug_shot(void)
{
    static unsigned long counter;
    const char *pszPath = NW_DEBUG_GETENV("NW_DEBUG_SCREENSHOT");
    const char *pszEvery;
    unsigned long every = 60;
    char szTrigger[1200];
    FILE *f;
    int bypassThrottle = 0;
    int x;
    int y;

    if (pszPath == NULL || pszPath[0] == '\0' || g_v.fb == NULL)
        return;
    /* A touch-file trigger lets a test capture a stable screen after the
     * intro without paying for a framebuffer write on every frame. */
    snprintf(szTrigger, sizeof szTrigger, "%s.go", pszPath);
    f = fopen(szTrigger, "rb");
    if (f != NULL) {
        fclose(f);
        remove(szTrigger);
        bypassThrottle = 1;
    }
    pszEvery = NW_DEBUG_GETENV("NW_DEBUG_SCREENSHOT_EVERY");
    if (pszEvery != NULL && pszEvery[0] != '\0') {
        unsigned long v = (unsigned long)strtoul(pszEvery, NULL, 0);
        if (v > 0)
            every = v;
    }
    counter++;
    if (!bypassThrottle && counter % every != 0)
        return;
    f = fopen(pszPath, "wb");
    if (f == NULL)
        return;
    fprintf(f, "P6\n%d %d\n255\n", g_v.w, g_v.h);
    for (y = 0; y < g_v.h; y++) {
        const unsigned char *src = g_v.fb +
            (size_t)(g_v.h - 1 - y) * (size_t)g_v.pitch;
        for (x = 0; x < g_v.w; x++) {
            uint32_t c = g_v.lut[src[x]];
            unsigned char rgb[3];
            rgb[0] = (unsigned char)((c >> 16) & 0xffu);
            rgb[1] = (unsigned char)((c >> 8) & 0xffu);
            rgb[2] = (unsigned char)(c & 0xffu);
            fwrite(rgb, 1, 3, f);
        }
    }
    fclose(f);
}

/* ---- presentation -------------------------------------------------------- */

NW_DIAG_CODE static void nw_cursor_rect(SDL_Surface *dst, int x, int y,
                                          int w, int h, Uint32 color)
{
    SDL_Rect rect;

    if (w <= 0 || h <= 0)
        return;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    (void)SDL_FillSurfaceRect(dst, &rect, color);
}

/* Draw a high-contrast crosshair while the controller owns the pointer.  The
 * host arrow may be hidden by the desktop after a period without mouse input,
 * and on scaled windows the system cursor is not representative of the game's
 * logical cursor.  Size the crosshair with the presentation scale so it stays
 * visible at 1:1 and at 2560x1920. */
NW_DIAG_CODE static void nw_draw_joystick_cursor(SDL_Surface *dst)
{
    int x = 0;
    int y = 0;
    int px;
    int py;
    int unit;
    int vx = 0;
    int vy = 0;
    int vw = 0;
    int vh = 0;
    Uint32 black;
    Uint32 white;

    if (dst == NULL || !NwSdlJoystickCursorActive())
        return;

    NwSdlJoystickCursorPosition(&x, &y);
    if (x < 0)
        x = 0;
    if (x > 639)
        x = 639;
    if (y < 0)
        y = 0;
    if (y > 479)
        y = 479;

    NwSdlFitViewport4x3(dst->w, dst->h, &vx, &vy, &vw, &vh);
    if (vw <= 0 || vh <= 0)
        return;
    px = vx + (int)((long)x * vw / 640);
    py = vy + (int)((long)y * vh / 480);
    unit = vw / 640;
    if (unit < 1)
        unit = 1;
    if (unit > 4)
        unit = 4;

    black = SDL_MapSurfaceRGBA(dst, 0, 0, 0, 255);
    white = SDL_MapSurfaceRGBA(dst, 255, 255, 255, 255);

    nw_cursor_rect(dst, px - 7 * unit, py - 2 * unit,
                   15 * unit, 5 * unit, black);
    nw_cursor_rect(dst, px - 2 * unit, py - 7 * unit,
                   5 * unit, 15 * unit, black);
    nw_cursor_rect(dst, px - 6 * unit, py - unit,
                   13 * unit, 3 * unit, white);
    nw_cursor_rect(dst, px - unit, py - 6 * unit,
                   3 * unit, 13 * unit, white);
}

/* The boot trace is a start-up diagnostic.  Once a real game frame is about
 * to reach SDL, the trace must never repaint the window surface: a boot mark
 * emitted from inside the video driver would otherwise replace the game image
 * with a black screen and one line of stage text. */
static int nw_boot_trace_suppressed;
/* Once the main loop is running it presents exactly once per frame.  The
 * blit path also calls NwSdlVideoPresent after each batch; on the console
 * every one of those presents waits for a vblank, which can halve or third
 * the frame rate.  Suppress the redundant batch presents after the loop
 * starts; the frame present at the end of the step shows the final DIB. */
static int nw_main_loop_active;
static int nw_present_frame_call;

/* A composed frame-cell animation can flush several dirty rectangles, and
 * every flush calls NwSdlVideoPresent().  On the console each present waits
 * for a vblank, so a multi-rectangle frame can take several display periods.
 * Suppress the intermediate presents and show the finished frame once. */
static int nw_compose_depth;
static Uint64 nw_compose_start_ns;
static unsigned nw_compose_frames;
static Uint64 nw_compose_total_ns;
static Uint64 nw_compose_max_ns;
static Uint64 nw_compose_present_total_ns;
static Uint64 nw_compose_present_max_ns;
static Uint64 nw_compose_interval_total_ns;
static Uint64 nw_compose_interval_max_ns;
static Uint64 nw_compose_last_end_ns;
static int nw_compose_have_last;
static unsigned nw_compose_target;
static unsigned nw_compose_step;
/* Main-loop composition accounting.  The anim_* counters above are for the
 * pre-loop intro; these are consumed once per rendered frame by MainLoopSdl. */
static Uint64   nw_compose_loop_ns;
static unsigned nw_compose_loop_frames;
static int      nw_compose_loop_first_logged;
static Uint64   nw_compose_slow_last_ns;

void NwSdlVideoTakeComposeStats(uint64_t *pNs, unsigned *pFrames)
{
    if (pNs != NULL)
        *pNs = nw_compose_loop_ns;
    if (pFrames != NULL)
        *pFrames = nw_compose_loop_frames;
    nw_compose_loop_ns = 0;
    nw_compose_loop_frames = 0;
}

#if defined(NW_PORTABLE_PS5) || defined(NW_PS5_NATIVE_TITLE)
/* One-shot frame audit: the first game frame's palette entries and a coarse
 * count of non-zero source pixels.  A black palette or an all-zero DIB is the
 * difference between a presentation bug and a renderer bug. */
NW_DIAG_CODE static void nw_mark_first_frame(void)
{
    static int done;
    char stage[160];
    unsigned nonblack = 0;
    int x;
    int y;

    if (done)
        return;
    done = 1;
    if (g_v.fb != NULL && g_v.pitch > 0 && g_v.w > 0 && g_v.h > 0) {
        for (y = 0; y < g_v.h; y += 16) {
            const unsigned char *row = g_v.fb +
                (size_t)(g_v.h - 1 - y) * (size_t)g_v.pitch;
            for (x = 0; x < g_v.w; x += 16) {
                if (row[x] != 0)
                    nonblack++;
            }
        }
    }
    snprintf(stage, sizeof stage, "frame0_lut_%08x_%08x_nz_%u_%dx%d",
             (unsigned)g_v.lut[0], (unsigned)g_v.lut[1], nonblack,
             g_v.w, g_v.h);
    NW_PS5_BOOT(stage);
}
#else
#define nw_mark_first_frame() ((void)0)
#endif

#if defined(__ANDROID__)
/*
 * Android's SDL video backend deliberately has no window framebuffer.  Keep
 * the existing palette conversion and controls overlay, but write them into an
 * ARGB surface that is uploaded to a streaming texture.  The texture contains
 * exactly the centered 4:3 viewport plus the black bars, so the GPU performs
 * the final scale to the activity surface.
 */
NW_DIAG_CODE static SDL_Surface *nw_android_target(void)
{
    int ww = 0;
    int wh = 0;

    if (g_v.renderer == NULL)
        return NULL;
    if (!SDL_GetWindowSize(g_v.win, &ww, &wh) || ww <= 0 || wh <= 0)
        return NULL;

    if (g_v.surface != NULL &&
        g_v.surface->w == ww && g_v.surface->h == wh &&
        g_v.texture != NULL)
        return g_v.surface;

    if (g_v.texture != NULL) {
        SDL_DestroyTexture(g_v.texture);
        g_v.texture = NULL;
    }
    if (g_v.surface != NULL) {
        SDL_DestroySurface(g_v.surface);
        g_v.surface = NULL;
    }

    g_v.surface = SDL_CreateSurface(ww, wh, SDL_PIXELFORMAT_ARGB8888);
    if (g_v.surface == NULL) {
        fprintf(stderr, "nw-sdl: Android surface: %s\n", SDL_GetError());
        return NULL;
    }
    g_v.texture = SDL_CreateTexture(g_v.renderer, SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING, ww, wh);
    if (g_v.texture == NULL) {
        fprintf(stderr, "nw-sdl: Android texture: %s\n", SDL_GetError());
        SDL_DestroySurface(g_v.surface);
        g_v.surface = NULL;
        return NULL;
    }
    (void)SDL_SetTextureScaleMode(g_v.texture, SDL_SCALEMODE_NEAREST);
    (void)SDL_FillSurfaceRect(g_v.surface, NULL, 0xff000000u);
    return g_v.surface;
}

NW_DIAG_CODE static int nw_android_present(SDL_Surface *dst)
{
    if (g_v.renderer == NULL || g_v.texture == NULL || dst == NULL)
        return 0;
    if (!SDL_UpdateTexture(g_v.texture, NULL, dst->pixels, dst->pitch))
        return 0;
    (void)SDL_SetRenderDrawColor(g_v.renderer, 0, 0, 0, 255);
    (void)SDL_RenderClear(g_v.renderer);
    (void)SDL_RenderTexture(g_v.renderer, g_v.texture, NULL, NULL);
    (void)SDL_RenderPresent(g_v.renderer);
    return 1;
}
#endif

NW_DIAG_CODE static void nw_present(int bForce)
{
    static Uint64 last_present;
    static int have_last_present;
    SDL_Surface *dst;
    Uint64 now;
    uint64_t nwProfStart;
    uint64_t nwConvStart;
    uint64_t nwUpdateStart;
    int nwProf;
    int x;
    int y;
    static int nwPresentEnterMarked;
    static int nwPresentNotReadyMarked;

    if (!nwPresentEnterMarked) {
        nwPresentEnterMarked = 1;
        NW_PS5_BOOT("present_enter");
    }
    if (!g_v.ready || g_v.fb == NULL || g_v.win == NULL) {
        if (!nwPresentNotReadyMarked) {
            nwPresentNotReadyMarked = 1;
            NW_PS5_BOOT("present_not_ready");
        }
        return;
    }
    if (nw_compose_depth > 0 && !bForce)
        return;
    if (nw_main_loop_active && !nw_present_frame_call && !bForce)
        return;
    nw_mark_first_frame();
    nw_mark_present_first();

    /* One display-list render can present several batches; each present scans
     * and converts the whole 640x480 framebuffer.  Coalesce those calls so the
     * Win32/ELF event and input pumps are not starved by redundant readbacks. */
    now = SDL_GetTicks();
    if (!bForce && have_last_present && (now - last_present) < 8u)
        return;
    last_present = now;
    have_last_present = 1;

    nwProf = NwProfOn();
    nwProfStart = nwProf ? NwProfNow() : 0;
    if (nwProf)
        NwProfBump(NWP_PRESENT);

#if defined(__ANDROID__)
    dst = nw_android_target();
#else
    dst = SDL_GetWindowSurface(g_v.win);
#endif
    if (dst == NULL || dst->pixels == NULL) {
        nw_mark_present_surface_fail();
        return;
    }
    nwConvStart = nwProf ? NwProfNow() : 0;

    {
        int vx = 0;
        int vy = 0;
        int vw = 0;
        int vh = 0;
        int needScale;
        const SDL_PixelFormatDetails *fmt = NULL;
        const SDL_Palette *pal = NULL;

        /* The game always occupies a centered 4:3 viewport.  In a normal 4:3
         * window this is the whole surface and the output is unchanged; in
         * borderless fullscreen on a wider display the surrounding bars are
         * filled black instead of stretching the game. */
        NwSdlFitViewport4x3(dst->w, dst->h, &vx, &vy, &vw, &vh);
        if (vw <= 0 || vh <= 0)
            return;

        if (vx != 0 || vy != 0 || vw != dst->w || vh != dst->h) {
            Uint32 black = 0;
            fmt = SDL_GetPixelFormatDetails(dst->format);
            if (fmt != NULL) {
                pal = SDL_GetSurfacePalette(dst);
                black = SDL_MapRGBA(fmt, pal, 0, 0, 0, 255);
            }
            (void)SDL_FillSurfaceRect(dst, NULL, black);
        }

        needScale = (vw != g_v.w || vh != g_v.h);

        if (dst->format == SDL_PIXELFORMAT_ARGB8888 ||
            dst->format == SDL_PIXELFORMAT_XRGB8888) {
            if (!needScale) {
                /* Fast path used by the normal 4:3 window and by a scale-1
                 * presentation.  It is still a direct palette lookup, so the
                 * scale-1 output remains pixel-identical to the original. */
                for (y = 0; y < vh; y++) {
                    const unsigned char *src = g_v.fb +
                        (size_t)(g_v.h - 1 - y) * (size_t)g_v.pitch;
                    uint32_t *d = (uint32_t *)((unsigned char *)dst->pixels +
                        (size_t)(vy + y) * (size_t)dst->pitch +
                        (size_t)vx * 4u);
                    for (x = 0; x < vw; x++)
                        d[x] = g_v.lut[src[x]];
                }
            } else {
                /* Fixed-point nearest-neighbour sampling into the viewport.
                 * The old per-pixel 64-bit division dominated the frame time
                 * at 4x: 1.55M divides for the 1440x1080 surface, then
                 * another 2.07M in the console driver. */
                const uint32_t xstep =
                    ((uint32_t)g_v.w << 16) / (uint32_t)vw;
                const uint32_t ystep =
                    ((uint32_t)g_v.h << 16) / (uint32_t)vh;
                uint32_t syacc = 0;

                for (y = 0; y < vh; y++) {
                    const int sy = (int)(syacc >> 16);
                    const unsigned char *src = g_v.fb +
                        (size_t)(g_v.h - 1 - sy) * (size_t)g_v.pitch;
                    uint32_t *d = (uint32_t *)((unsigned char *)dst->pixels +
                        (size_t)(vy + y) * (size_t)dst->pitch +
                        (size_t)vx * 4u);
                    uint32_t sxacc = 0;

                    for (x = 0; x < vw; x++) {
                        d[x] = g_v.lut[src[sxacc >> 16]];
                        sxacc += xstep;
                    }
                    syacc += ystep;
                }
            }
        } else {
            int bpp;
            fmt = SDL_GetPixelFormatDetails(dst->format);
            pal = SDL_GetSurfacePalette(dst);
            if (fmt == NULL)
                return;
            bpp = (int)fmt->bytes_per_pixel;

            if (!needScale) {
                for (y = 0; y < vh; y++) {
                    const unsigned char *src = g_v.fb +
                        (size_t)(g_v.h - 1 - y) * (size_t)g_v.pitch;
                    unsigned char *drow = (unsigned char *)dst->pixels +
                        (size_t)(vy + y) * (size_t)dst->pitch +
                        (size_t)vx * (size_t)bpp;
                    for (x = 0; x < vw; x++) {
                        uint32_t c = g_v.lut[src[x]];
                        uint32_t p = SDL_MapRGBA(fmt, pal,
                                                 (uint8_t)((c >> 16) & 0xff),
                                                 (uint8_t)((c >> 8) & 0xff),
                                                 (uint8_t)(c & 0xff),
                                                 (uint8_t)((c >> 24) & 0xff));
                        memcpy(drow + (size_t)x * (size_t)bpp, &p,
                               (size_t)bpp);
                    }
                }
            } else {
                for (y = 0; y < vh; y++) {
                    int sy = (int)((long)y * g_v.h / vh);
                    const unsigned char *src = g_v.fb +
                        (size_t)(g_v.h - 1 - sy) * (size_t)g_v.pitch;
                    unsigned char *drow = (unsigned char *)dst->pixels +
                        (size_t)(vy + y) * (size_t)dst->pitch +
                        (size_t)vx * (size_t)bpp;
                    for (x = 0; x < vw; x++) {
                        int sx = (int)((long)x * g_v.w / vw);
                        uint32_t c = g_v.lut[src[sx]];
                        uint32_t p = SDL_MapRGBA(fmt, pal,
                                                 (uint8_t)((c >> 16) & 0xff),
                                                 (uint8_t)((c >> 8) & 0xff),
                                                 (uint8_t)(c & 0xff),
                                                 (uint8_t)((c >> 24) & 0xff));
                        memcpy(drow + (size_t)x * (size_t)bpp, &p,
                               (size_t)bpp);
                    }
                }
            }
        }
    }

    nw_draw_joystick_cursor(dst);
#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
    nw_draw_android_controls(dst);
#endif
    nw_debug_shot();
    if (nwProf)
        NwProfAdd(NWP_CONV, nwConvStart, NwProfNow());
    nwUpdateStart = nwProf ? NwProfNow() : 0;
    nw_boot_trace_suppressed = 1;
#if defined(__ANDROID__)
    if (!nw_android_present(dst)) {
        nw_boot_trace_suppressed = 0;
        nw_mark_present_update_fail();
    } else {
        nw_mark_present_ok();
    }
#else
    if (!SDL_UpdateWindowSurface(g_v.win)) {
        nw_boot_trace_suppressed = 0;
        nw_mark_present_update_fail();
    } else {
        nw_mark_present_ok();
    }
#endif
    if (nwProf) {
        NwProfAdd(NWP_UPDATE, nwUpdateStart, NwProfNow());
        NwProfAdd(NWP_PRESENT, nwProfStart, NwProfNow());
    }
}

NW_DIAG_CODE void NwSdlVideoPresent(void)
{
    nw_present(0);
}

/* Used when the software joystick cursor is removed: the game may be idle, so
 * a normal coalesced present might be skipped and leave the crosshair on the
 * window surface. */
NW_DIAG_CODE void NwSdlVideoPresentNow(void)
{
    nw_present(1);
}

/* The main loop's once-per-frame present.  Batch presents from the blit path
 * are ignored once this is active. */
NW_DIAG_CODE void NwSdlVideoPresentFrame(void)
{
    nw_present_frame_call = 1;
    nw_present(0);
    nw_present_frame_call = 0;
}

void NwSdlVideoSetMainLoopActive(int active)
{
    nw_main_loop_active = active;
}

/* ComposeFrameCellsFromStreamRecords brackets one animation frame.  While it
 * is active, only the final present is allowed through.  Outside the main
 * loop (the built-in intro/menu animation) this also measures composition and
 * presentation so the console boot log can distinguish a slow renderer from a
 * slow VideoOut flip. */
void NwSdlVideoComposeBegin(unsigned target, unsigned step)
{
    if (nw_compose_depth == 0) {
        nw_compose_start_ns = SDL_GetTicksNS();
        nw_compose_target = target;
        nw_compose_step = step;
        if (!nw_main_loop_active && step <= 1u) {
            char stage[64];
            snprintf(stage, sizeof stage, "anim_group_begin_t%u", target);
            NW_PS5_LOG(stage);
        }
    }
    nw_compose_depth++;
}

void NwSdlVideoComposeEnd(int present)
{
    Uint64 end;
    Uint64 dur;
    Uint64 interval;

    if (nw_compose_depth > 0)
        nw_compose_depth--;
    else
        return;
    if (nw_compose_depth != 0)
        return;

    end = SDL_GetTicksNS();
    dur = end - nw_compose_start_ns;
    nw_compose_frames++;
    nw_compose_total_ns += dur;
    if (dur > nw_compose_max_ns)
        nw_compose_max_ns = dur;
    if (nw_compose_have_last) {
        interval = end - nw_compose_last_end_ns;
        nw_compose_interval_total_ns += interval;
        if (interval > nw_compose_interval_max_ns)
            nw_compose_interval_max_ns = interval;
    }
    nw_compose_last_end_ns = end;
    nw_compose_have_last = 1;

    if (nw_main_loop_active) {
        char stage[128];

        nw_compose_loop_ns += dur;
        nw_compose_loop_frames++;
        if (!nw_compose_loop_first_logged) {
            nw_compose_loop_first_logged = 1;
            snprintf(stage, sizeof stage, "compose_first_us_%u_t%u_s%u",
                     (unsigned)(dur / 1000u), nw_compose_target,
                     nw_compose_step);
            NW_PS5_LOG(stage);
        }
        /* A full-surface composition can be the first level's large 4x
         * background blit.  Log it once per second at most; a normal frame
         * stays well below this threshold. */
        if (dur > 10000000u &&
            (nw_compose_slow_last_ns == 0 ||
             end - nw_compose_slow_last_ns > 1000000000u)) {
            nw_compose_slow_last_ns = end;
            snprintf(stage, sizeof stage, "compose_slow_us_%u_t%u_s%u",
                     (unsigned)(dur / 1000u), nw_compose_target,
                     nw_compose_step);
            NW_PS5_LOG(stage);
        }
    }

    if (present && !nw_main_loop_active) {
        Uint64 present_start = SDL_GetTicksNS();
        Uint64 present_dur;

        NwSdlVideoPresentNow();
        present_dur = SDL_GetTicksNS() - present_start;
        nw_compose_present_total_ns += present_dur;
        if (present_dur > nw_compose_present_max_ns)
            nw_compose_present_max_ns = present_dur;
    }

    if (!nw_main_loop_active && nw_compose_frames >= 30u) {
        char stage[160];

        snprintf(stage, sizeof stage,
                 "anim_%u_avg_%uus_max_%uus_int_%uus_%uus_pres_%uus_%uus_t%u_s%u",
                 nw_compose_frames,
                 (unsigned)((nw_compose_total_ns / 1000u) /
                            nw_compose_frames),
                 (unsigned)(nw_compose_max_ns / 1000u),
                 (unsigned)((nw_compose_interval_total_ns / 1000u) /
                            nw_compose_frames),
                 (unsigned)(nw_compose_interval_max_ns / 1000u),
                 (unsigned)((nw_compose_present_total_ns / 1000u) /
                            nw_compose_frames),
                 (unsigned)(nw_compose_present_max_ns / 1000u),
                 nw_compose_target, nw_compose_step);
        NW_PS5_LOG(stage);
        nw_compose_frames = 0;
        nw_compose_total_ns = 0;
        nw_compose_max_ns = 0;
        nw_compose_interval_total_ns = 0;
        nw_compose_interval_max_ns = 0;
        nw_compose_present_total_ns = 0;
        nw_compose_present_max_ns = 0;
    }
}

/* ---- boot trace --------------------------------------------------------- */

/* The native title cannot use klog or the notification service: the klog
 * syscall is illegal and the raw notification call blocks.  Draw the last
 * startup stage straight into the window surface so it remains visible on
 * screen even when the game never reaches its first present. */
static const uint8_t nw_boot_font[41][5] = {
    {0x7e,0x09,0x09,0x09,0x7e}, /* A */
    {0x7f,0x49,0x49,0x49,0x36}, /* B */
    {0x3e,0x41,0x41,0x41,0x22}, /* C */
    {0x7f,0x41,0x41,0x41,0x3e}, /* D */
    {0x7f,0x49,0x49,0x49,0x41}, /* E */
    {0x7f,0x09,0x09,0x09,0x01}, /* F */
    {0x3e,0x41,0x41,0x49,0x7a}, /* G */
    {0x7f,0x08,0x08,0x08,0x7f}, /* H */
    {0x00,0x41,0x7f,0x41,0x00}, /* I */
    {0x20,0x40,0x41,0x3f,0x01}, /* J */
    {0x7f,0x08,0x14,0x22,0x41}, /* K */
    {0x7f,0x40,0x40,0x40,0x40}, /* L */
    {0x7f,0x02,0x0c,0x02,0x7f}, /* M */
    {0x7f,0x04,0x08,0x10,0x7f}, /* N */
    {0x3e,0x41,0x41,0x41,0x3e}, /* O */
    {0x7f,0x09,0x09,0x09,0x06}, /* P */
    {0x3e,0x41,0x51,0x21,0x5e}, /* Q */
    {0x7f,0x09,0x19,0x29,0x46}, /* R */
    {0x46,0x49,0x49,0x49,0x31}, /* S */
    {0x01,0x01,0x7f,0x01,0x01}, /* T */
    {0x3f,0x40,0x40,0x40,0x3f}, /* U */
    {0x1f,0x20,0x40,0x20,0x1f}, /* V */
    {0x3f,0x40,0x38,0x40,0x3f}, /* W */
    {0x63,0x14,0x08,0x14,0x63}, /* X */
    {0x07,0x08,0x70,0x08,0x07}, /* Y */
    {0x61,0x51,0x49,0x45,0x43}, /* Z */
    {0x3e,0x51,0x49,0x45,0x3e}, /* 0 */
    {0x00,0x42,0x7f,0x40,0x00}, /* 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 2 */
    {0x21,0x41,0x45,0x4b,0x31}, /* 3 */
    {0x18,0x14,0x12,0x7f,0x10}, /* 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 5 */
    {0x3c,0x4a,0x49,0x49,0x30}, /* 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 8 */
    {0x06,0x49,0x49,0x29,0x1e}, /* 9 */
    {0x40,0x40,0x40,0x40,0x40}, /* _ */
    {0x08,0x08,0x08,0x08,0x08}, /* - */
    {0x11,0x0a,0x04,0x00,0x00}, /* > */
    {0x00,0x00,0x40,0x00,0x00}, /* . */
    {0x00,0x00,0x00,0x00,0x00}, /* space */
};

static int nw_boot_font_index(int ch)
{
    if (ch >= 'a' && ch <= 'z')
        ch -= 'a' - 'A';
    if (ch >= 'A' && ch <= 'Z')
        return ch - 'A';
    if (ch >= '0' && ch <= '9')
        return 26 + ch - '0';
    if (ch == '_')
        return 36;
    if (ch == '-')
        return 37;
    if (ch == '>')
        return 38;
    if (ch == '.')
        return 39;
    return 40;
}

#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
static Uint32 nw_android_color(SDL_Surface *dst,
                               Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    const SDL_PixelFormatDetails *fmt =
        SDL_GetPixelFormatDetails(dst->format);
    if (fmt == NULL)
        return 0;
    return SDL_MapRGBA(fmt, SDL_GetSurfacePalette(dst), r, g, b, a);
}

/* Fill a rectangle with the given color.  When alpha is below 255 the pixels
 * are composited by hand: the window surface is opaque, so the platform would
 * otherwise ignore the alpha channel and paint the control solid. */
static void nw_android_rect(SDL_Surface *dst, const SDL_Rect *rect,
                            Uint32 color, Uint8 alpha)
{
    const SDL_PixelFormatDetails *fmt;
    const SDL_Palette *pal;
    SDL_Rect surfaceClip;
    SDL_Rect clip;
    int left;
    int top;
    int right;
    int bottom;
    Uint8 sr, sg, sb, sa;
    int x;
    int y;

    if (dst == NULL || dst->pixels == NULL || rect == NULL)
        return;
    if (alpha >= 255) {
        (void)SDL_FillSurfaceRect(dst, rect, color);
        return;
    }
    if (alpha == 0 || rect->w <= 0 || rect->h <= 0)
        return;

    fmt = SDL_GetPixelFormatDetails(dst->format);
    pal = SDL_GetSurfacePalette(dst);
    if (fmt == NULL)
        return;
    SDL_GetRGBA(color, fmt, pal, &sr, &sg, &sb, &sa);
    (void)sa;

    /* SDL_FillSurfaceRect honors the surface clip rectangle, but the manual
     * compositing below has to apply it explicitly while the card slides. */
    left = 0;
    top = 0;
    right = dst->w;
    bottom = dst->h;
    if (SDL_GetSurfaceClipRect(dst, &surfaceClip)) {
        left = surfaceClip.x;
        top = surfaceClip.y;
        right = surfaceClip.x + surfaceClip.w;
        bottom = surfaceClip.y + surfaceClip.h;
    }
    if (left < 0)
        left = 0;
    if (top < 0)
        top = 0;
    if (right > dst->w)
        right = dst->w;
    if (bottom > dst->h)
        bottom = dst->h;

    clip.x = rect->x;
    clip.y = rect->y;
    clip.w = rect->w;
    clip.h = rect->h;
    if (clip.x < left) {
        clip.w -= left - clip.x;
        clip.x = left;
    }
    if (clip.y < top) {
        clip.h -= top - clip.y;
        clip.y = top;
    }
    if (clip.x + clip.w > right)
        clip.w = right - clip.x;
    if (clip.y + clip.h > bottom)
        clip.h = bottom - clip.y;
    if (clip.w <= 0 || clip.h <= 0)
        return;

    if (fmt->bytes_per_pixel == 4) {
        if (dst->format == SDL_PIXELFORMAT_ARGB8888 ||
            dst->format == SDL_PIXELFORMAT_XRGB8888) {
            for (y = 0; y < clip.h; y++) {
                Uint32 *row = (Uint32 *)((Uint8 *)dst->pixels +
                                         (size_t)(clip.y + y) * dst->pitch) +
                              clip.x;
                for (x = 0; x < clip.w; x++) {
                    Uint32 dp = row[x];
                    Uint32 dr = (dp >> 16) & 0xffu;
                    Uint32 dg = (dp >> 8) & 0xffu;
                    Uint32 db = dp & 0xffu;
                    Uint32 rr = (sr * alpha + dr * (255u - alpha) + 127u) / 255u;
                    Uint32 gg = (sg * alpha + dg * (255u - alpha) + 127u) / 255u;
                    Uint32 bb = (sb * alpha + db * (255u - alpha) + 127u) / 255u;
                    row[x] = (dp & 0xff000000u) | (rr << 16) | (gg << 8) | bb;
                }
            }
            return;
        }
        for (y = 0; y < clip.h; y++) {
            Uint32 *row = (Uint32 *)((Uint8 *)dst->pixels +
                                     (size_t)(clip.y + y) * dst->pitch) +
                          clip.x;
            for (x = 0; x < clip.w; x++) {
                Uint8 dr, dg, db, da;
                Uint32 rr, gg, bb;
                SDL_GetRGBA(row[x], fmt, pal, &dr, &dg, &db, &da);
                (void)da;
                rr = (sr * alpha + dr * (255u - alpha) + 127u) / 255u;
                gg = (sg * alpha + dg * (255u - alpha) + 127u) / 255u;
                bb = (sb * alpha + db * (255u - alpha) + 127u) / 255u;
                row[x] = SDL_MapRGBA(fmt, pal, (Uint8)rr, (Uint8)gg,
                                     (Uint8)bb, 255);
            }
        }
        return;
    }

    /* Palette surfaces are not used by the mobile window targets; keep the
     * controls readable instead of blending in palette space. */
    (void)SDL_FillSurfaceRect(dst, rect, color);
}

static void nw_android_label(SDL_Surface *dst, const char *text,
                             const SDL_Rect *area, int scale, Uint32 color,
                             Uint8 alpha)
{
    int length = (int)strlen(text);
    int width;
    int height;
    int x0;
    int y0;
    int i;
    int col;
    int row;

    if (length <= 0 || area == NULL)
        return;
    if (scale < 1)
        scale = 1;
    if (scale > 8)
        scale = 8;
    width = length * 6 * scale - scale;
    while (scale > 1 && width > area->w) {
        scale--;
        width = length * 6 * scale - scale;
    }
    height = 7 * scale;
    x0 = area->x + (area->w - width) / 2;
    y0 = area->y + (area->h - height) / 2;

    for (i = 0; i < length; i++) {
        int idx = nw_boot_font_index((unsigned char)text[i]);
        for (col = 0; col < 5; col++) {
            uint8_t bits = nw_boot_font[idx][col];
            for (row = 0; row < 7; row++) {
                SDL_Rect r;
                if ((bits & (1u << row)) == 0)
                    continue;
                r.x = (Sint16)(x0 + (i * 6 + col) * scale);
                r.y = (Sint16)(y0 + row * scale);
                r.w = (Sint16)scale;
                r.h = (Sint16)scale;
                nw_android_rect(dst, &r, color, alpha);
            }
        }
    }
}

enum {
    NW_ANDROID_DRAW_ICON_EXIT,
    NW_ANDROID_DRAW_ICON_KEYBOARD,
    NW_ANDROID_DRAW_ICON_LOCK,
    NW_ANDROID_DRAW_ICON_UNLOCK,
    NW_ANDROID_DRAW_ICON_MOUSE_LEFT,
    NW_ANDROID_DRAW_ICON_MOUSE_RIGHT
};

static const uint32_t *nw_android_icon_rows(int icon)
{
    switch (icon) {
    case NW_ANDROID_DRAW_ICON_EXIT:
        return nw_android_icon_exit;
    case NW_ANDROID_DRAW_ICON_KEYBOARD:
        return nw_android_icon_keyboard;
    case NW_ANDROID_DRAW_ICON_UNLOCK:
        return nw_android_icon_unlock;
    case NW_ANDROID_DRAW_ICON_MOUSE_LEFT:
        return nw_android_icon_mouse_left;
    case NW_ANDROID_DRAW_ICON_MOUSE_RIGHT:
        return nw_android_icon_mouse_right;
    case NW_ANDROID_DRAW_ICON_LOCK:
    default:
        return nw_android_icon_lock;
    }
}

/* Draw a generated 1-bit icon at an integer scale, centered in area.  Drawing
 * runs of set bits keeps the per-frame SDL_FillSurfaceRect count low. */
static void nw_android_icon(SDL_Surface *dst, int icon,
                            const SDL_Rect *area, Uint32 color,
                            Uint8 alpha)
{
    const uint32_t *rows = nw_android_icon_rows(icon);
    int scale = (area->w < area->h ? area->w : area->h) /
                NW_ANDROID_ICON_SIZE;
    int iconW;
    int iconH;
    int x0;
    int y0;
    int y;

    if (scale < 1)
        scale = 1;
    iconW = NW_ANDROID_ICON_SIZE * scale;
    iconH = iconW;
    x0 = area->x + (area->w - iconW) / 2;
    y0 = area->y + (area->h - iconH) / 2;

    for (y = 0; y < NW_ANDROID_ICON_SIZE; y++) {
        uint32_t bits = rows[y];
        int x = 0;
        while (x < NW_ANDROID_ICON_SIZE) {
            int run;
            SDL_Rect r;
            if ((bits & (1u << x)) == 0) {
                x++;
                continue;
            }
            run = x;
            while (x < NW_ANDROID_ICON_SIZE && (bits & (1u << x)) != 0)
                x++;
            r.x = (Sint16)(x0 + run * scale);
            r.y = (Sint16)(y0 + y * scale);
            r.w = (Sint16)((x - run) * scale);
            r.h = (Sint16)scale;
            nw_android_rect(dst, &r, color, alpha);
        }
    }
}

/* Small triangle/chevron used by the pull-down tab. */
static void nw_android_chevron(SDL_Surface *dst, const SDL_Rect *handle,
                               int up, Uint32 color, Uint8 alpha)
{
    static const uint8_t downRows[4] = { 0xffu, 0x7eu, 0x3cu, 0x18u };
    static const uint8_t upRows[4] = { 0x18u, 0x3cu, 0x7eu, 0xffu };
    const uint8_t *rows = up ? upRows : downRows;
    int scale = (handle->h - 8) / 8;
    int iconW;
    int iconH;
    int x0;
    int y0;
    int y;

    if (scale < 1)
        scale = 1;
    if (scale > 3)
        scale = 3;
    iconW = 8 * scale;
    iconH = 4 * scale;
    x0 = handle->x + (handle->w - iconW) / 2;
    y0 = handle->y + (handle->h - iconH) / 2;

    for (y = 0; y < 4; y++) {
        uint8_t bits = rows[y];
        int x = 0;
        while (x < 8) {
            int run;
            SDL_Rect r;
            if ((bits & (1u << x)) == 0) {
                x++;
                continue;
            }
            run = x;
            while (x < 8 && (bits & (1u << x)) != 0)
                x++;
            r.x = (Sint16)(x0 + run * scale);
            r.y = (Sint16)(y0 + y * scale);
            r.w = (Sint16)((x - run) * scale);
            r.h = (Sint16)scale;
            nw_android_rect(dst, &r, color, alpha);
        }
    }
}

/* Draw the control buttons themselves, independent of whether they sit in a
 * letterbox bar or on the pull-down card. */
static void nw_android_draw_buttons(SDL_Surface *dst, const SDL_Rect *buttons,
                                    const char *const *labels,
                                    const int *icons, int pressed)
{
    int i;

    for (i = 0; i < NW_ANDROID_CONTROL_COUNT; i++) {
        SDL_Rect outer = buttons[i];
        SDL_Rect innerButton;
        SDL_Rect iconArea;
        int buttonBorder = (outer.w < outer.h ? outer.w : outer.h) / 24;
        int margin;
        int labelScale;
        int maxLabelScale;
        int labelHeight;
        int labelW;
        int showLabel;
        Uint32 fill;
        Uint32 borderColor;
        Uint32 text;

        if (buttonBorder < 2)
            buttonBorder = 2;
        if (buttonBorder > 6)
            buttonBorder = 6;

        if (pressed == i + 1) {
            borderColor = nw_android_color(dst, 150, 238, 212, 255);
            fill = nw_android_color(dst, 38, 104, 88, 255);
            text = nw_android_color(dst, 255, 255, 255, 255);
        } else {
            borderColor = nw_android_color(dst, 68, 194, 168, 255);
            fill = nw_android_color(dst, 24, 45, 52, 255);
            text = nw_android_color(dst, 234, 244, 240, 255);
        }
        nw_android_rect(dst, &outer, borderColor, 255);
        innerButton = outer;
        innerButton.x = (Sint16)(innerButton.x + buttonBorder);
        innerButton.y = (Sint16)(innerButton.y + buttonBorder);
        innerButton.w = (Sint16)(innerButton.w - buttonBorder * 2);
        innerButton.h = (Sint16)(innerButton.h - buttonBorder * 2);
        if (innerButton.w > 0 && innerButton.h > 0)
            nw_android_rect(dst, &innerButton, fill, 255);

        margin = (outer.w < outer.h ? outer.w : outer.h) / 16;
        if (margin < 3)
            margin = 3;
        if (margin > 10)
            margin = 10;
        labelW = (int)strlen(labels[i]) * 6 - 1;
        showLabel = outer.h >= 42 && outer.w - margin * 2 >= labelW;
        labelScale = outer.h / 48;
        if (labelScale < 1)
            labelScale = 1;
        maxLabelScale = (outer.w - margin * 2) / labelW;
        if (maxLabelScale < 1)
            maxLabelScale = 1;
        if (labelScale > maxLabelScale)
            labelScale = maxLabelScale;
        labelHeight = 7 * labelScale;

        iconArea.x = (Sint16)(outer.x + margin);
        iconArea.y = (Sint16)(outer.y + margin);
        iconArea.w = (Sint16)(outer.w - margin * 2);
        iconArea.h = (Sint16)(outer.h - margin * 2);
        if (showLabel) {
            SDL_Rect labelArea;
            labelArea.x = (Sint16)(outer.x + margin);
            labelArea.y = (Sint16)(outer.y + outer.h - margin - labelHeight);
            labelArea.w = (Sint16)(outer.w - margin * 2);
            labelArea.h = (Sint16)labelHeight;
            iconArea.h = (Sint16)(iconArea.h - labelHeight - margin);
            nw_android_label(dst, labels[i], &labelArea, labelScale,
                             text, 255);
        }
        if (iconArea.w > 0 && iconArea.h > 0)
            nw_android_icon(dst, icons[i], &iconArea, text, 255);
    }
}

NW_DIAG_CODE static void nw_draw_android_controls(SDL_Surface *dst)
{
    SDL_Rect buttons[NW_ANDROID_CONTROL_COUNT];
    SDL_Rect handle;
    SDL_Rect card;
    const char *labels[NW_ANDROID_CONTROL_COUNT] = {
        "EXIT", "KEYBOARD", "LOCK", "L CLICK"
    };
    int icons[NW_ANDROID_CONTROL_COUNT] = {
        NW_ANDROID_DRAW_ICON_EXIT,
        NW_ANDROID_DRAW_ICON_KEYBOARD,
        NW_ANDROID_DRAW_ICON_LOCK,
        NW_ANDROID_DRAW_ICON_MOUSE_LEFT
    };
    int pressed = NwSdlAndroidControlPressed();
    int progress;
    int inBar;

    if (NwSdlAndroidRotationLocked()) {
        labels[NW_ANDROID_CONTROL_LOCK - 1] = "UNLOCK";
        icons[NW_ANDROID_CONTROL_LOCK - 1] = NW_ANDROID_DRAW_ICON_UNLOCK;
    }
    if (NwSdlAndroidMouseRight()) {
        labels[NW_ANDROID_CONTROL_CLICK - 1] = "R CLICK";
        icons[NW_ANDROID_CONTROL_CLICK - 1] = NW_ANDROID_DRAW_ICON_MOUSE_RIGHT;
    }

    if (!NwSdlAndroidControlLayout(buttons))
        return;

    /* Controls that fit entirely in the black bars are always visible, so they
     * are drawn straight with no tab or card. */
    inBar = NwSdlAndroidControlsInBar();
    if (inBar) {
        nw_android_draw_buttons(dst, buttons, labels, icons, pressed);
        return;
    }

    if (!NwSdlAndroidControlCardLayout(&handle, &card))
        return;

    progress = NwSdlAndroidControlCardProgress();

    if (progress > 0) {
        SDL_Rect oldClip;
        SDL_Rect clip;
        SDL_Rect inner;
        int haveClip = SDL_GetSurfaceClipRect(dst, &oldClip);
        int visibleH = card.h * progress /
                       NW_SDL_CARD_PROGRESS_FULL;
        int border = (card.w < card.h ? card.w : card.h) / 24;
        Uint32 cardBorder = nw_android_color(dst, 68, 194, 168, 255);
        Uint32 cardFill = nw_android_color(dst, 15, 30, 37, 255);

        if (border < 2)
            border = 2;
        if (border > 4)
            border = 4;
        if (visibleH <= 0)
            visibleH = 1;

        clip = card;
        clip.h = (Sint16)visibleH;
        (void)SDL_SetSurfaceClipRect(dst, &clip);
        nw_android_rect(dst, &card, cardBorder, 235);
        inner = card;
        inner.x = (Sint16)(inner.x + border);
        inner.y = (Sint16)(inner.y + border);
        inner.w = (Sint16)(inner.w - border * 2);
        inner.h = (Sint16)(inner.h - border * 2);
        if (inner.w > 0 && inner.h > 0)
            nw_android_rect(dst, &inner, cardFill, 225);

        nw_android_draw_buttons(dst, buttons, labels, icons, pressed);

        if (haveClip)
            (void)SDL_SetSurfaceClipRect(dst, &oldClip);
        else
            (void)SDL_SetSurfaceClipRect(dst, NULL);
    }

    {
        SDL_Rect inner;
        int border = (handle.w < handle.h ? handle.w : handle.h) / 10;
        Uint8 alpha = progress > 0 ? 245 : 215;
        Uint32 borderColor = nw_android_color(dst, 68, 194, 168, 255);
        Uint32 fill = nw_android_color(dst, 15, 30, 37, 255);
        Uint32 text = nw_android_color(dst, 150, 238, 212, 255);

        if (border < 2)
            border = 2;
        if (border > 4)
            border = 4;
        nw_android_rect(dst, &handle, borderColor, alpha);
        inner = handle;
        inner.x = (Sint16)(inner.x + border);
        inner.y = (Sint16)(inner.y + border);
        inner.w = (Sint16)(inner.w - border * 2);
        inner.h = (Sint16)(inner.h - border * 2);
        if (inner.w > 0 && inner.h > 0)
            nw_android_rect(dst, &inner, fill, alpha);
        nw_android_chevron(dst, &handle, progress > 0, text, alpha);
    }
}
#endif

static void NwSdlShowBootTraceOnce(const char *stage)
{
    SDL_Window *win = g_nwSdlWindow;
    SDL_Surface *dst;
    const SDL_PixelFormatDetails *fmt;
    const SDL_Palette *pal;
    SDL_Rect full;
    Uint32 black;
    Uint32 white;
    char text[128];
    size_t length;
    int scale;
    int max_chars;
    int i;
    int col;
    int row;

    if (nw_boot_trace_suppressed)
        return;
    if (win == NULL)
        return;
    dst = SDL_GetWindowSurface(win);
    if (dst == NULL || dst->pixels == NULL)
        return;
    fmt = SDL_GetPixelFormatDetails(dst->format);
    if (fmt == NULL)
        return;
    pal = SDL_GetSurfacePalette(dst);
    black = SDL_MapRGBA(fmt, pal, 0, 0, 0, 255);
    white = SDL_MapRGBA(fmt, pal, 255, 255, 255, 255);

    full.x = 0;
    full.y = 0;
    full.w = dst->w;
    full.h = dst->h;
    (void)SDL_FillSurfaceRect(dst, &full, black);

    scale = dst->w / 320;
    if (scale < 1)
        scale = 1;
    if (scale > 8)
        scale = 8;
    max_chars = dst->w / (6 * scale) - 4;
    if (max_chars < 4)
        max_chars = 4;

    text[0] = 'N';
    text[1] = 'W';
    text[2] = ' ';
    length = 3;
    for (i = 0; stage[i] != '\0' && length + 1 < sizeof(text) &&
                (int)length < max_chars; i++)
        text[length++] = stage[i];
    text[length] = '\0';

    for (i = 0; i < (int)length; i++) {
        int idx = nw_boot_font_index((unsigned char)text[i]);
        for (col = 0; col < 5; col++) {
            uint8_t bits = nw_boot_font[idx][col];
            for (row = 0; row < 7; row++) {
                SDL_Rect r;
                if ((bits & (1u << row)) == 0)
                    continue;
                r.x = (Sint16)(16 + (i * 6 + col) * scale);
                r.y = (Sint16)(16 + row * scale);
                r.w = (Sint16)scale;
                r.h = (Sint16)scale;
                (void)SDL_FillSurfaceRect(dst, &r, white);
            }
        }
    }
    (void)SDL_UpdateWindowSurface(win);
}

void NwSdlShowBootTrace(const char *stage)
{
    /* The PS5 driver calls platform_boot_mark() while it is inside
     * SDL_UpdateWindowSurface(), and the mark draws a trace and updates the
     * surface again.  Without this guard the first presented frame recurses
     * until the native title's stack is exhausted. */
    static int active;
    if (active)
        return;
    active = 1;
    NwSdlShowBootTraceOnce(stage);
    active = 0;
}

/* ---- CreatePlayerDIBBackBuffer ------------------------------------------ */

NW_DIAG_CODE BOOL WINAPI CreatePlayerDIBBackBuffer(
    uintptr_t dwArg1Save,
    void *pPlayerCfg,
    PPLAYERBUFINFO pBufInfo)
{
    int w;
    int h;
    uint32_t nwOriginalCount = pBufInfo->dwCount;

    NW_WINDOW_PTR_REF(void, 0x5efc) = (void *)dwArg1Save;
    NW_WINDOW_PTR_REF(void, 0x5ef4) = pPlayerCfg;
    NW_WINDOW_PTR_REF(void, 0x5ef8) = pBufInfo;

    if (g_hWndPlayer == NULL)
        CreatePlayerMainWindow(pPlayerCfg);

    if (g_pvDIBits == NULL) {
        w = nw_dib_width();
        h = nw_dib_height();
        if (w <= 0 || h <= 0) {
            w = 640;
            h = 480;
        }

        /* Native high-resolution surface: the game's logical coordinates stay
         * at w x h, while the actual 8bpp DIB is w*scale x h*scale.  The
         * drawing entry points scale coordinates and sample source pixels. */
        {
            int scale = NwSdlRenderScale();
            if (scale > 1) {
                w *= scale;
                h *= scale;
            }
        }

        if (!nw_video_init(w, h)) {
            /* Keep the game alive with a plain framebuffer if SDL cannot
             * attach to the window; presentation is skipped in that case. */
            NW_PS5_BOOT("dib_video_fallback");
            g_v.w = w;
            g_v.h = h;
            g_v.pitch = (w + 3) & ~3;
            g_v.fb = (unsigned char *)malloc((size_t)g_v.pitch * (size_t)h);
            if (g_v.fb != NULL)
                memset(g_v.fb, 0, (size_t)g_v.pitch * (size_t)h);
        } else {
            NW_PS5_BOOT("dib_video_ok");
        }

        g_pvDIBits = g_v.fb;
        g_hdcMem = NULL;
    }

    /* These are guest-width mirror fields.  Native pointers live in the
     * window side table (g_pvDIBits / g_pvScratch) and are published by the
     * drawing code from there. */
    pBufInfo->pvDIBits = (uint32_t)(uintptr_t)g_pvDIBits;

    if (g_pvDIBits != NULL)
        pBufInfo->dwCount -= pBufInfo->dwHeader;
    pBufInfo->dwCount -= NW_SCRATCH_RESERVE;

    if (g_pvScratch == NULL) {
#if defined(NW_PORTABLE_PS5) || defined(NW_PS5_NATIVE_TITLE) || \
    defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
        /* This buffer is also the engine's heap base (window +0x6318), so its
         * address must fit the 32-bit mirror.  Try the low-address mmap path
         * first; the native libc heap is also too small for the 4x framebuffer
         * plus the game's ~5 MB scratch. */
        int low = 1;
        g_pvScratch = NwHostAllocLow((size_t)pBufInfo->dwCount);
        if (g_pvScratch == NULL) {
            low = 0;
            g_pvScratch = malloc((size_t)pBufInfo->dwCount);
        }
        {
            char dbg[96];
            snprintf(dbg, sizeof(dbg), "scratch_%s_%lx", low ? "low" : "high",
                     (unsigned long)(uintptr_t)g_pvScratch);
            NW_PS5_BOOT(dbg);
        }
#else
#if defined(_WIN64)
        /* Round-1 instrumentation: route the main guest arena through a
         * controllable allocator so NW_FORCE_HIGH can move it to >= 4 GiB,
         * simulating iOS (no sub-4GiB memory). */
        extern void *NwGuestArenaAlloc(size_t size);
        g_pvScratch = NwGuestArenaAlloc((size_t)pBufInfo->dwCount);
#else
        g_pvScratch = malloc((size_t)pBufInfo->dwCount);
#endif
#endif
    }

    pBufInfo->pvScratch = (uint32_t)(uintptr_t)g_pvScratch;
#if defined(NW_PS5_NATIVE_TITLE)
    {
        char dbg[192];
        snprintf(dbg, sizeof(dbg),
                 "dib w=%d h=%d ready=%d orig=%lx cnt=%lx hdr=%lx fb=%lx scratch=%lx",
                 w, h, g_v.ready, (unsigned long)nwOriginalCount,
                 (unsigned long)pBufInfo->dwCount,
                 (unsigned long)pBufInfo->dwHeader,
                 (unsigned long)(uintptr_t)g_pvDIBits,
                 (unsigned long)(uintptr_t)g_pvScratch);
        platform_boot_mark(dbg);
    }
#endif
    return (g_pvScratch != NULL);
}

/* ---- BlitRegionFromDcOrDib ---------------------------------------------- */

NW_DIAG_CODE void BlitRegionFromDcOrDib(
    int nTotalHeight,
    int nHeight,
    int nWidth,
    int nY,
    int nX,
    void *pBits)
{
    unsigned char *src = (unsigned char *)pBits;
    int src_pitch;
    int src_start;
    int dst_start;
    int row;

    if (g_v.fb == NULL || src == NULL)
        return;

    /* The common region blit reads the framebuffer itself.  The pixels are
     * already where they belong; presentation will pick them up. */
    if (src == g_v.fb)
        return;

    src_pitch = (int)g_dwSourceWidth;
    if (src_pitch <= 0)
        src_pitch = g_v.pitch;

    /* Both surfaces are bottom-up.  GDI's source y is measured from the
     * bottom, so the storage row of the logical top is H - y - height. */
    src_start = nTotalHeight - nY - nHeight;
    dst_start = g_v.h - nY - nHeight;

    for (row = 0; row < nHeight; row++) {
        int sr = src_start + row;
        int dr = dst_start + row;
        int x0 = nX;
        int x1 = nX + nWidth;

        if (sr < 0 || sr >= nTotalHeight)
            continue;
        if (dr < 0 || dr >= g_v.h)
            continue;
        if (x0 < 0)
            x0 = 0;
        if (x1 > g_v.w)
            x1 = g_v.w;
        if (x1 <= x0)
            continue;

        memcpy(g_v.fb + (size_t)dr * (size_t)g_v.pitch + (size_t)x0,
               src + (size_t)sr * (size_t)src_pitch + (size_t)x0,
               (size_t)(x1 - x0));
    }
}

/* ---- SetLogicalPaletteDrawGrid ------------------------------------------ */

NW_DIAG_CODE BOOL WINAPI SetLogicalPaletteDrawGrid(
    BOOL bUseSystemColors,
    char cGridPenIndex,
    UINT uNumPaletteEntries,
    const RGBTRIPLE *pRgbEntries)
{
    /* Windows system palette, 0x00RRGGBB.  GetSystemPaletteEntries() supplies
     * these two ten-colour blocks on the Win32 path. */
    static const uint32_t sys20[20] NW_DIAG_DATA = {
        0x000000u, 0x800000u, 0x008000u, 0x808000u, 0x000080u,
        0x800080u, 0x008080u, 0xc0c0c0u, 0xc0dcc0u, 0xa6caf0u,
        0xfffbf0u, 0xa0a0a4u, 0x808080u, 0xff0000u, 0x00ff00u,
        0xffff00u, 0x0000ffu, 0xff00ffu, 0x00ffffu, 0xffffffu
    };
    int i;

    if (pRgbEntries == NULL) {
        /* No palette table was staged for this call (for example during a
         * partial save rebuild); keep the default black palette. */
        return 1;
    }

    for (i = 0; i < 256; i++)
        g_v.lut[i] = 0xff000000u;

    for (i = 0; i < (int)uNumPaletteEntries && i < 256; i++) {
        unsigned r = pRgbEntries[i].rgbtBlue;
        unsigned g = pRgbEntries[i].rgbtGreen;
        unsigned b = pRgbEntries[i].rgbtRed;
        g_v.lut[i] = 0xff000000u | (r << 16) | (g << 8) | b;
    }

    if (bUseSystemColors) {
        for (i = 0; i < 10; i++)
            g_v.lut[i] = 0xff000000u | sys20[i];
        for (i = 0; i < 10; i++)
            g_v.lut[246 + i] = 0xff000000u | sys20[10 + i];
    }

    /* Asset extraction: append the active palette to the same dump directory
     * the sprite dumper uses.  Raw sprite files are palette indices, so this
     * is what makes them viewable/re-importable.  Inert unless NW_DEBUG_ASSET_DUMP. */
    {
        const char *pszDump = NW_DEBUG_GETENV("NW_DEBUG_ASSET_DUMP");
        if (pszDump != NULL && pszDump[0] != '\0') {
            char  szPath[1024];
            FILE *f;
            unsigned char hdr[8];
            int j;

            SDL_CreateDirectory(pszDump);
            snprintf(szPath, sizeof szPath, "%s\\palette.bin", pszDump);
            f = fopen(szPath, "ab");
            if (f != NULL) {
                hdr[0] = 0x00; hdr[1] = 'P'; hdr[2] = 'A'; hdr[3] = 'L';
                hdr[4] = 0x00; hdr[5] = 0x03;      /* palVersion 0x0300 */
                hdr[6] = (unsigned char)(uNumPaletteEntries & 0xffu);
                hdr[7] = (unsigned char)((uNumPaletteEntries >> 8) & 0xffu);
                fwrite(hdr, 1, sizeof hdr, f);
                for (j = 0; j < (int)uNumPaletteEntries && j < 256; j++) {
                    unsigned char rgb[3];
                    rgb[0] = (unsigned char)((g_v.lut[j] >> 16) & 0xffu);
                    rgb[1] = (unsigned char)((g_v.lut[j] >> 8) & 0xffu);
                    rgb[2] = (unsigned char)(g_v.lut[j] & 0xffu);
                    fwrite(rgb, 1, 3, f);
                }
                fclose(f);
            }
        }
    }

    if (g_v.fb != NULL && g_cxGridClient != 0 && g_cyGridClient != 0) {
        int scale = NwSdlRenderScale();
        int cx = g_cxGridClient * (scale > 0 ? scale : 1);
        int cy = g_cyGridClient * (scale > 0 ? scale : 1);
        int x;
        int y;

        if (cx > g_v.w)
            cx = g_v.w;
        if (cy > g_v.h)
            cy = g_v.h;

        if (cGridPenIndex == 0) {
            for (y = 0; y < cy; y++)
                memset(g_v.fb + (size_t)(g_v.h - 1 - y) * (size_t)g_v.pitch,
                       0, (size_t)cx);
        } else {
            uint32_t cref = g_rgcrGridPenColors[(int)cGridPenIndex];
            unsigned r = cref & 0xffu;
            unsigned g = (cref >> 8) & 0xffu;
            unsigned b = (cref >> 16) & 0xffu;
            unsigned char idx = (unsigned char)nw_nearest_index(r, g, b);
            int step = 8 * (scale > 0 ? scale : 1);
            int pen = scale > 0 ? scale : 1;
            int k;

            for (x = 0; x < cx; x += step) {
                for (y = 0; y < cy; y++)
                    for (k = 0; k < pen && x + k < g_v.w; k++)
                        g_v.fb[(size_t)(g_v.h - 1 - y) * (size_t)g_v.pitch
                               + (size_t)(x + k)] = idx;
            }
            for (y = 0; y < cy; y += step) {
                for (x = 0; x < cx; x++)
                    for (k = 0; k < pen && y + k < g_v.h; k++)
                        g_v.fb[(size_t)(g_v.h - 1 - (y + k)) *
                               (size_t)g_v.pitch + (size_t)x] = idx;
            }
        }
    }

    NwSdlVideoPresent();
    return 1;
}

#endif /* NW_UNIT_VideoSdl */
