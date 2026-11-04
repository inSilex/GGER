/*
 * Android-only virtual controls drawn in the unused bars around the centered
 * 4:3 game viewport.  The controls are handled by MainLoopSdl.c after the
 * touch-to-mouse bridge has consumed their events, so they never reach the
 * game's own pointer handling.
 *
 * The current set is Exit, Keyboard, rotation Lock/Unlock, and a single
 * Left/Right click-mode toggle.  The selected click mode determines how touch
 * input inside the game viewport is delivered; left click is selected at
 * startup.
 */

#ifndef NW_SDL_ANDROID_CONTROLS_H
#define NW_SDL_ANDROID_CONTROLS_H

#if defined(__ANDROID__)

#include <SDL3/SDL_rect.h>

#define NW_ANDROID_CONTROL_COUNT 4

enum {
    NW_ANDROID_CONTROL_EXIT = 1,
    NW_ANDROID_CONTROL_KEYBOARD = 2,
    NW_ANDROID_CONTROL_LOCK = 3,
    NW_ANDROID_CONTROL_CLICK = 4
};

int  NwSdlAndroidControlLayout(SDL_Rect *pButtons);
int  NwSdlAndroidControlHit(int x, int y);
int  NwSdlAndroidControlPressed(void);
void NwSdlAndroidControlPress(int id);
void NwSdlAndroidControlCancel(void);
int  NwSdlAndroidControlRelease(int x, int y);
int  NwSdlAndroidToggleKeyboard(void);
int  NwSdlAndroidToggleRotationLock(void);
int  NwSdlAndroidRotationLocked(void);
int  NwSdlAndroidMouseRight(void);
int  NwSdlAndroidToggleMouseRight(void);

#endif /* __ANDROID__ */

#endif /* NW_SDL_ANDROID_CONTROLS_H */
