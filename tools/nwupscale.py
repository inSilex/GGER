#!/usr/bin/env python3
"""nwupscale.py -- deliberately small nearest-neighbour upscale helper.

Two kinds of source images exist in this tree:

  sprites  an NW_DEBUG_ASSET_DUMP directory (sprites.tsv + sprites/*.idx).  The
           output is an NW_DEBUG_ASSET_PACK override pack: original geometry stays in
           the manifest, while the extra source_width/source_height columns
           point at the larger art.  Use it with NW_RENDER_SCALE=N.

  frames   the extracted movies/groupN/frame_XXXX.png files.  The output is
           a repacked overlay whose frame blocks contain native
           640*N x 480*N pixels.

Everything is nearest-neighbour; no filtering, no palette averaging.

Examples:
    python3 tools/nwupscale.py sprites /tmp/opencode/nwassets \
        -o /tmp/opencode/nwpack2 --scale 2
    NW_RENDER_SCALE=2 NW_DEBUG_ASSET_PACK=/tmp/opencode/nwpack2 ./build/linux-x86_64/NETWALK.EXE

    python3 tools/nwupscale.py frames extracted/NETWALK \
        -o /tmp/opencode/frames2 --scale 2 --groups 0 --limit 4
"""
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

W, H = 640, 480
HERE = Path(__file__).resolve().parent


class UpscaleError(Exception):
    pass


def nearest_idx(data: bytes, sw: int, sh: int, dw: int, dh: int) -> bytes:
    if len(data) != sw * sh:
        raise UpscaleError("indexed image is %d bytes, expected %d" %
                           (len(data), sw * sh))
    if sw == dw and sh == dh:
        return data
    out = bytearray(dw * dh)
    for y in range(dh):
        sy = (y * sh) // dh
        row = data[sy * sw:(sy + 1) * sw]
        dst = y * dw
        for x in range(dw):
            out[dst + x] = row[(x * sw) // dw]
    return bytes(out)


def scale_png(path: Path, width: int, height: int, scale: int) -> bytes:
    try:
        from PIL import Image
    except Exception as exc:
        raise UpscaleError("Pillow is required for PNG input: %s" % exc)
    im = Image.open(path).convert("RGB")
    target = (width * scale, height * scale)
    if im.size != target:
        im = im.resize(target, Image.Resampling.NEAREST)
    return im.tobytes()


def read_rows(path: Path) -> list[list[str]]:
    rows = []
    with path.open("r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            p = line.split()
            if len(p) >= 6:
                rows.append(p)
    return rows


def cmd_sprites(args) -> int:
    src = Path(args.input).resolve()
    out = Path(args.out).resolve()
    manifest = src / "sprites.tsv"
    if not manifest.is_file():
        raise UpscaleError("no sprites.tsv under %s" % src)
    rows = read_rows(manifest)
    if not rows:
        raise UpscaleError("sprites.tsv has no rows")
    if out.exists():
        shutil.rmtree(out)
    (out / "sprites").mkdir(parents=True)

    made: dict[tuple[int, int, int], tuple[str, int, int]] = {}
    lines = ["# nwassets 1",
             "# offset\twidth\theight\tstored\tcrc32\tfile\t"
             "source_width\tsource_height"]
    for p in rows:
        off = int(p[0], 16)
        w, h = int(p[1]), int(p[2])
        stored = int(p[3], 16)
        crc = int(p[4], 16)
        if len(p) >= 8:
            srcw, srch = int(p[6]), int(p[7])
        else:
            srcw, srch = w, h
        rel = p[5].replace("\\", "/")
        srcfile = src / rel
        if not srcfile.is_file():
            srcfile = src / "sprites" / Path(rel).name
        if not srcfile.is_file():
            continue
        key = (crc, w, h)
        if key not in made:
            sw, sh = w * args.scale, h * args.scale
            if srcfile.suffix.lower() == ".idx":
                data = nearest_idx(srcfile.read_bytes(), srcw, srch, sw, sh)
            else:
                data = scale_png(srcfile, w, h, args.scale)
            name = "sprites/h%08x_%dx%d.idx" % (crc, sw, sh)
            (out / name).write_bytes(data)
            made[key] = (name, sw, sh)
        name, sw, sh = made[key]
        lines.append("%08x\t%d\t%d\t%08x\t%08x\t%s\t%d\t%d" %
                     (off, w, h, stored, crc, name, sw, sh))
    (out / "sprites.tsv").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("sprites: %d row(s), %d unique image(s), scale=%d -> %s" %
          (len(lines) - 2, len(made), args.scale, out))
    print("run: NW_RENDER_SCALE=%d NW_DEBUG_ASSET_PACK=%s ./NETWALK.EXE" %
          (args.scale, out))
    return 0


def cmd_frames(args) -> int:
    root = Path(args.input).resolve()
    overlay = root / "overlay.nw"
    movies = root / "movies"
    if not overlay.is_file() or not movies.is_dir():
        raise UpscaleError("%s does not look like extracted/NETWALK" % root)
    out = Path(args.out).resolve()
    frames = out / "frames"
    if out.exists():
        shutil.rmtree(out)
    frames.mkdir(parents=True)

    groups = None
    if args.groups:
        groups = {int(v) for v in args.groups.split(",") if v.strip()}
    count = 0
    for gdir in sorted(movies.glob("group*")):
        try:
            gi = int(gdir.name[5:])
        except ValueError:
            continue
        if groups is not None and gi not in groups:
            continue
        dst_dir = frames / ("group%d" % gi)
        dst_dir.mkdir(parents=True)
        for src in sorted(gdir.glob("frame_*.*")):
            if src.suffix.lower() not in (".png", ".idx"):
                continue
            bi = int(src.stem.split("_")[-1])
            if args.limit is not None and bi >= args.limit:
                continue
            dst = dst_dir / ("frame_%04d.png" % bi)
            if src.suffix.lower() == ".idx":
                data = nearest_idx(src.read_bytes(), W, H,
                                   W * args.scale, H * args.scale)
                (dst_dir / ("frame_%04d.idx" % bi)).write_bytes(data)
            else:
                pix = scale_png(src, W, H, args.scale)
                try:
                    from PIL import Image
                except Exception as exc:
                    raise UpscaleError("Pillow is required to emit PNG: %s" % exc)
                Image.frombytes("RGB", (W * args.scale, H * args.scale),
                                pix).save(dst)
            count += 1
    if count == 0:
        raise UpscaleError("no frame images selected")

    repack_out = out / "overlay"
    palette = movies / "palette.pal"
    cmd = [sys.executable, str(HERE / "nwframe_repack.py"),
           str(overlay), "--frames", str(frames), "--frame-scale",
           str(args.scale), "-o", str(repack_out)]
    if palette.is_file():
        cmd += ["--palette", str(palette)]
    if args.force:
        cmd.append("--force")
    if args.verbose:
        cmd.append("--verbose")
    subprocess.run(cmd, check=True)
    print("frames: %d image(s), scale=%d" % (count, args.scale))
    print("run: NW_RENDER_SCALE=%d; copy %s/overlay.nw beside the executable" %
          (args.scale, repack_out))
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="mode", required=True)

    p = sub.add_parser("sprites", help="build an NW_DEBUG_ASSET_PACK source pack")
    p.add_argument("input", help="NW_DEBUG_ASSET_DUMP directory (sprites.tsv present)")
    p.add_argument("-o", "--out", required=True, help="output pack directory")
    p.add_argument("--scale", type=int, default=2)
    p.set_defaults(func=cmd_sprites)

    p = sub.add_parser("frames", help="upscale movies/ and rebuild overlay.nw")
    p.add_argument("input", help="extracted/NETWALK directory")
    p.add_argument("-o", "--out", required=True, help="output directory")
    p.add_argument("--scale", type=int, default=2)
    p.add_argument("--groups", help="comma-separated group indexes (default: all)")
    p.add_argument("--limit", type=int,
                   help="only frames below this index in each selected group")
    p.add_argument("--force", action="store_true")
    p.add_argument("--verbose", action="store_true")
    p.set_defaults(func=cmd_frames)

    args = ap.parse_args()
    if not 1 <= args.scale <= 8:
        raise UpscaleError("--scale must be in 1..8")
    try:
        return args.func(args)
    except (UpscaleError, subprocess.CalledProcessError) as exc:
        print("nwupscale: %s" % exc, file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
