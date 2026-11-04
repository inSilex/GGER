/*
 * NetWalk for the PlayStation 5 - what runs before the game's main.
 *
 * Derived from PS5_Dusklight's ps5/src/startup.c (MIT licence).
 *
 * The console's start-up is a constructor: open klog, dismiss the shell's
 * splash, and point the folder SDL derives its paths from at the title.
 *
 * SPDX-License-Identifier: MIT
 */
#include <SDL3/SDL_stdinc.h>

#include "platform.h"

/* A payload sees its own folder at /app0 and can write beside the ELF. A
 * native title mounts /app0 read-only; the loader gives it /download0 for
 * writable application state when param.json has a positive downloadDataSize.
 * Keep the payload path unchanged and select the title path at compile time. */
#if defined(NW_PS5_NATIVE_TITLE)
#define NW_PS5_DATA_HOME "/download0"
#else
#define NW_PS5_DATA_HOME "/app0/userdata"
#endif

__attribute__((constructor)) static void
ps5_startup(void)
{
   /* Some payload loaders run .init_array before main; main.c also runs it so
    * the POSIX compatibility constructors exist on loaders that do not.  Keep
    * the platform half idempotent so the splash and environment are set up
    * exactly once. */
   static int started;
   if (started)
      return;
   started = 1;
   platform_boot_mark("ps5_startup");
   platform_init(PS5_TITLE_NAME);
   /* SDL_GetPrefPath is $XDG_DATA_HOME/<org>/<app>/: saves and settings. Set in
    * SDL's own copy of the environment, which is all SDL_getenv reads: the
    * console's libc keeps what setenv sets apart from the `environ` SDL copies. */
   SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "XDG_DATA_HOME", NW_PS5_DATA_HOME, true);
   SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "HOME", NW_PS5_DATA_HOME, true);
   platform_boot_mark("ps5_env");
}
