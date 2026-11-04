#!/usr/bin/env python3
"""nwassets.py -- inspect and repack an existing runtime sprite dump.

A debug build can write decoded sprites when NW_DEBUG_ASSET_DUMP names a
directory.  The resulting tree contains:

    sprites.tsv
    sprites/o<offset>_<w>x<h>.idx
    palette.bin

This tool renders that tree to BMP/PNG files and can turn it into an
NW_DEBUG_ASSET_PACK source pack.  It does not launch the game: run the build
yourself, exercise the screens you want, then point this tool at the directory.

Commands:
  render  turn a dump into BMP/PNG files and a contact sheet
  pack    turn a dump into a high-resolution override pack

Examples:
  python3 tools/nwassets.py render /tmp/opencode/nwassets
  python3 tools/nwassets.py pack /tmp/opencode/nwassets
  python3 tools/nwassets.py pack /tmp/opencode/nwassets \
      --out /tmp/opencode/nwpack_novice --only-crc 0955a268
"""
import argparse
import os
import shutil
import struct


def log(msg):
    print(msg, flush=True)


def read_manifest(path):
    rows = []
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            p = line.split()
            if len(p) < 6:
                continue
            rows.append(dict(
                offset=int(p[0], 16),
                w=int(p[1]),
                h=int(p[2]),
                stored=int(p[3], 16),
                crc=int(p[4], 16),
                file=p[5].replace("\\", "/"),
            ))
    return rows


def last_palette(dump_dir):
    path = os.path.join(dump_dir, "palette.bin")
    if not os.path.isfile(path):
        return None
    data = open(path, "rb").read()
    rec = 8 + 256 * 3
    pal = None
    for i in range(0, len(data) - rec + 1, rec):
        b = data[i:i + rec]
        if b[:4] != b"\x00PAL":
            continue
        n = min(struct.unpack_from("<H", b, 6)[0], 256)
        pal = [tuple(b[8 + j * 3:11 + j * 3]) for j in range(n)]
    return pal


def write_bmp8(path, data, w, h, palette):
    row = (w + 3) & ~3
    pix = bytearray()
    for y in range(h - 1, -1, -1):
        line = data[y * w:(y + 1) * w]
        pix += line + b"\x00" * (row - w)
    hdr = bytearray(14 + 40 + 1024)
    hdr[0:2] = b"BM"
    struct.pack_into("<I", hdr, 2, 54 + 1024 + len(pix))
    struct.pack_into("<I", hdr, 10, 54 + 1024)
    struct.pack_into("<I", hdr, 14, 40)
    struct.pack_into("<i", hdr, 18, w)
    struct.pack_into("<i", hdr, 22, h)
    struct.pack_into("<H", hdr, 26, 1)
    struct.pack_into("<H", hdr, 28, 8)
    struct.pack_into("<I", hdr, 34, len(pix))
    struct.pack_into("<I", hdr, 46, 256)
    for i in range(256):
        r, g, b = palette[i] if palette and i < len(palette) else (0, 0, 0)
        hdr[54 + i * 4 + 0] = b
        hdr[54 + i * 4 + 1] = g
        hdr[54 + i * 4 + 2] = r
    with open(path, "wb") as f:
        f.write(hdr)
        f.write(pix)


def cmd_render(args):
    rows = read_manifest(os.path.join(args.dump, "sprites.tsv"))
    pal = last_palette(args.dump)
    out = args.out or os.path.join(args.dump, "render")
    os.makedirs(out, exist_ok=True)
    try:
        from PIL import Image
    except Exception:
        Image = None
    images = []
    for e in rows:
        src = os.path.join(args.dump, e["file"])
        if not os.path.isfile(src):
            continue
        data = open(src, "rb").read()
        if len(data) != e["w"] * e["h"]:
            continue
        base = os.path.splitext(os.path.basename(e["file"]))[0]
        bmp = os.path.join(out, base + ".bmp")
        write_bmp8(bmp, data, e["w"], e["h"], pal)
        if Image is not None:
            Image.open(bmp).save(os.path.join(out, base + ".png"))
        images.append((e, bmp))
    if Image is not None and images:
        # Contact sheet: 96x96 cells, scaled down for a quick visual scan.
        cols = 10
        cw = ch = 96
        rowsn = (len(images) + cols - 1) // cols
        sheet = Image.new("RGB", (cols * cw, rowsn * ch), (32, 32, 32))
        for i, (e, bmp) in enumerate(images):
            im = Image.open(bmp).convert("RGB")
            im.thumbnail((cw, ch))
            sheet.paste(im, ((i % cols) * cw, (i // cols) * ch))
        sheet.save(os.path.join(out, "sheet.png"))
    log("render: %d sprite(s) -> %s" % (len(images), out))


def cmd_pack(args):
    rows = read_manifest(os.path.join(args.dump, "sprites.tsv"))
    if args.only_crc:
        want = int(args.only_crc, 16)
        rows = [e for e in rows if e["crc"] == want]
    if args.only_offset:
        want = int(args.only_offset, 16)
        rows = [e for e in rows if e["offset"] == want]
    if not rows:
        raise SystemExit("pack: no manifest row matched the filter")
    out = args.out or os.path.join(args.dump, "pack")
    if os.path.isdir(out):
        shutil.rmtree(out)
    os.makedirs(os.path.join(out, "sprites"), exist_ok=True)
    scale = max(1, args.scale)
    made = {}
    lines = ["# nwassets 1"]
    for e in rows:
        src = os.path.join(args.dump, e["file"])
        if not os.path.isfile(src):
            continue
        data = open(src, "rb").read()
        if len(data) != e["w"] * e["h"]:
            continue
        key = (e["crc"], e["w"], e["h"])
        if key not in made:
            W, H = e["w"] * scale, e["h"] * scale
            hi = bytearray(W * H)
            for y in range(H):
                sy = y // scale
                row = data[sy * e["w"]:(sy + 1) * e["w"]]
                for x in range(W):
                    hi[y * W + x] = row[x // scale]
            if args.marker:
                mw, mh = max(1, W // 4), max(1, H // 4)
                for y in range(mh):
                    hi[y * W:y * W + mw] = bytes([255]) * mw
            name = "sprites/h%08x_%dx%d.idx" % (e["crc"], W, H)
            with open(os.path.join(out, name), "wb") as f:
                f.write(hi)
            made[key] = (name, W, H)
        name, W, H = made[key]
        lines.append("%08x\t%d\t%d\t%08x\t%08x\t%s\t%d\t%d"
                     % (e["offset"], e["w"], e["h"], e["stored"], e["crc"],
                        name, W, H))
    with open(os.path.join(out, "sprites.tsv"), "w") as f:
        f.write("\n".join(lines) + "\n")
    log("pack: %d row(s), %d unique sprite(s), scale=%d -> %s"
        % (len(lines) - 1, len(made), scale, out))


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("render", help="convert a dump to BMP/PNG + contact sheet")
    p.add_argument("dump", help="NW_DEBUG_ASSET_DUMP directory")
    p.add_argument("--out")
    p.set_defaults(func=cmd_render)

    p = sub.add_parser("pack", help="build a high-resolution override pack")
    p.add_argument("dump", help="NW_DEBUG_ASSET_DUMP directory")
    p.add_argument("--out")
    p.add_argument("--scale", type=int, default=4)
    p.add_argument("--only-crc", help="pack only this decoded-pixel CRC32 (hex)")
    p.add_argument("--only-offset", help="pack only this logical offset (hex)")
    p.add_argument("--no-marker", dest="marker", action="store_false",
                   help="omit the test marker block")
    p.set_defaults(func=cmd_pack, marker=True)

    args = ap.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
