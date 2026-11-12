# Testing

The shipping GGER/NetWalk targets must build in Release and behave identically
with respect to the fixed VM, save format, high scores, controls, and rendering.
Android has its own touch controls and IME path, but the same game canvas and
save format.

## Build gates

From a clean tree:

```sh
cmake --preset linux-x86_64
cmake --build --preset linux-x86_64

cmake --preset windows-x64-mingw
cmake --build --preset windows-x64-mingw

cmake --preset linux-x86_64-debug
cmake --build --preset linux-x86_64-debug

cmake --preset windows-x64-mingw-debug
cmake --build --preset windows-x64-mingw-debug
```

The Android build gate uses the CMake presets and both default ABIs. The debug
preset needs no signing key; the release preset needs the four `NW_ANDROID_*`
signing variables:

```sh
export ANDROID_HOME="$HOME/Android/Sdk"
cmake --preset android-debug
cmake --build --preset android-debug

# Signed release gate.
export NW_ANDROID_KEYSTORE=/absolute/path/to/release.jks
export NW_ANDROID_KEYSTORE_PASSWORD='keystore-password'
export NW_ANDROID_KEY_ALIAS=netwalk
export NW_ANDROID_KEY_PASSWORD='key-password'
cmake --preset android
cmake --build --preset android

apk=build/android/NetWalk-release.apk
unzip -l "$apk" |
    grep -E 'lib/(arm64-v8a|x86_64)/(libmain|libSDL3)\.so|assets/(overlay\.nw|gm\.sf2|NETWALK0\.282)'
rm -rf /tmp/nw-apk && mkdir -p /tmp/nw-apk
unzip -q "$apk" -d /tmp/nw-apk
for so in /tmp/nw-apk/lib/*/*.so; do
    echo "$so"
    readelf -lW "$so" | awk '/LOAD/{print $NF}'
done
"$ANDROID_HOME/build-tools/35.0.0/zipalign" -c -P 16 -v 4 "$apk"
"$ANDROID_HOME/build-tools/35.0.0/apksigner" verify --verbose "$apk"
```

Every native `LOAD` segment must report `0x4000` alignment, `zipalign` must
succeed, and `apksigner` must verify the release signature. This checks the
16 KB page-size packaging used by newer Android devices.

A debug APK is signed with the standard Android debug key. The release APK is
signed with the configured `NW_ANDROID_*` key; a missing variable or missing
keystore fails both CMake configure and the Gradle `validateReleaseSigning`
task.

The Docker packaging gate builds the image and exercises debug, release, and
compile-only modes:

```sh
docker build --platform linux/amd64 \
    -f docker/Dockerfile -t netwalk-build:test .

# Debug-signed APK plus Linux, Windows, and PS5 releases.
NW_DOCKER_IMAGE=netwalk-build:test docker/build-releases.sh \
    --no-build-image --android-debug --ps5 \
    --out /tmp/nw-docker-debug

# Release-signed APK plus Linux, Windows, and PS5 releases.
NW_DOCKER_IMAGE=netwalk-build:test \
NW_ANDROID_KEYSTORE=/absolute/path/to/release.jks \
NW_ANDROID_KEYSTORE_PASSWORD='keystore-password' \
NW_ANDROID_KEY_ALIAS=netwalk \
NW_ANDROID_KEY_PASSWORD='key-password' \
    docker/build-releases.sh --no-build-image --android --ps5 --ffpkg \
    --out /tmp/nw-docker-release

# Compile-only path for a checkout without an overlay.
NW_DOCKER_IMAGE=netwalk-build:test docker/build-releases.sh \
    --no-build-image --ps5 --no-overlay \
    --out /tmp/nw-docker-compile-only
```

Check that the image reports JDK 17, `sdkmanager`, NDK `27.2.12479018`,
build-tools `35.0.0`, SDK CMake `3.22.1`, LLVM 20, `ps5-payload-sdk`, and the
prebuilt PS5 native tools. The debug and release output directories must
contain `linux-x86_64/`, `windows-x64-mingw/`, `android/NetWalk.apk`, and
`PPSA99877/`; the release output with `--ffpkg` must also contain
`PPSA99877.ffpkg`. PS5 and Android are opt-in, so a plain release build omits
both. The compile-only output must omit both desktop overlay files, the APK,
and the PS5 overlay, but still contains the PS5 title when `--ps5` is passed.
Verify the
release APK with `apksigner` and both APKs with `zipalign -c -P 16`. Validate
the PS5 title and `.ffpkg` as described below. Docker validates packaging only;
launch the APK on a device or emulator for the runtime checks below.

The host PS5 build gate needs the SDK and prepared native tooling:

```sh
platforms/ps5/tools/setup-native.sh
platforms/ps5/tools/build-native.sh --clean
python3 platforms/ps5/tools/validate-native.py dist/PPSA99877
python3 platforms/ps5/tools/make-ffpkg.py dist/PPSA99877
```

The Docker path uses the bundled toolchain; the same validator runs inside the
container during staging, and `--ffpkg` verifies the image it creates.
`validate-native.py` checks the FSELF container, ELF type, ABI, program headers,
process-parameter segment, title metadata, icon, and runtime module. It does not
prove that a console will launch the title. `make-ffpkg.py` checks the UFS2
superblock and verifies the image contents with its built-in structural reader;
it does not prove that a loader will mount or launch the image.

## Embedded overlay packaging

`--embed-overlay` changes where a desktop binary loads the overlay from; it does
not replace the runtime checks below. For either desktop output, verify:

- `overlay.nw` is absent while `gm.sf2` remains; `NETWALK0.282` is created by
  the game at runtime;
- the final 38 bytes still end in `=VS=`, and the trailer's `dbase` field equals
  the pre-append executable size (`nwembed.py` prints that size);
- running from a directory without a sidecar overlay reaches the normal game
  path;
- adding an external `overlay.nw` beside the binary still takes precedence.

Do not strip or Authenticode-sign an embedded Windows binary after embedding.

## Linux behavior checks

Run from the build output directory so the staged assets are found:

```sh
cd build/linux-x86_64
./NETWALK.EXE
```

Check:

- The intro and menu render in a centered 4:3 viewport.
- Mouse coordinates map correctly at 1x and 4x, including during resize.
- `F` enters and leaves borderless fullscreen; the viewport stays 4:3.
- `H` toggles between the high-resolution overlay and the original assets.
- The high-score name prompt accepts keyboard input.
- Exiting through the game's normal `EXIT` path rewrites `NETWALK0.282`.
- Relaunching preserves the high score.
- Deleting the live save and relaunching without a rebuild creates a fresh
  `NETWALK0.282` during startup and reaches the menu on that first launch.

The release build must not create `nwprof.csv`, PPM screenshots, asset dumps, or
other debug output during a normal session.

Controller testing needs physical hardware or the optional Linux virtual
joystick build:

```sh
cmake --preset linux-x86_64 -DNW_SDL_VIRTUAL_JOYSTICK=ON
cmake --build --preset linux-x86_64
```

A virtual device test is not a substitute for a real controller test.

A headless startup smoke can use SDL's offscreen video driver. It exercises
startup and save creation, but not rendering or input:

```sh
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy ./NETWALK.EXE
```

On 2026-10-31 the Linux release build ran this way for 20 seconds from a clean
directory and created a fresh `NETWALK0.282`. The same run kept the process
alive until the timeout, so it also covered the first-launch save repair path.

## Windows behavior checks

Run `build/windows-x64-mingw/netwalk.exe` on Windows, or under Wine when
available. Repeat the desktop checks above. Confirm:

- The window is resizable and aspect-locked.
- `F` fullscreen works.
- The executable icon on the desktop, taskbar, and title bar has a transparent
  background rather than a black or white square.
- XInput/DirectInput controllers and hot-plugging work.
- The save is written beside the executable and is not overwritten by a
  rebuild or update.
- The SoundFont loads and music/SFX are audible.

Wine testing does not exercise real Windows input or audio drivers fully.
On 2026-10-31 the release executable also started under Wine with SDL's dummy
video/audio drivers, stayed alive for 25 seconds, and created `NETWALK0.282`;
rendering, input, and audio under Wine remain unverified.

## Android checks

Install the debug APK on a device or emulator running Android 5.0 (API 21) or
newer with the matching ABI:

```sh
adb install -r build/android-debug/NetWalk-debug.apk
adb logcat -s NetWalk SDL
```

Check:

- The title and menu render, with the overlay assets visible rather than a
  black screen or a missing-sprite placeholder.
- The activity starts immersive full-screen: the status and navigation bars are
  hidden, and the 4:3 game image is centered and not stretched on a wide screen.
  A deliberate edge swipe may reveal the system bars temporarily.
- `EXIT` closes the activity through the normal shutdown path.
- `KEYBOARD` shows the IME, accepts text, and hides again on a second tap. While
  the IME is visible the activity resizes with the system bars still hidden; the
  game viewport and all four controls stay on screen. The soft keyboard's Enter
  key remains visible and delivers the game's Enter action instead of dismissing
  the IME.
- `LOCK` pins the current orientation and switches to the inverted button with
  the closed-padlock icon; tapping it again restores the normal button, the
  open padlock, and sensor rotation. Force-stop or relaunch the app and confirm
  the active lock state and pinned orientation are restored; unlock it, relaunch
  again, and confirm it follows the sensors.
- `L CLICK` starts in left-click mode with the left mouse button filled and the
  other button outlined. Tapping it fills the right mouse button and outlines
  the left; tapping again returns to the left-filled icon. Touch input inside
  the viewport follows the selected mode, and a physical mouse or pen keeps its
  own buttons. Holding the control flips the icon to the opposite mode while
  the finger stays down, a second finger clicking in the viewport uses that
  inverted mode, and releasing the control restores the selected mode without
  toggling it.
- Taps and drags inside the viewport move the game pointer and use the selected
  click mode. Taps on the buttons never reach the game.
- The activity follows portrait and both landscape directions while unlocked,
  keeps the 4:3 game viewport centered, and stays in the locked orientation
  after `LOCK` is tapped.
- The controls use `EXIT` and `KEYBOARD` on one side and the lock plus the
  click-mode toggle on the other. Names are drawn under the icons only in a
  tablet-sized layout, where they read `EXIT`, `KEYS`, `LOCK`/`LOCKED`, and
  `LEFT`/`RIGHT`.
- No in-app back button is present. The platform navigation controls are hidden
  by immersive full-screen but remain available through the system edge gesture
  where the device provides one.
- The first launch creates `NETWALK0.282` in internal storage and reaches the
  menu.
- Relaunching preserves high scores, and reinstalling or updating the APK does
  not overwrite an existing save.
- The activity returns from background, rotates between both landscape
  directions, and keeps the game view centered.
- The launcher displays the generated NetWalk application icon with its
  transparent background; the adaptive foreground keeps clear padding inside
  Android's 66 dp safe-zone circle on masked launchers.
- No fatal `NetWalk` or SDL error appears in logcat.

The runtime checks below require an Android device or emulator with the
matching ABI.

### Android validation status

Validated on 2026-11-01 with an API 28 x86_64 emulator (software TCG, no KVM)
at 1920x1080. The four-control debug build was reinstalled and rechecked for
the control layout, click-mode toggle, closed/open padlock states, rotation
lock, and IME resize. The immersive full-screen debug build was then checked
for launch-time bar hiding, IME resize with the bars still hidden, and
orientation-lock behavior. The signed x86_64 release APK was installed and
launched with the same control set after the full-screen change. Passed:

- The title, menu, and game screens render with the overlay assets; the 4:3
  image is centered and not stretched in portrait, landscape, and reverse
  landscape.
- The activity starts full-screen with the status and navigation bars hidden.
  The system bars stay hidden during gameplay and while the IME is visible.
- Tapping the playfield rotates a tile through the normal left-click path, and
  the same tap in `R CLICK` mode rotates it through the right-click path.
- `EXIT` ends the activity through the normal shutdown path.
- `KEYBOARD` shows and hides the IME. While the IME is visible the activity
  resizes, the system bars stay hidden, the game viewport stays centered, and
  all four controls remain reachable. Tapping or clicking anywhere outside the
  keyboard in the game window dismisses it, and the tap still reaches the game
  or the control under it.
- The soft keyboard's Enter key remains visible, does not dismiss the IME, and
  delivers the game's carriage-return input action.
- A portrait window shows one wide row of four buttons in the bottom black
  bar (or the top bar when the bottom is unusable), outside the game view.
  A wide landscape window shows two stacked buttons in each side bar:
  `EXIT`/`KEYBOARD` left and `LOCK`/`L CLICK` right. Both bar layouts leave the
  game view completely uncovered.
- Only on a near-4:3 window, where no bar can hold the controls, does the card
  appear: only a small top tab is visible during play, and tapping it slides
  down a row of square buttons with the card sized to the row plus its padding.
  Tapping outside the card or waiting about four seconds closes it.
- The square is capped at about 18 mm of physical display, measured from the
  reported density, with a tighter 64 dp ceiling for phone side columns. A
  tablet therefore gets a phone-sized control instead of a slab; the icon is a
  little under half the square. Phones stay icon-only, while a tablet adds a
  readable short name under the icon.
- Hot rotation between portrait and landscape switches between the bar and
  card layouts and keeps the game view centered.
- `LOCK` pins the current orientation and switches to the inverted button with
  the closed-padlock icon; tapping it again restores the normal button, open
  padlock, and sensor rotation. The lock state and exact orientation survived
  force-stop/relaunch in reverse landscape and reverse portrait, and rotating
  the emulator while locked did not change the display. Relaunching after
  unlocking restored sensor rotation.
- `L CLICK` starts with the selected left mouse button filled and the other
  button outlined; switching to `R CLICK` fills the right button and outlines
  the left. Tapping again returns to `L CLICK`. Touch input follows the selected
  mode while a physical mouse or pen keeps its own buttons. A long press inverts
  the effective mode for as long as the control is held, so a second finger can
  click with the opposite button; releasing it keeps the previous selection.
- The controls render the generated exit, keyboard, closed-padlock,
  open-padlock, left-mouse, and right-mouse sprites from the embedded atlas;
  labels are baked into the tablet variants. The pull-down tab and card use
  matching high-resolution panels and chevrons. Control taps are consumed
  before game input. No in-app back button is present.
- A first launch with no save creates `NETWALK0.282` in internal storage and
  reaches the title/menu.
- Reinstalling the APK keeps the existing save and does not re-copy unchanged
  `overlay.nw` or `gm.sf2` assets.
- Backgrounding and resuming the activity redraws the game.
- The launcher displays the generated NetWalk application icon from the final
  release APK, with transparent corners and no crop from the adaptive mask.
- Every native `LOAD` segment in both ABIs is aligned to `0x4000`, and
  `zipalign -c -P 16` passes for the debug and signed release APKs.
- `apksigner verify` accepts the release APK signed with a test release key.
- logcat shows no fatal `NetWalk` or SDL errors in the app process during these
  checks.

Additional checks:

- Run the runtime checks on an arm64 device or emulator; the APK contains
  `arm64-v8a`.
- Check full-screen behavior on Android 11 (API 30) or newer, including the
  `WindowInsetsController` path and Android 15 edge-to-edge enforcement.
- Enter text in the high-score name prompt through the IME.
- Check audible audio output, hardware keyboard, and gamepad input.
- Check safe-area/cutout placement on a real device.
- Check high-resolution overlay assets.
- Check real-device performance and thermals.

## iOS checks

The iOS artifact is optional and built only inside the Docker image with a
user-supplied Apple SDK. It needs an overlay:

```sh
docker/build-releases.sh --ios --ios-sdk /path/to/Xcode.xip \
    --overlay /path/to/overlay.nw
```

The release directory contains only the IPA, so unpack it for the static
checks (run inside the image or with LLVM 20 tools):

```sh
tmp=$(mktemp -d)
unzip -q dist/docker-releases/ios/NetWalk.ipa -d "$tmp"
app="$tmp/Payload/NetWalk.app"
llvm-objdump --macho --private-headers "$app/NetWalk" |
    grep -E 'LC_BUILD_VERSION|platform 2|minos 13.0|__PAGEZERO|LC_CODE_SIGNATURE'
unzip -l dist/docker-releases/ios/NetWalk.ipa |
    grep -E 'Payload/NetWalk.app/(NetWalk|Info.plist|overlay.nw|gm.sf2|AppIcon)'
python3 - "$app/Info.plist" <<'PY'
import plistlib, sys
p = plistlib.load(open(sys.argv[1], "rb"))
assert p["CFBundleIdentifier"] == "com.gamos.netwalk"
assert p["MinimumOSVersion"] == "13.0"
assert p["UIRequiresFullScreen"] is True
assert p["UIFileSharingEnabled"] is True
assert p["LSSupportsOpeningDocumentsInPlace"] is True
assert "UIInterfaceOrientationPortrait" in p["UISupportedInterfaceOrientations~ipad"]
PY
```

The game executable must report an arm64 Mach-O, `platform ios`, `minos 13.0`,
the linker-default `__PAGEZERO` of `0x100000000`, and (when signing is
enabled) an `LC_CODE_SIGNATURE` load command. The bundle must contain the
executable, `Info.plist`, `overlay.nw`, `gm.sf2`, and the generated icons.
`llvm-objdump --macho --private-headers` also shows `__TEXT` at
`0x100000000`; that is what makes `nw_guest_base` non-zero and puts the game
in the 64-bit guest window described in
[PORTABLE_64BIT.md](PORTABLE_64BIT.md).

Device runtime checks:

- The game reaches the title and menu with overlay assets visible, in both
  portrait and landscape. The fixed 4:3 image is letterboxed, never stretched
  or cropped, and the bars are symmetric. `main()` no longer overrides
  `SDL_HINT_ORIENTATIONS`: the launch orientation comes from the bundle's
  `Info.plist` (`UIRequiresFullScreen` is set), so SDL's window frame and the
  UIKit view agree.
- The game writes no run log or crash file. Errors from the iOS entry point
  such as a missing bundled resource still go through `SDL_Log` to the unified
  log, but the engine has no per-stage trace and no fatal-signal reporter.
  Use Xcode's debugger or the system crash report when a device fault needs
  investigation.
- The Windows high proxy runs at render scale 4 and uses the scaled sprite
  blitter. The iOS build ships at scale 1, which reaches the row-copy
  functions instead; exercise that path with `NW_RENDER_SCALE=1` when
  validating the proxy.
- The same four virtual controls as Android are drawn directly in the
  letterbox bars when they fit there, and on the pull-down card otherwise:
  `EXIT`, `KEYBOARD`, `LOCK`/`UNLOCK`, and `L CLICK`/`R CLICK`.
  - `EXIT` leaves the game through the normal shutdown path.
  - `KEYBOARD` shows and hides the iOS software keyboard. It stays hidden at
    launch; tapping a text field's on-screen control is what opens it. Tapping
    anywhere outside the keyboard in the game window dismisses it, and the tap
    still reaches the game or the control under it.
  - `LOCK` pins the current orientation; the button switches to its inverted
    active style with a closed padlock and the `LOCKED` label. Tapping it again
    restores the normal dark button, open padlock, and `LOCK` label and
    restores all orientations from `Info.plist`.
    Rotation between portrait and either landscape direction still works
    while unlocked. While locked, rotating the device by 180 degrees must not
    change the presentation. Lock the app, force-quit it, and relaunch: the
    control comes back locked and inverted, the app starts in the orientation
    that was locked even if the device is now held the other way, and the game
    view is not cropped or offset. With the app locked, turn the device to
    another orientation and tap the lock button: the app immediately follows
    the device without needing another rotation. Unlock, relaunch again, and
    confirm it follows the sensors. The choice lives in `rotation.cfg` in the
    `Gamos/NetWalk` preference directory, not in `NETWALK0.282`.
  - In portrait, the buttons sit in the bottom black bar as one wide row; in
    wide landscape they split into two columns in the side bars. The game view
    stays fully visible in both bar layouts. On a near-4:3 window the controls
    move to the pull-down card: only a small top tab is visible while closed,
    tapping it slides down a row of square buttons with the card sized to the
    row plus its padding, and tapping outside the card or waiting about four
    seconds closes it.
  - The buttons are square, not a full-width toolbar, and their side is capped
    at about 18 mm of physical display so an iPad does not get oversized
    controls. The icon is a little under half the square. Labels are drawn only
    in a tablet-sized layout, where a readable short name fits under the icon
    (`EXIT`, `KEYS`, `LOCK`/`LOCKED`, `LEFT`/`RIGHT`); a phone-sized layout stays
    icon-only.
  - `L CLICK` starts in left-click mode with the left mouse half filled. Tapping
    it fills the right half (and changes the label to `RIGHT` where labels are
    shown); tapping again returns to left-click mode. Holding it past the tap
    window inverts the icon and the click mode while the control stays down,
    without changing the selection; a second finger can click in the viewport
    with the opposite button meanwhile. A physical mouse or pen keeps its own
    buttons.
- A tap rotates a tile through SDL's touch-to-mouse bridge, delivered as the
  selected click mode. Touches on a virtual control are consumed before the
  game sees them.
- Send the app to the Home screen and rotate the iPad several times before
  returning. The first tap after returning still lands where it is aimed, and
  the bar controls remain clickable. SDL keeps its window size synced to the
  live view bounds for touches and safe-area changes, redraws immediately when
  the safe area changes, and forgets a synthetic touch-to-mouse press that the
  background transition interrupted.
- `EXIT` leaves the game through the normal shutdown path. A first launch
  creates `NETWALK0.282` in `SDL_GetPrefPath("Gamos", "NetWalk")`; relaunching
  preserves high scores.
- Audio is audible and the launcher icon has a transparent background.

Ad-hoc signing should leave `_CodeSignature/CodeResources` in the bundle and
`LC_CODE_SIGNATURE` in the executable. For an Apple signature, pass a
development `.p12` and a `.mobileprovision`; the build derives the entitlements
from the profile with `tools/mobileprovision.py` and embeds
`embedded.mobileprovision`. Install the resulting IPA on a device to validate
the signature and runtime behavior.

As of 2026-11-12 the Docker toolchain, the full game, and ad-hoc signing have
built and passed the static checks above. Device launch and runtime validation
remain pending because no Apple signing identity/device was available.

## PS5 checks

The native package is under `dist/PPSA99877/`. Copy the whole directory to the
console; do not copy only `eboot.bin`. The package contains the runtime module
and title metadata. Alternatively, deploy `dist/PPSA99877.ffpkg` to a loader
that mounts UFS2 images; the image contains the same files and external overlay.

Before deploying the image, confirm it reports the UFS2 magic and contains the
expected tree:

```sh
file dist/PPSA99877.ffpkg
python3 platforms/ps5/tools/make-ffpkg.py --verify dist/PPSA99877.ffpkg dist/PPSA99877
```

The verifier reads the UFS2 superblock and directory tree directly; it does not
need a mount, root, or a .NET runtime.

The image uses 4096-byte UFS sectors. That matches ShadowMountPlus's LVD UFS
default; when using its MD backend, set `md_ufs_sector_size=4096` or an
`image_sector=PPSA99877.ffpkg:4096` override.

Console checks:

- The loader mounts `PPSA99877.ffpkg` and launches the title with the overlay
  and SoundFont visible at `/app0`.
- The title launches and presents the intro and menu.
- L3 and Select/Options open the system keyboard on press edges.
- Keyboard input reaches the typed-token field and the high-score name prompt.
- R3 toggles high-resolution and original assets without changing the 4:3
  output or pointer mapping.
- The selected save path is one of the documented candidates and is writable.
- High scores persist across a title relaunch and across a console reboot.
- Migration from a legacy save directory works.
- Startup displays no debug overlay or boot trace.

## Asset tools

The extraction and repacking tools operate on the original `NETWALK.EXE` and
`overlay.nw`, not on a live save. A minimal round trip:

```sh
python3 tools/nwextract.py /path/to/NETWALK.EXE -o extracted/NETWALK --assets
python3 tools/nwrepack.py extracted/NETWALK/overlay.nw \
    --pack edited_assets -o repacked
```

For a high-resolution pack, follow [assets/EXTRACTION.md](assets/EXTRACTION.md)
and run a debug build with `NW_DEBUG_ASSET_PACK` when testing an external pack.
The embedded overlay path does not require that variable.

## Performance checks

`tools/nwbench.py` measures frame pacing with the debug profiler. It launches a
debug build with `NW_DEBUG_PROFILE` set and copies the staged assets into a
scratch directory:

```sh
python3 tools/nwbench.py --exe build/linux-x86_64-debug/NETWALK.EXE \
    --native --seconds 5 --scale 4
```

For a direct run use an explicit profile path; the release build does not emit a
profile:

```sh
NW_DEBUG_PROFILE=/tmp/netwalk-profile.csv \
  ./build/linux-x86_64-debug/NETWALK.EXE
```

The tool does not inject input. An idle run may return a `startup-compose`
report rather than presented-frame pacing; see
[PERFORMANCE.md](PERFORMANCE.md) for the expected 4x workload and 4:3
presentation path.

## Regression notes

- Do not replace the low-address `VirtualAlloc` path with a plain `mmap`; the
  32-bit guest pointer fields require addresses below 4 GiB.
- Do not change the save fallback order without a hardware test and a migration
  plan.
- Do not move the `.got`/`.got.plt` sections out of the RELRO segment in the
  PS5 linker script; the native converter depends on that ordering.
- Do not add routine file logging. Debug output is opt-in and compiled out of
  release.
