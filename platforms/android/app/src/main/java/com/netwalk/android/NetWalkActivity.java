package com.netwalk.android;

import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.Display;
import android.view.Surface;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * SDL activity that stages the read-only APK assets into the app's writable
 * internal files directory before the native game starts.
 *
 * The game's Win32-compatible path layer expects a module directory containing
 * overlay.nw, gm.sf2, and its save files.  Android's APK assets are not regular
 * files, so copy them once (or when the bundled size changes).  The game creates
 * its save in the same writable directory on first run.
 *
 * NetWalkActivity owns orientation.  Unlocked, it follows all four sensor
 * orientations so the centered 4:3 viewport can use portrait or either
 * landscape direction.  The native LOCK control pins the current orientation.
 */
public class NetWalkActivity extends SDLActivity {
    private static final String TAG = "NetWalk";
    private static final int COPY_BUFFER = 64 * 1024;
    private static final String PREFS_NAME = "netwalk_android";
    private static final String PREF_ROTATION_LOCKED = "rotation_locked";
    private static final String PREF_LOCKED_ORIENTATION = "locked_orientation";

    /* The native LOCK/UNLOCK control toggles this through setRotationLocked().
     * The lock state and the exact locked orientation are persisted so the
     * next launch starts in the same configuration. */
    private volatile boolean rotationLocked = false;
    private volatile int lockedOrientation = ActivityInfo.SCREEN_ORIENTATION_LOCKED;
    private volatile int requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;

    /* SDLActivity.onCreate posts its ordinary window style after this activity
     * enters fullscreen.  Keep correcting that startup change briefly, then
     * stop so an immersive-sticky edge swipe can reveal the bars normally. */
    private boolean fullscreenBootstrap = true;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        loadRotationPreferences();

        File files = getFilesDir();
        copyAsset(files, "overlay.nw", false);
        copyAsset(files, "gm.sf2", false);
        super.onCreate(savedInstanceState);

        /* SDL's orientation helper runs during super.onCreate(); reapply the
         * saved lock after it so the persisted choice always wins. */
        applyRotationPreference();
        enterFullscreen();
        /* SDLActivity.onCreate queues its normal non-fullscreen window style.
         * Reapply after that message has run so the system bars stay hidden. */
        getWindow().getDecorView().postDelayed(new Runnable() {
            @Override
            public void run() {
                if (!isFinishing() && !isDestroyed()) {
                    enterFullscreen();
                }
                fullscreenBootstrap = false;
            }
        }, 2000);
    }

    @Override
    protected void onResume() {
        super.onResume();
        enterFullscreen();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            enterFullscreen();
        }
    }

    @Override
    public void onSystemUiVisibilityChange(int visibility) {
        super.onSystemUiVisibilityChange(visibility);
        /* SDLActivity.onCreate asks for its ordinary window style after this
         * activity has already entered fullscreen.  Its decor listener reports
         * that change here, so put the immersive flags back immediately rather
         * than depending on a later focus event.  This correction is limited to
         * startup so a later immersive-sticky edge swipe still reveals the
         * system bars temporarily. */
        if (fullscreenBootstrap
                && ((visibility & View.SYSTEM_UI_FLAG_FULLSCREEN) == 0
                    || (visibility & View.SYSTEM_UI_FLAG_HIDE_NAVIGATION) == 0)) {
            enterFullscreen();
        }
    }

    /**
     * Keep the game edge-to-edge with the status and navigation bars hidden.
     * Immersive-sticky lets a deliberate edge swipe reveal the bars
     * temporarily; they hide again automatically.  The orientation-lock and
     * IME paths remain activity-owned.
     */
    private void enterFullscreen() {
        Window window = getWindow();
        if (window == null) {
            return;
        }

        View decor = window.getDecorView();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            WindowInsetsController controller = window.getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.statusBars()
                        | WindowInsets.Type.navigationBars());
                controller.setSystemBarsBehavior(
                        WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        }

        /* Deliberately leave the LAYOUT_* flags off.  They extend the surface
         * behind the system bars, which also stops adjustResize from shrinking
         * the window when the soft keyboard appears.  With the bars hidden the
         * normal content area already fills the display, and the IME can still
         * resize the activity and keep every control on screen. */
        decor.setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION);
        /* Use only the system-UI visibility flags for fullscreen.  The
         * FLAG_FULLSCREEN window flag (set by the manifest theme) prevents
         * adjustResize from shrinking the window while the soft keyboard is
         * visible. */
        window.clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        window.clearFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);

        WindowManager.LayoutParams attributes = window.getAttributes();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            attributes.layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            attributes.layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }
        window.setAttributes(attributes);
    }

    /**
     * Called from native code when the on-screen rotation-lock control is
     * tapped.  Locking captures the current fixed orientation; unlocking
     * resumes all four sensor orientations.  Both choices are written to
     * SharedPreferences so they survive process death and later launches.
     */
    public void setRotationLocked(final boolean locked) {
        if (locked) {
            lockedOrientation = currentFixedOrientation();
        }
        rotationLocked = locked;
        saveRotationPreferences();
        applyRotationPreference();
    }

    /** Query the persisted lock state from native code. */
    public boolean isRotationLocked() {
        return rotationLocked;
    }

    @Override
    public void setOrientationBis(int w, int h, boolean resizable, String hint) {
        /* SDL's helper only chooses a fixed orientation set from the hint.
         * NetWalk owns the full-sensor/locked toggle instead. */
        applyRotationPreference();
    }

    private void applyRotationPreference() {
        final int orientation = rotationLocked
                ? lockedOrientation
                : ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR;

        if (orientation == requestedOrientation) {
            return;
        }
        requestedOrientation = orientation;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (!isFinishing() && !isDestroyed()) {
                    setRequestedOrientation(orientation);
                }
            }
        });
    }

    /**
     * Resolve the current rotation into a fixed ActivityInfo orientation.
     * The display rotation is relative to the device's natural orientation,
     * so first infer whether the natural orientation is portrait or
     * landscape; this preserves reverse-landscape/reverse-portrait locks too.
     */
    private int currentFixedOrientation() {
        int orientation = getResources().getConfiguration().orientation;
        if (orientation != Configuration.ORIENTATION_PORTRAIT
                && orientation != Configuration.ORIENTATION_LANDSCAPE) {
            return ActivityInfo.SCREEN_ORIENTATION_LOCKED;
        }

        int rotation = displayRotation();
        boolean naturalPortrait;
        if (rotation == Surface.ROTATION_0 || rotation == Surface.ROTATION_180) {
            naturalPortrait = orientation == Configuration.ORIENTATION_PORTRAIT;
        } else {
            naturalPortrait = orientation == Configuration.ORIENTATION_LANDSCAPE;
        }

        if (naturalPortrait) {
            if (orientation == Configuration.ORIENTATION_LANDSCAPE) {
                return rotation == Surface.ROTATION_90
                        ? ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
                        : ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE;
            }
            return rotation == Surface.ROTATION_0
                    ? ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
                    : ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT;
        }

        if (orientation == Configuration.ORIENTATION_LANDSCAPE) {
            return rotation == Surface.ROTATION_0
                    ? ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
                    : ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE;
        }
        return rotation == Surface.ROTATION_90
                ? ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
                : ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT;
    }

    private int displayRotation() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Display display = getDisplay();
            if (display != null) {
                return display.getRotation();
            }
        }
        return getWindowManager().getDefaultDisplay().getRotation();
    }

    private void loadRotationPreferences() {
        SharedPreferences preferences = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        rotationLocked = preferences.getBoolean(PREF_ROTATION_LOCKED, false);
        lockedOrientation = preferences.getInt(
                PREF_LOCKED_ORIENTATION,
                ActivityInfo.SCREEN_ORIENTATION_LOCKED);
        if (!isFixedOrientation(lockedOrientation)) {
            lockedOrientation = ActivityInfo.SCREEN_ORIENTATION_LOCKED;
        }
    }

    private void saveRotationPreferences() {
        SharedPreferences.Editor editor =
                getSharedPreferences(PREFS_NAME, MODE_PRIVATE).edit();
        editor.putBoolean(PREF_ROTATION_LOCKED, rotationLocked);
        editor.putInt(PREF_LOCKED_ORIENTATION, lockedOrientation);
        editor.apply();
    }

    private static boolean isFixedOrientation(int orientation) {
        return orientation == ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
                || orientation == ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
                || orientation == ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE
                || orientation == ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT
                || orientation == ActivityInfo.SCREEN_ORIENTATION_LOCKED;
    }

    private void copyAsset(File files, String name, boolean keepExisting) {
        File destination = new File(files, name);
        long assetLength = assetLength(name);

        if (keepExisting && destination.isFile()) {
            return;
        }
        if (destination.isFile() && assetLength >= 0
                && destination.length() == assetLength) {
            return;
        }

        File temporary = new File(files, name + ".tmp");
        try (InputStream input = getAssets().open(name);
             OutputStream output = new FileOutputStream(temporary)) {
            byte[] buffer = new byte[COPY_BUFFER];
            int count;
            while ((count = input.read(buffer)) > 0) {
                output.write(buffer, 0, count);
            }
        } catch (IOException error) {
            Log.e(TAG, "Unable to stage asset " + name, error);
            temporary.delete();
            return;
        }

        if (destination.exists() && !destination.delete()) {
            Log.e(TAG, "Unable to replace " + destination);
            temporary.delete();
            return;
        }
        if (!temporary.renameTo(destination)) {
            Log.e(TAG, "Unable to install " + destination);
            temporary.delete();
            return;
        }
        Log.i(TAG, "Staged " + name + " (" + destination.length() + " bytes)");
    }

    private long assetLength(String name) {
        /* APK assets are compressed by default, so openFd() is not available
         * for them.  AssetInputStream.available() reports the uncompressed
         * length, which is what the destination file will contain. */
        try (InputStream input = getAssets().open(name)) {
            return input.available();
        } catch (IOException ignored) {
            return -1;
        }
    }
}
