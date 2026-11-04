# Architecture

GGER is a behavioral re-creation of the Gamos 1.51 runtime, with NetWalk as the
current game integration. The recovered Win32 program uses a fixed 32-bit memory
image. The port keeps the recovered object layout while replacing the
operating-system and rendering layers with a portable C and SDL3 implementation.

## Source organization

The recovered code remains grouped by subsystem:

```text
app/       startup, lifecycle, main-loop dispatch
audio/     MIDI synthesis, PCM mixing, SDL3 and Win32 transports
data/      embedded data-block image
diag/      opt-in diagnostics and crash handling
game/      game state and handlers
generated/ fixed layout glue
grid/      grid state, span allocation, marking, mapping
include/   group headers and shared interfaces
input/     input routing, fields, Win32 and SDL3 event bridges
io/        file slots, saves, stream records, Win32 path glue
runtime/   slots, hook tables, platform services
ui/        dialogs, window chrome, menus
video/     assets, palette conversion, blitting, software framebuffer
vm/        opcode execution and stream copying
```

Most `.c` files are *group files*: a file contains several related recovered
units. `src/units.tsv` selects one unit per compilation. The selector macro is
defined in a generated wrapper translation unit, which then includes the group
file. This preserves the original unit boundaries and link order without
requiring one file per function.

`src/sources.list` and `src/link_order.txt` are retained as source/layout
references. CMake uses `units.tsv` as the authoritative unit map.

## Portable source generation

`tools/nw_portableize.py` turns the recovered tree into an assembly-free C
source tree at configure time. It:

- resolves the checked-in data-window bridge in `baseline/window_snapshot.s`;
- emits pointer-side-table accessors and the fixed global layout;
- removes inline assembly and adds C replacements where required;
- maps Windows compatibility calls to `platforms/linux/linux_compat.c` on Linux
  and PS5;
- emits `windows/` or `linux/` glue in the portable build tree according to the
  target.

The release build contains no assembly translation units. The checked-in
`baseline/window_snapshot.s` is input data for the generator, not linked into
the game.

`src/generated/LayoutGlue.c` and `src/data/nw_datablock.c` define the fixed
data-window image. The loader must leave `nw_data_window` at `0x418030`; the
32-bit fields inside it are part of the recovered ABI. Native 64-bit pointers
are stored in side tables so the guest words remain four bytes.

## Platform layers

Platform-specific source lives under `platforms/linux/`, `platforms/android/`,
and `platforms/ps5/`; the Windows compatibility layer remains under `windows/`.

### Desktop

- `platforms/linux/linux_compat.c` supplies the Win32 compatibility layer used
  by both Linux and PS5. It also implements low-address allocation and process
  startup.
- `windows/win64_compat.c` and `windows/win64_entry.c` supply the MinGW entry
  point and Win64 compatibility shims.
- `src/app/sdl3/MainLoopSdl.c` owns the SDL3 window, event loop, pointer bridge,
  fullscreen handling, gamepad hot-plug, and PS5 IME bridge.
- `src/video/sdl3/VideoSdl.c` owns the 8-bit software framebuffer, palette
  conversion, presentation, 4:3 viewport, and optional snapshots.
- `src/audio/sdl3/` implements the SDL3 MIDI/PCM paths.

### PlayStation 5

- `platforms/ps5/src/native_crt.c` is the native title entry point. It
  initializes the environment, runs init arrays, enters the shared game path,
  and returns through the native process-closure helper.
- `platforms/ps5/src/platform.c` supplies klog output, active-user lookup, pad
  and touchpad access, audio output, the system keyboard, and the save-path
  selection/migration logic.
- `platforms/ps5/sdl/` contains the VideoOut, AudioOut, and DualSense SDL3
  drivers.
- `platforms/ps5/native/` contains the linker script, hide map, and title
  metadata.

The PS5 build is a native title (`eboot.bin`), not a payload ELF. The native
converter is a build dependency; it is not part of the game source.

## Rendering

The logical canvas is 640x480. The default `NW_RENDER_SCALE=4` produces a
2560x1920 framebuffer. The desktop window starts at 640x480 and can be resized;
the game always presents the largest centered 4:3 rectangle that fits the
client area. The unused area is black.

The original overlay and the high-resolution section are carried in
`assets/overlay.nw`. `H` or R3 switches the asset source used for the next frame
without changing canvas geometry. The embedded high-resolution pack takes
priority; a debug external pack can override it.

## Input

The SDL event loop converts keyboard and mouse events back into the Win32-style
messages the recovered game expects. Pointer coordinates are converted from
window space into the centered 4:3 viewport before dispatch, so letterboxed
areas do not generate game clicks. Gamepad input uses semantic SDL buttons
where available and falls back to raw joystick indices.

The PS5 touchpad exposes contact motion as an absolute pointer and its click as
the gamepad touchpad button. L3/Select/Options open the system IME on press
edges; R3 toggles assets.

## Saves

The save is `NETWALK0.282`. Desktop builds store it beside the executable. The
PS5 platform layer probes writable title directories in a fixed order, migrates
legacy saves with an atomic copy, and never overwrites an existing live save.
See [SAVES.md](SAVES.md).

## Diagnostics

`NW_DEBUG_TOOLS` is off by default and off in release builds. Debug builds can
enable tracing, profiling, screenshots, asset dumps, and VM tracing via
`NW_DEBUG_*` variables. Normal release runs create no log files. The crash
handler may write a crash-only report when the process actually crashes.

The conversion and naming policy is documented in [CONVERSION.md](CONVERSION.md).
