# PS5 helper tools

The native PlayStation 5 build helpers live under
[`../../platforms/ps5/tools/`](../../platforms/ps5/tools/). They are not part of
the desktop `tools/` directory, but are documented here so the complete tool
surface is in one place.

The PS5 target is a native title, not a payload ELF. It requires an external
`ps5-payload-dev` SDK and LLVM 20; neither is vendored in this repository.

## setup-sdl.sh

```sh
platforms/ps5/tools/setup-sdl.sh [--out DIR] [--sdl-src DIR] [--force]
```

Copies the vendored SDL3 tree, applies `platforms/ps5/patches/sdl.patch`, and
writes the patched source tree to `.tmp/ps5/SDL` by default. The result path is
printed on stdout. It needs `patch`; no network access is required.

## setup-native.sh

```sh
platforms/ps5/tools/setup-native.sh [--out DIR] [--force]
```

Fetches the pinned `drakmor/ps5-native-app-boilerplate` revision, builds
`ps5-native-tool` and `libc-builder`, and emits `libc.prx` into
`.tmp/ps5/native/tool/` by default. It needs Git, a C++20 host compiler, and
zlib development headers, plus network access when the pinned checkout is not
already present.

The generated `libc.prx` is GPL-3.0-or-later. See
[`../../docs/PS5.md`](../../docs/PS5.md) before redistributing it.

## build-native.sh

```sh
platforms/ps5/tools/build-native.sh [--build-dir DIR] [--out DIR]
                                    [--debug] [--clean] [--no-setup]
                                    [--no-overlay]
                                    [--ffpkg [--ffpkg-out FILE]]
                                    [-- CMake args]
```

The preferred PS5 build wrapper. It prepares the patched SDL tree, configures
the top-level CMake project with the Prospero toolchain, builds the native
target, stages the title, and runs the structural validator.

It requires:

- `PS5_SDK` pointing at a `ps5-payload-dev` SDK root;
- `PS5_LLVM` pointing at LLVM 20;
- the native converter and `libc.prx` from `setup-native.sh` (created
  automatically unless `--no-setup` is used);
- the generated `assets/overlay.nw` and the tracked SoundFont. The wrapper
  stops with an error if either is missing; see
  [`../../docs/BUILD.md#assets`](../../docs/BUILD.md#assets).

The default package is `dist/PPSA99877/`. `--debug` enables `NW_DEBUG_TOOLS`;
the release path forces it off. `--no-overlay` omits `overlay.nw` from the
staged title; the game then searches for an external overlay at runtime, so the
package is not runnable until one is supplied. `--ffpkg` additionally creates
`dist/PPSA99877.ffpkg`; `--ffpkg-out` selects a different image path.
`--no-overlay` and `--ffpkg` are mutually exclusive.

## setup-packaging.sh

```sh
platforms/ps5/tools/setup-packaging.sh [--out DIR] [--force]
```

Prepares a pinned, patched native `makefs` for `.ffpkg` creation under
`.tmp/ps5/tools` by default. It fetches the pinned source and builds it with
Git, make, and a C compiler. The patch disables the optional HAMMER2, exFAT,
and mtree digest code, so no OpenSSL development files and no .NET runtime or
SDK are needed. `NW_PS5_MAKEFS` selects an existing executable; this is also
the offline path. The helper does not install anything system-wide.

## make-ffpkg.py

```sh
python3 platforms/ps5/tools/make-ffpkg.py [--out FILE] [--tool PATH]
                                [--label LABEL] [--allow-missing-overlay]
                                [--no-setup] [--no-verify]
                                [--verify IMAGE]
                                dist/PPSA99877
```

Creates a raw UFS2 filesystem image with the extension `.ffpkg`. The image is
for a ShadowMount-style loader, which mounts it read-only and exposes its root
as `/app0`; it is not Sony's signed retail PKG/FPKG container. The tool includes
the whole staged directory, requires `overlay.nw` (or `overlay.bin` plus
`overlay.trailer`) for a runnable image, checks the UFS2 superblock, and
verifies every expected file with its built-in UFS2 reader. `--no-verify` skips
the per-file check but not the superblock check. The default output is
`<title_dir>.ffpkg`; `--verify IMAGE` checks an existing image instead of
building one.

## stage-native.py

```sh
python3 platforms/ps5/tools/stage-native.py \
    --eboot build/ps5-native/native/eboot.bin \
    --libc .tmp/ps5/native/tool/libc.prx \
    --param platforms/ps5/native/param.json \
    --overlay assets/overlay.nw \
    --soundfont third_party/windowsgs/gm.sf2 \
    --out dist/PPSA99877
```

Assembles the loader-visible title directory around an already converted
`eboot.bin`: `sce_module/libc.prx`, `sce_sys/param.json`, an icon, and optional
`overlay.nw` and `gm.sf2`. `--save-seed PATH` can also copy a save file under
its original name. It does not install or modify a console.

## validate-native.py

```sh
python3 platforms/ps5/tools/validate-native.py dist/PPSA99877
```

Checks the staged title's FSELF container, `ET_SCE_EXEC_ASLR` executable shape,
process-parameter record, runtime module, and presentation metadata. This is a
host-side structural check, not a console test.

## Related documentation

- [`docs/PS5.md`](../../docs/PS5.md)
- [`docs/BUILD.md`](../../docs/BUILD.md)
- [`docs/SAVES.md`](../../docs/SAVES.md)
