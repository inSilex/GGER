# window_globals.py

`window_globals.py` publishes recovered data-window globals as link-time
absolute aliases. The recovered code declares many globals with their original
address in a comment; without aliases they would become private `.bss` copies,
separate from the relocated data window.

CMake runs this tool before configuring the portable source tree. It is
normally not necessary to run it by hand.

## Requirements

Python 3.8 or newer. Only the standard library is used.

## Usage

Run from the repository root:

```sh
# Write build/window_globals.args for the portable link.
python3 tools/window_globals.py emit

# Include the pointer side-table form used by the native builds.
python3 tools/window_globals.py emit --ptr-side-table

# Remove aliased definitions from a generated stubs file.
python3 tools/window_globals.py filter build/stubs.c

# Print the curated symbol-to-window offsets.
python3 tools/window_globals.py list
```

The default output is `build/window_globals.args`. Each entry is a pair of
linker `--defsym` definitions that resolves the C name and its historical
underscore-prefixed spelling against `nw_data_window+offset`.

Only globals whose original address is explicitly documented in the corpus are
included. This keeps aliases deterministic and avoids inventing addresses for
unresolved symbols.

## Related documentation

- [`docs/ARCHITECTURE.md`](../../docs/ARCHITECTURE.md)
- [`docs/CONVERSION.md`](../../docs/CONVERSION.md)
- [`nw_portableize.py`](nw_portableize.md)
