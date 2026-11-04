#!/usr/bin/env python3
"""Generate FUNCTION_MAP.md from the conversion index.

`docs/reference/CONVERSION_MAP.tsv` is authoritative: one row per original source unit,
with the original symbol and entry address, the consolidated implementation
file, and the unit selector used to build that unit.  `docs/reference/ASM_INVENTORY.tsv` and
`docs/reference/asm_converted.txt` supply the historical asm state.

Run from the repository root:  python3 tools/gen_function_map.py
Check:         python3 tools/gen_function_map.py --check
"""

from __future__ import annotations

import csv
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "reference" / "FUNCTION_MAP.md"
CONVERSION = ROOT / "docs" / "reference" / "CONVERSION_MAP.tsv"
INVENTORY = ROOT / "docs" / "reference" / "ASM_INVENTORY.tsv"
CONVERTED = ROOT / "docs" / "reference" / "asm_converted.txt"

VA_RE = re.compile(r"^0x[0-9a-fA-F]{8}$")


def read_conversion() -> list[dict[str, str]]:
    with CONVERSION.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def read_inventory() -> dict[str, dict[str, str]]:
    if not INVENTORY.exists():
        return {}
    with INVENTORY.open(newline="", encoding="utf-8") as handle:
        return {row["path"]: row for row in csv.DictReader(handle, delimiter="\t")}


def read_converted() -> set[str]:
    if not CONVERTED.exists():
        return set()
    return {
        line.strip()
        for line in CONVERTED.read_text(encoding="utf-8").splitlines()
        if line.strip() and not line.lstrip().startswith("#")
    }


def records() -> list[dict[str, str]]:
    inventory = read_inventory()
    converted = read_converted()
    result: list[dict[str, str]] = []
    for row in read_conversion():
        path = row["new_path"]
        inv = inventory.get("src/" + path, {})
        blocks = int(inv.get("code_blocks", "0") or 0)
        if blocks > 0:
            state = f"asm ({blocks} blocks)"
        elif "src/" + path in converted:
            state = "C (ratcheted)"
        else:
            state = "C / alias"
        address = row.get("original_address", "-").strip() or "-"
        result.append({
            "va": address if VA_RE.match(address) else "-",
            "name": row.get("original_symbol", ""),
            "path": "src/" + path,
            "unit": row.get("unit_macro", ""),
            "state": state,
            "blocks": str(blocks),
            "cluster": row.get("module", "") or "ordinary C",
            "original": row.get("original_file", ""),
        })
    result.sort(key=lambda item: (
        item["va"] == "-",
        int(item["va"], 16) if item["va"] != "-" else 0,
        item["name"].lower(),
    ))
    return result


def render(items: list[dict[str, str]]) -> str:
    total_blocks = sum(int(item["blocks"]) for item in items)
    asm_files = sum(1 for item in items if int(item["blocks"]) > 0)
    converted_files = len({item["path"] for item in items if item["state"] == "C (ratcheted)"})
    lines = [
        "# Original-to-replacement function map",
        "",
        "This is the address-ordered index from each original source unit to the",
        "consolidated group file that now implements it.  The authoritative data is",
        "`docs/reference/CONVERSION_MAP.tsv`; `docs/reference/FUNCTION_INDEX.tsv` provides the same index",
        "in a direct original-file-to-implementation-file form.",
        "",
        f"* mapped entries: **{len(items)}**",
        f"* files still carrying executable asm: **{asm_files}**",
        f"* ratcheted C replacements: **{converted_files}**",
        f"* executable asm blocks in the mapped files: **{total_blocks}**",
        "",
        "The `unit selector` column is the preprocessor symbol used when the group",
        "file is compiled as that unit.  One group file contains several independent",
        "units, so the selector is the precise navigation key inside the file.",
        "",
        "This is a historical recovery map.  Some diagnostic units listed here were",
        "removed from the final SDL3 tree; `src/units.tsv` is the current build manifest.",
        "",
        "## Replacement contracts worth preserving",
        "",
        "| original | replacement | contract |",
        "|---|---|---|",
        "| `0x00405ab0` `DispatchPendingCommandLoop` | `src/app/AppLifecycle.c` (`NW_UNIT_DispatchPendingCommandLoop`) | `__stdcall`, `RET 4`; C callers use `_DispatchPendingCommandLoop`; the five window-table anchors `WAnch_00405cd3`/`d00`/`d12`/`d24`/`d78` remain defined in `.text` even though the C body switches directly. |",
        "| `0x00406e48` `RetryLockedProbeDispatch` | `src/runtime/Hooks.c` (`NW_UNIT_RetryLockedProbeDispatch`) | `__stdcall`, undecorated symbol `_RetryLockedProbeDispatch`; the data-window table at `+0x2f8` still points at real `WAnch_00406e84`…`eb0` action functions. |",
        "| `0x0040fb74` `AllocatePooledBlock` | `src/runtime/Pool.c` (`NW_UNIT_AllocatePooledBlock`) | cdecl, returns payload `block+4`; the slot lock uses C11 `__atomic_*` on the 16-bit counter at `+0x641a`. |",
        "| `0x00402fb4` `ProcessQueuedTaskRecords` | `src/app/AppLifecycle.c` (`NW_UNIT_ProcessQueuedTaskRecords`) | defines only AL; callers must cast the result to `uint8_t` before comparing. |",
        "| `0x0040fb20` `CallHookIfInstalled` | `src/runtime/Hooks.c` (`NW_UNIT_CallHookIfInstalled`) | documented deviation: returns 0 for a NULL slot, whereas the shipped asm left EAX untouched. |",
        "| `0x00403d38` `FunAxisWrapClone` | `src/grid/GridCore.c` (`NW_UNIT_WrapGridAxesAndDispatch`) | observer slot is called through a cdecl-typed pointer even though the shipped slot is a `RET 4` no-op; the original does not adjust ESP after that call. |",
        "| `0x0040f290` `InitializeGlobalThreadBufferOrDie` | `src/runtime/Heap.c` (`NW_UNIT_InitializeGlobalThreadBufferOrDie`) | returns the allocated buffer base in EAX, matching the original return value. |",
        "| `0x0040ed04` `DispatchExitHooksAndClearShutdownFlags` | `src/app/AppLifecycle.c` (`NW_UNIT_DispatchExitHooksAndClearShutdownFlags`) | bare symbol (no leading underscore) because the asm exit path calls it by that name. |",
        "| `0x0040f52c` `BuildQuotedCommandImage` | `src/app/win32/AppShutdown.c` (`NW_UNIT_BuildQuotedCommandImage`) | preserves the shipped store-through-the-allocation-result on the OOM path rather than silently converting it to a guard. |",
        "| `0x004101b4` `BuildTokenPointerArrayFromGlobalString` | `src/runtime/RuntimeSupport.c` (`NW_UNIT_BuildTokenPointerArrayFromGlobalString`) | on a token-copy failure returns -1 with the partially populated array still installed. |",
        "",
        "## Mapping",
        "",
        "| original VA | original symbol | replacement file | unit selector | state | blocks | module |",
        "|---|---|---|---|---|---:|---|",
    ]
    for item in items:
        name = item["name"].replace("|", "\\|")
        path = item["path"].replace("|", "\\|")
        unit = item["unit"].replace("|", "\\|")
        cluster = item["cluster"].replace("|", "\\|")
        lines.append(
            f"| `{item['va']}` | `{name}` | `{path}` | `{unit}` | {item['state']} | {item['blocks']} | {cluster} |"
        )
    lines += [
        "",
        "## Regenerating",
        "",
        "```sh",
        "cd <repository root>",
        "python3 tools/gen_function_map.py",
        "python3 tools/gen_function_map.py --check",
        "```",
        "",
        "`--check` fails when the checked-in document differs from the generated one.",
        "Keep `docs/reference/CONVERSION_MAP.tsv`, `docs/reference/FUNCTION_INDEX.tsv`,",
        "`docs/reference/ASM_INVENTORY.tsv`, and `docs/reference/asm_converted.txt`",
        "in the same commit as every source change.",
        "",
    ]
    return "\n".join(lines)


def main() -> int:
    args = sys.argv[1:]
    if "-h" in args or "--help" in args:
        print(__doc__.strip())
        return 0
    unknown = [arg for arg in args if arg != "--check"]
    if unknown:
        print("unknown argument: %s" % unknown[0], file=sys.stderr)
        return 2
    text = render(records())
    if "--check" in args:
        current = OUT.read_text() if OUT.exists() else ""
        if current != text:
            print("FUNCTION_MAP.md is stale; regenerate it with tools/gen_function_map.py", file=sys.stderr)
            return 1
        print("FUNCTION_MAP.md OK")
        return 0
    OUT.write_text(text)
    print(f"wrote {OUT.relative_to(ROOT)} ({len(text.splitlines())} lines)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
