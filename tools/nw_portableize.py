#!/usr/bin/env python3
"""Generate a C-only, assembly-free copy of the recovered NetWalk sources.

The checked-in corpus is deliberately close to the recovered disassembly and
uses GNU assembly for:
  * the initial data-window image (baseline/window_snapshot.s),
  * symbol aliases matching the original leading-underscore names.

This script emits a build-tree copy of src/ plus windows/win64_compat.c and
windows/win64_entry.c with those constructs replaced by plain C.  It also
generates:

  nw_window_data.c        initial data-window bytes + pointer fixup table
  nw_portable_globals.h   macros for the recovered window globals

The CMake portable target compiles the generated tree.  It does not use
window_snapshot.s, window_globals.args, -fleading-underscore, or __asm__.
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def strip_asm(text: str) -> str:
    """Remove every __asm__( ... ) construct, preserving C declarations.

    It is a small lexical scanner rather than a regex so the multi-line
    .globl/.set blocks and the standalone __asm__ statements both disappear
    without touching nearby C.
    """
    out: list[str] = []
    i = 0
    n = len(text)
    in_line = False
    in_block = False
    in_string = False
    while i < n:
        c = text[i]
        if in_line:
            out.append(c)
            i += 1
            if c == "\n":
                in_line = False
            continue
        if in_block:
            out.append(c)
            i += 1
            if c == "*" and i < n and text[i] == "/":
                out.append("/")
                i += 1
                in_block = False
            continue
        if in_string:
            out.append(c)
            i += 1
            if c == "\\" and i < n:
                out.append(text[i])
                i += 1
            elif c == '"':
                in_string = False
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            out.append("//")
            i += 2
            in_line = True
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            out.append("/*")
            i += 2
            in_block = True
            continue
        if c == '"':
            out.append(c)
            i += 1
            in_string = True
            continue
        if text.startswith("__asm__", i):
            start = i
            k = i + len("__asm__")
            while k < n and text[k].isspace():
                k += 1
            if k < n and text[k] == "(":
                depth = 0
                while k < n:
                    if text[k] == '"':
                        k += 1
                        while k < n:
                            if text[k] == "\\":
                                k += 2
                                continue
                            if text[k] == '"':
                                k += 1
                                break
                            k += 1
                        continue
                    if text[k] == "(":
                        depth += 1
                    elif text[k] == ")":
                        depth -= 1
                        if depth == 0:
                            k += 1
                            break
                    k += 1
                j = k
                while j < n and text[j] in " \t":
                    j += 1
                prev = ""
                p = start - 1
                while p >= 0 and text[p].isspace():
                    p -= 1
                if p >= 0:
                    prev = text[p]
                # A declarator suffix is followed by a semicolon that belongs
                # to the C declaration.  A standalone asm statement is not.
                if j < n and text[j] == ";" and (
                        prev == ")" or prev.isalnum() or prev == "_"):
                    out.append(";")
                    i = j + 1
                else:
                    i = j
                continue
            # Not a statement shape; drop the token only.
            i = start + len("__asm__")
            continue
        out.append(c)
        i += 1
    return "".join(out)



def strip_attribute_constructs(text: str, keep_constructors: bool = False) -> str:
    """Remove any remaining GNU __attribute__((...)) construct."""
    out: list[str] = []
    i = 0
    n = len(text)
    while i < n:
        j = text.find("__attribute__", i)
        if j < 0:
            out.append(text[i:])
            break
        out.append(text[i:j])
        k = j + len("__attribute__")
        while k < n and text[k].isspace():
            k += 1
        attr_start = j
        if k < n and text[k] == "(":
            depth = 0
            in_str = False
            while k < n:
                c = text[k]
                if in_str:
                    if c == "\\":
                        k += 2
                        continue
                    if c == '"':
                        in_str = False
                elif c == '"':
                    in_str = True
                elif c == "(":
                    depth += 1
                elif c == ")":
                    depth -= 1
                    if depth == 0:
                        k += 1
                        break
                k += 1
        if keep_constructors and "constructor" in text[attr_start:k]:
            out.append(text[attr_start:k])
        i = k
    return "".join(out)
def strip_attributes(text: str, keep_constructors: bool = False) -> str:
    text = re.sub(
        r"__attribute__\s*\(\(\s*section\s*\([^)]*\)\s*\)\)[ \t]*", "", text)
    text = re.sub(
        r"typedef\s+struct\s+__attribute__\s*\(\(\s*__packed__\s*\)\)"
        r"\s+(\w+)\s*\{(.*?)\}\s*\1\s*;",
        r"#pragma pack(push, 1)\ntypedef struct \1 {\2} \1;\n"
        r"#pragma pack(pop)",
        text, flags=re.S)
    text = text.replace("__attribute__((__packed__))", "")
    text = text.replace("__attribute__((dllimport))", "")
    text = text.replace("__attribute__((noreturn))", "")
    text = text.replace("__attribute__((fallthrough));", "/* fallthrough */;")
    # Any remaining GNU attribute is not meaningful for the portable build.
    text = strip_attribute_constructs(text, keep_constructors)
    return text


def parse_globals(path: Path) -> dict[str, tuple[int, bool]]:
    result: dict[str, tuple[int, bool]] = {}
    for line in read(path).splitlines():
        m = re.match(r"-Wl,--defsym,([^=]+)=(.+)", line.strip())
        if not m:
            continue
        name, target = m.group(1), m.group(2)
        if name.startswith("_"):
            continue
        sm = re.search(r"_g_nw_window_ptr_slots\+0x([0-9a-fA-F]+)", target)
        if sm:
            result[name] = (int(sm.group(1), 16), True)
            continue
        wm = re.search(r"nw_data_window\+0x([0-9a-fA-F]+)", target)
        if wm:
            result[name] = (int(wm.group(1), 16), False)
    return result


def parse_decls(files: list[Path], names: set[str]) -> dict[str, tuple[str, str]]:
    """Return name -> (declaration text without extern/name/semicolon)."""
    result: dict[str, tuple[str, str]] = {}
    for path in files:
        text = read(path)
        for line in text.splitlines():
            if "extern" not in line or ";" not in line:
                continue
            if "__asm__" in line:
                # Keep the declarator before the asm suffix.
                line = line.split("__asm__", 1)[0] + ";"
            line = re.sub(r"/\*.*?\*/", " ", line)
            line = line.strip()
            m = re.match(r"extern\s+(.+?)\s*;", line)
            if not m:
                continue
            decl = m.group(1).strip()
            hit = None
            for name in names:
                if re.search(r"\b" + re.escape(name) + r"\b", decl):
                    hit = name
                    break
            if not hit:
                continue
            result.setdefault(hit, (decl, str(path)))
    return result


def decl_to_macro(name: str, decl: str, offset: int, side: bool) -> str:
    """Turn one C external declaration into a typed window-access macro.

    A typedef of the complete declarator is generated first.  That handles
    scalar values, arrays, function pointers, and arrays of function pointers
    without trying to reconstruct C declarator syntax in a string.
    """
    m = re.search(r"\b" + re.escape(name) + r"\b", decl)
    if not m:
        return ""
    before = decl[:m.start()].rstrip()
    after = decl[m.end():].lstrip()
    typedef_name = "NW_T_" + name + "__t"
    guard_name = "NW_T_" + name + "__defined"

    addr = ("(void *)(&g_nw_window_ptr_slots[%du])" % (offset // 8)
            if side else "(void *)(nw_data_window + 0x%xu)" % offset)

    # An incomplete [] cannot be dereferenced through a pointer-to-array.
    # The games uses these as flexible buffers, so a one-element array is a
    # valid C spelling of the same storage for indexing and pointer decay.
    if after.strip() == "[]":
        after = "[1]"
    typedef_decl = (before + " " + typedef_name + " " + after).strip()
    return ("#ifndef %s\n#define %s 1\ntypedef %s;\n#endif\n"
            "#define %s (*(%s *)%s)"
            % (guard_name, guard_name, typedef_decl, name, typedef_name, addr))


def collect_c_aliases(files: list[Path]) -> dict[str, str]:
    """Collect .set aliases and the small Windows-thunk aliases."""
    aliases: dict[str, str] = {}
    for path in files:
        text = read(path)
        for m in re.finditer(
                r"\.set\s+([A-Za-z_]\w*)\s*,\s*@?([A-Za-z_]\w*)@?\d*",
                text):
            alias = m.group(1).lstrip("_")
            target = m.group(2).lstrip("_")
            if alias and target and alias != target:
                aliases[alias] = target
    aliases.update({
        "WinGetCurrentThreadId": "GetCurrentThreadId",
        "WinGetEnvironmentStringsA": "GetEnvironmentStringsA",
        "WinGetVersion": "GetVersion",
    })
    return aliases

def collect_asm_symbols(files: list[Path]) -> dict[str, str]:
    mapping: dict[str, str] = {}
    for path in files:
        text = read(path)
        for m in re.finditer(r"__asm__\s*\(\s*\"([^\"]+)\"\s*\)", text):
            symbol = m.group(1)
            prefix = text[:m.start()]
            pm = re.search(r"([A-Za-z_]\w*)\s*\([^()]*\)\s*$", prefix)
            if pm and pm.group(1) not in ("__attribute__", "__declspec"):
                mapping[symbol] = pm.group(1)
        for m in re.finditer(
                r"NW_RNG_TABLE_HANDLER\(\s*([A-Za-z_]\w*)\s*,\s*\"([^\"]+)\"",
                text):
            mapping[m.group(2)] = m.group(1)
    mapping.update({
        "WAnch_00405cd3": "DispatchPendingCommandLoop",
        "WAnch_00405d00": "DispatchPendingCommandLoop",
        "WAnch_00405d12": "DispatchPendingCommandLoop",
        "WAnch_00405d24": "DispatchPendingCommandLoop",
        "WAnch_00405d78": "DispatchPendingCommandLoop",
        "WAnch_0040df7e": "InitializeAndRunWinMain",
        "WAnch_0040df8a": "InitializeAndRunWinMain",
    })
    return mapping

def symbol_expr(sym: str, asm_map: dict[str, str]) -> tuple[str, str | None]:
    sym = sym.strip()
    if sym == "0":
        return "(void *)0", None
    m = re.match(r"nw_data_window\s*\+\s*0x([0-9a-fA-F]+)$", sym)
    if m:
        return "(void *)(nw_data_window + 0x%s)" % m.group(1), None
    m = re.match(r"([A-Za-z_]\w*)\s*\+\s*(\d+)$", sym)
    if m:
        base, off = m.group(1), m.group(2)
        cname = asm_map.get(base, base[1:] if base.startswith("_") else base)
        return ("(void *)((char *)&%s + %s)" % (cname, off)), cname
    cname = asm_map.get(sym, sym[1:] if sym.startswith("_") else sym)
    return ("(void *)&%s" % cname), cname


def parse_snapshot(path: Path) -> tuple[bytearray, list[tuple[int, str]]]:
    data = bytearray()
    longs: list[tuple[int, str]] = []
    for line in read(path).splitlines():
        s = line.strip()
        if not s or s.startswith("/*") or s.startswith("*"):
            continue
        if s.startswith(".byte"):
            for x in s[5:].split(","):
                if x.strip():
                    data.append(int(x.strip(), 0))
        elif s.startswith(".long"):
            longs.append((len(data), s[5:].strip()))
            data.extend(b"\0\0\0\0")
    return data, longs


def c_bytes(data: bytes) -> str:
    lines = []
    for i in range(0, len(data), 16):
        lines.append("    " + ",".join("0x%02x" % b for b in data[i:i + 16]) + ",")
    return "\n".join(lines)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--globals", type=Path, default=None)
    ap.add_argument("--platform", choices=("windows", "linux", "android", "ios"),
                    default="windows")
    args = ap.parse_args()
    root = args.root.resolve()
    out = args.out.resolve()
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)

    globals_path = args.globals or (root / "build" / "window_globals.args")
    (root / "build").mkdir(exist_ok=True)
    if not globals_path.is_file():
        subprocess.run([sys.executable, str(root / "tools" / "window_globals.py"),
                        "emit", "--ptr-side-table"], cwd=root, check=True)
        globals_path = root / "build" / "window_globals.args"
    globals_map = parse_globals(globals_path)

    src_files: list[Path] = []
    for p in sorted((root / "src").rglob("*")):
        if p.suffix in (".c", ".h"):
            src_files.append(p)
    platform_root = root / "platforms"
    if args.platform == "linux":
        platform_files = [platform_root / "linux" / "linux_compat.c",
                          platform_root / "linux" / "linux_main.c"]
    elif args.platform == "android":
        platform_files = [platform_root / "linux" / "linux_compat.c",
                          platform_root / "android" / "android_main.c"]
    elif args.platform == "ios":
        platform_files = [platform_root / "linux" / "linux_compat.c",
                          platform_root / "ios" / "ios_main.c"]
    else:
        platform_files = [root / "windows" / "win64_compat.c",
                          root / "windows" / "win64_entry.c"]
    all_files = src_files + platform_files
    decls = parse_decls(all_files, set(globals_map))
    asm_map = collect_asm_symbols(all_files)
    c_aliases = collect_c_aliases(all_files)
    if args.platform in ("linux", "android", "ios"):
        # linux_compat.c contains real WinGet* thunk functions; the Windows
        # wrappers are not aliases in the native build.
        for key in ("WinGetCurrentThreadId", "WinGetEnvironmentStringsA",
                    "WinGetVersion"):
            c_aliases.pop(key, None)

    # Copy and transform the source tree.  Each extern declaration is
    # replaced at its own site with a typed macro, because different units
    # deliberately declare the same recovered slot with different C types
    # (for example g_pSlotsTop as both uint8_t * and uintptr_t).
    for p in src_files:
        rel = p.relative_to(root)
        text = strip_attributes(strip_asm(read(p)))
        for alias, target in c_aliases.items():
            text = re.sub(r"\b" + re.escape(alias) + r"\b", target, text)
        lines = []
        for line in text.splitlines(keepends=True):
            stripped = line.strip()
            if stripped.startswith("extern") and ";" in stripped:
                decl_text = re.sub(r"/\*.*?\*/", " ", stripped)
                decl_text = decl_text.split("//", 1)[0].strip()
                decl_text = decl_text.split(";", 1)[0].strip()
                if decl_text.startswith("extern"):
                    decl_text = decl_text[7:].strip()
                hit = None
                for n in globals_map:
                    if re.search(r"\b" + re.escape(n) + r"\b", decl_text):
                        hit = n
                        break
                if hit is not None:
                    off, side = globals_map[hit]
                    macro = decl_to_macro(hit, decl_text, off, side)
                    if macro:
                        lines.append("#ifndef %s\n%s\n#endif\n" % (hit, macro))
                        continue
            lines.append(line)
        write(out / rel, "".join(lines))
    for p in platform_files:
        text = strip_attributes(strip_asm(read(p)),
                                keep_constructors=(args.platform in
                                                   ("linux", "android", "ios")))
        for alias, target in c_aliases.items():
            text = re.sub(r"\b" + re.escape(alias) + r"\b", target, text)
        if p.name == "win64_compat.c":
            text = text.replace(
                "nw_host_ptr_t g_nw_window_ptr_slots[NW_WINDOW_BYTES / 4u];",
                "nw_host_ptr_t g_nw_window_ptr_slots[NW_WINDOW_BYTES / 4u];\n"
                "extern void NwWindowApplyFixups(void);")
            text = text.replace(
                "        g_nw_window_ptr_slots[i] = (nw_host_ptr_t)(uintptr_t)value;\n"
                "    }\n}",
                "        g_nw_window_ptr_slots[i] = (nw_host_ptr_t)(uintptr_t)value;\n"
                "    }\n    NwWindowApplyFixups();\n}")
        if p.name == "linux_compat.c":
            text = text.replace(
                "nw_host_ptr_t g_nw_window_ptr_slots[NW_WINDOW_BYTES / 4u];",
                "nw_host_ptr_t g_nw_window_ptr_slots[NW_WINDOW_BYTES / 4u];\n"
                "extern void NwWindowApplyFixups(void);")
            text = text.replace(
                "        g_nw_window_ptr_slots[i] = (nw_host_ptr_t)(uintptr_t)value;\n"
                "    }\n}",
                "        g_nw_window_ptr_slots[i] = (nw_host_ptr_t)(uintptr_t)value;\n"
                "    }\n    NwWindowApplyFixups();\n}")
        write(out / p.parent.name / p.name, text)

    # Global access macros.  `#ifndef` lets nwglobals.h keep any equivalent
    # macro it already provides.
    hdr = [
        "#ifndef NW_PORTABLE_GLOBALS_H",
        "#define NW_PORTABLE_GLOBALS_H",
        "#include <stdint.h>",
        "#include <stddef.h>",
        "#include <string.h>",
        "/* Guest/host address-space seam (nw_guest_base, nw_guest_to_host). */",
        "#include \"nwarch.h\"",
        "#ifdef _MSC_VER",
        "#include <intrin.h>",
        "#define __builtin_memcpy memcpy",
        "#define __builtin_memset memset",
        "#define __builtin_return_address(n) _ReturnAddress()",
        "#define __ATOMIC_SEQ_CST 0",
        "static __forceinline uint16_t __atomic_add_fetch(volatile uint16_t *p, uint16_t v, int m) { (void)m; return (uint16_t)(_InterlockedExchangeAdd16((volatile short *)p, (short)v) + v); }",
        "static __forceinline uint16_t __atomic_sub_fetch(volatile uint16_t *p, uint16_t v, int m) { (void)m; return (uint16_t)(_InterlockedExchangeAdd16((volatile short *)p, -(short)v) - v); }",
        "#endif",
        "#if !defined(_WIN32) && !defined(__int64)",
        "#define __int64 long long",
        "#endif",
        "extern unsigned char nw_data_window[];",
        "#if UINTPTR_MAX > 0xffffffffu",
        "extern uintptr_t g_nw_window_ptr_slots[];",
        "#endif",
        "",
    ]
    hdr.append("#endif /* NW_PORTABLE_GLOBALS_H */")
    write(out / "src" / "include" / "nw_portable_globals.h", "\n".join(hdr) + "\n")

    # Data window and native-pointer fixup table.
    data, longs = parse_snapshot(root / "baseline" / "window_snapshot.s")
    if len(data) != 0x6820:
        raise SystemExit("unexpected window size: 0x%x" % len(data))
    fix_lines = []
    unresolved = []
    cnames = set()
    for off, sym in longs:
        expr, cname = symbol_expr(sym, asm_map)
        if cname is None and sym != "0" and not sym.startswith("nw_data_window"):
            unresolved.append((off, sym))
        if cname:
            cnames.add(cname)
        fix_lines.append("    { 0x%04xu, %s }," % (off, expr))

    window_c = [
        '#include "nwarch.h"',
        '#include <stdint.h>',
        '#include <stddef.h>',
        "",
    ]
    for cn in sorted(cnames):
        if re.match(r"^[A-Za-z_]\w*$", cn):
            window_c.append("extern void %s(void);" % cn)
    window_c += [
        "",
        "typedef struct { uint32_t offset; void *value; } NwWindowFixup;",
        "",
        "#ifdef _MSC_VER",
        "__declspec(align(16))",
        "#else",
        "__attribute__((aligned(16)))",
        "#endif",
        "unsigned char nw_data_window[0x6820] = {",
        c_bytes(data),
        "};",
        "",
        "/* Guest-window base.  Zero for every layout whose window lives below",
        " * 4 GiB (the original i686 image and the Linux/Android low builds);",
        " * the 4 GiB-aligned high-image proxy and iOS use a non-zero base. */",
        "uintptr_t nw_guest_base = 0;",
        "",
        "static const NwWindowFixup nw_window_fixups[] = {",
        "\n".join(fix_lines),
        "};",
        "",
        "void NwWindowApplyFixups(void)",
        "{",
        "    unsigned i;",
        "    /* The window is always 4 GiB-aligned in guest terms: truncating to a",
        "     * 32-bit word yields the guest offset, so the base is the high part",
        "     * of the window address.  It is zero for every low layout. */",
        "    nw_guest_base = (uintptr_t)nw_data_window & ~(uintptr_t)0xFFFFFFFFu;",
        "    for (i = 0; i < sizeof nw_window_fixups / sizeof nw_window_fixups[0]; i++) {",
        "        uintptr_t v = (uintptr_t)nw_window_fixups[i].value;",
        "        *(uint32_t *)(void *)(nw_data_window + nw_window_fixups[i].offset) = (uint32_t)v;",
        "#if NW_WINDOW_PTR_SIDETABLE",
        "        g_nw_window_ptr_slots[nw_window_fixups[i].offset >> 2] = v;",
        "#endif",
        "    }",
        "}",
        "",
    ]
    write(out / "nw_window_data.c", "\n".join(window_c))

    print("portable source tree:", out)
    print("globals:", len(globals_map), "macros:", sum(
        1 for n in globals_map if n in decls))
    print("window bytes:", len(data), "pointer fixups:", len(longs))
    if unresolved:
        print("WARNING: unresolved snapshot symbols:")
        for off, sym in unresolved:
            print("  0x%04x %s" % (off, sym))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
