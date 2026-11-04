#!/usr/bin/env python3
"""Verify the Gamos 1.51 reference executables and inspect original functions.

The verifier is intentionally limited to the Python standard library. It checks
the two reference binaries by SHA-256, verifies their shared PE sections, checks
the original addresses in docs/reference/CONVERSION_MAP.tsv against the loaded
PE image, and can disassemble a function with GNU objdump.

Examples, from the repository root:

    python3 tools/verify_reference.py --netwalk /path/to/NETWALK.EXE
    python3 tools/verify_reference.py \
        --netwalk /path/to/NETWALK.EXE \
        --flipflop /path/to/FFLOPE.EXE
    python3 tools/verify_reference.py \
        --netwalk /path/to/NETWALK.EXE \
        --disassemble 0x00404b90 --size 42

The reference executables are not part of this repository. Use legally obtained
copies. The release names in the output are the historical Gamos release names;
the local working files may have different names.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import shutil
import struct
import subprocess
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_MAP = ROOT / "docs" / "reference" / "CONVERSION_MAP.tsv"

# Address used by the portable fixed-layout build for the recovered data
# window. It is not file-backed in the reference PE.
DATA_WINDOW_VA = 0x00418030

# The Gamos runtime is the same in both verified releases. These hashes cover
# the raw section contents, not the PE header.
ENGINE_SECTIONS = {
    ".idata": "9a101af9a5266a10d74bbfcbdecdcf02789f29f310c6c37c23bc98b653deb879",
    "_TEXT": "a8d3ca1ac4a06064b3c57eee97af4bb21b6863483b84cb59726cd0b2dd41e8cb",
    ".CRT$XIA": "f182342471f90e82cbed562b05fc3cd1a2b18ac7286be33eef7700e9064d597a",
    ".reloc": "e2a9909877ba782a8d97074979bb56f03a977c50af6755c048187343db1c842e",
}


@dataclass(frozen=True)
class Reference:
    release: str
    sha256: str
    size: int
    rsrc_sha256: str


REFERENCES = {
    "netwalk": Reference(
        release="NetWalk_v1.51_Arsenal eng",
        sha256="258d7406ea98bf3088e90d8dd9988fbdc2e270ad75869a2a584c429855bffb9d",
        size=4_979_981,
        rsrc_sha256="129041bb97098aa5e201a96bfd0caa0eaf8390a4c04ba78c8c9a363792eef4e8",
    ),
    "flipflop": Reference(
        release="FlipFlop_v1.51_Arsenal_Eng",
        sha256="b6e34011349cd2de386914bba542dc05c81a66cfe013827f77b2889d9beeb1b2",
        size=4_633_340,
        rsrc_sha256="d37f714c516f638f8f5b5ab7081974d8eb32ab767fe113893307b404eb1eeaf7",
    ),
}


@dataclass(frozen=True)
class Section:
    name: str
    virtual_address: int
    virtual_size: int
    raw_size: int
    raw_offset: int
    characteristics: int

    @property
    def executable(self) -> bool:
        return bool(self.characteristics & 0x20000000)

    @property
    def end(self) -> int:
        return self.virtual_address + max(self.virtual_size, self.raw_size)


class PEImage:
    """Minimal PE32/PE32+ reader for address and section inspection."""

    def __init__(self, path: Path):
        self.path = path
        self.data = path.read_bytes()
        if self.data[:2] != b"MZ":
            raise ValueError("not an MZ executable")
        pe_offset = self._u32(0x3C)
        if self.data[pe_offset:pe_offset + 4] != b"PE\0\0":
            raise ValueError("missing PE signature")
        coff = pe_offset + 4
        self.machine = self._u16(coff)
        self.number_of_sections = self._u16(coff + 2)
        optional_size = self._u16(coff + 16)
        optional = coff + 20
        magic = self._u16(optional)
        if magic == 0x10B:
            self.pe32_plus = False
            self.image_base = self._u32(optional + 28)
        elif magic == 0x20B:
            self.pe32_plus = True
            self.image_base = self._u64(optional + 24)
        else:
            raise ValueError(f"unknown optional-header magic 0x{magic:04x}")

        section_table = optional + optional_size
        self.sections: list[Section] = []
        for index in range(self.number_of_sections):
            offset = section_table + index * 40
            name = self.data[offset:offset + 8].rstrip(b"\0").decode("latin-1")
            virtual_size, virtual_address, raw_size, raw_offset = struct.unpack_from(
                "<IIII", self.data, offset + 8
            )
            characteristics = self._u32(offset + 36)
            self.sections.append(
                Section(
                    name=name,
                    virtual_address=virtual_address,
                    virtual_size=virtual_size,
                    raw_size=raw_size,
                    raw_offset=raw_offset,
                    characteristics=characteristics,
                )
            )
        self.section_by_name = {section.name: section for section in self.sections}

    def _u16(self, offset: int) -> int:
        return struct.unpack_from("<H", self.data, offset)[0]

    def _u32(self, offset: int) -> int:
        return struct.unpack_from("<I", self.data, offset)[0]

    def _u64(self, offset: int) -> int:
        return struct.unpack_from("<Q", self.data, offset)[0]

    def section_for(self, va: int) -> Section | None:
        rva = va - self.image_base
        if rva < 0:
            return None
        for section in self.sections:
            if section.virtual_address <= rva < section.end:
                return section
        return None

    def file_offset(self, va: int, size: int = 1) -> int | None:
        section = self.section_for(va)
        if section is None:
            return None
        delta = va - self.image_base - section.virtual_address
        if delta < 0 or delta + size > section.raw_size:
            return None
        return section.raw_offset + delta

    def read_bytes(self, va: int, size: int) -> bytes | None:
        offset = self.file_offset(va, size)
        if offset is None:
            return None
        return self.data[offset:offset + size]

    def section_hash(self, name: str) -> str | None:
        section = self.section_by_name.get(name)
        if section is None:
            return None
        raw = self.data[section.raw_offset:section.raw_offset + section.raw_size]
        return hashlib.sha256(raw).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def format_int(value: int) -> str:
    return f"0x{value:08x}"


def check_reference(kind: str, path: Path) -> tuple[PEImage | None, bool]:
    reference = REFERENCES[kind]
    print(f"\n{reference.release}")
    print(f"  file:   {path}")
    ok = True
    try:
        data_size = path.stat().st_size
        actual_hash = sha256_file(path)
    except OSError as exc:
        print(f"  result: FAIL ({exc})")
        return None, False

    print(f"  size:   {data_size:,} bytes")
    print(f"  sha256: {actual_hash}")
    if actual_hash != reference.sha256:
        print(f"  expected sha256: {reference.sha256}")
        ok = False
    if data_size != reference.size:
        print(f"  expected size:   {reference.size:,} bytes")
        ok = False

    try:
        pe = PEImage(path)
    except (OSError, ValueError, struct.error) as exc:
        print(f"  result: FAIL ({exc})")
        return None, False

    print(
        f"  PE:     {'PE32+' if pe.pe32_plus else 'PE32'}, "
        f"machine 0x{pe.machine:04x}, image base {format_int(pe.image_base)}"
    )
    if pe.machine != 0x014C:
        print("  expected machine: 0x014c (i386)")
        ok = False
    if pe.image_base != 0x00400000:
        print("  expected image base: 0x00400000")
        ok = False

    expected_sections = dict(ENGINE_SECTIONS)
    expected_sections[".rsrc"] = reference.rsrc_sha256
    for name, expected in expected_sections.items():
        actual = pe.section_hash(name)
        section = pe.section_by_name.get(name)
        if section is None:
            print(f"  {name:<10} missing")
            ok = False
            continue
        status = "ok" if actual == expected else "MISMATCH"
        if actual != expected:
            ok = False
        print(
            f"  {name:<10} {format_int(pe.image_base + section.virtual_address)}"
            f"-{format_int(pe.image_base + section.end - 1)}  {actual}  {status}"
        )

    print(f"  result: {'OK' if ok else 'FAIL'}")
    return pe, ok


def check_shared(netwalk: PEImage, flipflop: PEImage) -> bool:
    print("\nShared runtime sections")
    ok = True
    for name in ENGINE_SECTIONS:
        left = netwalk.section_hash(name)
        right = flipflop.section_hash(name)
        if left == right:
            print(f"  {name:<10} identical  {left}")
        else:
            print(f"  {name:<10} DIFFERENT")
            ok = False
    left_rsrc = netwalk.section_hash(".rsrc")
    right_rsrc = flipflop.section_hash(".rsrc")
    if left_rsrc == right_rsrc:
        print("  .rsrc      identical (unexpected for these releases)")
        ok = False
    else:
        print("  .rsrc      differs (expected: game resources differ)")
    return ok


def check_map(pe: PEImage, map_path: Path) -> bool:
    print(f"\nConversion map: {map_path}")
    try:
        with map_path.open(newline="", encoding="utf-8") as handle:
            rows = list(csv.DictReader(handle, delimiter="\t"))
    except OSError as exc:
        print(f"  result: FAIL ({exc})")
        return False

    missing = 0
    malformed: list[tuple[str, str]] = []
    by_address: dict[int, list[str]] = defaultdict(list)
    image_base = 0
    data_window = 0
    code = 0
    data_sections: Counter[str] = Counter()
    unbacked: list[tuple[int, str]] = []
    code_without_bytes: list[tuple[int, str]] = []

    for row in rows:
        raw = (row.get("original_address") or "").strip()
        name = row.get("original_symbol", "")
        if not raw or raw == "-":
            missing += 1
            continue
        try:
            va = int(raw, 16)
        except ValueError:
            malformed.append((raw, name))
            continue
        by_address[va].append(name)

        if va == pe.image_base:
            image_base += 1
            continue
        if va == DATA_WINDOW_VA:
            data_window += 1
            continue
        section = pe.section_for(va)
        if section is None:
            unbacked.append((va, name))
            continue
        if section.executable:
            code += 1
            if pe.file_offset(va, 1) is None:
                code_without_bytes.append((va, name))
        else:
            data_sections[section.name] += 1

    duplicates = {va: names for va, names in by_address.items() if len(names) > 1}
    print(f"  entries:        {len(rows)}")
    print(f"  addresses:      {len(by_address)} distinct ({missing} without an address)")
    print(f"  executable:     {code}")
    for name, count in sorted(data_sections.items()):
        print(f"  data {name:<10} {count}")
    print(f"  image base:     {image_base}")
    print(f"  data window:    {data_window} ({format_int(DATA_WINDOW_VA)})")
    if unbacked:
        print("  unbacked:       " + ", ".join(
            f"{format_int(va)} {name}" for va, name in unbacked
        ))
    if duplicates:
        print("  duplicate VAs:  " + ", ".join(
            f"{format_int(va)} ({len(names)} entries)"
            for va, names in sorted(duplicates.items())
        ))

    ok = not malformed and not code_without_bytes
    if malformed:
        print("  malformed addresses: " + ", ".join(raw for raw, _ in malformed))
    if code_without_bytes:
        print("  executable addresses without file bytes: " + ", ".join(
            format_int(va) for va, _ in code_without_bytes
        ))
    print(f"  result: {'OK' if ok else 'FAIL'}")
    return ok


def disassemble(pe: PEImage, raw_address: str, size: int) -> bool:
    try:
        va = int(raw_address, 16)
    except ValueError:
        print(f"\ninvalid address: {raw_address}", file=sys.stderr)
        return False

    section = pe.section_for(va)
    if section is not None:
        stop = min(va + size, pe.image_base + section.end)
    else:
        stop = va + size
    image = pe.read_bytes(va, max(0, stop - va))
    print(f"\n{format_int(va)}  ({stop - va} bytes, file offset ", end="")
    offset = pe.file_offset(va, max(1, stop - va))
    print(f"{format_int(offset) if offset is not None else 'not file-backed'})")

    objdump = shutil.which("objdump")
    if objdump is None:
        if image is None:
            print("  no file-backed bytes; install binutils for disassembly", file=sys.stderr)
            return False
        print("  objdump not found; raw bytes:")
        for line_start in range(0, len(image), 16):
            chunk = image[line_start:line_start + 16]
            print(f"  {format_int(va + line_start)}  {chunk.hex(' ')}")
        return False

    command = [
        objdump,
        "-D",
        "-M",
        "intel",
        f"--start-address={format_int(va)}",
        f"--stop-address={format_int(stop)}",
        str(pe.path),
    ]
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  objdump failed: {result.stderr.strip()}", file=sys.stderr)
        return False
    print(result.stdout.rstrip())
    return True


def parse_int(value: str) -> int:
    try:
        return int(value, 0)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"invalid integer: {value}") from exc


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Verify the Gamos 1.51 reference executables and inspect original functions."
    )
    parser.add_argument("--netwalk", type=Path, help="path to the NetWalk reference executable")
    parser.add_argument("--flipflop", type=Path, help="path to the FlipFlop reference executable")
    parser.add_argument(
        "--map",
        type=Path,
        default=DEFAULT_MAP,
        help=f"conversion map (default: {DEFAULT_MAP.relative_to(ROOT)})",
    )
    parser.add_argument(
        "--disassemble",
        nargs="+",
        metavar="VA",
        help="disassemble bytes at one or more virtual addresses (for example 0x00404b90)",
    )
    parser.add_argument(
        "--size",
        type=parse_int,
        default=64,
        help="bytes to show per --disassemble address (default: 64)",
    )
    args = parser.parse_args()

    if args.netwalk is None and args.flipflop is None:
        parser.error("provide --netwalk and/or --flipflop")
    if args.size <= 0:
        parser.error("--size must be positive")

    failures: list[str] = []
    netwalk: PEImage | None = None
    flipflop: PEImage | None = None

    if args.netwalk is not None:
        netwalk, ok = check_reference("netwalk", args.netwalk)
        if not ok:
            failures.append("NetWalk reference check failed")
    if args.flipflop is not None:
        flipflop, ok = check_reference("flipflop", args.flipflop)
        if not ok:
            failures.append("FlipFlop reference check failed")
    if netwalk is not None and flipflop is not None:
        if not check_shared(netwalk, flipflop):
            failures.append("shared runtime section check failed")
    if netwalk is not None:
        if not check_map(netwalk, args.map):
            failures.append("conversion map check failed")

    if args.disassemble:
        target = netwalk or flipflop
        if target is None:
            failures.append("no reference image available for disassembly")
        else:
            for address in args.disassemble:
                if not disassemble(target, address, args.size):
                    failures.append(f"disassembly failed at {address}")

    if failures:
        print("\nFAIL: " + "; ".join(failures), file=sys.stderr)
        return 1
    print("\nOK: reference binaries and conversion map verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
