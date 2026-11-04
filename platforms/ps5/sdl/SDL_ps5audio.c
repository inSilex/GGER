/*
 * NetWalk for the PlayStation 5 - SDL3's audio driver for the console.
 *
 * One playback device: an AudioOut main port, 48 kHz stereo 16-bit, a grain
 * of 256 frames. sceAudioOutOutput blocks while the console plays the grain
 * before, which paces SDL's audio thread.
 *
 * Derived from PS5_Dusklight's ps5/sdl/SDL_ps5audio.c (Zlib licence).
 *
 * SPDX-License-Identifier: Zlib
 */
#include "SDL_internal.h"

#ifdef SDL_AUDIO_DRIVER_PS5

#include "audio/SDL_audiodev_c.h"
#include "audio/SDL_sysaudio.h"

#include <stdio.h>

int sceAudioOutInit(void);
int sceAudioOutOpen(int32_t user, int32_t type, int32_t index, uint32_t grain, uint32_t rate, uint32_t format);
int sceAudioOutOutput(int32_t port, const void *samples);
int sceAudioOutClose(int32_t port);
int klog_printf(const char *format, ...);
extern int platform_active_user(void);
extern void platform_boot_mark(const char *stage);

#define PS5_AUDIO_GRAIN               256
#define PS5_AUDIO_ALREADY_INITIALISED ((int)0x8026000e)
#define PS5_AUDIO_USER_SYSTEM         0xff
#define PS5_AUDIO_USER_PRIMARY        0x10000000
#define PS5_AUDIO_PORT_MAIN           0
#define PS5_AUDIO_FORMAT_S16_STEREO   1

struct SDL_PrivateAudioData
{
    int port;
    Uint8 *buffer;
};

static int PS5AUDIO_OpenPort(int user)
{
    const int port = sceAudioOutOpen(user, PS5_AUDIO_PORT_MAIN, 0,
                                     PS5_AUDIO_GRAIN, 48000,
                                     PS5_AUDIO_FORMAT_S16_STEREO);
    char stage[80];

    if (port >= 0) {
        snprintf(stage, sizeof stage, "audioout_ok_0x%x", (unsigned)user);
        platform_boot_mark(stage);
#ifdef NW_DEBUG_TOOLS
        klog_printf("[NetWalk] PS5 audio: user 0x%x -> port %d\n",
                    (unsigned)user, port);
#endif
    } else {
        snprintf(stage, sizeof stage, "audioout_fail_0x%x_%08x",
                 (unsigned)user, (unsigned)port);
        platform_boot_mark(stage);
        klog_printf("[NetWalk] PS5 audio: user 0x%x failed 0x%08x\n",
                    (unsigned)user, (unsigned)port);
    }
    return port;
}

static bool PS5AUDIO_OpenDevice(SDL_AudioDevice *device)
{
    int init;
    int users[4];
    int user_count = 0;
    int i;

    device->hidden = (struct SDL_PrivateAudioData *)SDL_calloc(1, sizeof(*device->hidden));
    if (!device->hidden) {
        return false;
    }
    device->hidden->port = -1;

    device->spec.format = SDL_AUDIO_S16LE;
    device->spec.channels = 2;
    device->spec.freq = 48000;
    device->sample_frames = PS5_AUDIO_GRAIN;
    SDL_UpdatedAudioDeviceFormat(device);

    device->hidden->buffer = (Uint8 *)SDL_aligned_alloc(64, device->buffer_size);
    if (!device->hidden->buffer) {
        platform_boot_mark("audioout_buffer_fail");
        return false;
    }
    SDL_memset(device->hidden->buffer, 0, device->buffer_size);

    platform_boot_mark("audioout_init_begin");
    init = sceAudioOutInit();
    if (init < 0 && init != PS5_AUDIO_ALREADY_INITIALISED) {
        char stage[64];
        snprintf(stage, sizeof stage, "audioout_init_fail_%08x",
                 (unsigned)init);
        platform_boot_mark(stage);
        klog_printf("[NetWalk] PS5 audio: sceAudioOutInit failed 0x%08x\n",
                    (unsigned)init);
        return SDL_SetError("sceAudioOutInit: 0x%08x", (unsigned)init);
    }
    platform_boot_mark("audioout_init_ok");

    /* A native title runs as its signed-in user; a payload runs as the system
     * user.  Try the active user first, then both known fallbacks. */
    users[user_count++] = platform_active_user();
    if (users[0] != PS5_AUDIO_USER_PRIMARY)
        users[user_count++] = PS5_AUDIO_USER_PRIMARY;
    if (users[0] != PS5_AUDIO_USER_SYSTEM)
        users[user_count++] = PS5_AUDIO_USER_SYSTEM;
    users[user_count++] = 0;
    for (i = 0; i < user_count; i++) {
        int j;
        for (j = 0; j < i; j++) {
            if (users[j] == users[i])
                break;
        }
        if (j != i)
            continue;
        device->hidden->port = PS5AUDIO_OpenPort(users[i]);
        if (device->hidden->port >= 0)
            break;
    }
    if (device->hidden->port < 0) {
        platform_boot_mark("audioout_open_fail");
        return SDL_SetError("sceAudioOutOpen: no usable user");
    }
    platform_boot_mark("audioout_open_ok");
    return true;
}

static bool PS5AUDIO_PlayDevice(SDL_AudioDevice *device, const Uint8 *buffer, int buffer_size)
{
    return sceAudioOutOutput(device->hidden->port, buffer) >= 0;
}

static bool PS5AUDIO_WaitDevice(SDL_AudioDevice *device)
{
    return true; // PlayDevice blocks
}

static Uint8 *PS5AUDIO_GetDeviceBuf(SDL_AudioDevice *device, int *buffer_size)
{
    return device->hidden->buffer;
}

static void PS5AUDIO_CloseDevice(SDL_AudioDevice *device)
{
    if (device->hidden) {
        if (device->hidden->port >= 0) {
            (void)sceAudioOutOutput(device->hidden->port, NULL); // drain
            (void)sceAudioOutClose(device->hidden->port);
        }
        SDL_aligned_free(device->hidden->buffer);
        SDL_free(device->hidden);
        device->hidden = NULL;
    }
}

static bool PS5AUDIO_Init(SDL_AudioDriverImpl *impl)
{
    impl->OpenDevice = PS5AUDIO_OpenDevice;
    impl->PlayDevice = PS5AUDIO_PlayDevice;
    impl->WaitDevice = PS5AUDIO_WaitDevice;
    impl->GetDeviceBuf = PS5AUDIO_GetDeviceBuf;
    impl->CloseDevice = PS5AUDIO_CloseDevice;
    impl->OnlyHasDefaultPlaybackDevice = true;
    return true;
}

AudioBootStrap PS5AUDIO_bootstrap = {
    "ps5", "PlayStation 5 audio driver", PS5AUDIO_Init, false, false
};

#endif // SDL_AUDIO_DRIVER_PS5
