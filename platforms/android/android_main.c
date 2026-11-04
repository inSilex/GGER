/*
 * android_main.c -- Android entry point for the portable NetWalk build.
 *
 * SDLActivity loads libmain.so and calls the exported SDL_main() from its
 * dedicated game thread.  The shared library is not the process executable,
 * so /proc/self/exe cannot provide a useful module directory.  The APK's
 * assets are copied into the app's internal files directory by
 * NetWalkActivity before SDL starts; point the Win32-compatible path layer at
 * a synthetic NETWALK.EXE there so the overlay, temporary files, saves, and
 * SoundFont all resolve inside writable storage.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <android/log.h>

#include <SDL3/SDL_main.h>
#include <SDL3/SDL_system.h>

#define NW_ANDROID_LOG_TAG "NetWalk"

extern void NwLinuxSetArgs(int argc, char **argv);
extern void NwLinuxSetModulePath(const char *pszPosixPath);
extern int InitializeAndRunWinMain(void);

SDLMAIN_DECLSPEC int SDLCALL SDL_main(int argc, char **argv)
{
    const char *pszHome;
    char szModule[4096];

    pszHome = SDL_GetAndroidInternalStoragePath();
    if (pszHome == NULL || pszHome[0] == '\0') {
        __android_log_print(ANDROID_LOG_ERROR, NW_ANDROID_LOG_TAG,
                            "internal storage path is unavailable");
        return 1;
    }

    if (chdir(pszHome) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, NW_ANDROID_LOG_TAG,
                            "cannot enter internal storage: %s", pszHome);
        return 1;
    }

    (void)snprintf(szModule, sizeof(szModule), "%s/NETWALK.EXE", pszHome);
    NwLinuxSetModulePath(szModule);
    NwLinuxSetArgs(argc, argv);

    __android_log_print(ANDROID_LOG_INFO, NW_ANDROID_LOG_TAG,
                        "starting from %s", pszHome);
    return InitializeAndRunWinMain();
}
