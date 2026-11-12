# Docker release builds

The Docker path builds the GGER/NetWalk Linux and Windows release presets in an
Ubuntu 24.04 image and stages the resulting playable directories. Native PS5,
Android, and iOS packaging are opt-in. The default Docker image (the
`netwalk-ps5` build stage) bundles a pinned `ps5-payload-dev` SDK, LLVM 20, and
the native PS5 converter/runtime/makefs tools. Pass `--ps5-sdk` and
`--ps5-llvm` together to mount an external toolchain instead.

The Linux and Windows Docker release path is exercised by default, and the
native PS5 output is exercised with `--ps5`. Android packaging is exercised
with the opt-in flags. The iOS game is exercised with `--ios` and a
user-supplied Apple SDK.

## Quick start

From the repository root:

```sh
docker/build-releases.sh
```

Linux and Windows are built by default. Add `--ps5` for the native PS5 title:

```sh
docker/build-releases.sh --ps5
```

Android is skipped; to include it, export the release signing variables and
pass `--android`:

```sh
export NW_ANDROID_KEYSTORE=/absolute/path/to/release.jks
export NW_ANDROID_KEYSTORE_PASSWORD='keystore-password'
export NW_ANDROID_KEY_ALIAS=netwalk
export NW_ANDROID_KEY_PASSWORD='key-password'

docker/build-releases.sh --android
```

Without a release key, build a debug-signed APK with
`docker/build-releases.sh --android-debug`.

iOS is opt-in and needs an Apple SDK supplied by the user; nothing Apple-specific
is installed on the host. Point `--ios-sdk` (or `NW_IOS_SDK`) at an extracted
`iPhoneOS.sdk`, an `Xcode.app` tree, or a downloaded `.xip`:

```sh
# Full game (needs an overlay).
docker/build-releases.sh --ios --ios-sdk /path/to/Xcode.xip \
    --overlay /path/to/overlay.nw
```

The container builds compiler-rt's iOS builtins, compiles the game with
Clang/LLD, and stages only `ios/NetWalk.ipa`; the intermediate `NetWalk.app`
stays in the build directory. Extracted SDKs and builtins are cached in
`NW_IOS_SDK_CACHE` (default
`~/.cache/netwalk/ios-sdk`). The game bundle contains `overlay.nw`, `gm.sf2`,
and the icons; `platforms/ios/ios_main.c` copies the data files into the app's
preference directory at launch because the bundle itself is read-only. The
game keeps the linker-default 4 GiB `__PAGEZERO`, so the image loads above
4 GiB inside the 64-bit guest window.

The app is unsigned by default. Signing uses the `rcodesign` bundled in the
image and is opt-in: `--ios-sign adhoc` writes an ad-hoc signature, while
`--ios-sign apple --ios-sign-p12 ... [--ios-sign-password-file ...]
[--ios-provisioning-profile ...] [--ios-entitlements ...]` signs with an Apple
certificate. With a provisioning profile but no `--ios-entitlements`, the
build extracts the profile's Entitlements dictionary, expands a wildcard App ID
to the concrete bundle identifier, and verifies that the profile covers the
app before signing. Only a correctly signed and provisioned app installs on a
stock device.

If `assets/overlay.nw` is absent, pass an original `NETWALK.EXE` or an existing
overlay. A compile-only build can omit the overlay entirely, and desktop
executables can carry the overlay internally. Android needs an overlay, so it
cannot be combined with `--no-overlay`:

```sh
docker/build-releases.sh /path/to/NETWALK.EXE
docker/build-releases.sh --extract-assets /path/to/NETWALK.EXE
docker/build-releases.sh --overlay /path/to/overlay.nw
docker/build-releases.sh --no-overlay
docker/build-releases.sh --ps5
docker/build-releases.sh --ps5 --no-overlay
docker/build-releases.sh --android-debug
docker/build-releases.sh --skip-android
docker/build-releases.sh --embed-overlay
docker/build-releases.sh --overlay /path/to/overlay.nw --embed-overlay
docker/build-releases.sh --ffpkg
```

Passing the reference executable positionally is equivalent to
`--extract-assets`; both use its lowercase filename for the Linux and Windows
release executables. `NETWALK.EXE` therefore produces `netwalk.exe` in both
desktop directories, while a reference named
`NetWalk_v1.51_Arsenal eng.exe` produces `netwalk_v1.51_arsenal eng.exe`.

By default this:

1. builds the `netwalk-build:latest` image, including the pinned PS5 SDK,
   LLVM 20, and native PS5 tools (Docker's layer cache makes repeat builds
   cheap; use `--no-build-image` to trust an existing tag);
2. copies the current working tree into a writable container directory;
3. builds `linux-x86_64` and `windows-x64-mingw`; `--ps5` adds the native
   `PPSA99877` title, and `--android` or `--android-debug` adds an APK;
4. writes the release directories under `dist/docker-releases/`.

The output layout is:

```text
dist/docker-releases/
  linux-x86_64/
    NETWALK.EXE              # input filename lowercased when supplied
    overlay.nw               # omitted by --no-overlay and --embed-overlay
    gm.sf2
  windows-x64-mingw/
    netwalk.exe              # input filename lowercased when supplied
    overlay.nw               # omitted by --no-overlay and --embed-overlay
    gm.sf2
  android/                   # only with --android or --android-debug
    NetWalk.apk              # signed release, or debug with --android-debug
  PPSA99877/                 # only with --ps5 or an option that implies it
    eboot.bin
    overlay.nw               # PS5 always uses an external overlay
    ...
  PPSA99877.ffpkg            # only with --ffpkg, built in the container
```

`dist/` is ignored by git. `--no-overlay` omits `overlay.nw` from both desktop
directories and, when PS5 is built, from the PS5 title; it cannot be combined
with `--android` or `--android-debug`.
`--embed-overlay` omits it from the Linux and Windows directories while leaving
the PS5 overlay external and the APK's packaged overlay intact. When built, the
staged Android APK is verified in the output directory with
`zipalign -c -P 16` and, for a release build, `apksigner verify`.

## GitHub Actions

[`.github/workflows/docker-build.yml`](../.github/workflows/docker-build.yml)
builds the image and compile-checks the Linux and Windows releases on every
push and pull request. Native PS5 is opt-in on manual dispatch with the
`build_ps5` input. The overlay is not tracked, so CI uses
`--no-overlay --skip-android` and uploads compile-only artifacts; the
executables, and the PS5 title when requested, still expect an external
`overlay.nw` at runtime. If the workflow's checkout contains
`assets/overlay.nw`, it stages the full desktop release and, when requested, the
PS5 title instead. The workflow skips Android because no release signing key is
available. Run it locally with the overlay and signing variables for a playable
APK.

## Options

```sh
# Build Linux and Windows. PS5, Android, and iOS are skipped by default.
docker/build-releases.sh

# Add the native PS5 title.
docker/build-releases.sh --ps5

# Build the signed Android release APK.
docker/build-releases.sh --android

# Build a debug-signed APK instead of the release-signed APK.
docker/build-releases.sh --android-debug

# Build the NetWalk game (needs an overlay).
docker/build-releases.sh --ios --ios-sdk /path/to/Xcode.xip \
    --overlay /path/to/overlay.nw

# Build and sign the game with a development certificate.
docker/build-releases.sh --ios --ios-sdk /path/to/Xcode.xip \
    --overlay /path/to/overlay.nw --ios-sign apple \
    --ios-sign-p12 /path/to/development.p12 \
    --ios-sign-password-file /path/to/p12-password \
    --ios-provisioning-profile /path/to/profile.mobileprovision

# Explicitly skip the Android APK, PS5 title, or iOS app (all defaults).
docker/build-releases.sh --skip-android
docker/build-releases.sh --skip-ps5
docker/build-releases.sh --skip-ios

# Use an already-built image.
docker/build-releases.sh --no-build-image

# Extract a missing overlay from the original executable and name the desktop
# outputs after its lowercase filename. --extract-assets is equivalent.
docker/build-releases.sh /path/to/NETWALK.EXE
docker/build-releases.sh --extract-assets /path/to/NETWALK.EXE

# Use an external overlay.nw, or compile without one.
docker/build-releases.sh --overlay /path/to/overlay.nw
docker/build-releases.sh --no-overlay

# Embed the overlay in the Linux and Windows executables.
docker/build-releases.sh --embed-overlay
docker/build-releases.sh --overlay /path/to/overlay.nw --embed-overlay
docker/build-releases.sh --extract-assets /path/to/NETWALK.EXE --embed-overlay

# Build PS5 and create a mountable UFS2 image from the staged title.
docker/build-releases.sh --ffpkg
docker/build-releases.sh --ps5 --ffpkg

# Change the output location or parallel job count.
docker/build-releases.sh --out /tmp/netwalk-releases --jobs 8

# Use an external SDK/LLVM instead of the image-bundled toolchain.
docker/build-releases.sh \
    --ps5-sdk /path/to/ps5-payload-sdk \
    --ps5-llvm /path/to/llvm20

# Fail if PS5 cannot be built (for example, with a lean netwalk-base image).
docker/build-releases.sh --require-ps5
```

`PS5_SDK` and `PS5_LLVM` override the bundled toolchain when `--ps5` is
selected and must be set together. The wrapper also honors `NW_DOCKER_IMAGE`,
`NW_DOCKER_OUT`, `NW_DOCKER_PLATFORM`, `NW_JOBS`, and `DOCKER`. The Android
release key is supplied through `NW_ANDROID_KEYSTORE`,
`NW_ANDROID_KEYSTORE_PASSWORD`, `NW_ANDROID_KEY_ALIAS`, and
`NW_ANDROID_KEY_PASSWORD`; it is required only when `--android` is selected.
iOS builds read the default SDK from `NW_IOS_SDK`, the deployment target from
`NW_IOS_MIN`, and the writable extraction/builtins cache from
`NW_IOS_SDK_CACHE`. Optional signing defaults come from `NW_IOS_SIGN_MODE`,
`NW_IOS_SIGN_P12`, `NW_IOS_SIGN_P12_PASSWORD_FILE`,
`NW_IOS_SIGN_ENTITLEMENTS`, `NW_IOS_SIGN_PROVISIONING_PROFILE`, and
`NW_IOS_SIGN_TEAM_ID`; the bytes of the certificate, password file,
entitlements, and profile are mounted read-only, never copied into the image.

## Assets

The release stage requires:

| Path | Source |
| --- | --- |
| `assets/overlay.nw` | Extracted from the original `NETWALK.EXE`; ignored by Git |
| `third_party/windowsgs/gm.sf2` | Tracked General MIDI SoundFont |

No save file is required. The game creates `NETWALK0.282` on first run in the
writable save directory.

The overlay can come from one of three sources:

- the checkout's `assets/overlay.nw` by default;
- `--overlay PATH`, which stages the supplied file even if a checkout copy
  exists;
- a positional `EXE` or `--extract-assets EXE`, which extracts a missing overlay
  inside the container. Its lowercase filename is used for the Linux and
  Windows release executables.

The three source options are mutually exclusive. If the overlay is absent and
none of them is given, the wrapper stops with an error. The container then
builds from a writable source copy; the host checkout is mounted read-only and
is not changed.

`--no-overlay` deliberately omits `overlay.nw`. The staged executables search
for an external overlay beside themselves at runtime, so this mode is useful
for compile checks but cannot be run or validated until an overlay is added.
The output still contains the SoundFont. Android packages the overlay inside
the APK, so `--no-overlay` cannot be combined with `--android` or
`--android-debug`.

`--embed-overlay` is separate from the source choice. It appends the resolved
overlay to the Linux and Windows executables with
[`tools/nwembed.py`](../tools/documentation/nwembed.md) and removes
`overlay.nw` from those desktop output directories. It can be combined with a
positional executable, `--overlay`, or `--extract-assets`, but not with
`--no-overlay`. The PS5 title always keeps `overlay.nw` external, and the
Android APK always carries its own packaged `assets/overlay.nw`. `gm.sf2` is
not embedded and remains beside every desktop executable; saves are created at
runtime.

`--ffpkg` runs inside the container. It converts the staged `PPSA99877/`
directory into a mountable UFS2 image, `PPSA99877.ffpkg`, using
`platforms/ps5/tools/make-ffpkg.py` and the pinned native `makefs` in the image.
It enables the PS5 build and requires an external overlay, so it cannot be
combined with `--skip-ps5` or `--no-overlay`. See
[`../docs/PS5.md`](../docs/PS5.md#package-as-ffpkg).

Only the overlay comes from the executable. The SoundFont is not embedded in
`NETWALK.EXE` and must be present separately. See
[`../docs/BUILD.md`](../docs/BUILD.md#assets) for the manual extraction command.

## The image

`docker/Dockerfile` is based on `ubuntu:24.04` and provides:

- GCC/G++, CMake, Ninja, Python 3, and the Python requirements;
- MinGW-w64 C/C++ for the Windows build;
- JDK 17, Android SDK platform 35, build-tools 35.0.0, NDK 27.2.12479018, and
  SDK CMake 3.22.1 for the Android APK;
- the X11, PulseAudio, and zlib development packages used by Linux and SDL;
- LLVM 20, a pinned `ps5-payload-dev` SDK, the native `ps5-native-tool`,
  `libc.prx`, and a pinned native `makefs` for the PS5 title and `.ffpkg`.

The image is intended for `linux/amd64`; the Windows and PS5 compiler drivers
are x86-64 host tools. The Android SDK/NDK versions match
`platforms/android/app/build.gradle` and
[docs/BUILD.md](../docs/BUILD.md#android). The Docker wrapper passes
`--platform` explicitly.

Ubuntu 24.04 rather than Debian Bookworm is required because the checked-in
static PulseAudio archives use glibc 2.38+ `__isoc23_*` entry points.
Bookworm's glibc 2.36 cannot link the Linux release. Ubuntu 24.04 provides glibc
2.39, which also matches the common WSL2/Ubuntu 24.04 runtime. If the vendored
PulseAudio archives are rebuilt with an even newer toolchain, keep the image
glibc at least as new as that build environment.

The Linux executable links glibc dynamically. A release built in this image
therefore requires glibc 2.39 or newer at runtime, including WSL2 on Ubuntu
24.04. Building on an older glibc would lower the requirement, but the static
PulseAudio archives currently set a 2.38 floor.

The Android command-line tools, platform, build-tools, and NDK are downloaded
from Google's repository during the image build, so the first image build needs
network access. The PS5 SDK, LLVM 20 packages, native converter, `libc.prx`, and
`makefs` are also downloaded or built while creating the image; the Dockerfile
pins the SDK version and checksum and retains the fetched source trees. The
`ps5-payload-dev` SDK, generated `libc.prx`, and `makefs` have their own
licenses (GPL and BSD); review and comply with them if you redistribute the
image. PS5 builds from the finished image do not need network access. Linux and
Windows builds do not need network access after the image is built.

Build `--target netwalk-base` for a lean Linux/Windows/Android image without
the PS5 toolchain. The default final stage is `netwalk-ps5`; the default image
tag is `netwalk-build:latest`.

## How the build works

`docker/container-build.sh` is the image entry point. It:

- copies the mounted source tree to `/work`, excluding `.git`, `build/`,
  `dist/`, `.tmp/`, the Gradle/NDK build directories, and common generated
  files;
- resolves the overlay from the checkout, `--overlay`, or `--extract-assets`,
  or omits it for `--no-overlay`, then checks the required assets;
- configures and builds the release presets with `cmake --preset` and
  `cmake --build --preset`;
- when Android is requested, builds the APK with the `android` or
  `android-debug` preset, checks 16 KB zip alignment, verifies the release
  signature with `apksigner`, and copies it to `/out/android/NetWalk.apk`;
- copies only the staged artifact files to `/out`;
- removes previously staged target directories first, so a skipped PS5 or
  Android build or a changed overlay mode cannot leave stale files beside the
  new release;
- for `--embed-overlay`, appends the overlay to the staged Linux and Windows
  executables and verifies that no external overlay remains in those
  directories;
- when `--ps5` is requested, runs `platforms/ps5/tools/build-native.sh` and
  stages `dist/PPSA99877` using the image-bundled or mounted PS5 toolchain;
- with `--ffpkg`, runs `platforms/ps5/tools/make-ffpkg.py` and writes
  `PPSA99877.ffpkg` beside the staged title;
- deletes the copied source tree and intermediate build directory on exit,
  including failure paths.

The wrapper removes a previous `.ffpkg` before the container runs, so a
release directory always reflects the current `--ffpkg` choice.

The host wrapper mounts the checkout read-only at `/src`, so the host build tree
is not modified. It also passes the host uid/gid to the container so the output
files remain owned by the invoking user.

## Verification boundary

This is a packaging/build helper, not a test runner. It verifies the packaged
output and signatures, including Android 16 KB zip alignment and structural
validation of the `.ffpkg` image. Use
[docs/TESTING.md](../docs/TESTING.md) for the runtime checks.
