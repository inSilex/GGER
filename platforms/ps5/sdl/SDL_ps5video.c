/*
 * NetWalk for the PlayStation 5 - SDL3 video driver.
 *
 * The console has one output and no desktop windows.  SDL still owns a window
 * object for the game's framebuffer; this driver gives that window a 4:3
 * software surface and presents it through VideoOut.
 *
 * The display and VideoOut setup follow the public ps5-native-app-boilerplate
 * and PS5_Dusklight's SDL_ps5video.c.  NetWalk's game renderer is a software
 * 8bpp surface, so this port adds the CreateWindowFramebuffer hooks that the
 * Dusklight Vulkan driver does not need.
 *
 * SDL_PS5_DISPLAY=WIDTHxHEIGHT in the environment sets the output size
 * (default 1920x1080).
 *
 * SPDX-License-Identifier: Zlib
 */
#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_PS5

#include "video/SDL_sysvideo.h"
#include "events/SDL_events_c.h"
#include "SDL3/SDL_timer.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* VideoOut is exported by libSceVideoOut.so; the payload SDK does not ship
 * public headers for it. */
extern int sceVideoOutOpen(int32_t user_id, int32_t bus_type, int32_t index,
                           const void *param);
extern int sceVideoOutSetFlipRate(int32_t handle, int32_t rate);
extern int sceVideoOutSubmitFlip(int32_t handle, int32_t buffer_index,
                                 uint32_t flip_mode, int64_t flip_argument);
extern int sceVideoOutWaitVblank(int32_t handle);
extern int sceVideoOutClose(int32_t handle);
extern void sceVideoOutSetBufferAttribute2(void *attribute,
                                           uint64_t pixel_format,
                                           uint32_t tiling_mode,
                                           uint32_t width,
                                           uint32_t height,
                                           uint64_t option,
                                           uint32_t dcc_control,
                                           uint64_t dcc_clear_color);
extern int sceVideoOutRegisterBuffers2(int32_t handle, int32_t set_index,
                                       int32_t buffer_index_start,
                                       void *buffers, int32_t buffer_count,
                                       void *attribute, int32_t category,
                                       void *option);

extern size_t sceKernelGetDirectMemorySize(void);
extern int sceKernelAllocateMainDirectMemory(size_t size,
                                             size_t alignment,
                                             int memory_type,
                                             int64_t *physical_address);
extern int sceKernelAllocateDirectMemory(int64_t search_start,
                                         int64_t search_end,
                                         size_t length,
                                         size_t alignment,
                                         int memory_type,
                                         int64_t *physical_address);
extern int sceKernelMapDirectMemory(void **address, size_t length,
                                    int protection, int flags,
                                    int64_t physical_address,
                                    size_t alignment);
extern int klog_printf(const char *format, ...);
/* Implemented by the game's platform layer.  klog is the progress signal, so
 * the video path marks each stage that can fail before the first frame. */
extern void platform_boot_mark(const char *stage);
/* Large host-side buffers must stay off the payload/native libc heap and, for
 * the game's 32-bit pointer mirrors, below 4 GiB. */
extern void *NwHostAllocLow(size_t size);
extern void NwHostFreeLow(void *p, size_t size);

#define PS5_PIXEL_FORMAT_RGBA8_SRGB UINT64_C(0x8000000022000000)
/* VideoOut's RGBA8 path accepts tiling mode 0, which is the tiled layout the
 * upstream PS5 boilerplate writes with its tiled_byte_offset() helper.  It is
 * not a linear framebuffer: a row-major copy produces a scrambled image.
 * Passing mode 1 returns SCE_VIDEO_OUT_ERROR_INVALID_TILING_MODE
 * (0x80290007) from sceVideoOutRegisterBuffers2. */
#define PS5_TILING_TILED 0u
#define PS5_MEMORY_TYPE_WC_GARLIC 3
#define PS5_MAP_PROTECTION 0x33
#define PS5_ALIGNMENT 0x200000u
/* The upstream PS5 boilerplate allocates one 16 MiB slot per 1080p frame.
 * VideoOut validates that the registered pointers are in the displayable
 * direct-memory pool and spaced by the buffer size; an 8 MiB stride is
 * rejected with SCE_VIDEO_OUT_ERROR_INVALID_MEMORY (0x80290013). */
#define PS5_FRAME_ALIGNMENT 0x1000000u

typedef struct {
    void *data;
    void *metadata;
    void *reserved0;
    void *reserved1;
} PS5_VideoBuffer;

typedef struct {
    uint8_t reserved[80];
} PS5_VideoAttribute;

static int ps5_display_w = 1920;
static int ps5_display_h = 1080;

static int ps5_video_handle = -1;
static void *ps5_video_mapping;
static size_t ps5_video_mapping_size;
static size_t ps5_frame_stride;
static int64_t ps5_video_physical;
static uint32_t *ps5_video_frames[2];
static int ps5_video_index;
static int64_t ps5_flip_argument;
static int ps5_video_ready;
/* A failed VideoOut setup is not recoverable by retrying from the next boot
 * mark; retrying just opens and closes the display device again. */
static int ps5_video_failed;

typedef struct {
    void *pixels;
    int pitch;
    int w;
    int h;
    size_t bytes;
    int low;
} PS5_WindowFramebuffer;

#define PS5_SURFACE_PROPERTY "SDL.internal.ps5.window_surface"

static int ps5_first_flip_logged;
/* VideoOut submission is asynchronous; the old code waited for a vblank
 * after every flip.  That turns any frame which overruns 16.7 ms into a full
 * extra vblank (30 Hz for a while) instead of letting the loop recover.  Pace
 * submissions before the flip instead, and wait only for the first one. */
static Uint64 ps5_last_flip_ns;
static int ps5_have_last_flip;
/* The pillarbox bars are constant for a fixed window.  Fill them once per
 * VideoOut buffer instead of rewriting a quarter of the output every frame. */
static int ps5_bars_ready[2];
static int ps5_bars_geometry_valid;
static int ps5_bars_x, ps5_bars_y, ps5_bars_w, ps5_bars_h;

/* VideoOut's tiled layout, from the upstream boilerplate's
 * tiled_byte_offset(): 128x128 32-bit tiles, each occupying 64 KiB, with the
 * XOR swizzle below inside a tile.  The swizzle separates into an x-only and
 * a y-only part, so two 128-entry tables replace the per-pixel shift chain. */
static uint32_t ps5_tile_x[128];
static uint32_t ps5_tile_y[128];
static int ps5_tile_tables_ready;

static void PS5_InitTileTables(void)
{
    int i;

    if (ps5_tile_tables_ready)
        return;
    for (i = 0; i < 128; i++) {
        const uint32_t x = (uint32_t)i;
        const uint32_t y = (uint32_t)i;
        ps5_tile_x[i] = ((x << 2) & 0x0000000cu) ^
                        ((x << 5) & 0x00000380u) ^
                        ((x << 4) & 0x00000400u) ^
                        ((x << 6) & 0x00000800u) ^
                        ((x << 9) & 0x0000a000u);
        ps5_tile_y[i] = ((y << 4) & 0x00000070u) ^
                        ((y << 5) & 0x00000f00u) ^
                        ((y << 9) & 0x00001000u) ^
                        ((y << 8) & 0x00004000u);
    }
    ps5_tile_tables_ready = 1;
}

/* SDL ARGB8888 (0xAARRGGBB) -> VideoOut RGBA8 (0xAABBGGRR). */
static inline uint32_t PS5_SwapRedBlue(uint32_t c)
{
    return (c & 0x0000ff00u) |
           ((c & 0x000000ffu) << 16) |
           ((c & 0x00ff0000u) >> 16) |
           (c & 0xff000000u);
}

static bool PS5_VideoFailure(const char *operation, int result)
{
    char stage[96];

    SDL_SetError("PS5: %s failed: 0x%08x", operation, (unsigned int)result);
    klog_printf("[NetWalk] PS5 video: %s failed: 0x%08x\n", operation,
                (unsigned int)result);
    snprintf(stage, sizeof stage, "videoout_fail_%s_0x%08x", operation,
             (unsigned int)result);
    platform_boot_mark(stage);
    return false;
}

/* Direct memory is mapped write-combined.  The CPU's stores are not ordered
 * with respect to the GPU's scanout until the cache lines are written back and
 * a fence retires; the upstream native demo does the same before its first
 * flip. */
static void PS5_FlushFramebuffer(const void *pixels, size_t length)
{
    const uint8_t *at = (const uint8_t *)pixels;
    const uint8_t *end = at + length;

    for (; at < end; at += 64) {
        __asm__ volatile("clflush (%0)" : : "r"(at) : "memory");
    }
    __asm__ volatile("mfence" ::: "memory");
}

static bool PS5_InitVideoOutInner(void)
{
    size_t frame_bytes;
    size_t frame_stride;
    PS5_VideoAttribute attribute;
    PS5_VideoBuffer buffers[2];

    if (ps5_video_ready) {
        return true;
    }
    platform_boot_mark("videoout_init");
    if (ps5_display_w <= 0 || ps5_display_h <= 0) {
        return PS5_VideoFailure("display size", -1);
    }

    ps5_video_handle = sceVideoOutOpen(0xff, 0, 0, NULL);
    if (ps5_video_handle < 0) {
        return PS5_VideoFailure("sceVideoOutOpen", ps5_video_handle);
    }
    platform_boot_mark("videoout_open");

    frame_bytes = (size_t)ps5_display_w * (size_t)ps5_display_h * 4u;
    frame_stride = (frame_bytes + PS5_FRAME_ALIGNMENT - 1u) &
                   ~(PS5_FRAME_ALIGNMENT - 1u);
    ps5_frame_stride = frame_stride;
    ps5_video_mapping_size = frame_stride * 2u;

    {
        /* A payload is not a native title: VideoOut only accepts buffers from
         * the main direct-memory pool there.  Try main first for both launch
         * contexts, then fall back to the generic pool used by the upstream
         * native boilerplate if main direct memory is unavailable. */
        int result = sceKernelAllocateMainDirectMemory(
            ps5_video_mapping_size, PS5_ALIGNMENT, PS5_MEMORY_TYPE_WC_GARLIC,
            &ps5_video_physical);
        if (result < 0) {
            result = sceKernelAllocateDirectMemory(
                0, (int64_t)sceKernelGetDirectMemorySize(),
                ps5_video_mapping_size, PS5_ALIGNMENT,
                PS5_MEMORY_TYPE_WC_GARLIC, &ps5_video_physical);
        }
        if (result < 0) {
            sceVideoOutClose(ps5_video_handle);
            ps5_video_handle = -1;
            return PS5_VideoFailure("direct-memory allocation", result);
        }
    }
    platform_boot_mark("videoout_alloc");
    {
        const int result = sceKernelMapDirectMemory(
            &ps5_video_mapping, ps5_video_mapping_size, PS5_MAP_PROTECTION, 0,
            ps5_video_physical, PS5_ALIGNMENT);
        if (result < 0) {
            sceVideoOutClose(ps5_video_handle);
            ps5_video_handle = -1;
            return PS5_VideoFailure("direct-memory mapping", result);
        }
    }
    platform_boot_mark("videoout_map");
#ifdef NW_DEBUG_TOOLS
    klog_printf("[NetWalk] PS5 video: direct memory phys=0x%llx map=%p "
                "size=0x%zx stride=0x%zx\n",
                (unsigned long long)ps5_video_physical, ps5_video_mapping,
                ps5_video_mapping_size, frame_stride);
#endif

    ps5_video_frames[0] = (uint32_t *)ps5_video_mapping;
    ps5_video_frames[1] = (uint32_t *)((uint8_t *)ps5_video_mapping + frame_stride);
    SDL_zero(attribute);
    SDL_zero(buffers);
    buffers[0].data = ps5_video_frames[0];
    buffers[1].data = ps5_video_frames[1];
    sceVideoOutSetBufferAttribute2(&attribute, PS5_PIXEL_FORMAT_RGBA8_SRGB,
                                   PS5_TILING_TILED,
                                   (uint32_t)ps5_display_w,
                                   (uint32_t)ps5_display_h,
                                   0, 0, 0);
    {
        const int result = sceVideoOutRegisterBuffers2(
            ps5_video_handle, 0, 0, buffers, 2, &attribute, 0, NULL);
        if (result < 0) {
            sceVideoOutClose(ps5_video_handle);
            ps5_video_handle = -1;
            return PS5_VideoFailure("sceVideoOutRegisterBuffers2", result);
        }
    }
    platform_boot_mark("videoout_buffers");

    (void)sceVideoOutSetFlipRate(ps5_video_handle, 0);
    ps5_video_index = 0;
    ps5_flip_argument = 0;
    ps5_last_flip_ns = 0;
    ps5_have_last_flip = 0;
    ps5_video_ready = 1;
#ifdef NW_DEBUG_TOOLS
    klog_printf("[NetWalk] PS5 video: %dx%d tiled framebuffer ready\n",
                ps5_display_w, ps5_display_h);
#endif
    return true;
}

static bool PS5_InitVideoOut(void)
{
    /* A boot mark draws into the window surface and presents it, which can
     * re-enter the framebuffer update while the first VideoOut setup is still
     * running.  Let the outer call finish exactly one setup. */
    static int initializing;
    bool result;

    if (ps5_video_ready)
        return true;
    if (ps5_video_failed)
        return false;
    if (initializing)
        return false;
    initializing = 1;
    result = PS5_InitVideoOutInner();
    initializing = 0;
    if (!result)
        ps5_video_failed = 1;
    return result;
}

static bool PS5_VideoInit(SDL_VideoDevice *_this)
{
    SDL_DisplayMode mode;
    int w = 1920, h = 1080;
    const char *size = SDL_getenv("SDL_PS5_DISPLAY");

    platform_boot_mark("sdl_video_init");
    if (size && SDL_sscanf(size, "%dx%d", &w, &h) != 2) {
        w = 1920;
        h = 1080;
    }
    if (w < 320 || h < 240) {
        w = 1920;
        h = 1080;
    }
    ps5_display_w = w;
    ps5_display_h = h;

    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_ARGB8888;
    mode.w = w;
    mode.h = h;
    mode.refresh_rate = 60.0f;
    if (SDL_AddBasicVideoDisplay(&mode) == 0) {
        klog_printf("[NetWalk] PS5 video: SDL_AddBasicVideoDisplay failed: %s\n",
                    SDL_GetError());
        return false;
    }
    platform_boot_mark("sdl_display");
#ifdef NW_DEBUG_TOOLS
    klog_printf("[NetWalk] PS5 video: display %dx%d\n", w, h);
#endif
    return true;
}

static void PS5_VideoQuit(SDL_VideoDevice *_this)
{
    (void)_this;
    if (ps5_video_handle >= 0) {
        sceVideoOutClose(ps5_video_handle);
        ps5_video_handle = -1;
    }
    ps5_video_ready = 0;
    ps5_video_failed = 0;
    ps5_video_mapping = NULL;
    ps5_video_frames[0] = NULL;
    ps5_video_frames[1] = NULL;
    ps5_first_flip_logged = 0;
    ps5_last_flip_ns = 0;
    ps5_have_last_flip = 0;
}

static void PS5_PumpEvents(SDL_VideoDevice *_this)
{
    (void)_this;
}

static bool PS5_CreateWindow(SDL_VideoDevice *_this, SDL_Window *window,
                             SDL_PropertiesID props)
{
    int w = ps5_display_w;
    int h = ps5_display_h;

    (void)_this;
    (void)props;

    /* Present a 4:3 picture on a 16:9 output: the game's own window surface is
     * 4:3, and UpdateWindowFramebuffer pillar-boxes it into VideoOut. */
    if (w * 3 > h * 4) {
        w = (h * 4) / 3;
    } else {
        h = (w * 3) / 4;
    }
    if (w < 320) {
        w = 320;
    }
    if (h < 240) {
        h = 240;
    }
    window->w = w;
    window->h = h;
    window->windowed.w = w;
    window->windowed.h = h;
    window->floating.w = w;
    window->floating.h = h;

    /* The window is the screen, and always has the pad's focus. */
    SDL_SetKeyboardFocus(window);
    platform_boot_mark("sdl_window");
    return true;
}

static bool PS5_SetWindowPosition(SDL_VideoDevice *_this, SDL_Window *window)
{
    (void)_this;
    (void)window;
    return true;
}

static void PS5_SetWindowSize(SDL_VideoDevice *_this, SDL_Window *window)
{
    int w = ps5_display_w;
    int h = ps5_display_h;

    (void)_this;
    /* The size stays the display's 4:3 rectangle.  SDL_SetWindowSize has
     * already put the caller's request in pending; overwrite it so the game's
     * desktop aspect-correction path cannot shrink the console surface. */
    if (w * 3 > h * 4) {
        w = (h * 4) / 3;
    } else {
        h = (w * 3) / 4;
    }
    if (w < 320) w = 320;
    if (h < 240) h = 240;
    window->w = w;
    window->h = h;
    window->windowed.w = w;
    window->windowed.h = h;
    window->floating.w = w;
    window->floating.h = h;
    window->pending.w = w;
    window->pending.h = h;
    window->last_size_pending = false;
}

static SDL_FullscreenResult PS5_SetWindowFullscreen(SDL_VideoDevice *_this,
                                                    SDL_Window *window,
                                                    SDL_VideoDisplay *display,
                                                    SDL_FullscreenOp fullscreen)
{
    (void)_this;
    (void)window;
    (void)display;
    (void)fullscreen;
    return SDL_FULLSCREEN_SUCCEEDED;
}

static bool PS5_CreateWindowFramebuffer(SDL_VideoDevice *_this,
                                        SDL_Window *window,
                                        SDL_PixelFormat *format,
                                        void **pixels, int *pitch)
{
    PS5_WindowFramebuffer *fb;
    int w, h;

    (void)_this;
    platform_boot_mark("sdl_surface_begin");
    SDL_GetWindowSizeInPixels(window, &w, &h);
    if (w <= 0 || h <= 0) {
        platform_boot_mark("sdl_surface_bad_size");
        return SDL_SetError("PS5: invalid window framebuffer size");
    }
    fb = (PS5_WindowFramebuffer *)SDL_calloc(1, sizeof(*fb));
    if (!fb) {
        platform_boot_mark("sdl_surface_struct_fail");
        return false;
    }
    fb->w = w;
    fb->h = h;
    fb->pitch = w * 4;
    fb->bytes = (size_t)fb->pitch * (size_t)h;
    fb->low = 0;
    fb->pixels = NwHostAllocLow(fb->bytes);
    if (fb->pixels != NULL) {
        fb->low = 1;
    } else {
        fb->pixels = SDL_aligned_alloc(64, fb->bytes);
    }
    if (!fb->pixels) {
        char stage[96];
        snprintf(stage, sizeof stage, "sdl_surface_fail_%dx%d", w, h);
        platform_boot_mark(stage);
        klog_printf("[NetWalk] PS5 video: window surface %dx%d failed: %s\n",
                    w, h, SDL_GetError());
        SDL_free(fb);
        return false;
    }
    {
        char stage[112];
        snprintf(stage, sizeof stage, "sdl_surface_%s_%dx%d_%lx",
                 fb->low ? "low" : "high", w, h,
                 (unsigned long)(uintptr_t)fb->pixels);
        platform_boot_mark(stage);
    }
    SDL_memset(fb->pixels, 0, fb->bytes);
    SDL_SetPointerProperty(SDL_GetWindowProperties(window),
                           PS5_SURFACE_PROPERTY, fb);
    *format = SDL_PIXELFORMAT_ARGB8888;
    *pixels = fb->pixels;
    *pitch = fb->pitch;
    return true;
}

static bool PS5_UpdateWindowFramebuffer(SDL_VideoDevice *_this,
                                        SDL_Window *window,
                                        const SDL_Rect *rects, int numrects)
{
    PS5_WindowFramebuffer *fb;
    const uint32_t *surface;
    int surface_w, surface_h, surface_pitch;
    uint8_t *dst;
    int out_w, out_h, out_x, out_y;
    int x, y;

    (void)_this;
    (void)rects;
    (void)numrects;

    if (!PS5_InitVideoOut()) {
        return false;
    }
    fb = (PS5_WindowFramebuffer *)SDL_GetPointerProperty(
        SDL_GetWindowProperties(window), PS5_SURFACE_PROPERTY, NULL);
    if (!fb || !fb->pixels) {
        return SDL_SetError("PS5: no window framebuffer");
    }
    /* Use the size stored with the surface.  SDL can report the display size
     * here after a window-size change while the allocated surface still has
     * the old pitch; re-querying it would overrun the allocation. */
    surface_w = fb->w;
    surface_h = fb->h;
    {
        int window_w = 0;
        int window_h = 0;
        SDL_GetWindowSizeInPixels(window, &window_w, &window_h);
        if (window_w > 0 && window_h > 0 &&
            (window_w != surface_w || window_h != surface_h)) {
            static int logged;
            if (!logged) {
                logged = 1;
#ifdef NW_DEBUG_TOOLS
                klog_printf("[NetWalk] PS5 video: window %dx%d, surface %dx%d\n",
                            window_w, window_h, surface_w, surface_h);
#endif
            }
        }
    }
    if (surface_w <= 0 || surface_h <= 0) {
        return SDL_SetError("PS5: invalid window surface size");
    }
    surface = (const uint32_t *)fb->pixels;
    surface_pitch = fb->pitch;

    dst = (uint8_t *)ps5_video_frames[ps5_video_index];

    /* Fit the game's 4:3 surface inside the output without stretching.
     * The branch picks the dimension that becomes the limiting dimension:
     * a surface wider than the output is fitted by width, otherwise by
     * height.  Using the output's other dimension here scales a 4:3 surface
     * to 2560x1080 on a 1920x1080 output and writes outside the buffer. */
    if ((int64_t)surface_w * ps5_display_h >
        (int64_t)surface_h * ps5_display_w) {
        out_w = ps5_display_w;
        out_h = (int)((int64_t)ps5_display_w * surface_h / surface_w);
    } else {
        out_h = ps5_display_h;
        out_w = (int)((int64_t)ps5_display_h * surface_w / surface_h);
    }
    if (out_w < 1) out_w = 1;
    if (out_h < 1) out_h = 1;
    if (out_w > ps5_display_w) out_w = ps5_display_w;
    if (out_h > ps5_display_h) out_h = ps5_display_h;
    out_x = (ps5_display_w - out_w) / 2;
    out_y = (ps5_display_h - out_h) / 2;

    /* VideoOut's tiling mode 0 is a 128x128 32-bit tile layout, not a linear
     * framebuffer.  The upstream PS5 boilerplate writes it with the same XOR
     * swizzle; writing rows linearly scrambles the image.  The swizzle splits
     * into an x-only and a y-only term, so the per-pixel work is one XOR and
     * a table lookup after the block base.  The bars outside the 4:3 image
     * are opaque black. */
    PS5_InitTileTables();
    {
        const uint32_t blocks_per_row =
            ((uint32_t)ps5_display_w + 127u) >> 7;
        const uint32_t block_rows =
            ((uint32_t)ps5_display_h + 127u) >> 7;
        const int geometry_changed =
            !ps5_bars_geometry_valid ||
            ps5_bars_x != out_x || ps5_bars_y != out_y ||
            ps5_bars_w != out_w || ps5_bars_h != out_h;

        if (geometry_changed) {
            /* Both buffers need their bars rebuilt when the window changes. */
            ps5_bars_ready[0] = 0;
            ps5_bars_ready[1] = 0;
            ps5_bars_geometry_valid = 1;
            ps5_bars_x = out_x;
            ps5_bars_y = out_y;
            ps5_bars_w = out_w;
            ps5_bars_h = out_h;
        }
        if (!ps5_bars_ready[ps5_video_index]) {
            /* First use of this buffer: make the whole display black.  The
             * image is written below. */
            for (y = 0; y < ps5_display_h; y++) {
                const uint32_t y_part = ps5_tile_y[y & 127];
                const uint32_t block_row =
                    (uint32_t)(y >> 7) * blocks_per_row;
                for (x = 0; x < ps5_display_w; x++) {
                    const uint32_t off =
                        ((block_row + (uint32_t)(x >> 7)) << 16) +
                        (y_part ^ ps5_tile_x[x & 127]);
                    *(uint32_t *)(dst + off) = 0xff000000u;
                }
            }
            ps5_bars_ready[ps5_video_index] = 1;
        }

        {
            /* Fixed-point sampling removes the per-pixel 64-bit divide.
             * At 1:1 the step is exactly 65536 and the copy is unchanged. */
            const uint32_t xstep =
                ((uint32_t)surface_w << 16) / (uint32_t)out_w;
            const uint32_t ystep =
                ((uint32_t)surface_h << 16) / (uint32_t)out_h;
            uint32_t syacc = 0;
            uint32_t row_off[128];
            int i;

            for (y = 0; y < out_h; y++) {
                const int sy = (int)(syacc >> 16);
                const int gy = out_y + y;
                const uint32_t y_part = ps5_tile_y[gy & 127];
                const uint32_t block_row =
                    (uint32_t)(gy >> 7) * blocks_per_row;
                const uint32_t *src =
                    (const uint32_t *)((const uint8_t *)surface +
                                       (size_t)sy * (size_t)surface_pitch);
                uint32_t sxacc = 0;

                for (i = 0; i < 128; i++)
                    row_off[i] = y_part ^ ps5_tile_x[i];
                for (x = 0; x < out_w; x++) {
                    const int gx = out_x + x;
                    const uint32_t color =
                        PS5_SwapRedBlue(src[sxacc >> 16]);
                    const uint32_t off =
                        ((block_row + (uint32_t)(gx >> 7)) << 16) +
                        row_off[gx & 127];
                    *(uint32_t *)(dst + off) = color;
                    sxacc += xstep;
                }
                syacc += ystep;
            }
        }
        {
            size_t used = (size_t)blocks_per_row * (size_t)block_rows * 0x10000u;
            if (ps5_frame_stride != 0 && used > ps5_frame_stride)
                used = ps5_frame_stride;
            PS5_FlushFramebuffer(dst, used);
        }
    }

    {
        int result;

        /* Pace submissions at the display's 60 Hz instead of blocking after
         * the flip.  If the game just produced a slow frame, it can start the
         * next one immediately; the old post-flip wait forced it to lose a
         * whole vblank and visibly settle into 30 Hz. */
        if (ps5_have_last_flip) {
            const Uint64 now = SDL_GetTicksNS();
            const Uint64 elapsed = now - ps5_last_flip_ns;
            if (elapsed < 16666667ULL)
                SDL_DelayNS(16666667ULL - elapsed);
        }

        result = sceVideoOutSubmitFlip(ps5_video_handle, ps5_video_index, 1,
                                       ps5_flip_argument++);
        if (result < 0) {
            SDL_SetError("PS5: sceVideoOutSubmitFlip failed: 0x%08x",
                         (unsigned int)result);
            klog_printf("[NetWalk] PS5 video: sceVideoOutSubmitFlip failed: 0x%08x\n",
                        (unsigned int)result);
            return false;
        }
        ps5_last_flip_ns = SDL_GetTicksNS();
        ps5_have_last_flip = 1;
    }
    if (!ps5_first_flip_logged) {
        ps5_first_flip_logged = 1;
        /* The boilerplate waits once so the first scene is actually on screen
         * before the caller continues; later flips are asynchronous. */
        (void)sceVideoOutWaitVblank(ps5_video_handle);
        platform_boot_mark("videoout_first_flip");
#ifdef NW_DEBUG_TOOLS
        klog_printf("[NetWalk] PS5 video: first frame presented\n");
#endif
    }
    ps5_video_index ^= 1;
    return true;
}

static void PS5_DestroyWindowFramebuffer(SDL_VideoDevice *_this,
                                         SDL_Window *window)
{
    PS5_WindowFramebuffer *fb;

    (void)_this;
    fb = (PS5_WindowFramebuffer *)SDL_GetPointerProperty(
        SDL_GetWindowProperties(window), PS5_SURFACE_PROPERTY, NULL);
    if (fb) {
        if (fb->pixels != NULL) {
            if (fb->low)
                NwHostFreeLow(fb->pixels, fb->bytes);
            else
                SDL_aligned_free(fb->pixels);
        }
        SDL_free(fb);
    }
    SDL_ClearProperty(SDL_GetWindowProperties(window), PS5_SURFACE_PROPERTY);
}

static void PS5_DeleteDevice(SDL_VideoDevice *device)
{
    SDL_free(device);
}

static SDL_VideoDevice *PS5_CreateDevice(void)
{
    SDL_VideoDevice *device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        return NULL;
    }
    device->VideoInit = PS5_VideoInit;
    device->VideoQuit = PS5_VideoQuit;
    device->PumpEvents = PS5_PumpEvents;
    device->CreateSDLWindow = PS5_CreateWindow;
    device->SetWindowPosition = PS5_SetWindowPosition;
    device->SetWindowSize = PS5_SetWindowSize;
    device->SetWindowFullscreen = PS5_SetWindowFullscreen;
    device->CreateWindowFramebuffer = PS5_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = PS5_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = PS5_DestroyWindowFramebuffer;
    device->free = PS5_DeleteDevice;
    /* The driver keeps the window at its 4:3 surface size; SDL must not
     * overwrite it with the 16:9 display mode. */
    device->device_caps = VIDEO_DEVICE_CAPS_FULLSCREEN_ONLY |
                          VIDEO_DEVICE_CAPS_SENDS_FULLSCREEN_DIMENSIONS;
    return device;
}

VideoBootStrap PS5_bootstrap = {
    "ps5", "PlayStation 5 video driver",
    PS5_CreateDevice,
    NULL, /* no ShowMessageBox implementation */
    false
};

#endif /* SDL_VIDEO_DRIVER_PS5 */
