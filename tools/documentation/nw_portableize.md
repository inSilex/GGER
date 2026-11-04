# nw_portableize.py

`nw_portableize.py` converts the recovered source corpus into the C-only tree
used by the portable builds. It removes GNU-assembly constructs and the
original leading-underscore convention, rewrites recovered globals as typed
window accesses, and generates the initial data-window image and pointer fixup
table.

CMake invokes it automatically during configure for Linux, Windows, and PS5.
It is normally not necessary to run it by hand.

## Requirements

Python 3.8 or newer. Only the standard library is used.

## Usage

```sh
python3 tools/nw_portableize.py \
    --root "$PWD" \
    --out /tmp/netwalk-portable \
    --globals build/window_globals.args \
    --platform linux
```

Options:

- `--root DIR`: repository root; defaults to the directory above the tool.
- `--out DIR`: output tree. An existing directory is removed and recreated.
- `--globals PATH`: linker alias file from `window_globals.py`; defaults to
  `build/window_globals.args` and is generated if absent.
- `--platform windows|linux`: selects the platform compatibility files and
  constructor handling; default `windows`.

The output is a complete copy of `src/` plus the selected platform files
(`platforms/linux/linux_compat.c` and `platforms/linux/linux_main.c`, or
`windows/win64_compat.c` and `windows/win64_entry.c`) with generated
`nw_window_data.c` and `nw_portable_globals.h`. Build directories containing
this output are ignored and are not source.

## Related documentation

- [`docs/ARCHITECTURE.md`](../../docs/ARCHITECTURE.md)
- [`docs/CONVERSION.md`](../../docs/CONVERSION.md)
- [`window_globals.py`](window_globals.md)
