# run_profiled.cmd

`run_profiled.cmd` is a small Windows command helper for a profiled run. It
changes to the executable's directory so the staged assets are found, enables
`NW_DEBUG_PROFILE`, disables audio, and optionally sets the render scale.

## Requirements

- Windows `cmd.exe`.
- A debug build (`windows-x64-mingw-debug` or a locally built equivalent).

## Usage

```bat
tools\run_profiled.cmd build\windows-x64-mingw-debug\netwalk.exe
tools\run_profiled.cmd build\windows-x64-mingw-debug\netwalk.exe 4
```

The first argument is required and must be an existing executable. The optional
second argument is the render scale. The script runs from the executable's
directory with:

```text
NW_DEBUG_PROFILE=1
NW_NO_AUDIO=1
NW_RENDER_SCALE=<optional scale>
```

The profiler writes `nwprof.csv` next to the executable, in the current
directory, or in `%TEMP%` according to the runtime fallback rules. Use
`nwbench.py` for an automated summary.

## Related documentation

- [`docs/PERFORMANCE.md`](../../docs/PERFORMANCE.md)
- [`nwbench.py`](nwbench.md)
