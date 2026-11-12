/*
 * ios_main.c -- iOS entry point for the portable NetWalk build.
 *
 * SDL3's UIKit delegate calls SDL_main() from the application's main thread.
 * An iOS .app is read-only, so the game gets its writable data directory from
 * SDL_GetPrefPath().  The overlay and SoundFont are copied out of the bundle
 * on first launch (and refreshed when the bundled file changes), then the
 * Win32-compatible path layer is pointed at a synthetic NETWALK.EXE inside
 * that directory.  Saves, temporary files, and the SoundFont therefore resolve
 * exactly as they do in the Android and desktop ports.
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

extern void NwLinuxSetArgs(int argc, char **argv);
extern void NwLinuxSetModulePath(const char *pszPosixPath);
extern int InitializeAndRunWinMain(void);

#define NW_IOS_COPY_BUFFER (64 * 1024)

static int NwIosCopyFile(const char *pszSource, const char *pszDest)
{
    FILE *pIn;
    FILE *pOut;
    char *pBuffer;
    int fOk = 1;

    pIn = fopen(pszSource, "rb");
    if (pIn == NULL) {
        SDL_Log("ios: cannot open bundled resource %s: %s",
                pszSource, strerror(errno));
        return 0;
    }
    pOut = fopen(pszDest, "wb");
    if (pOut == NULL) {
        SDL_Log("ios: cannot create %s: %s", pszDest, strerror(errno));
        fclose(pIn);
        return 0;
    }
    pBuffer = (char *)SDL_malloc(NW_IOS_COPY_BUFFER);
    if (pBuffer == NULL) {
        SDL_Log("ios: out of memory copying %s", pszSource);
        fclose(pOut);
        fclose(pIn);
        return 0;
    }
    for (;;) {
        size_t cbRead = fread(pBuffer, 1, NW_IOS_COPY_BUFFER, pIn);
        if (cbRead != 0 && fwrite(pBuffer, 1, cbRead, pOut) != cbRead) {
            SDL_Log("ios: write failed for %s: %s", pszDest, strerror(errno));
            fOk = 0;
            break;
        }
        if (cbRead < NW_IOS_COPY_BUFFER) {
            if (ferror(pIn)) {
                SDL_Log("ios: read failed for %s: %s",
                        pszSource, strerror(errno));
                fOk = 0;
            }
            break;
        }
    }
    SDL_free(pBuffer);
    if (fclose(pOut) != 0) {
        fOk = 0;
    }
    fclose(pIn);
    if (!fOk) {
        (void)remove(pszDest);
    }
    return fOk;
}

/*
 * Copy a bundle file into the writable directory when the destination is
 * missing or a different size.  A size change catches application upgrades
 * that ship a new overlay or SoundFont without discarding unrelated saves.
 * A missing source is not fatal: the game reports the missing overlay itself
 * and the SoundFont is optional.
 */
static void NwIosSyncResource(const char *pszBase, const char *pszPref,
                              const char *pszName)
{
    char szSource[4096];
    char szDest[4096];
    struct stat stSource;
    struct stat stDest;

    if (SDL_snprintf(szSource, sizeof(szSource), "%s%s", pszBase,
                     pszName) <= 0 ||
        SDL_snprintf(szDest, sizeof(szDest), "%s%s", pszPref, pszName) <= 0) {
        SDL_Log("ios: resource path too long: %s", pszName);
        return;
    }
    if (stat(szSource, &stSource) != 0) {
        SDL_Log("ios: bundled %s is missing (%s)", pszName,
                strerror(errno));
        return;
    }
    if (stat(szDest, &stDest) == 0 &&
        stDest.st_size == stSource.st_size &&
        stDest.st_mtime >= stSource.st_mtime) {
        return;
    }
    (void)NwIosCopyFile(szSource, szDest);
}

int main(int argc, char **argv)
{
    const char *pszBase;
    char *pszPref;
    char szModule[4096];

    /* The engine is a fixed 4:3 canvas; presentation letterboxes it into the
     * current window, so portrait and landscape are both valid.  Do not force
     * an orientation hint here: UIKit has already chosen the launch
     * orientation from Info.plist by the time main() runs, and a hint that
     * disagreed with that choice made SDL compute a window frame for the
     * wrong orientation (cropped image with asymmetric black bars).
     * SDL's touch-to-mouse bridge turns taps into the left-button events the
     * desktop input code already understands. */
    (void)SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");
    (void)SDL_SetHint(SDL_HINT_RETURN_KEY_HIDES_IME, "0");

    pszBase = SDL_GetBasePath();
    if (pszBase == NULL) {
        SDL_Log("ios: SDL_GetBasePath failed: %s", SDL_GetError());
        return 1;
    }
    pszPref = SDL_GetPrefPath("Gamos", "NetWalk");
    if (pszPref == NULL) {
        SDL_Log("ios: SDL_GetPrefPath failed: %s", SDL_GetError());
        return 1;
    }

    NwIosSyncResource(pszBase, pszPref, "overlay.nw");
    NwIosSyncResource(pszBase, pszPref, "gm.sf2");

    if (chdir(pszPref) != 0) {
        SDL_Log("ios: cannot enter writable directory %s: %s",
                pszPref, strerror(errno));
        SDL_free(pszPref);
        return 1;
    }

    if (SDL_snprintf(szModule, sizeof(szModule), "%sNETWALK.EXE",
                     pszPref) <= 0) {
        SDL_Log("ios: writable directory path is too long");
        SDL_free(pszPref);
        return 1;
    }

    NwLinuxSetModulePath(szModule);
    NwLinuxSetArgs(argc, argv);

    SDL_free(pszPref);
    return InitializeAndRunWinMain();
}
