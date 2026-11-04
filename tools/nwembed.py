#!/usr/bin/env python3
"""Embed an external NetWalk overlay into a desktop executable.

The runtime searches for overlay.nw/overlay.bin beside the module and falls
back to the module file itself.  That fallback reads the 38-byte =VS= trailer
from the end of the file.  This tool uses the fallback by appending the overlay
stream and a copy of the trailer to the executable.

The compact embedded layout is:

    [executable][raw overlay][38-byte trailer with dbase = executable size]

The raw overlay starts at the external overlay's dbase, so the 0x12800 pad in
overlay.nw is not copied.  Rewriting dbase to the pre-append executable size
keeps the index entries valid.  The same layout preserves overlays carrying a
high-resolution section because that section remains immediately before the
trailer; the tool does not rewrite or validate overlay contents.

The overlay must remain the final data in the file, and the trailer must remain
the final 38 bytes.  Do not strip or otherwise rewrite the executable after
embedding.  Windows Authenticode appends its certificate table after the image,
which would put data behind the trailer and break the fallback; embedding is
therefore intended for unsigned Windows builds.  Detached signatures over the
final file are fine.
"""

from __future__ import annotations

import argparse
import os
import shutil
import stat
import struct
import sys
import tempfile
from pathlib import Path

TRAILER_SIZE = 38
TRAILER_MAGIC = b"=VS="
DBASE_OFFSET = 19


class EmbedError(Exception):
    pass


def u32(buf: bytes, offset: int) -> int:
    return struct.unpack_from("<I", buf, offset)[0]


def put_u32(buf: bytearray, offset: int, value: int) -> None:
    if not 0 <= value <= 0xFFFFFFFF:
        raise EmbedError("value does not fit in 32 bits: %d" % value)
    struct.pack_into("<I", buf, offset, value)


def load_overlay(path: Path) -> tuple[bytes, bytes, int]:
    try:
        data = path.read_bytes()
    except OSError as exc:
        raise EmbedError("cannot read overlay %s: %s" % (path, exc)) from exc

    if len(data) < TRAILER_SIZE or data[-4:] != TRAILER_MAGIC:
        raise EmbedError("%s: no '=VS=' data trailer at end of file" % path)

    trailer = data[-TRAILER_SIZE:]
    dbase = u32(trailer, DBASE_OFFSET)
    if dbase == 0 or dbase >= len(data) - TRAILER_SIZE:
        raise EmbedError("%s: implausible trailer dbase 0x%x" % (path, dbase))

    raw = data[dbase:-TRAILER_SIZE]
    if not raw:
        raise EmbedError("%s: empty overlay stream" % path)
    return raw, trailer, dbase


def looks_embedded(exe: Path) -> bool:
    """Return true when the file already ends in a plausible embedded trailer."""
    try:
        with exe.open("rb") as f:
            f.seek(0, os.SEEK_END)
            size = f.tell()
            if size < TRAILER_SIZE:
                return False
            f.seek(-TRAILER_SIZE, os.SEEK_END)
            trailer = f.read(TRAILER_SIZE)
    except OSError:
        return False

    if trailer[-4:] != TRAILER_MAGIC:
        return False
    dbase = u32(trailer, DBASE_OFFSET)
    return 0 < dbase <= size - TRAILER_SIZE


def embed(exe: Path, overlay: Path, force: bool) -> None:
    if not exe.is_file():
        raise EmbedError("executable not found: %s" % exe)
    if not overlay.is_file():
        raise EmbedError("overlay not found: %s" % overlay)

    raw, trailer, source_dbase = load_overlay(overlay)
    if looks_embedded(exe) and not force:
        raise EmbedError(
            "%s already ends in an embedded =VS= trailer; pass --force to append another copy"
            % exe
        )

    try:
        info = exe.stat()
        base = info.st_size
        if base <= 0:
            raise EmbedError("executable is empty: %s" % exe)
        if base > 0xFFFFFFFF:
            raise EmbedError("executable is too large for the 32-bit dbase field: %s" % exe)

        patched = bytearray(trailer)
        put_u32(patched, DBASE_OFFSET, base)

        fd, temp_name = tempfile.mkstemp(
            prefix=exe.name + ".", suffix=".embed.tmp", dir=str(exe.parent)
        )
        try:
            with exe.open("rb") as src, os.fdopen(fd, "wb") as dst:
                shutil.copyfileobj(src, dst, 1024 * 1024)
                dst.write(raw)
                dst.write(patched)
                dst.flush()
                os.fsync(dst.fileno())
            os.chmod(temp_name, stat.S_IMODE(info.st_mode))
            os.replace(temp_name, exe)
        except BaseException:
            try:
                os.unlink(temp_name)
            except OSError:
                pass
            raise
    except OSError as exc:
        raise EmbedError("cannot embed overlay into %s: %s" % (exe, exc)) from exc

    total = base + len(raw) + TRAILER_SIZE
    print("embedded overlay into %s" % exe)
    print(
        "  exe_base=0x%x source_dbase=0x%x raw=%d trailer=%d total=%d"
        % (base, source_dbase, len(raw), TRAILER_SIZE, total)
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Append an external NetWalk overlay to a desktop executable."
    )
    parser.add_argument("--exe", required=True, help="executable to modify in place")
    parser.add_argument("--overlay", required=True, help="external overlay.nw to embed")
    parser.add_argument(
        "--force",
        action="store_true",
        help="append even when the executable already looks embedded",
    )
    args = parser.parse_args(argv)

    try:
        embed(Path(args.exe), Path(args.overlay), args.force)
    except EmbedError as exc:
        print("nwembed.py: %s" % exc, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
