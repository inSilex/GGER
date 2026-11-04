#!/usr/bin/env python3
"""Stage a NetWalk native PS5 title folder.

The converter and FSELF wrapper run in the CMake build. This script assembles
the loader-visible title directory around the resulting eboot.bin:

    <out>/
      eboot.bin
      sce_module/libc.prx
      sce_sys/param.json
      sce_sys/icon0.png
      overlay.nw          (when supplied)
      gm.sf2              (when supplied)
      save file           (when supplied)

It intentionally does not install, deploy, or modify a console.
"""
from __future__ import annotations

import argparse
import json
import shutil
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools"))

from netwalk_icon import png_bytes

DEFAULT_TITLE_ID = "PPSA99877"


def parse_args() -> argparse.Namespace:
    root = Path(__file__).resolve().parents[3]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--eboot", type=Path, required=True, help="signed eboot.bin")
    parser.add_argument("--libc", type=Path, required=True, help="generated libc.prx")
    parser.add_argument("--param", type=Path,
                        default=root / "platforms/ps5/native/param.json")
    parser.add_argument("--icon-source", type=Path, default=None,
                        help="optional 512x512 icon0.png; otherwise generate one")
    parser.add_argument("--overlay", type=Path, default=None,
                        help="optional overlay.nw to copy beside eboot.bin")
    parser.add_argument("--overlay-trailer", type=Path, default=None,
                        help="optional overlay.trailer to copy beside eboot.bin")
    parser.add_argument("--soundfont", type=Path, default=None,
                        help="optional gm.sf2 to copy beside eboot.bin")
    parser.add_argument("--save-seed", type=Path, default=None,
                        help="optional save file to include under its own name")
    parser.add_argument("--out", type=Path, required=True, help="title directory")
    parser.add_argument("--title-id", default=None,
                        help="expected titleId; defaults to the value in param.json")
    return parser.parse_args()


def png_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    if len(data) < 24 or data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        raise SystemExit(f"{path}: not a PNG file")
    return struct.unpack(">II", data[16:24])


def write_icon(path: Path) -> None:
    path.write_bytes(png_bytes(512))


def validate_param(path: Path, title_id: str) -> dict:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise SystemExit(f"{path}: {exc}") from exc

    if data.get("titleId") != title_id:
        raise SystemExit(f"{path}: titleId must be {title_id!r}")
    if len(title_id) != 9 or not title_id.startswith("PPSA") or not title_id[4:].isdigit():
        raise SystemExit(f"{path}: titleId must be PPSA plus five digits")
    content_id = data.get("contentId", "")
    if title_id not in content_id:
        raise SystemExit(f"{path}: contentId does not contain {title_id}")
    if int(data.get("downloadDataSize", 0)) <= 0:
        raise SystemExit(f"{path}: downloadDataSize must be positive for /download0")
    title = data.get("localizedParameters", {}).get("en-US", {}).get("titleName")
    if not title:
        raise SystemExit(f"{path}: missing en-US titleName")
    return data


def main() -> int:
    args = parse_args()
    if not args.eboot.is_file():
        raise SystemExit(f"{args.eboot}: missing signed eboot.bin")
    if not args.libc.is_file():
        raise SystemExit(f"{args.libc}: missing libc.prx")
    if not args.param.is_file():
        raise SystemExit(f"{args.param}: missing param.json")

    raw = json.loads(args.param.read_text(encoding="utf-8"))
    title_id = args.title_id or raw.get("titleId", DEFAULT_TITLE_ID)
    data = validate_param(args.param, title_id)
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "sce_sys").mkdir(exist_ok=True)
    (args.out / "sce_module").mkdir(exist_ok=True)

    shutil.copy2(args.eboot, args.out / "eboot.bin")
    shutil.copy2(args.libc, args.out / "sce_module/libc.prx")
    shutil.copy2(args.param, args.out / "sce_sys/param.json")

    icon = args.out / "sce_sys/icon0.png"
    if args.icon_source is not None:
        width, height = png_size(args.icon_source)
        if (width, height) != (512, 512):
            raise SystemExit(f"{args.icon_source}: icon0.png must be 512x512, got {width}x{height}")
        shutil.copy2(args.icon_source, icon)
    else:
        write_icon(icon)

    if args.overlay is not None and args.overlay.is_file():
        shutil.copy2(args.overlay, args.out / "overlay.nw")
    if args.overlay_trailer is not None and args.overlay_trailer.is_file():
        shutil.copy2(args.overlay_trailer, args.out / "overlay.trailer")
    if args.soundfont is not None and args.soundfont.is_file():
        shutil.copy2(args.soundfont, args.out / "gm.sf2")
    if args.save_seed is not None and args.save_seed.is_file():
        shutil.copy2(args.save_seed, args.out / args.save_seed.name)

    files = sorted(path for path in args.out.rglob("*") if path.is_file())
    total = sum(path.stat().st_size for path in files)
    print(f"Native title: {args.out}")
    print(f"  title: {data['localizedParameters']['en-US']['titleName']} ({title_id})")
    for path in files:
        print(f"  {path.relative_to(args.out)}  {path.stat().st_size} bytes")
    print(f"  total {total} bytes")
    print("  deploy the whole directory, not only eboot.bin; hardware testing is still required")
    return 0


if __name__ == "__main__":
    sys.exit(main())
