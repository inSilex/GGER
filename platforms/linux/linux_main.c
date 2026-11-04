/* Native ELF entry point for NetWalk. */
#include <stddef.h>
#include <stdlib.h>

extern void NwLinuxSetArgs(int argc, char **argv);
extern int InitializeAndRunWinMain(void);

int main(int argc, char **argv) __asm__("main");

/*
 * The static SDL3 build links PulseAudio and ALSA.  Prefer PulseAudio
 * (WSLg exposes it through /mnt/wslg/PulseServer), then ALSA, then the
 * harmless dummy sink when the caller has not selected a driver explicitly.
 * NW_NO_AUDIO=1 still suppresses the subsystem entirely; SDL_AUDIODRIVER
 * overrides this default.
 */
static void NwLinuxSetDefaultAudioDriver(void)
{
    if (getenv("SDL_AUDIODRIVER") == NULL &&
        getenv("SDL_AUDIO_DRIVER") == NULL) {
        (void)setenv("SDL_AUDIODRIVER", "pulseaudio,alsa,dummy", 0);
    }
}

int main(int argc, char **argv)
{
    NwLinuxSetDefaultAudioDriver();
    NwLinuxSetArgs(argc, argv);
    return InitializeAndRunWinMain();
}
