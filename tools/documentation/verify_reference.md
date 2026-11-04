# verify_reference.py

`verify_reference.py` checks the historical Gamos 1.51 reference executables
and reads original functions from the NetWalk image. It is dependency-free
except for Python 3; instruction text requires GNU `objdump` from binutils.

The full provenance record, release hashes, and section comparison are in
[`../../docs/PROVENANCE.md`](../../docs/PROVENANCE.md).

## Check the reference images

From the repository root:

```sh
python3 tools/verify_reference.py \
    --netwalk /path/to/NETWALK.EXE \
    --flipflop /path/to/FFLOPE.EXE
```

The check verifies:

- the complete-file SHA-256 and size for both verified releases;
- PE32 i386 type, image base `0x00400000`, and section layouts;
- the shared `.idata`, `_TEXT`, `.CRT$XIA`, and `.reloc` hashes;
- that the `.rsrc` sections differ between the two games;
- the original addresses in `docs/reference/CONVERSION_MAP.tsv`.

With only `--netwalk`, the image and conversion map are checked without the
FlipFlop comparison. Pass `--map PATH` to use a different conversion map.

## Disassemble a mapped function

Use an address from [`../../docs/reference/FUNCTION_MAP.md`](../../docs/reference/FUNCTION_MAP.md):

```sh
python3 tools/verify_reference.py \
    --netwalk /path/to/NETWALK.EXE \
    --disassemble 0x00404b90 --size 42
```

The output shows the file offset, then the original bytes and Intel-syntax
instructions from the reference PE. `--size` is a read window; obtain exact
function extents from the replacement contracts in `FUNCTION_MAP.md` or the
historical notes in
[`../../docs/reference/CONVERSION_PROVENANCE.md`](../../docs/reference/CONVERSION_PROVENANCE.md).

Multiple addresses may be supplied after one `--disassemble`:

```sh
python3 tools/verify_reference.py \
    --netwalk /path/to/NETWALK.EXE \
    --disassemble 0x00405ab0 0x0040ed04 0x0040fb74 --size 64
```

## Exit status

`0` means every requested check passed. A hash, section, map, or disassembly
failure returns nonzero. The reference executables are not part of this
repository.
