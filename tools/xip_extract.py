#!/usr/bin/env python3
"""Extract files from an Apple .xip archive on Linux.

The .xip container is a XAR archive whose ``Content`` member is a PBZX
stream.  PBZX chunks contain a cpio (newc) archive.  This tool streams the
whole chain so it can extract a subtree (normally the iPhoneOS SDK) without
materialising the full Xcode tree.

The only external tools required are ``bsdtar`` (read the XAR member) and,
for LZFSE-compressed payloads, ``lzfse``.  XZ/LZMA payloads use Python's
standard library.

Typical use::

    xip_extract.py --xip Xcode.xip --out /cache/sdk \
        --prefix Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/

Use ``--list`` to inspect matching paths without writing anything.
"""

from __future__ import annotations

import argparse
import lzma
import os
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path, PurePosixPath

PBZX_MAGIC = b"pbzx"
XZ_MAGIC = b"\xfd7zXZ\x00"
LZFSE_MAGICS = (b"bvx2", b"bvxn", b"bvx-", b"bvx$")
CPIO_NEWC_MAGICS = (b"070701", b"070702")
CPIO_ODC_MAGIC = b"070707"
CPIO_TRAILER = "TRAILER!!!"
DEFAULT_PREFIX = (
    "Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/"
    "Developer/SDKs/"
)


class XipError(RuntimeError):
    pass


class PBZXStream:
    """Sequential reader over a PBZX stream's decompressed blocks.

    The header is ``pbzx`` followed by a 64-bit block size.  Each block is
    ``uncompressed_size``, ``compressed_size`` and the payload.  A block is
    stored literally when the two sizes are equal.  Xcode 26.x ends with a
    short final block whose size is not the usual 16 MiB, so the reader must
    run until EOF rather than stopping when the 16 MiB bit disappears.
    """

    def __init__(self, raw):
        self._raw = raw
        self._buf = bytearray()
        self._eof = False
        header = raw.read(12)
        if len(header) != 12 or header[:4] != PBZX_MAGIC:
            raise XipError("not a PBZX stream")
        self._block_size = struct.unpack(">Q", header[4:12])[0]

    def _next_chunk(self):
        if self._eof:
            return None
        raw_uncompressed = self._raw.read(8)
        if len(raw_uncompressed) == 0:
            self._eof = True
            return None
        if len(raw_uncompressed) != 8:
            raise XipError("truncated PBZX block size")
        uncompressed_size = struct.unpack(">Q", raw_uncompressed)[0]
        raw_compressed = self._raw.read(8)
        if len(raw_compressed) != 8:
            raise XipError("truncated PBZX compressed size")
        compressed_size = struct.unpack(">Q", raw_compressed)[0]
        data = self._raw.read(compressed_size)
        if len(data) != compressed_size:
            raise XipError("truncated PBZX block payload")

        if uncompressed_size == compressed_size:
            return data
        if data[:6] == XZ_MAGIC:
            out = lzma.decompress(data)
        elif data[:4] in LZFSE_MAGICS:
            out = _lzfse_decode(data)
        else:
            raise XipError(
                "unknown PBZX block compression: "
                f"uncompressed={uncompressed_size} compressed={compressed_size}"
            )
        if len(out) != uncompressed_size:
            raise XipError(
                "PBZX block size mismatch: "
                f"expected {uncompressed_size}, got {len(out)}"
            )
        return out

    def read_exact(self, size):
        while len(self._buf) < size:
            chunk = self._next_chunk()
            if chunk is None:
                raise XipError(
                    "unexpected end of PBZX stream "
                    f"(wanted {size} bytes, have {len(self._buf)})"
                )
            self._buf.extend(chunk)
        out = bytes(self._buf[:size])
        del self._buf[:size]
        return out


def _lzfse_decode(data):
    exe = shutil.which("lzfse")
    if exe is None:
        raise XipError("LZFSE-compressed PBZX chunk found but 'lzfse' is missing")
    proc = subprocess.run([exe, "-decode"], input=data, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE)
    if proc.returncode != 0:
        raise XipError(
            "lzfse failed to decode a PBZX chunk: "
            + proc.stderr.decode("utf-8", "replace").strip()
        )
    return proc.stdout


class CpioReader:
    """Minimal cpio parser fed by a PBZXStream.

    Xcode 26.x uses the old portable ASCII ("odc", magic 070707) format;
    older Xcode releases used newc (070701/070702).  Both are supported.
    """

    def __init__(self, stream):
        self._stream = stream

    def _read_header(self):
        magic = self._stream.read_exact(6)
        if magic in CPIO_NEWC_MAGICS:
            rest = self._stream.read_exact(104)
            values = [int(rest[i * 8:(i + 1) * 8], 16) for i in range(13)]
            (ino, mode, uid, gid, nlink, mtime, filesize,
             devmajor, devminor, _rdevmajor, _rdevminor,
             namesize, _check) = values
            return {
                "ino": ino,
                "dev": (devmajor << 32) | devminor,
                "mode": mode,
                "uid": uid,
                "gid": gid,
                "nlink": nlink,
                "mtime": mtime,
                "filesize": filesize,
                "namesize": namesize,
                "format": "newc",
            }, "newc"
        if magic == CPIO_ODC_MAGIC:
            rest = self._stream.read_exact(70)
            # dev, ino, mode, uid, gid, nlink, rdev, mtime,
            # namesize, filesize.  All fields are octal.
            (dev, ino, mode, uid, gid, nlink, _rdev, mtime,
             namesize, filesize) = (
                int(rest[0:6], 8),
                int(rest[6:12], 8),
                int(rest[12:18], 8),
                int(rest[18:24], 8),
                int(rest[24:30], 8),
                int(rest[30:36], 8),
                int(rest[36:42], 8),
                int(rest[42:53], 8),
                int(rest[53:59], 8),
                int(rest[59:70], 8),
            )
            return {
                "ino": ino,
                "dev": dev,
                "mode": mode,
                "uid": uid,
                "gid": gid,
                "nlink": nlink,
                "mtime": mtime,
                "filesize": filesize,
                "namesize": namesize,
                "format": "odc",
            }, "odc"
        raise XipError(f"unsupported cpio magic: {magic!r}")

    @staticmethod
    def _pad4(size):
        return (4 - (size % 4)) % 4

    def entries(self):
        while True:
            info, fmt = self._read_header()
            name_raw = self._stream.read_exact(info["namesize"])
            name = name_raw.split(b"\x00", 1)[0].decode("utf-8", "surrogateescape")
            if name == CPIO_TRAILER:
                return
            data = b""
            if info["filesize"]:
                data = self._stream.read_exact(info["filesize"])
                if fmt == "newc":
                    pad = self._pad4(info["filesize"])
                    if pad:
                        self._stream.read_exact(pad)
            if fmt == "newc":
                pad = self._pad4(110 + info["namesize"])
                if pad:
                    self._stream.read_exact(pad)
            yield name, info, data


def _normalise_name(name):
    while name.startswith("./"):
        name = name[2:]
    return name.lstrip("/")


def _under_prefix(name, prefix):
    if not prefix:
        return True
    return name == prefix.rstrip("/") or name.startswith(prefix.rstrip("/") + "/")


def _safe_dest(root, name):
    rel = PurePosixPath(name)
    if rel.is_absolute() or ".." in rel.parts:
        raise XipError(f"refusing unsafe archive path: {name!r}")
    return root.joinpath(*rel.parts)


def _is_hardlink_placeholder(data, info, fmt):
    """True when this entry only stands in for a hardlink's data.

    Apple's XIP cpio uses two schemes: newc hardlink continuations have a
    zero-length body, while odc continuations keep the real file size but
    fill the body with ``NULLcanary\\0...``.  The real data appears once,
    sometimes outside the extraction prefix (for example in XROS.sdk).
    """
    if info["nlink"] <= 1:
        return False
    if fmt == "newc" and info["filesize"] == 0:
        return True
    return data.startswith(b"NULLcanary\x00")


def _hardlink_key(info):
    if info["nlink"] <= 1:
        return None
    return info.get("dev", 0), info["ino"]


def _write_entry(root, name, info, data):
    dest = _safe_dest(root, name)
    mode = info["mode"]
    fmt = mode & 0o170000
    if fmt == 0o040000:
        dest.mkdir(parents=True, exist_ok=True)
        os.chmod(dest, mode & 0o7777)
        return
    dest.parent.mkdir(parents=True, exist_ok=True)
    if fmt == 0o120000:
        if dest.exists() or dest.is_symlink():
            dest.unlink()
        os.symlink(data.decode("utf-8", "surrogateescape"), dest)
        return
    if fmt != 0o100000:
        # Device nodes, FIFOs, sockets are irrelevant for an SDK extraction.
        return
    with open(dest, "wb") as f:
        f.write(data)
    os.chmod(dest, mode & 0o7777)
    try:
        os.utime(dest, (info["mtime"], info["mtime"]))
    except OSError:
        pass


def extract_xip(xip_path, out_dir, prefix, list_only=False):
    xip_path = Path(xip_path)
    if not xip_path.is_file():
        raise XipError(f"XIP not found: {xip_path}")
    out_dir = None if out_dir is None else Path(out_dir)

    bsdtar = shutil.which("bsdtar")
    if bsdtar is None:
        raise XipError("bsdtar is required to read the XAR container")

    if not list_only:
        out_dir.mkdir(parents=True, exist_ok=True)

    proc = subprocess.Popen(
        [bsdtar, "-xOf", str(xip_path), "Content"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    assert proc.stdout is not None
    hardlink_dir = None if list_only else tempfile.mkdtemp(prefix="xip-hardlinks-")
    try:
        stream = PBZXStream(proc.stdout)
        cpio = CpioReader(stream)
        # Real data for hardlinked files is written once, possibly outside the
        # requested prefix, so keep it on disk until every reference is seen.
        hardlink_data = {}
        pending = {}
        matched = 0
        for name, info, data in cpio.entries():
            clean = _normalise_name(name)
            in_prefix = _under_prefix(clean, prefix)
            if in_prefix:
                matched += 1
            if list_only:
                if in_prefix:
                    print(clean)
                continue

            key = _hardlink_key(info)
            if key is None:
                if in_prefix and not list_only:
                    _write_entry(out_dir, clean, info, data)
                continue

            placeholder = _is_hardlink_placeholder(
                data, info, info.get("format", "odc")
            )
            if not placeholder:
                stored = os.path.join(
                    hardlink_dir, f"{key[0]:x}-{key[1]:x}"
                )
                with open(stored, "wb") as f:
                    f.write(data)
                hardlink_data[key] = stored
                if in_prefix and not list_only:
                    _write_entry(out_dir, clean, info, data)
            elif in_prefix and not list_only:
                stored = hardlink_data.get(key)
                if stored is None:
                    pending.setdefault(key, []).append((clean, info))
                else:
                    with open(stored, "rb") as f:
                        _write_entry(out_dir, clean, info, f.read())

        for key, items in pending.items():
            stored = hardlink_data.get(key)
            if stored is None:
                names = ", ".join(name for name, _ in items[:3])
                raise XipError(
                    "hardlink data missing for archive inode "
                    f"{key[1]}: {names}"
                )
            with open(stored, "rb") as f:
                data = f.read()
            for clean, info in items:
                _write_entry(out_dir, clean, info, data)

        if matched == 0 and prefix:
            raise XipError(
                f"no entries matched prefix {prefix!r}; use --list to inspect paths"
            )
        return matched
    finally:
        if hardlink_dir is not None:
            shutil.rmtree(hardlink_dir, ignore_errors=True)
        if proc.poll() is None:
            proc.terminate()
        try:
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
        stderr = b""
        if proc.stderr is not None:
            stderr = proc.stderr.read()
        if proc.returncode not in (0, None, -15) and not list_only:
            msg = stderr.decode("utf-8", "replace").strip()
            if msg:
                print(f"xip_extract: bsdtar: {msg}", file=sys.stderr)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--xip", required=True, help="path to the .xip file")
    parser.add_argument("--out", help="destination directory")
    parser.add_argument("--prefix", default=DEFAULT_PREFIX,
                        help=f"archive path prefix to extract (default: {DEFAULT_PREFIX})")
    parser.add_argument("--list", action="store_true",
                        help="print matching archive paths instead of extracting")
    args = parser.parse_args(argv)

    if not args.list and not args.out:
        parser.error("--out is required unless --list is used")

    try:
        count = extract_xip(args.xip, args.out, args.prefix, args.list)
    except XipError as exc:
        print(f"xip_extract: {exc}", file=sys.stderr)
        return 1
    print(f"xip_extract: {count} matching entries", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
