#!/usr/bin/env python3
"""Create a mountable UFS2 .ffpkg image from a staged PS5 native title.

A .ffpkg in this tree is a raw UFS2 filesystem image, not Sony's signed retail
PKG/FPKG container.  ShadowMount-style loaders mount the image read-only and
expose its root as the title's /app0, so the staged title directory is copied
verbatim: eboot.bin, sce_module/, sce_sys/, the external overlay, and the
SoundFont.

The image is built with the small native makefs tool prepared by
platforms/ps5/tools/setup-packaging.sh.  The resulting image is checked for the UFS2
superblock and for every required file before it replaces the destination.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import mmap
import os
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path
from typing import NoReturn

UFS2_MAGIC = 0x19540119
UFS2_SUPERBLOCK_OFFSET = 0x10000
UFS2_SUPERBLOCK_SIZE = 0x560
UFS2_MAGIC_OFFSET = UFS2_SUPERBLOCK_OFFSET + 0x55C
UFS2_SBLOCKLOC_OFFSET = UFS2_SUPERBLOCK_OFFSET + 1000
UFS2_INODE_SIZE = 256
UFS2_DIRBLKSIZ = 512
UFS2_ROOT_INO = 2
UFS2_IFMT = 0xF000
UFS2_IFDIR = 0x4000
UFS2_IFREG = 0x8000
MAKEFS_OPTIONS = (
    "version=2,bsize=32768,fsize=4096,minfree=0,"
    "softupdates=0,optimization=space"
)
SECTOR_SIZE = 4096
TITLE_ID_RE = re.compile(r"PPSA\d{5}")
LABEL_RE = re.compile(r"[A-Za-z0-9_.-]{1,32}")


def parse_args() -> argparse.Namespace:
    root = Path(__file__).resolve().parents[3]
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "title_dir",
        nargs="?",
        type=Path,
        default=root / "dist/PPSA99877",
        help="staged native title directory (default: dist/PPSA99877)",
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=None,
        help="output image (default: <title_dir>.ffpkg)",
    )
    parser.add_argument(
        "--tool",
        type=Path,
        default=None,
        help="makefs executable (default: NW_PS5_MAKEFS, .tmp, or setup)",
    )
    parser.add_argument(
        "--label",
        default=None,
        help="UFS2 volume label (default: the param.json title ID)",
    )
    parser.add_argument(
        "--allow-missing-overlay",
        action="store_true",
        help="build an image without overlay.nw (not runnable on the console)",
    )
    parser.add_argument(
        "--no-setup",
        action="store_true",
        help="do not run platforms/ps5/tools/setup-packaging.sh when makefs is absent",
    )
    parser.add_argument(
        "--no-verify",
        action="store_true",
        help="skip the per-file image verification after creation",
    )
    parser.add_argument(
        "--verify",
        type=Path,
        default=None,
        metavar="IMAGE",
        help="verify an existing image against title_dir, then exit",
    )
    return parser.parse_args()


def fail(message: str) -> NoReturn:
    raise SystemExit(f"make-ffpkg.py: {message}")


def is_executable(path: Path) -> bool:
    return path.is_file() and os.access(path, os.X_OK)


def resolve_tool(args: argparse.Namespace, root: Path) -> Path:
    explicit = args.tool
    if explicit is None:
        env_tool = os.environ.get("NW_PS5_MAKEFS")
        if env_tool:
            explicit = Path(env_tool)
    if explicit is not None:
        candidate = explicit.expanduser()
        if not candidate.is_absolute():
            candidate = Path.cwd() / candidate
        if not is_executable(candidate):
            fail(f"makefs is not executable: {candidate}")
        return candidate

    candidate = root / ".tmp/ps5/tools/makefs"
    if is_executable(candidate):
        return candidate

    if not args.no_setup:
        setup = root / "platforms/ps5/tools/setup-packaging.sh"
        if setup.is_file():
            print("make-ffpkg.py: preparing native makefs...", file=sys.stderr)
            result = subprocess.run(["bash", str(setup)], check=False)
            if result.returncode != 0:
                fail("native makefs setup failed")
            if is_executable(candidate):
                return candidate

    fail(
        "makefs was not found. Run platforms/ps5/tools/setup-packaging.sh, set "
        "NW_PS5_MAKEFS, or pass --tool PATH."
    )


def load_param(title: Path) -> tuple[dict, str]:
    path = title / "sce_sys/param.json"
    if not path.is_file():
        fail(f"missing title metadata: {path}")
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"{path}: {exc}")
    if not isinstance(data, dict):
        fail(f"{path}: expected a JSON object")
    title_id = data.get("titleId", "")
    if not isinstance(title_id, str) or TITLE_ID_RE.fullmatch(title_id) is None:
        fail(f"{path}: titleId must be PPSA followed by five digits")
    if title_id not in str(data.get("contentId", "")):
        fail(f"{path}: contentId does not contain {title_id}")
    size = data.get("downloadDataSize")
    if isinstance(size, bool) or not isinstance(size, int) or size <= 0:
        fail(f"{path}: downloadDataSize must be a positive integer")
    return data, title_id


def validate_title(title: Path, allow_missing_overlay: bool) -> tuple[dict, str]:
    if not title.is_dir():
        fail(f"title directory not found: {title}")

    required = [
        "eboot.bin",
        "sce_module/libc.prx",
        "sce_sys/param.json",
        "sce_sys/icon0.png",
    ]
    missing = [name for name in required if not (title / name).is_file()]
    if missing:
        fail("missing required title file(s): " + ", ".join(missing))
    for name in required:
        path = title / name
        if path.stat().st_size == 0:
            fail(f"required title file is empty: {path}")

    data, title_id = load_param(title)

    overlay_nw = title / "overlay.nw"
    overlay_bin = title / "overlay.bin"
    overlay_trailer = title / "overlay.trailer"
    if overlay_nw.is_file() and overlay_nw.stat().st_size == 0:
        fail(f"overlay is empty: {overlay_nw}")
    if overlay_bin.is_file() and overlay_bin.stat().st_size == 0:
        fail(f"overlay is empty: {overlay_bin}")
    if overlay_trailer.is_file() and overlay_trailer.stat().st_size != 38:
        fail(f"overlay trailer must be exactly 38 bytes: {overlay_trailer}")
    has_overlay = overlay_nw.is_file() or (
        overlay_bin.is_file() and overlay_trailer.is_file()
    )
    if not has_overlay:
        if allow_missing_overlay:
            print(
                "make-ffpkg.py: warning: no overlay.nw or overlay.bin+"
                "overlay.trailer; the image will not be runnable until an "
                "overlay is supplied.",
                file=sys.stderr,
            )
        else:
            fail(
                "no external overlay in the title directory. Stage "
                "overlay.nw (or overlay.bin + overlay.trailer), or pass "
                "--allow-missing-overlay for a non-runnable compile-check "
                "image."
            )
    elif overlay_bin.is_file() and not overlay_trailer.is_file():
        print(
            "make-ffpkg.py: warning: overlay.bin has no overlay.trailer; "
            "the game may fall back to the default trailer.",
            file=sys.stderr,
        )

    if not (title / "gm.sf2").is_file():
        print(
            "make-ffpkg.py: warning: gm.sf2 is missing; music and SoundFont "
            "output will be unavailable.",
            file=sys.stderr,
        )
    return data, title_id


def expected_files(title: Path) -> list[str]:
    names = [
        "eboot.bin",
        "sce_module/libc.prx",
        "sce_sys/param.json",
        "sce_sys/icon0.png",
    ]
    for name in (
        "overlay.nw",
        "overlay.bin",
        "overlay.trailer",
        "gm.sf2",
    ):
        if (title / name).is_file():
            names.append(name)
    return names


def output_path(args: argparse.Namespace, title: Path) -> Path:
    if args.out is None:
        return (title.parent / (title.name + ".ffpkg")).resolve()
    out = args.out.expanduser()
    if not out.is_absolute():
        out = Path.cwd() / out
    return out.resolve()


def check_free_space(title: Path, out: Path) -> None:
    input_bytes = sum(
        path.stat().st_size for path in title.rglob("*") if path.is_file()
    )
    existing = out.stat().st_size if out.is_file() else 0
    headroom = max(64 * 1024 * 1024, input_bytes // 4)
    needed = input_bytes + headroom + existing
    free = shutil.disk_usage(out.parent).free
    if free < needed:
        fail(
            f"not enough free space in {out.parent}: need about "
            f"{needed:,} bytes, have {free:,}"
        )


def run_tool(
    tool: Path, args: list[str], *, capture: bool = False
) -> subprocess.CompletedProcess[str]:
    command = [str(tool), *args]
    try:
        if capture:
            return subprocess.run(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                errors="replace",
                check=False,
            )
        return subprocess.run(command, text=True, check=False)
    except OSError as exc:
        fail(f"could not run {tool}: {exc}")


class ImageError(Exception):
    """A structural problem in a UFS2 image."""


class Ufs2Image:
    """Read enough of a UFS2 filesystem to verify the staged title tree."""

    def __init__(self, path: Path):
        self.path = path
        try:
            self._fd = path.open("rb")
            self._data = mmap.mmap(self._fd.fileno(), 0, access=mmap.ACCESS_READ)
        except (OSError, ValueError) as exc:
            self.close()
            raise ImageError(f"could not open {path}: {exc}") from exc
        try:
            self._read_superblock()
        except Exception:
            self.close()
            raise

    def close(self) -> None:
        data = getattr(self, "_data", None)
        if data is not None:
            data.close()
            self._data = None
        fd = getattr(self, "_fd", None)
        if fd is not None:
            fd.close()
            self._fd = None

    def __enter__(self) -> "Ufs2Image":
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()

    def _check(self, offset: int, size: int) -> None:
        if offset < 0 or size < 0 or offset + size > len(self._data):
            raise ImageError(f"read outside the image at offset {offset:#x}")

    def _u16(self, offset: int) -> int:
        self._check(offset, 2)
        return struct.unpack_from("<H", self._data, offset)[0]

    def _i32(self, offset: int) -> int:
        self._check(offset, 4)
        return struct.unpack_from("<i", self._data, offset)[0]

    def _u32(self, offset: int) -> int:
        self._check(offset, 4)
        return struct.unpack_from("<I", self._data, offset)[0]

    def _i64(self, offset: int) -> int:
        self._check(offset, 8)
        return struct.unpack_from("<q", self._data, offset)[0]

    def _u64(self, offset: int) -> int:
        self._check(offset, 8)
        return struct.unpack_from("<Q", self._data, offset)[0]

    def _read_superblock(self) -> None:
        sb = UFS2_SUPERBLOCK_OFFSET
        self._check(sb, UFS2_SUPERBLOCK_SIZE)
        self.magic = self._u32(UFS2_MAGIC_OFFSET)
        if self.magic != UFS2_MAGIC:
            raise ImageError(
                f"bad UFS2 magic {self.magic:#x} at 0x{UFS2_MAGIC_OFFSET:x}"
            )
        self.sblockloc = self._i64(UFS2_SBLOCKLOC_OFFSET)
        if self.sblockloc != UFS2_SUPERBLOCK_OFFSET:
            raise ImageError(
                "UFS2 superblock location is not "
                f"0x{UFS2_SUPERBLOCK_OFFSET:x}: {self.sblockloc:#x}"
            )
        self.bsize = self._i32(sb + 48)
        self.fsize = self._i32(sb + 52)
        self.frag = self._i32(sb + 56)
        self.fragshift = self._i32(sb + 96)
        self.inopb = self._i32(sb + 120)
        self.ncg = self._i32(sb + 44)
        self.ipg = self._i32(sb + 184)
        self.fpg = self._i32(sb + 188)
        self.iblkno = self._i32(sb + 16)
        self.size = self._i64(sb + 1080)
        if self.bsize <= 0 or self.fsize <= 0 or self.frag <= 0:
            raise ImageError("invalid UFS2 block geometry")
        if self.bsize % self.fsize != 0 or self.bsize // self.fsize != self.frag:
            raise ImageError("inconsistent UFS2 block and fragment sizes")
        if self.inopb <= 0 or self.ncg <= 0 or self.ipg <= 0 or self.fpg <= 0:
            raise ImageError("invalid UFS2 inode geometry")
        if self.iblkno < 0 or self.size <= 0:
            raise ImageError("invalid UFS2 layout")
        if self.size * self.fsize > len(self._data):
            raise ImageError("image is shorter than its UFS2 superblock claims")

    def _inode_offset(self, ino: int) -> int:
        if ino <= 0:
            raise ImageError(f"invalid inode number {ino}")
        cg, rem = divmod(ino, self.ipg)
        if cg >= self.ncg:
            raise ImageError(f"inode {ino} is outside the cylinder groups")
        block, index = divmod(rem, self.inopb)
        fsba = self.fpg * cg + self.iblkno + block * self.frag
        offset = fsba * self.fsize + index * UFS2_INODE_SIZE
        self._check(offset, UFS2_INODE_SIZE)
        return offset

    def inode(self, ino: int) -> tuple[int, int, tuple[int, ...]]:
        offset = self._inode_offset(ino)
        mode = self._u16(offset)
        size = self._u64(offset + 16)
        blocks = struct.unpack_from("<12Q", self._data, offset + 112)
        return mode, size, blocks

    def read_data(self, ino: int) -> bytes:
        mode, size, blocks = self.inode(ino)
        if size > len(self._data):
            raise ImageError(f"inode {ino} has an impossible size {size}")
        output = bytearray()
        remaining = size
        for block in blocks:
            if remaining <= 0:
                break
            length = min(self.bsize, remaining)
            if block == 0:
                output.extend(b"\0" * length)
            else:
                offset = block * self.fsize
                self._check(offset, length)
                output.extend(self._data[offset:offset + length])
            remaining -= length
        if remaining:
            raise ImageError(
                f"inode {ino} needs indirect blocks; cannot verify its "
                f"directory data"
            )
        return bytes(output)

    def listdir(self, ino: int) -> dict[str, int]:
        mode, _size, _blocks = self.inode(ino)
        if mode & UFS2_IFMT != UFS2_IFDIR:
            raise ImageError(f"inode {ino} is not a directory")
        data = self.read_data(ino)
        entries: dict[str, int] = {}
        position = 0
        while position < len(data):
            if position + 8 > len(data):
                raise ImageError(f"truncated directory entry at {position}")
            d_ino, reclen, _d_type, namlen = struct.unpack_from(
                "<IHBB", data, position
            )
            if (
                reclen < 8
                or reclen % 4 != 0
                or position % UFS2_DIRBLKSIZ + reclen > UFS2_DIRBLKSIZ
                or position + reclen > len(data)
            ):
                raise ImageError(f"invalid directory entry at {position}")
            if d_ino:
                name_end = position + 8 + namlen
                if namlen > 255 or name_end > position + reclen:
                    raise ImageError(f"invalid directory name at {position}")
                name = data[position + 8:name_end].decode(
                    "utf-8", "surrogateescape"
                )
                if name in entries:
                    raise ImageError(f"duplicate directory entry {name!r}")
                entries[name] = d_ino
            position += reclen
        return entries

    def lookup(self, name: str) -> int:
        ino = UFS2_ROOT_INO
        for part in Path(name).parts:
            if part in ("", ".", "/"):
                continue
            entries = self.listdir(ino)
            if part not in entries:
                raise ImageError(f"the image does not contain /{name}")
            ino = entries[part]
        return ino


def verify_image(image: Path, title: Path) -> None:
    expected = expected_files(title)
    try:
        with Ufs2Image(image) as filesystem:
            root_mode, _root_size, _root_blocks = filesystem.inode(
                UFS2_ROOT_INO
            )
            if root_mode & UFS2_IFMT != UFS2_IFDIR:
                raise ImageError("the UFS2 root inode is not a directory")
            for name in expected:
                source = title / name
                try:
                    source_size = source.stat().st_size
                except OSError as exc:
                    raise ImageError(f"cannot stat {source}: {exc}") from exc
                ino = filesystem.lookup(name)
                mode, size, _blocks = filesystem.inode(ino)
                if mode & UFS2_IFMT != UFS2_IFREG:
                    raise ImageError(f"/{name} is not a regular file")
                if size != source_size:
                    raise ImageError(
                        f"/{name} is {size:,} bytes in the image, expected "
                        f"{source_size:,}"
                    )
    except ImageError as exc:
        fail(f"image verification failed: {exc}")
    except (OSError, struct.error) as exc:
        fail(f"image verification failed: {exc}")


def check_superblock(image: Path) -> None:
    try:
        with image.open("rb") as stream:
            stream.seek(UFS2_SUPERBLOCK_OFFSET)
            superblock = stream.read(UFS2_SUPERBLOCK_SIZE)
    except OSError as exc:
        fail(f"could not read the new image: {exc}")
    if len(superblock) != UFS2_SUPERBLOCK_SIZE:
        fail(
            "the new image is too small to contain a complete UFS2 "
            f"superblock at 0x{UFS2_SUPERBLOCK_OFFSET:x}"
        )
    magic = struct.unpack_from(
        "<I", superblock, UFS2_MAGIC_OFFSET - UFS2_SUPERBLOCK_OFFSET
    )[0]
    if magic != UFS2_MAGIC:
        fail(
            "the new image does not have the UFS2 superblock magic at "
            f"0x{UFS2_MAGIC_OFFSET:x}; use a makefs that writes the standard "
            "UFS2 superblock location"
        )
    sblockloc = struct.unpack_from(
        "<q", superblock, UFS2_SBLOCKLOC_OFFSET - UFS2_SUPERBLOCK_OFFSET
    )[0]
    if sblockloc != UFS2_SUPERBLOCK_OFFSET:
        fail(
            "the new image has a nonstandard UFS2 superblock location "
            f"{sblockloc:#x}; use a makefs that writes it at "
            f"0x{UFS2_SUPERBLOCK_OFFSET:x}"
        )


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    args = parse_args()
    root = Path(__file__).resolve().parents[3]
    title = args.title_dir.expanduser()
    if not title.is_absolute():
        title = Path.cwd() / title
    title = title.resolve()

    if args.verify is not None and args.no_verify:
        fail("--verify and --no-verify are mutually exclusive")

    if args.verify is not None:
        validate_title(title, allow_missing_overlay=True)
        image = args.verify.expanduser()
        if not image.is_absolute():
            image = Path.cwd() / image
        image = image.resolve()
        if not image.is_file():
            fail(f"image not found: {image}")
        verify_image(image, title)
        print(f"make-ffpkg.py: verified {image}")
        print(f"  size: {image.stat().st_size:,} bytes")
        print(f"  sha256: {sha256(image)}")
        return 0

    data, title_id = validate_title(title, args.allow_missing_overlay)
    label = args.label or title_id
    if LABEL_RE.fullmatch(label) is None:
        fail("--label must be 1-32 characters from A-Z, a-z, 0-9, _, ., or -")
    out = output_path(args, title)
    if out == title or title in out.parents:
        fail(f"output image must not be inside the title directory: {out}")
    out.parent.mkdir(parents=True, exist_ok=True)
    tool = resolve_tool(args, root)
    check_free_space(title, out)

    localized = data.get("localizedParameters")
    title_name = "unknown"
    if isinstance(localized, dict):
        language = localized.get("defaultLanguage", "en-US")
        entry = localized.get(language) or localized.get("en-US")
        if isinstance(entry, dict) and entry.get("titleName"):
            title_name = str(entry["titleName"])

    print(f"make-ffpkg.py: title {title_id} ({title_name})")
    print(f"make-ffpkg.py: input {title}")
    print(f"make-ffpkg.py: output {out}")

    tmp = out.parent / f".{out.name}.tmp.{os.getpid()}"
    if tmp.exists():
        tmp.unlink()
    command = [
        "-S",
        str(SECTOR_SIZE),
        "-b",
        "20%",
        "-t",
        "ffs",
        "-o",
        f"{MAKEFS_OPTIONS},label={label}",
        str(tmp),
        str(title),
    ]
    try:
        result = run_tool(tool, command, capture=True)
        if result.returncode != 0:
            detail = result.stdout.strip()
            suffix = f":\n{detail}" if detail else ""
            fail(f"makefs failed with status {result.returncode}{suffix}")
        if not tmp.is_file() or tmp.stat().st_size == 0:
            fail("makefs did not produce an image")

        check_superblock(tmp)
        if not args.no_verify:
            verify_image(tmp, title)

        try:
            os.replace(tmp, out)
        except OSError as exc:
            fail(f"could not replace {out}: {exc}")
    finally:
        if tmp.exists():
            tmp.unlink()

    digest = sha256(out)
    print(f"FFPKG: {out}")
    print(f"  size: {out.stat().st_size:,} bytes")
    print(f"  sha256: {digest}")
    print("  note: mountable UFS2 image for a ShadowMount-style loader; "
          "not a signed retail PKG/FPKG.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
