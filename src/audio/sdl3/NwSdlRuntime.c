/*
 * NwSdlRuntime -- statically linked SDL3 entry-point table shared by the MIDI
 * and PCM sinks.
 *
 * Module: audio/sdl3
 * Interface: include/NwSdlRuntime.h
 *
 * Unit:
 *   NwSdlRuntime                                               audio/sdl3/NwSdlRuntime.c
 *
 * SDL3 is linked into the executable as a static library.  The table below is
 * populated with the real function addresses at first use; keeping one table
 * means the two sinks share one code path whether the build is static (this
 * build) or, in a future port, resolved from a shared object.
 *
 * The whole translation unit is placed in the pinned `.nwdiag` section because
 * the fixed image has no room left in `.text`.  The table lives in `.bss`.
 */

#ifdef NW_UNIT_NwSdlRuntime

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#include <SDL3/SDL_init.h>

#include "NwSdlRuntime.h"

#define NW_DIAG __attribute__((section(".nwdiag")))

static NwSdlApi g_api;
static int      g_loaded;
static int      g_audioRefs;

/*
 * libmingwex's gdtoa lock registers a thread cleanup through atexit().  This
 * image uses a custom -nostartfiles entry and never runs the CRT atexit list,
 * and the gdtoa lock is process-lifetime here, so a no-op registration is
 * correct.  Supplying it explicitly also keeps gen_stubs.py from emitting a
 * trap stub with the wrong (void) signature, which conflicts with <stdlib.h>.
 */
#if defined(__MINGW32__) || (defined(_WIN32) && !defined(_MSC_VER))
/* MinGW's custom -nostartfiles entry does not run the CRT atexit list.  MSVC
 * supplies the real CRT implementation, so defining this there is a duplicate
 * symbol at link time. */
NW_DIAG int atexit(void (*func)(void))
{
    (void)func;
    return 0;
}
#endif

NW_DIAG int NwSdl_Load(void)
{
    if (g_loaded)
        return 1;

    /* Static link: every symbol is resolved by the linker. */
    g_api.InitSubSystem           = SDL_InitSubSystem;
    g_api.QuitSubSystem           = SDL_QuitSubSystem;
    g_api.GetError                = SDL_GetError;
    g_api.LoadFile                = SDL_LoadFile;
    g_api.Free                    = SDL_free;
    g_api.OpenAudioDeviceStream   = SDL_OpenAudioDeviceStream;
    g_api.DestroyAudioStream      = SDL_DestroyAudioStream;
    g_api.ResumeAudioStreamDevice = SDL_ResumeAudioStreamDevice;
    g_api.PauseAudioStreamDevice  = SDL_PauseAudioStreamDevice;
    g_api.PutAudioStreamData      = SDL_PutAudioStreamData;
    g_api.LockAudioStream         = SDL_LockAudioStream;
    g_api.UnlockAudioStream       = SDL_UnlockAudioStream;
    g_api.SetAudioStreamGain      = SDL_SetAudioStreamGain;
    g_api.GetAudioStreamQueued    = SDL_GetAudioStreamQueued;
    g_api.ClearAudioStream        = SDL_ClearAudioStream;

    g_loaded = 1;
    return 1;
}

NW_DIAG NwSdlApi *NwSdl(void)
{
    return g_loaded ? &g_api : 0;
}

/*
 * The regression harness must be able to reproduce the old "SDL3 absent"
 * configuration now that SDL3 is linked in: NW_NO_AUDIO=1 makes AcquireAudio
 * fail exactly as a missing SDL3.dll did, so the sequencer stays unarmed and
 * the VM trace remains byte-identical.  The explicit --audio gate clears it.
 */
NW_DIAG static int NwAudioSuppressed(void)
{
    const char *v = getenv("NW_NO_AUDIO");
    return (v != 0 && v[0] != '\0' && v[0] != '0') ? 1 : 0;
}

NW_DIAG int NwSdl_AcquireAudio(void)
{
    if (NwAudioSuppressed())
        return 0;
    if (!NwSdl_Load())
        return 0;
    if (!g_api.InitSubSystem(SDL_INIT_AUDIO))
        return 0;
    g_audioRefs++;
    return 1;
}

NW_DIAG void NwSdl_ReleaseAudio(void)
{
    if (g_audioRefs <= 0)
        return;
    /* Keep the SDL audio subsystem alive for the process lifetime.  The MIDI
     * sink owns a reference too, and a failed bank lookup releases that
     * reference while the PCM stream is still playing.  Quitting on the way
     * out used to let the MIDI failure tear down the already-open SFX device,
     * which made every sound disappear even though audioout_open_ok had
     * already been logged.  The streams are destroyed by their own shutdown
     * paths; leaving the subsystem initialized costs nothing at exit. */
    g_audioRefs--;
}

#endif /* NW_UNIT_NwSdlRuntime */
