# pe_layout_check.py

`pe_layout_check.py` validates the Windows PE section layout and can emit a
zero-filled gap-filler section definition. It exists because the real Windows
loader requires image sections to be contiguous at page granularity; a page
between sections that belongs to no section can make the image fail with
`STATUS_INVALID_IMAGE_FORMAT`, even when Wine accepts it.

This is a developer/forensic utility. It is not part of the normal CMake build.

## Requirements

Python 3.8 or newer. Only the standard library is used.

## Usage

```sh
# Inspect one executable and fail on layout problems.
python3 tools/pe_layout_check.py check build/windows-x64-mingw/netwalk.exe

# Quiet check with the same exit status.
python3 tools/pe_layout_check.py check netwalk.exe quiet

# Plan zero-filled sections for the holes in a pass-1 image.
python3 tools/pe_layout_check.py plan pass1.exe build/fillers
```

`check` prints the section table, hole locations, and `SizeOfImage` status. It
exits `1` if any hole or structural problem is found, and `0` when the sections
are contiguous at page granularity and `SizeOfImage` covers the image.

`plan` writes:

- `build/fillers/nwfill.s`: one generated zero-filled section per page gap;
- `build/fillers/nwfill.args`: `-Wl,--section-start=...` arguments that pin
  the fillers without moving the real sections.

The command parser is positional rather than `argparse`-based. Run the script
without arguments to print its built-in reference text.

## Related documentation

- [`docs/CONVERSION.md`](../../docs/CONVERSION.md)
- [`docs/ARCHITECTURE.md`](../../docs/ARCHITECTURE.md)
