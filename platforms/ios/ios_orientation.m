/*
 * ios_orientation.m -- apply and release the in-app orientation lock.
 *
 * Narrowing SDL_HINT_ORIENTATIONS only changes the set UIKit reports from
 * supportedInterfaceOrientations.  UIKit does not necessarily re-evaluate the
 * current interface orientation until the next sensor event, so a restored
 * lock or an unlock while the device is already turned could leave the game in
 * the old orientation.  These helpers ask the active scene to update now.
 */

#import <UIKit/UIKit.h>

#include <string.h>

static UIWindowScene *NwIosActiveScene(UIViewController **ppController)
{
    UIApplication *app = [UIApplication sharedApplication];
    UIWindowScene *fallbackScene = nil;
    UIViewController *fallbackController = nil;

    for (UIScene *candidate in app.connectedScenes) {
        if (![candidate isKindOfClass:[UIWindowScene class]]) {
            continue;
        }
        UIWindowScene *scene = (UIWindowScene *)candidate;
        if (scene.activationState != UISceneActivationStateForegroundActive &&
            scene.activationState != UISceneActivationStateForegroundInactive) {
            continue;
        }
        for (UIWindow *window in scene.windows) {
            if (window.rootViewController == nil) {
                continue;
            }
            if (window.isKeyWindow) {
                if (ppController != NULL) {
                    *ppController = window.rootViewController;
                }
                return scene;
            }
            /* The SDL window is still hidden while the saved lock is applied,
             * so it is not key yet.  Keep it as a fallback. */
            if (fallbackController == nil) {
                fallbackScene = scene;
                fallbackController = window.rootViewController;
            }
        }
    }
    if (ppController != NULL) {
        *ppController = fallbackController;
    }
    return fallbackScene;
}

static void NwIosUpdateSceneOrientation(UIInterfaceOrientationMask mask)
{
    UIViewController *controller = nil;
    UIWindowScene *scene;

    @autoreleasepool {
        scene = NwIosActiveScene(&controller);
        if (scene == nil || controller == nil) {
            return;
        }

        if (@available(iOS 16.0, *)) {
            /* Re-evaluate the supported set and ask the scene to take the
             * requested orientation without waiting for another sensor
             * event. */
            [controller setNeedsUpdateOfSupportedInterfaceOrientations];
            UIWindowSceneGeometryPreferencesIOS *preferences =
                [[UIWindowSceneGeometryPreferencesIOS alloc]
                    initWithInterfaceOrientations:mask];
            [scene requestGeometryUpdateWithPreferences:preferences
                                           errorHandler:^(NSError *error) {
                (void)error;
            }];
#if !__has_feature(objc_arc)
            [preferences release];
#endif
        } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            [UIViewController attemptRotationToDeviceOrientation];
#pragma clang diagnostic pop
        }
    }
}

/* Rotate to one saved SDL_HINT_ORIENTATIONS name after the window exists. */
void NwIosRequestOrientation(const char *pszMask)
{
    UIInterfaceOrientationMask mask = 0;

    if (pszMask == NULL || *pszMask == '\0') {
        return;
    }
    /* SDL maps these names straight onto the UIKit masks. */
    if (strstr(pszMask, "LandscapeLeft") != NULL) {
        mask |= UIInterfaceOrientationMaskLandscapeLeft;
    }
    if (strstr(pszMask, "LandscapeRight") != NULL) {
        mask |= UIInterfaceOrientationMaskLandscapeRight;
    }
    if (strstr(pszMask, "PortraitUpsideDown") != NULL) {
        mask |= UIInterfaceOrientationMaskPortraitUpsideDown;
    }
    if (strstr(pszMask, "Portrait") != NULL) {
        mask |= UIInterfaceOrientationMaskPortrait;
    }
    if (mask != 0) {
        NwIosUpdateSceneOrientation(mask);
    }
}

/* Allow every orientation again after the player taps UNLOCK. */
void NwIosRefreshInterfaceOrientation(void)
{
    NwIosUpdateSceneOrientation(UIInterfaceOrientationMaskAll);
}
