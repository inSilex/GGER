# GGER documentation

Gamos Game Engine Reanimated (GGER) is a behavioral re-creation of the Gamos
1.51 runtime. The NetWalk game is the current integration. Start here:

| Document | Contents |
| --- | --- |
| [BUILD.md](BUILD.md) | Prerequisites, release/debug presets, platform builds, debug variables |
| [CONTROLS.md](CONTROLS.md) | Keyboard, mouse, gamepad, PS5 IME, fullscreen |
| [SAVES.md](SAVES.md) | Desktop save, PS5 path/migration order, first run, clean start |
| [PS5.md](PS5.md) | Native title build, staging, deployment, save/IME caveats |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Source layout, portableizer, platform layers, rendering |
| [PROVENANCE.md](PROVENANCE.md) | Verified release hashes, section checks, function disassembly |
| [CONVERSION.md](CONVERSION.md) | Naming, assembly removal, provenance and cleanup policy |
| [TESTING.md](TESTING.md) | Build gates, desktop, Android, and PS5 checks |
| [PERFORMANCE.md](PERFORMANCE.md) | 4x render cost, profiling, benchmark tooling |
| [../tools/documentation/README.md](../tools/documentation/README.md) | Per-tool reference, Python requirements, Docker release builds |

## Asset tools

The detailed asset documentation is grouped under [assets/](assets/):

- [EXTRACTION.md](assets/EXTRACTION.md) - extracting the original overlay and sprites
- [REPACKING.md](assets/REPACKING.md) - editing and rebuilding overlay packs
- [OFFLINE_EXTRACTOR.md](assets/OFFLINE_EXTRACTOR.md) - extraction without running the game

The importer/repacker entry points are `tools/nwextract.py`, `tools/nwrepack.py`,
`tools/nwframe_repack.py`, and `tools/nwupscale.py`. `tools/nwassets.py` renders
and repacks an existing runtime sprite dump.

## Reference

The generated conversion records are under [reference/](reference/):

- [FUNCTION_MAP.md](reference/FUNCTION_MAP.md)
- [CONVERSION_MAP.tsv](reference/CONVERSION_MAP.tsv)
- [FUNCTION_INDEX.tsv](reference/FUNCTION_INDEX.tsv)
- [GROUPS.tsv](reference/GROUPS.tsv)
- [FILE_RENAMES.tsv](reference/FILE_RENAMES.tsv)
- [ASM_INVENTORY.tsv](reference/ASM_INVENTORY.tsv)
- [asm_converted.txt](reference/asm_converted.txt)
- [CONVERSION_PROVENANCE.md](reference/CONVERSION_PROVENANCE.md)

These files are historical recovery records. They may describe original
Windows-only behavior that the final SDL3/PS5 tree replaces; the current build
and behavior docs take precedence.
