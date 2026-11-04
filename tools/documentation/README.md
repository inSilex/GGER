# GGER tool reference

This directory documents the developer tools shipped with the final GGER source
tree. It is an index and quick reference; the detailed asset, build, performance,
and conversion notes remain under [`../../docs/`](../../docs/).

## Python environment

The supported setup is a virtual environment from the repository root:

```sh
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install -r tools/requirements.txt
```

The only external Python package is Pillow, used by the asset and image tools.
Everything else in `tools/` and `platforms/ps5/tools/` uses the Python standard
library. See [`../requirements.txt`](../requirements.txt).

## Build and source-generation tools

| Tool | Purpose |
| --- | --- |
| [`window_globals.py`](window_globals.md) | Publish recovered data-window globals as linker aliases. |
| [`nw_portableize.py`](nw_portableize.md) | Generate the C-only portable build tree from the recovered sources. |
| [`gen_function_map.py`](gen_function_map.md) | Regenerate the historical function/address map. |
| [`verify_reference.py`](verify_reference.md) | Verify the reference executables and disassemble original functions. |
| [`pe_layout_check.py`](pe_layout_check.md) | Check PE section layout and plan page-gap fillers. |
| [`build_android.sh`](../build_android.sh) | Drive the Gradle wrapper for an Android APK build. |
| [`gen_app_icons.py`](../gen_app_icons.py) | Regenerate every Android, Windows, Linux/SDL, and PS5 application icon from `assets/icons/icon.png`. |

## Asset pipeline

| Tool | Purpose |
| --- | --- |
| [`nwextract.py`](nwextract.md) | Extract the appended VM overlay, or the full decoded asset tree with `--assets`. |
| [`nwembed.py`](nwembed.md) | Append an external `overlay.nw` to a Linux or Windows executable. |
| [`nwrepack.py`](nwrepack.md) | Replace image records and rebuild an external `overlay.nw`. |
| [`nwframe_repack.py`](nwframe_repack.md) | Replace frame-cell startup/menu animation frames. |
| [`nwupscale.py`](nwupscale.md) | Nearest-neighbour upscaling for sprite packs and animation frames. |
| [`nwassets.py`](nwassets.md) | Render an existing runtime sprite dump and build override packs. |

## Diagnostics and performance

| Tool | Purpose |
| --- | --- |
| [`nwbench.py`](nwbench.md) | Run a debug build with profiling and summarize frame pacing. |
| [`run_profiled.cmd`](run_profiled.md) | Windows helper for a profiled run from the executable directory. |

## PS5 helpers

The native-title helpers live under
[`../../platforms/ps5/tools/`](../../platforms/ps5/tools/) and are described
together in [`ps5-tools.md`](ps5-tools.md):

- `setup-sdl.sh`
- `setup-native.sh`
- `build-native.sh`
- `setup-packaging.sh`
- `make-ffpkg.py`
- `stage-native.py`
- `validate-native.py`

The PS5 SDK and LLVM toolchain are external prerequisites; see
[`../../docs/PS5.md`](../../docs/PS5.md).

## Containerized releases

[`../../docker/README.md`](../../docker/README.md) describes the Ubuntu build
image and the `docker/build-releases.sh` release wrapper. The wrapper builds
Linux and Windows by default; PS5 is opt-in with `--ps5`, and Android packaging
is opt-in with `--android` or `--android-debug`. Pass
`--extract-assets /path/to/NETWALK.EXE` or
`--overlay /path/to/overlay.nw` when the source overlay is absent, or
`--no-overlay` for a compile-only desktop package that searches for an external
overlay at runtime. Add `--embed-overlay` to put the overlay inside the Linux
and Windows executables; the PS5 title keeps a separate `overlay.nw`, and an
Android APK always packages its own overlay. Android signing uses the four
`NW_ANDROID_*` environment variables described in
[`../../docs/BUILD.md`](../../docs/BUILD.md#android).

## Conventions

- Run tools from the repository root unless a page says otherwise.
- Relative paths shown in examples are repository-relative.
- Generated extraction, repack, build, and profile output is ignored or should
  be kept outside the source tree.
- `docs/reference/` is a historical recovery record; `src/units.tsv` is the
  current source-unit manifest.
