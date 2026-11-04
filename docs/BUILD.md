# Building GGER

GGER builds the NetWalk game through one top-level CMake entry point. Linux,
Windows, PS5, and Android packaging use the checked-in CMake presets. The
Android presets invoke the Gradle wrapper, which in turn invokes CMake with the
NDK toolchain. Release is the default build type, and debug builds are separate
presets or Gradle variants. All four targets use the same recovered source tree
and the same asset overlay.

## Prerequisites

- CMake 3.21 or newer
- Ninja
- Python 3
- A C11 compiler:
  - Linux: GCC or Clang and the static PulseAudio archives in `third_party/`
  - Windows: MinGW-w64 GCC (`x86_64-w64-mingw32-gcc`); SDL3 also needs the
    matching `x86_64-w64-mingw32-g++`
  - PS5: the `ps5-payload-dev` SDK and its Prospero LLVM toolchain
  - Android: Android SDK platform 35, build-tools 35.0.0, NDK
    `27.2.12479018`, the SDK CMake `3.22.1` package, and JDK 17 or newer
- Optional: Wine for Windows smoke tests on Linux

The vendored dependencies are in `third_party/`:

- `SDL3-3.4.18/`: SDL3 source used by all targets
- `tinysoundfont/`: TinySoundFont
- `windowsgs/gm.sf2`: General MIDI SoundFont
- `pulse-static/`: static PulseAudio libraries for the Linux build

## Assets

`assets/overlay.nw` contains the original game data and is generated from the
original Windows executable. It is ignored by Git because it is large game
data. Extract it once into the source tree before staging a runnable build:

```sh
python3 tools/nwextract.py /path/to/NETWALK.EXE -o extracted/NETWALK
cp extracted/NETWALK/overlay.nw assets/overlay.nw
```

No save file is needed to build or run the game. The game creates
`NETWALK0.282` on first run in its writable save directory. The release staging
helpers require `third_party/windowsgs/gm.sf2`.

A compile can proceed without the overlay, but the resulting package will not
run correctly. The Docker release wrapper checks the required files before it
builds and refuses to continue when one is missing unless `--no-overlay` is
given. That mode stages the binaries and supporting files without `overlay.nw`;
the game then searches for an external copy beside the executable. Such a
package is compile-verified but not runnable until an overlay is supplied.

`platforms/ps5/tools/build-native.sh` accepts the same `--no-overlay` switch for
a compile-only native title.

## Release and debug

Release is selected automatically when no `CMAKE_BUILD_TYPE` is supplied. The
release presets explicitly set `Release`; the debug presets set `Debug` and
`NW_DEBUG_TOOLS=ON`.

```sh
# Linux release
cmake --preset linux-x86_64
cmake --build --preset linux-x86_64

# Linux debug (symbols, no optimization, optional diagnostics)
cmake --preset linux-x86_64-debug
cmake --build --preset linux-x86_64-debug

# Windows release / debug
cmake --preset windows-x64-mingw
cmake --build --preset windows-x64-mingw
cmake --preset windows-x64-mingw-debug
cmake --build --preset windows-x64-mingw-debug

# PS5 release / debug (see docs/PS5.md for tooling setup)
cmake --preset ps5-native
cmake --build --preset ps5-native
cmake --preset ps5-native-debug
cmake --build --preset ps5-native-debug

# Android debug APK (CMake drives the Gradle wrapper and the NDK)
cmake --preset android-debug
cmake --build --preset android-debug

# Android signed release APK
cmake --preset android
cmake --build --preset android
```

`NW_DEBUG_TOOLS` is off by default, including for builds configured without a
preset. The debug presets enable it; release builds compile the diagnostic hooks
out and ignore the `NW_DEBUG_*` variables.

## Linux

```sh
cmake --preset linux-x86_64
cmake --build --preset linux-x86_64
./build/linux-x86_64/NETWALK.EXE
```

The output is a dynamically linked ELF that uses the build system's glibc. The
executable keeps the historical name `NETWALK.EXE`. The post-build step copies
`overlay.nw` and `gm.sf2` beside it. The game creates `NETWALK0.282` there on
first run if no save exists.

SDL's X11 backend is loaded through `dlopen` at runtime. X11 is therefore not a
link-time dependency and can come from the host system. This is the supported
Linux configuration, including WSL2.

The binary's minimum glibc version is the version present in the build
environment. Build on the oldest distribution whose glibc you intend to
support; building on a newer distribution raises the runtime requirement.

A fully static build is possible only when both glibc and X11 are static:

```sh
cmake --preset linux-x86_64 \
    -DNW_LINUX_STATIC_X11=ON \
    -DNW_LINUX_STATIC_GLIBC=ON
cmake --build --preset linux-x86_64
```

That mode requires the static X11/Xext development libraries (`libx11-dev` and
`libxext-dev` on Debian/Ubuntu). Do not enable `NW_LINUX_STATIC_GLIBC` without
`NW_LINUX_STATIC_X11`: a static glibc that loads a shared X11 library pulls in a
second libc, which is unsupported and can fail with `SIGFPE` on WSL2.

### WSL2

Build inside the WSL distribution with the normal preset:

```sh
cmake --preset linux-x86_64-debug
cmake --build --preset linux-x86_64-debug
```

Do not copy a binary built on a host with a newer glibc into WSL. The dynamic
loader rejects it before the game starts. The dynamic-glibc build is the
supported configuration; a static-glibc binary that `dlopen`s X11 is not.

### Linux test-driver option

`NW_SDL_VIRTUAL_JOYSTICK=ON` builds SDL3 with only the virtual joystick driver,
for automated input tests on a headless machine. It is off by default and is not
part of a release build.

## Windows

```sh
cmake --preset windows-x64-mingw
cmake --build --preset windows-x64-mingw
```

The result is `build/windows-x64-mingw/netwalk.exe`, with `overlay.nw` and
`gm.sf2` beside it. The game creates `NETWALK0.282` on first run.

The Windows build compiles the vendored SDL3 source with the MinGW toolchain.
It therefore needs both `x86_64-w64-mingw32-gcc` and
`x86_64-w64-mingw32-g++`; CMake resolves the C++ frontend from
`NW_MINGW_CC` so a cross build cannot use the host compiler accidentally. SDL
is configured with the Windows video, audio, XInput, DirectInput, RawInput,
WGI, and HIDAPI backends enabled. MSVC is also supported, but the maintained
Windows preset is MinGW.

## Android

Android is a first-class SDLActivity target built with the NDK and Gradle. The
activity runs immersive full-screen with the status and navigation bars hidden,
while the game still renders its centered 4:3 viewport; the unused side or
bottom area holds the Android-only **EXIT**, **KEYBOARD**, **LOCK/UNLOCK**, and
**L CLICK/R CLICK** controls. Touches inside the game viewport are delivered as
the selected click mode; left click is selected at startup. The activity
follows all four sensor orientations; **LOCK** pins the current orientation and
**UNLOCK** resumes sensor rotation. Full-screen setup deliberately keeps
`adjustResize` working so the IME can shrink the SDL surface without losing the
game viewport or controls.

Install the SDK components once (the exact NDK is the tested version):

```sh
sdkmanager "platforms;android-35" "build-tools;35.0.0" \
    "ndk;27.2.12479018" "cmake;3.22.1"
```

Build and install the debug APK through CMake:

```sh
export ANDROID_HOME="$HOME/Android/Sdk"
cmake --preset android-debug
cmake --build --preset android-debug
adb install -r build/android-debug/NetWalk-debug.apk
```

The same build is available directly through the Gradle wrapper:

```sh
tools/build_android.sh assembleDebug
# platforms/android/app/build/outputs/apk/debug/app-debug.apk
```

Release APKs require a real signing key. Export all four variables before
configuring; CMake and Gradle both reject a release build when one is missing:

```sh
export NW_ANDROID_KEYSTORE=/absolute/path/to/release.jks
export NW_ANDROID_KEYSTORE_PASSWORD='keystore-password'
export NW_ANDROID_KEY_ALIAS=netwalk
export NW_ANDROID_KEY_PASSWORD='key-password'

cmake --preset android
cmake --build --preset android
# build/android/NetWalk-release.apk
```

`NW_ANDROID_KEYSTORE` may be absolute or relative to the repository root. The
passwords are read from the environment and are not written to the CMake cache
or the Gradle build files. `tools/build_android.sh assembleRelease` uses the
same four variables. After configuration, the packaging target is also
available as `cmake --build build/android --target android-apk`.

The wrapper finds `ANDROID_HOME`/`ANDROID_SDK_ROOT`, then the tested NDK, and
falls back to the newest installed NDK. Override it with
`ANDROID_NDK_HOME=/path/to/ndk` or `-PnetwalkNdkVersion=...`.

The default build produces `arm64-v8a` and `x86_64` libraries:

- `libmain.so` contains the recovered game and the Android entry point.
- `libSDL3.so` is built from the vendored SDL3 source.
- `assets/overlay.nw` and `third_party/windowsgs/gm.sf2` are staged into the
  APK.

The Gradle build passes `ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON` to the NDK, so
the native libraries use 16 KB ELF segment alignment. This is required for
devices with 16 KB memory pages even though the APK currently extracts native
libraries at install time. Verify either APK with:

```sh
apk=build/android/NetWalk-release.apk   # or build/android-debug/NetWalk-debug.apk
rm -rf /tmp/nw-apk && mkdir -p /tmp/nw-apk
unzip -q "$apk" -d /tmp/nw-apk
for so in /tmp/nw-apk/lib/*/*.so; do
    echo "$so"
    readelf -lW "$so" | awk '/LOAD/{print $NF}'
done
"$ANDROID_HOME/build-tools/35.0.0/zipalign" -c -P 16 -v 4 "$apk"
"$ANDROID_HOME/build-tools/35.0.0/apksigner" verify --verbose "$apk"
```

Every `LOAD` alignment must be `0x4000`, `zipalign` must report success, and
`apksigner verify` must accept the release APK.

`NetWalkActivity` copies those two files into the app's internal files
directory before native startup. `overlay.nw` and `gm.sf2` are refreshed when
their bundled size changes. The game creates `NETWALK0.282` in that directory
on first run, so updating the APK does not overwrite high scores.

Common Android options:

```sh
# One ABI for a faster device build
cmake --preset android-debug -DNW_ANDROID_ABIS=arm64-v8a
cmake --build --preset android-debug

# Package a different overlay (for example a high-resolution pack);
# the selected file is packaged under the game's required name, overlay.nw
cmake --preset android-debug -DNW_ANDROID_OVERLAY=/absolute/path/pack.nw
cmake --build --preset android-debug

# Match a high-resolution overlay with the renderer scale
cmake --preset android-debug -DNW_ANDROID_RENDER_SCALE=4
cmake --build --preset android-debug
```

The direct Gradle wrapper accepts the equivalent `-PnetwalkAbis`,
`-PnetwalkOverlay`, and `-PnetwalkRenderScale` properties. The Android renderer
defaults to scale 1 because the default overlay contains the standard assets.
Do not raise the render scale unless the packaged overlay actually contains
matching high-resolution data. The debug APK is signed with the standard
Android debug key; the release APK is signed with the configured
`NW_ANDROID_*` key.

### Android application icon

The launcher, window, and title artwork comes from one place:
`assets/icons/icon.png` (or `assets/icon.png`). Replace that PNG, then
regenerate the checked-in outputs:

```sh
python3 tools/gen_app_icons.py
```

This updates the Android `mipmap-*` launcher and adaptive icons, the Windows
`.ico` embedded as resource 100, the SDL window icon in
`src/include/NwAppIcon.h`, and `assets/icons/netwalk-512.png` used by the PS5
stager. The same generated artwork is therefore used by every executable.
The source PNG's alpha channel is preserved: generated icons have a transparent
background rather than a flattened opaque square. The Windows ICO stores each
size as a PNG-compressed entry; Windows Vista and later decode these entries
directly and preserve their alpha channel, so Explorer and desktop shortcuts
show the transparent background. Android's adaptive icon supplies its own
`ic_launcher_background` color behind the transparent foreground; its artwork
is fitted inside the 66 dp safe-zone circle so launcher masks do not crop the
monitor. Legacy square and round launcher icons keep their keyline padding too.

For native-only debugging without Gradle, configure the NDK toolchain directly:

```sh
export ANDROID_NDK_HOME="$ANDROID_HOME/ndk/27.2.12479018"
cmake -S . -B build/android-arm64 -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-21 \
    -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON \
    -DCMAKE_BUILD_TYPE=Debug -DNW_ANDROID_RENDER_SCALE=1
cmake --build build/android-arm64
```

That produces the native libraries only; Gradle is still required to assemble
and sign the APK.

## PlayStation 5

The PS5 target is a native title, not a payload. See [PS5.md](PS5.md) for the
SDK setup, converter, signing, staging, deployment, and hardware caveats.

The short version, after installing the SDK and its LLVM toolchain:

```sh
platforms/ps5/tools/setup-native.sh
platforms/ps5/tools/build-native.sh
# package: dist/PPSA99877/

platforms/ps5/tools/build-native.sh --ffpkg
# package plus mountable UFS2 image: dist/PPSA99877.ffpkg
```

The Docker image already contains this SDK/LLVM/native-tool setup; the host
commands above are only needed for a native host build. The containerized
release path is described below.

`build-native.sh` is the preferred wrapper. It exports the SDK/LLVM paths,
prepares the private SDL tree, configures the top-level CMake project, builds the
native target, stages the title, and runs the structural validator. Use
`--debug` for a diagnostic build, `--no-setup` to reuse the prepared tooling,
`--out DIR` to choose a different package directory, and `--ffpkg` to create a
UFS2 image for a ShadowMount-style loader. `.ffpkg` is a filesystem image, not
a signed retail package; see [PS5.md](PS5.md#package-as-ffpkg).

## Containerized release build

`docker/build-releases.sh` builds the Linux and Windows release presets inside
an Ubuntu 24.04 image and writes the staged directories under
`dist/docker-releases/`. Native PS5 packaging is opt-in with `--ps5`, and
Android packaging is opt-in with `--android` or `--android-debug`. The Linux
executable links the image's glibc dynamically, so it requires glibc 2.39 or
newer at runtime; that includes WSL2 on Ubuntu 24.04. The default image bundles
a pinned `ps5-payload-dev` SDK, LLVM 20, and the native PS5 tools. Pass
`--ps5-sdk` and `--ps5-llvm` together (they imply `--ps5`), or set both
`PS5_SDK` and `PS5_LLVM` with `--ps5`, to mount an external toolchain instead;
see [../docker/README.md](../docker/README.md) for the image contents, mounts,
and validation boundaries.

The opt-in Android release APK needs the same four signing variables as the
CMake preset. The wrapper mounts the keystore read-only and passes the passwords
through the container environment:

```sh
export NW_ANDROID_KEYSTORE=/absolute/path/to/release.jks
export NW_ANDROID_KEYSTORE_PASSWORD='keystore-password'
export NW_ANDROID_KEY_ALIAS=netwalk
export NW_ANDROID_KEY_PASSWORD='key-password'

docker/build-releases.sh
docker/build-releases.sh --ps5
docker/build-releases.sh --android
docker/build-releases.sh --android-debug
docker/build-releases.sh --skip-ps5
docker/build-releases.sh /path/to/NETWALK.EXE
docker/build-releases.sh --extract-assets /path/to/NETWALK.EXE
docker/build-releases.sh --overlay /path/to/overlay.nw
docker/build-releases.sh --no-overlay
docker/build-releases.sh --embed-overlay
docker/build-releases.sh --ffpkg
docker/build-releases.sh --ps5 --ffpkg
```

The desktop build is written to `dist/docker-releases/linux-x86_64/` and
`dist/docker-releases/windows-x64-mingw/`. PS5 is skipped by default; `--ps5`
adds `dist/docker-releases/PPSA99877/`, and `--require-ps5`, `--ffpkg`, or
`--ps5-sdk`/`--ps5-llvm` also enable it. With `--android`, the signed APK is
written to `dist/docker-releases/android/NetWalk.apk`. `--android-debug` writes
a debug-signed APK instead and does not require the signing variables. Android
is skipped by default; `--skip-android` and `--skip-ps5` are accepted for
explicit omission.

An original executable can be passed positionally or with `--extract-assets`;
both extract a missing `assets/overlay.nw` and name the Linux and Windows
release executables after the lowercased input filename. `--overlay` stages a
supplied `overlay.nw` instead. None of these options modifies the source
checkout. If no overlay source is supplied, a missing overlay or SoundFont
stops the release before compilation. `--no-overlay` deliberately omits the
overlay for a non-runnable compile check; it cannot be combined with
`--android` or `--android-debug`.

`--embed-overlay` is a desktop placement option. It appends the resolved
overlay to the Linux and Windows executables using
[`tools/nwembed.py`](../tools/documentation/nwembed.md), and omits
`overlay.nw` from those release directories. It can be combined with a
positional executable, `--extract-assets`, or `--overlay`, but not with
`--no-overlay`. The PS5 title still receives an external `overlay.nw`, and an
Android APK always packages its own `assets/overlay.nw`. `gm.sf2` remains a
separate supporting file in every mode; saves are created at runtime.

`--ffpkg` runs inside the container and creates
`dist/docker-releases/PPSA99877.ffpkg` from the staged PS5 directory. It
enables the PS5 build and needs an external overlay, so it cannot be combined
with `--skip-ps5` or `--no-overlay`. See [PS5.md](PS5.md#package-as-ffpkg).

The image uses the Python dependencies in `tools/requirements.txt`.

## Useful CMake options

| Option | Default | Purpose |
| --- | --- | --- |
| `NW_DEBUG_TOOLS` | `OFF` | Compile in opt-in developer diagnostics |
| `NW_LINUX_STATIC_X11` | `OFF` | Link static X11/Xext into the Linux binary (advanced) |
| `NW_LINUX_STATIC_GLIBC` | `OFF` | Statically link glibc (requires static X11; advanced) |
| `NW_ANDROID_PACKAGE` | `OFF` | Add the host-side Android APK packaging target |
| `NW_ANDROID_BUILD_TYPE` | `Release` | Android package variant (`Release` or `Debug`) |
| `NW_ANDROID_ABIS` | `arm64-v8a,x86_64` | Comma-separated Android ABIs to package |
| `NW_ANDROID_OVERLAY` | empty (`assets/overlay.nw`) | Optional overlay packaged as the APK's `overlay.nw` |
| `NW_ANDROID_OUTPUT` | `build/android/NetWalk-release.apk` | Copied APK output path |
| `NW_ANDROID_RENDER_SCALE` | `1` | Android native framebuffer scale; must match the packaged overlay |
| `NW_SDL_VIRTUAL_JOYSTICK` | `OFF` | Build the SDL virtual joystick test driver (Linux) |
| `NW_OVERLAY_ASSET` | `assets/overlay.nw` | Overlay copied beside the desktop executable |
| `NW_PS5_NATIVE_TITLE_DIR` | `dist/PPSA99877` | PS5 title output directory |
| `NW_PS5_NATIVE_OVERLAY` | `assets/overlay.nw` | Overlay staged in the PS5 package |
| `NW_PS5_NATIVE_SOUNDFONT` | `third_party/windowsgs/gm.sf2` | SoundFont staged in the PS5 package |

## Debug diagnostics

Debug builds expose opt-in environment variables. They are compiled out of
release builds and are off when unset. They write only when explicitly enabled;
normal runs create no log file.

| Variable | Effect |
| --- | --- |
| `NW_DEBUG_PROFILE=1` or a path | Write `nwprof.csv` (or the named file) with frame timings |
| `NW_DEBUG_SCREENSHOT=PATH` | Write a PPM framebuffer snapshot |
| `NW_DEBUG_SCREENSHOT_EVERY=N` | Throttle screenshots (default 60 frames); `PATH.go` forces one |
| `NW_DEBUG_ASSET_DUMP=DIR` | Dump decoded sprites and palette |
| `NW_DEBUG_ASSET_PACK=DIR` | Load an external sprite override pack |
| `NW_DEBUG_ASSET_LOG=PATH` | Log applied asset overrides |
| `NW_DEBUG_TRACE=1` | Enable the low-level tag trace (`netwalk_debug.log`) |
| `NW_DEBUG_VM_TRACE=1` or a path | Trace VM opcode/record state (`netwalk_vm.txt`) |
| `NW_DEBUG_VIDEO=1` | Print SDL video/gamepad initialization details |
| `NW_DEBUG_SMOKE=1` | Run the SDL platform smoke path instead of the game |

`NW_NO_AUDIO=1` is a runtime compatibility workaround, not a debug hook; it is
accepted in release builds and disables audio output.

Other runtime configuration:

| Variable | Default | Meaning |
| --- | --- | --- |
| `NW_RENDER_SCALE=N` | 4 | Logical render scale; assets and canvas are `640*N x 480*N` |
| `NW_FRAMEBUFFER_SCALE=N` | same as render scale | Compatibility alias for the framebuffer scale |
| `NW_WINDOW_SCALE=N` | render scale | Initial desktop window scale |
| `NW_WINDOW_RESIZABLE=0` | enabled | Disable desktop window resizing |
| `NW_SF2=PATH` | `gm.sf2` | Alternate General MIDI SoundFont |
| `SDL_PS5_DISPLAY=WxH` | `1920x1080` | PS5 VideoOut display size |

## Clean rebuild

Build trees and generated packages are intentionally ignored by Git. To reset:

```sh
rm -rf build dist
```

Local `build/` trees are deliberately kept between successful builds for
incremental compiles. Docker releases are different: the container removes its
copied source/build tree on exit and replaces the staged release directories on
each run, so only deliverables are left in the host output directory.

Do not delete a live `NETWALK0.282` outside an ignored build/package directory
unless you intend to reset the high scores. It is the live save, not a backup
of user data.
