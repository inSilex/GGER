# Conversion notes

This project is Gamos Game Engine Reanimated (GGER), a behavioral reconstruction
of the Gamos 1.51 runtime from the 32-bit NetWalk Windows executable. The
conversion was performed against the shipped binary, not against a source
archive. The reference tables under `docs/reference/` record the original units,
addresses, hashes, and group-file mapping. The verified reference releases and
the command that checks their original functions are documented in
[PROVENANCE.md](PROVENANCE.md).

## Naming policy

Public unit names are descriptive and stable. A rename must not change object
layout, link order, or external symbol names. The source map remains the
authority:

- `docs/reference/CONVERSION_MAP.tsv`: original unit to group file/selector
- `docs/reference/FUNCTION_INDEX.tsv`: original file to group file
- `docs/reference/FUNCTION_MAP.md`: function/address map
- `docs/reference/GROUPS.tsv`: group, header, and member units
- `docs/reference/CONVERSION_PROVENANCE.md`: archived leading conversion notes
- `docs/reference/FILE_RENAMES.tsv`: renames and safety notes

`src/units.tsv` is the build-time unit map. Each row selects a group file and
unit macro. Object names and link order are intentionally independent of the
group filename so a source rename cannot silently move a section.

## Assembly removal

The shipping tree is assembly-free. The portableizer resolves the fixed data
window from `baseline/window_snapshot.s`, emits C accessors and a native pointer
side table, and replaces the remaining inline-assembly shims with C. The
generated sources are build products and are not committed.

The fixed layout requirements remain:

- `nw_data_window` at `0x418030`
- 32-bit guest pointer fields stored as four bytes
- low-address host allocations below 4 GiB for guest pointer compatibility
- PS5 linker ordering: `.got` and `.got.plt` before `.data.rel.ro` in RELRO

The last item is a converter invariant. Changing it can make `DT_INIT_ARRAY`
read relocation data and crash before startup logging.

## Cleanup policy

The final tree keeps general-purpose developer facilities and removes one-off
investigation hooks:

- Retained: the portableizer, unit/link maps, asset extraction/repacking,
  benchmark, frame repacker, layout checker, and the generic `NW_DEBUG_*`
  trace/profile/screenshot/asset-dump/VM-trace hooks.
- Removed: bug-specific probes, environment-variable experiments, temporary
  auto-skip timers, palette probes, one-off seed/map dumps, and compiled helper
  binaries.
- Debug code is compiled only when `NW_DEBUG_TOOLS=ON`. Release builds ignore
  `NW_DEBUG_*` variables. Each retained hook is opt-in even in a debug build.
- Normal runs create no log files. Runtime errors go to stderr/klog; the crash
  handler may write a crash-only report.

See [BUILD.md](BUILD.md) for the debug variables and [TESTING.md](TESTING.md)
for the checks that exercise the layout and save behavior.
