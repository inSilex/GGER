# gen_function_map.py

`gen_function_map.py` regenerates
`docs/reference/FUNCTION_MAP.md` from the historical conversion tables. The map
lists original units, recovered symbols and entry addresses, the consolidated
C implementation file, the unit selector, and the historical assembly state.

This is a documentation generator, not a source or build manifest.
`src/units.tsv` is the authoritative current unit list.

## Requirements

Python 3.8 or newer. Only the standard library is used.

## Usage

Run from the repository root:

```sh
# Regenerate the checked-in map.
python3 tools/gen_function_map.py

# Fail if the checked-in map is stale.
python3 tools/gen_function_map.py --check

# Print the built-in help.
python3 tools/gen_function_map.py --help
```

Inputs:

- `docs/reference/CONVERSION_MAP.tsv`
- `docs/reference/ASM_INVENTORY.tsv`
- `docs/reference/asm_converted.txt`

Keep the generated map with the reference inputs when those historical records
change. The current build uses `src/units.tsv`; removed diagnostic units may
still appear in the historical map.

## Related documentation

- [`docs/CONVERSION.md`](../../docs/CONVERSION.md)
- [`docs/reference/FUNCTION_MAP.md`](../../docs/reference/FUNCTION_MAP.md)
- [`docs/reference/CONVERSION_MAP.tsv`](../../docs/reference/CONVERSION_MAP.tsv)
