# PlayStation 5 native title

The PS5 target is a native title, `PPSA99877`. The payload ELF target has been
removed from this tree; there is one PS5 build and it participates in the
top-level CMake project.

## Prerequisites

- A `ps5-payload-dev` SDK with the Prospero toolchain
- LLVM 20, as required by `prospero-clang`
- The native converter and runtime shim, prepared by
  `platforms/ps5/tools/setup-native.sh`
- `PS5_SDK` pointing at the SDK root, or the wrapper's default under
  `.tmp/ps5/tools/ps5-payload-sdk`
- `PS5_LLVM` pointing at the LLVM root, or the wrapper's default under
  `.tmp/ps5/tools/llvm20`
- For `.ffpkg` images, Git, make, and a C compiler;
  `platforms/ps5/tools/setup-packaging.sh` fetches and builds a pinned native
  `makefs` under `.tmp` (no .NET runtime or SDK is used). Set
  `NW_PS5_MAKEFS` to reuse an existing executable offline.

The native tooling is fetched into the ignored `.tmp` tree from the pinned
`drakmor/ps5-native-app-boilerplate` revision. It is not vendored in this
repository. The build does not install or launch anything on a console.

## Docker

The default Docker image, built from the `netwalk-ps5` stage, contains a
pinned `ps5-payload-dev` SDK, LLVM 20, the native converter, `libc.prx`, and
`makefs`. It can build the native title and `.ffpkg` without host SDK setup:

```sh
docker/build-releases.sh --ps5
docker/build-releases.sh --ps5 --ffpkg
```

Pass `--ps5-sdk` and `--ps5-llvm` together, or set both `PS5_SDK` and
`PS5_LLVM` with `--ps5`, to mount an external toolchain instead. The SDK,
generated `libc.prx`, and `makefs` are GPL/BSD build dependencies; see the
licensing notes below and [../docker/README.md](../docker/README.md).

## Build

The preferred one-step build is:

```sh
platforms/ps5/tools/setup-native.sh  # first time, or after a clean .tmp
platforms/ps5/tools/build-native.sh
```

`build-native.sh` stops if the generated overlay or SoundFont is missing.
Extract a missing overlay as described in [BUILD.md](BUILD.md#assets); the
SoundFont is a tracked file. Pass `--no-overlay` to stage a compile-only title
without `overlay.nw`; it will search for an external overlay at runtime and
cannot run until one is supplied.

The wrapper prepares the private SDL tree, configures the top-level CMake
project, builds `netwalk_native`, stages the title, and runs the structural
validator. The default package is:

```text
dist/PPSA99877/
  eboot.bin
  overlay.nw
  gm.sf2
  sce_module/libc.prx
  sce_sys/param.json
  sce_sys/icon0.png
```

With `--ffpkg`, a mountable UFS2 image is written beside the directory as
`dist/PPSA99877.ffpkg`.

The package does not contain a save. The game creates `NETWALK0.282` in the
selected writable directory on first run and does not overwrite a live save.

Useful options:

```sh
platforms/ps5/tools/build-native.sh --debug
platforms/ps5/tools/build-native.sh --clean
platforms/ps5/tools/build-native.sh --no-setup
platforms/ps5/tools/build-native.sh --no-overlay
platforms/ps5/tools/build-native.sh --out /tmp/netwalk-title
platforms/ps5/tools/build-native.sh --build-dir /tmp/netwalk-ps5-build
platforms/ps5/tools/build-native.sh --ffpkg
platforms/ps5/tools/build-native.sh --ffpkg --ffpkg-out /tmp/PPSA99877.ffpkg
```

`--debug` builds an unoptimized title with `NW_DEBUG_TOOLS` enabled. The wrapper
prints the package path and inspects both `eboot.bin` and `libc.prx` with
`ps5-native-tool self --inspect`.

### Direct CMake

After `setup-native.sh` has produced the converter and `libc.prx`, the presets
also work:

```sh
export PS5_SDK=/path/to/ps5-payload-sdk
cmake --preset ps5-native
cmake --build --preset ps5-native
```

The preset uses `cmake/toolchain-ps5-native.cmake` and the default native tool
paths under `.tmp/ps5/native/tool`. The wrapper is still recommended because it
sets `PS5_LLVM`, `PATH`, and `LD_LIBRARY_PATH` for the SDK's compiler wrapper.

## Native build pipeline

1. Compile the game with `platforms/ps5/src/native_crt.c` as the native CRT.
   It calls `_init_env`, runs the preinit/init arrays, enters the shared game
   path, and returns through `catchReturnFromMain()`.
2. Link an intermediate PIE with `platforms/ps5/native/native.ld`. The script
   places `.text` at address 0, file data at `0x4000`, and keeps the RELRO
   ordering required by the converter.
3. Convert with `ps5-native-tool link`: add PS5 process parameters, module
   records, NIDs, and loader program headers.
4. Sign with `ps5-native-tool self --sign`: produce the FSELF `eboot.bin`
   (magic `0x1D3D154F`).
5. Stage the title directory and validate its container and metadata.
6. Optionally pack the staged directory into a mountable UFS2 `.ffpkg` image.

The staged `param.json` uses title ID `PPSA99877`, content ID
`UP9000-PPSA99877_00-NETWALK000000001`, and `downloadDataSize: 256`.

## Package as .ffpkg

A staged directory can be converted to a mountable UFS2 image:

```sh
platforms/ps5/tools/build-native.sh --ffpkg
# or, from an existing directory:
python3 platforms/ps5/tools/make-ffpkg.py dist/PPSA99877
```

The output is `dist/PPSA99877.ffpkg`. This `.ffpkg` is a raw UFS2 filesystem
image for ShadowMount-style loaders; it is not Sony's signed retail PKG/FPKG
container. The loader mounts it read-only and exposes its root as `/app0`, so
the whole staged directory is included: `eboot.bin`, `sce_module/`, `sce_sys/`,
`overlay.nw`, and `gm.sf2`.

`--ffpkg` cannot be combined with `--no-overlay`: an image without the external
overlay would not be runnable. `--allow-missing-overlay` exists only for a
non-runnable compile-check image. A high-resolution overlay is packaged exactly
like the default one:

```sh
NW_PS5_OVERLAY=.tmp/highres4/overlay.nw \
    platforms/ps5/tools/build-native.sh --ffpkg
```

The first packaging run prepares a native, patched `makefs` under
`.tmp/ps5/tools` with `platforms/ps5/tools/setup-packaging.sh`. It needs Git,
make, and a C compiler; no .NET runtime or SDK is used. Set `NW_PS5_MAKEFS`
or pass `--tool` to use an existing `makefs` executable. Use one that writes
the standard UFS2 superblock location; some older distribution packages write a
legacy layout that the loader may reject. The image is checked for the UFS2
superblock and for every expected file with the built-in structural reader
before it replaces the destination. `--no-verify` skips the per-file check;
`--no-setup` prevents automatic builder setup. `--verify IMAGE` checks an
existing image against the staged title and exits.

The image uses 4096-byte UFS sectors, matching ShadowMountPlus's default LVD
sector size for UFS images. If the MD backend is selected instead, set
`md_ufs_sector_size=4096` or an equivalent
`image_sector=PPSA99877.ffpkg:4096` override in its `config.ini`; the MD UFS
default is 512 bytes. The image root contains `sce_sys/param.json` directly,
which is also required by ShadowMountPlus.

The image is mounted read-only by default. The game writes saves to its normal
writable save candidates, not back into `/app0`. The image contains the
filesystem, not a signed package.
Deploy it to a loader that mounts `.ffpkg` images, for example under a
ShadowMountPlus scan path or through its `manual.lst`.

## Deploy

Copy the whole `dist/PPSA99877/` directory to the console's title/homebrew
location, or deploy `dist/PPSA99877.ffpkg` to a loader that mounts UFS2 images.
Do not copy only `eboot.bin`; the title needs `sce_module/libc.prx`,
`sce_sys/param.json`, the icon, and the assets.

The build does not install, launch, or modify a console. Deployment is the
user's loader's responsibility. Restage into an empty directory for a clean
package; do not overwrite a live save inside the title directory.

## Save paths

The native platform layer probes writable directories in this order:

```text
/data/netwalk/PPSA99877
/download0/netwalk/PPSA99877
/app0/userdata/netwalk/PPSA99877
/tmp/netwalk/PPSA99877
```

`/data` is preferred when the loader exposes it because the save is visible over
FTP. `/download0` is the title-private writable location backed by
`downloadDataSize`; `/app0/userdata` and `/tmp` are fallbacks. The first usable
directory is reused. A failed save operation advances to the next candidate.

When the selected directory has no save, the game migrates the first available
legacy copy from the paths listed in [SAVES.md](SAVES.md), then creates a fresh
save if none is found. Migration is atomic and never overwrites an existing
destination. The official `/savedata0` mount is not used because the tested
fake-native/ShadowMount loader does not complete the SaveData initialization
sequence.

## Controls and IME

- L3 (left-stick click) or Select/Options opens the PS5 system keyboard.
- R3 (right-stick click) toggles high-resolution and original assets.
- Touchpad contact moves the pointer; touchpad click is a left click.
- South/A is left click; East/B is right click.
- The desktop `F` fullscreen hotkey is not built for PS5.

The IME is the PS4-inherited `sceImeDialog` API. The IME sysmodule is requested
during startup, before the loader closes the title sandbox. The dialog is opened
on a button press edge, not continuously. Confirmed ASCII text is delivered one
byte per frame to the game's normal text input. The dialog layout is checked
against the SDK's `sceImeDialog` structure with `_Static_assert`.

Keyboard entry, R3 toggling, and save-path selection still require confirmation
on hardware; they are not claimed as verified by a local build.

## High-resolution assets

The default package stages `assets/overlay.nw`, which contains the original
assets. That overlay is generated from the original executable and is ignored
by Git; if it is absent, extract it as described in
[BUILD.md](BUILD.md#assets). A high-resolution pack can be staged instead:

```sh
NW_PS5_OVERLAY=/path/to/highres/overlay.nw \
    platforms/ps5/tools/build-native.sh
```

`NW_PS5_OVERLAY_TRAILER` is only needed when the pack is split into
`overlay.bin` plus `overlay.trailer`; a normal `overlay.nw` already carries its
trailer. Set `NW_PS5_NATIVE_OVERLAY` / `NW_PS5_NATIVE_OVERLAY_TRAILER` for a
direct CMake configure. `tools/nwrepack.py --highres --scale 4` produces the
pack; see [assets/REPACKING.md](assets/REPACKING.md). The 4x canvas and overlay
stay resident; R3 switches the source used for the next frame.

## Scope and limitations

- The native title can be deployed as a directory or as a mountable `.ffpkg`
  UFS2 image. The image path is not a signed retail package.
- The VideoOut presenter is a CPU conversion plus a blocking vblank wait. It is
  intentionally simple.
- The native loader's visible filesystem determines which save candidate wins.
  `/data` may be unavailable in some loader configurations.
- The default `libc.prx` is a clean-room loader-visible compatibility shim, not
  a full C library. It has been validated by the upstream project on firmware
  6.02 and 12.70, but not against this title on every firmware. If the console
  rejects it, supply a signed module from a game dump for the same or an older
  firmware:

  ```sh
  NW_PS5_NATIVE_LIBC=$HOME/ps5/libc.prx platforms/ps5/tools/build-native.sh --clean
  ```

  The override must be a signed FSELF (magic `0xEEF51454`), not a decrypted ELF.
  It is Sony-owned and must not be committed or redistributed.
- The converter maps the RELRO load segment from `.got`'s file offset to the
  lowest RELRO virtual address. Keep `.got` and `.got.plt` before
  `.data.rel.ro` in `platforms/ps5/native/native.ld`.
- Native guest pointers must stay below 4 GiB.
  `platforms/linux/linux_compat.c` uses the `MAP_32BIT`/low-hint fallback; do
  not replace it with a plain `mmap`.
- Release builds emit no routine debug klog markers; use `--debug` and
  `NW_DEBUG_TOOLS` when diagnosing startup or save selection.

## Licensing

- `platforms/ps5/sdl/*.c`: Zlib, derived from PS5_Dusklight's SDL drivers.
- `platforms/ps5/src/platform.[ch]`, `platforms/ps5/src/startup.c`: MIT,
  derived from PS5_Dusklight.
- `platforms/ps5/src/native_crt.c`, `platforms/ps5/native/native.ld`,
  `platforms/ps5/native/hide.map`, `platforms/ps5/tools/stage-native.py`,
  `platforms/ps5/tools/validate-native.py`,
  `platforms/ps5/tools/make-ffpkg.py`,
  `platforms/ps5/tools/setup-packaging.sh`,
  `platforms/ps5/tools/patches/makefs-no-crypto.patch`: MIT.
- `platforms/ps5/patches/sdl.patch`: MIT.
- SDL3: Zlib.
- `kusumi/makefs` is a BSD-2-Clause build dependency fetched into `.tmp` for
  host builds; the Docker image also keeps a pinned copy under
  `/opt/ps5-packaging`. If a generated image is redistributed, comply with its
  license.
- The `ps5-payload-dev` SDK is a GPL-licensed build dependency. Host builds use
  an external copy; the Docker image downloads a pinned copy under
  `/opt/ps5-payload-sdk`.
- The optional native converter and runtime shim are GPL-3.0-or-later, fetched
  at build time into `.tmp` for host builds and under `/opt/ps5-native` in the
  Docker image; they are not part of this source tree. If a staged title or the
  Docker image is redistributed, comply with those licenses.
