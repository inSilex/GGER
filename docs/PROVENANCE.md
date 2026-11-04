# Provenance and reference verification

Gamos Game Engine Reanimated (GGER) is a behavioral re-creation of the Gamos
1.51 game runtime. The implementation was reconstructed from disassembly of the
NetWalk release, not from an original source archive. FlipFlop was used as a
second release of the same runtime; its resources and game data differ, but the
recovered engine code does not.

## Verified reference releases

| Release | Role | Size (bytes) | SHA-256 |
| --- | --- | ---: | --- |
| `NetWalk_v1.51_Arsenal eng` | Source of the disassembly and current game integration | 4,979,981 | `258d7406ea98bf3088e90d8dd9988fbdc2e270ad75869a2a584c429855bffb9d` |
| `FlipFlop_v1.51_Arsenal_Eng` | Second Gamos 1.51 release used to verify the shared runtime | 4,633,340 | `b6e34011349cd2de386914bba542dc05c81a66cfe013827f77b2889d9beeb1b2` |

The copies used for this audit were named `NETWALK.EXE` and `FFLOPE.EXE`. The
hashes identify the file contents, not the local filenames. Neither executable
is distributed with this repository; use legally obtained copies.

Both files are PE32 i386 executables with image base `0x00400000`. The following
raw sections are byte-identical in the two releases:

| Section | SHA-256 |
| --- | --- |
| `.idata` | `9a101af9a5266a10d74bbfcbdecdcf02789f29f310c6c37c23bc98b653deb879` |
| `_TEXT` | `a8d3ca1ac4a06064b3c57eee97af4bb21b6863483b84cb59726cd0b2dd41e8cb` |
| `.CRT$XIA` | `f182342471f90e82cbed562b05fc3cd1a2b18ac7286be33eef7700e9064d597a` |
| `.reloc` | `e2a9909877ba782a8d97074979bb56f03a977c50af6755c048187343db1c842e` |

Only the resource section and the appended game overlay differ. That is
consistent with a shared runtime plus game-specific resources and VM data, and
is why the recovered engine code can be checked against both titles.

The current game integration is NetWalk. `tools/nwextract.py` can also extract
the FlipFlop overlay, but the game-specific source paths in this tree implement
the NetWalk content.

## Check a reference copy

From the repository root:

```sh
python3 tools/verify_reference.py \
    --netwalk /path/to/NETWALK.EXE \
    --flipflop /path/to/FFLOPE.EXE
```

The verifier uses only the Python standard library. It checks the complete-file
hashes and sizes, verifies the PE machine type and image base, compares every
runtime section, and confirms that the resource sections differ. With only
`--netwalk`, it still verifies the NetWalk image and the conversion map.

A successful run ends with:

```text
OK: reference binaries and conversion map verified
```

## Access the original functions

`docs/reference/CONVERSION_MAP.tsv` is the authoritative original-unit index.
`docs/reference/FUNCTION_MAP.md` is its address-ordered view. The map contains
361 entries: 350 executable addresses in `_TEXT`, three data addresses in
`.CRT$XIA`, the image base, the portable data-window address, and six unit-level
entries that have no original address.

The verifier resolves the code addresses against the executable and reports any
address that does not point at file-backed executable bytes. To read the
original instructions for a mapped function:

```sh
python3 tools/verify_reference.py \
    --netwalk /path/to/NETWALK.EXE \
    --disassemble 0x00404b90 --size 42
```

`0x00404b90` is `SeekStreamLogicalOffset`, mapped to
`src/io/StreamRecords.c` and `NW_UNIT_SeekStreamLogicalOffset`. The command
prints the original PE bytes with GNU `objdump`:

```text
00404b90 <_TEXT+0x2b90>:
  404b90: 83 7c 24 04 01        cmp    DWORD PTR [esp+0x4],0x1
  404b95: 75 09                 jne    0x404ba0
  404b97: a1 b4 26 41 00        mov    eax,ds:0x4126b4
  404b9c: 29 44 24 08           sub    DWORD PTR [esp+0x8],eax
  404ba0: c7 05 b4 26 41 00 00  mov    DWORD PTR ds:0x4126b4,0x0
  404ba7: 00 00 00
  404baa: ff 74 24 08           push   DWORD PTR [esp+0x8]
  404bae: ff 74 24 08           push   DWORD PTR [esp+0x8]
  404bb2: e8 a9 7b 00 00        call   0x40c760
  404bb7: c2 08 00              ret    0x8
```

Replace the address with any `original VA` from the function map. `--size` is a
read window, not a function length; use the replacement contract in
`FUNCTION_MAP.md` or the original body notes in
`docs/reference/CONVERSION_PROVENANCE.md` when the exact extent matters.

The recovery audit also retrieved function bodies and pseudocode from the
reference image with Ghidra 12.1.3 for `SeekStreamLogicalOffset` (`0x00404b90`),
`DispatchPendingCommandLoop` (`0x00405ab0`),
`DispatchExitHooksAndClearShutdownFlags` (`0x0040ed04`), and
`AllocatePooledBlock` (`0x0040fb74`). The checked-in verifier does not require
Ghidra; it needs only Python 3 and GNU binutils for instruction text.

## Limits

- The hashes identify the exact reference copies in this audit. A different
  release, patch level, or locale may have different bytes.
- The conversion map is a historical record. It proves that an original address
  can be read and mapped to a replacement unit; it does not prove textual
  equivalence of the C implementation.
- The verifier does not extract or validate game assets. The overlay is
  generated separately and remains user-supplied.
