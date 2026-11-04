# nwbench.py

`nwbench.py` runs a debug executable with the built-in profiler and summarizes
frame pacing from the `nwprof.csv` data. It copies the executable and staged
assets into a scratch directory so the original build tree is not polluted.

Profiling exists only in `NW_DEBUG_TOOLS` builds. Release executables ignore
`NW_DEBUG_PROFILE`.

## Requirements

- Python 3.8 or newer; the tool itself uses only the standard library.
- A debug build from `linux-x86_64-debug`, `windows-x64-mingw-debug`, or
  `ps5-native-debug`.
- Wine on Linux when running a Windows executable without `--native`.

## Usage

```sh
python3 tools/nwbench.py \
    --exe build/linux-x86_64-debug/NETWALK.EXE \
    --native --seconds 10 --scale 4

python3 tools/nwbench.py \
    --exe build/windows-x64-mingw-debug/netwalk.exe \
    --dummy --seconds 5 --scale 4
```

Options:

- `--exe PATH`: debug executable to copy and run.
- `--seconds N`: measurement window after the first presentable frame;
  default 10.
- `--warmup N`: delay after the first frame before measuring; default 1.
- `--run DIR`: scratch directory; recreated on each run. Default is
  `nwbench_run` in the system temporary directory.
- `--scale N`: sets `NW_RENDER_SCALE`; default 1.
- `--audio`: allows audio and copies `gm.sf2`; audio is disabled by default.
- `--dummy`: sets `SDL_VIDEODRIVER=dummy`; no window and no input path.
- `--native`: run directly instead of through Wine.
- `--timeout N`: time allowed for the first frame or compose event; default 60.
- `--baseline PATH`: compare p95 frame time with an earlier
  `bench_report.json`.

## Output

The scratch directory contains:

- `nwprof.csv`: profiler rows from the debug build;
- `stdout.txt`, `stderr.txt`: process output;
- `bench_report.json`: parsed summary and baseline comparison.

The tool does not inject input. An idle run may never present a menu frame; in
that case the report has `mode: startup-compose` and summarizes startup
animation composition instead of gameplay frame pacing. Drive a non-dummy run
by hand if presented-frame rows are required.

## Related documentation

- [`docs/PERFORMANCE.md`](../../docs/PERFORMANCE.md)
- [`run_profiled.cmd`](run_profiled.md)
