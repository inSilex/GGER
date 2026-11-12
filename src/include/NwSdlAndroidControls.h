/*
 * Mobile virtual controls on Android and iOS.  The letterbox bars get first
 * claim on them: when a bar can hold the whole button set, the buttons are
 * drawn there directly and never cover the game.  On a window too close to
 * 4:3 for that, they move to a card that drops down from a small tab at the
 * top; opening it reveals a row of square buttons with the card sized to that
 * row plus its padding and it closes after a few idle seconds or when the
 * player taps outside it.  The
 * controls are handled by MainLoopSdl.c after the touch-to-mouse bridge has
 * consumed their events, so they never reach the game's own pointer handling.
 *
 * The current set is Exit, Keyboard, rotation Lock/Unlock, and a single
 * Left/Right click-mode toggle.  The selected click mode determines how touch
 * input inside the game viewport is delivered; left click is selected at
 * startup.  A short tap on the click control toggles the selection.  Holding
 * it turns it into a momentary modifier instead: the effective mode is the
 * opposite of the selection while the control stays down, and releasing it
 * leaves the selection unchanged, so one finger can hold the modifier while
 * another clicks in the game.  The rotation-lock choice is remembered across
 * launches: Android
 * keeps it in its activity preferences, and iOS keeps it in a separate
 * rotation.cfg file in the writable preference directory (not in the game
 * save) and reapplies the pinned orientation before the first window.
 */

#ifndef NW_SDL_ANDROID_CONTROLS_H
#define NW_SDL_ANDROID_CONTROLS_H

#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)

#include <SDL3/SDL_rect.h>

#define NW_ANDROID_CONTROL_COUNT 4
#define NW_SDL_CARD_PROGRESS_FULL 255

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
int  NwSdlAndroidControlCardLayout(SDL_Rect *pHandle, SDL_Rect *pCard);
int  NwSdlAndroidControlsInBar(void);
int  NwSdlAndroidControlCardProgress(void);
int  NwSdlAndroidControlCardIsOpen(void);
void NwSdlAndroidControlCardToggle(void);
void NwSdlAndroidControlCardClose(void);
int  NwSdlAndroidControlCardTick(void);
int  NwSdlAndroidControlHandleHit(int x, int y);
int  NwSdlAndroidControlCardHit(int x, int y);
int  NwSdlAndroidHideKeyboard(void);
int  NwSdlAndroidToggleKeyboard(void);
int  NwSdlAndroidToggleRotationLock(void);
int  NwSdlAndroidRotationLocked(void);
int  NwSdlAndroidMouseRight(void);
int  NwSdlAndroidToggleMouseRight(void);
int  NwSdlAndroidClickModifierActive(void);
void NwSdlAndroidTouchPoint(float nx, float ny, int *px, int *py);
float NwSdlAndroidUnitsPerDp(void);
int  NwSdlAndroidTabletLayout(void);

#endif /* __ANDROID__ || NW_PORTABLE_IOS */

#endif /* NW_SDL_ANDROID_CONTROLS_H */
