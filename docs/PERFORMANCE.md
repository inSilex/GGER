# Performance

The default presentation path renders a 640x480 logical game into a 2560x1920
8-bit framebuffer (`NW_RENDER_SCALE=4`), converts it to the window surface, and
presents it in a centered 4:3 viewport. The conversion is a table-driven palette
lookup plus the platform's own scaling; there is no floating-point per-pixel
pass in the shipping path.

## Costs

- The software framebuffer is allocated once at startup.
- Asset decoding and the high-resolution overlay load during startup.
- The framebuffer-to-window conversion runs once per presented frame; multiple
  blits are accumulated into one present.
- Input polling and the pointer bridge run once per SDL event/frame and are
  negligible compared with the framebuffer conversion.
- PS5 VideoOut uses a CPU conversion and a blocking vblank wait in the native
  driver. It is simple and predictable; a GPU presenter is out of scope.

The 4x scale increases the converted pixel count by 16x relative to 1x. This is
the main rendering cost on low-end CPUs. `NW_RENDER_SCALE=1` is useful for
baseline comparisons but is not the default experience.

## Profiling

The profiler is a debug-build-only tool. Configure and run a debug build:

```sh
cmake --preset linux-x86_64-debug
cmake --build --preset linux-x86_64-debug
NW_DEBUG_PROFILE=/tmp/netwalk-profile.csv \
  ./build/linux-x86_64-debug/NETWALK.EXE
```

`NW_DEBUG_PROFILE=1` writes `nwprof.csv` in the current directory, then the
executable directory, then the temporary directory, in that order. A path value
is used directly. The CSV contains one `F` row per rendered frame with frame
timestamps and timing buckets (present, convert, update, event, audio).

`tools/nwbench.py` wraps repeated profile runs, copies the staged assets into a
scratch directory, and summarizes frame pacing:

```sh
python3 tools/nwbench.py --exe build/linux-x86_64-debug/NETWALK.EXE \
    --native --seconds 10 --scale 4
python3 tools/nwbench.py --exe build/windows-x64-mingw-debug/netwalk.exe \
    --dummy --seconds 5
```

`--scale` defaults to 1 so a run is cheap to compare; pass `--scale 4` to match
the shipping presentation. The tool does not inject input. When the run never
presents a menu frame, the report uses `mode: startup-compose` and summarizes
the startup animation instead; a non-dummy run can be driven by hand if frame
rows are required.

On Windows, `tools\run_profiled.cmd path\to\netwalk.exe [scale]` is a small
wrapper that runs the executable from its own directory with profiling and
audio disabled for a clean pacing sample.

A release build ignores `NW_DEBUG_PROFILE` and emits no profile file. The
60-frame summary and stall lines are also gated by the profiler, so normal runs
do not spam klog.

## Interpreting results

- Compare runs with the same render scale, display size, and asset source.
- The first frames include startup and asset loading; discard the warm-up.
- Fullscreen/presentation changes can alter frame pacing because the driver may
  wait for vblank.
- `NW_RENDER_SCALE=1` versus `4` isolates the software conversion cost.
- On PS5, the VideoOut presenter's blocking wait makes frame pacing sensitive to
  the current display mode; use the debug build and klog markers when
  investigating a stall.

Do not use benchmark results from a debug build as release performance numbers;
debug builds are unoptimized and include diagnostic bookkeeping.
