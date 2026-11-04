#!/usr/bin/env python3
"""Replace frame-cell animation frames in the NETWALK overlay.

The startup/menu animation is not AVI/MPEG.  It is a sequence of frame blocks:

    uint32 type
    uint32 stored_bytes
    uint32 raw_bytes
    stored_bytes of frame-cell command data

`nwextract.py` renders those blocks to 640x480 PNGs under movies/.  This tool
takes edited 640x480 (or larger, nearest-fitted) PNG/IDX frames and writes each
one as a full-surface frame block.  The block, delta-payload length, index
entry 2, and trailer are rebuilt.

The default writes the original 640x480 cell.  `--frame-scale N` writes a
native 640*N x 480*N cell and accepts sources at that size; use
`NW_RENDER_SCALE=N` when running the SDL build.  `nwextract.py --frame-scale`
changes only extracted preview PNGs.
"""
from __future__ import annotations

import argparse
import os
import shutil
import struct
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from nwextract import (  # noqa: E402
    Overlay,
    render_frame_block,
    scan_animation_palette,
    scan_frame_groups,
    walk_stream,
)
from nwrepack import TRAILER_SIZE, gap_control, put_u32  # noqa: E402

W, H = 640, 480


class FrameError(Exception):
    pass


class Palette:
    def __init__(self, data: bytes):
        if len(data) < 768:
            raise FrameError("palette must be 768 bytes")
        self.c = [tuple(data[i:i + 3]) for i in range(0, 768, 3)]
        self.cache = {}

    def idx(self, rgb):
        got = self.cache.get(rgb)
        if got is not None:
            return got
        best, bd = 0, 1 << 30
        for i, p in enumerate(self.c):
            d = ((rgb[0] - p[0]) ** 2 + (rgb[1] - p[1]) ** 2 +
                 (rgb[2] - p[2]) ** 2)
            if d < bd:
                bd, best = d, i
                if d == 0:
                    break
        self.cache[rgb] = best
        return best


def top_down(frame: bytes, width: int = W, height: int = H) -> bytes:
    return b"".join(frame[y * width:(y + 1) * width]
                    for y in range(height - 1, -1, -1))


def bottom_up(pix: bytes, width: int = W, height: int = H) -> bytes:
    return b"".join(pix[y * width:(y + 1) * width]
                    for y in range(height - 1, -1, -1))


def nearest_scale(pix: bytes, sw: int, sh: int, dw: int, dh: int) -> bytes:
    if sw == dw and sh == dh:
        return pix
    out = bytearray(dw * dh)
    for y in range(dh):
        sy = (y * sh) // dh
        row = pix[sy * sw:(sy + 1) * sw]
        out[y * dw:(y + 1) * dw] = bytes(row[(x * sw) // dw]
                                          for x in range(dw))
    return bytes(out)


def load_frame(path: Path, palette: Palette, width: int, height: int) -> bytes:
    """Return a top-down width x height palette-index image."""
    if path.suffix.lower() == ".idx":
        pix = path.read_bytes()
        if len(pix) != width * height:
            raise FrameError("%s: expected %d bytes, got %d" %
                             (path, width * height, len(pix)))
        return pix
    try:
        from PIL import Image
    except Exception as exc:
        raise FrameError("Pillow is required for PNG frames: %s" % exc)
    im = Image.open(path).convert("RGB")
    if im.size != (width, height):
        im = im.resize((width, height), Image.Resampling.NEAREST)
    getdata = getattr(im, "get_flattened_data", im.getdata)
    return bytes(palette.idx(px) for px in getdata())


def make_block(old_header: bytes, pix_top_down: bytes,
               width: int = W, height: int = H) -> bytes:
    # A single full-surface cell in logical or native coordinates:
    #   flags 0xB0 = explicit x/y/w/h, decoder 0 (plain), end-of-command.
    #   x=0, y=0, w=width, h=height.
    # The compositor's command decoder consumes source rows top-down even
    # though its framebuffer memory is bottom-up; render_frame_block()
    # already accounts for that when it flips the captured image.
    if width > 0xFFFF or height > 0xFFFF:
        raise FrameError("frame dimensions exceed the cell encoding")
    flags = 0x80 | 0x10 | 0x20
    if width <= 0xFF:
        flags &= ~0x10
    if height <= 0xFF:
        flags &= ~0x20
    body = bytearray([flags, 0, 0, width & 0xFF])
    if flags & 0x10:
        body += bytes([(width >> 8) & 0xFF])
    body += bytes([height & 0xFF])
    if flags & 0x20:
        body += bytes([(height >> 8) & 0xFF])
    body += pix_top_down
    old_type = struct.unpack_from("<I", old_header, 0)[0]
    typ = (old_type & 0xFFFFFF00) | 1       # preserve high flag byte, force kind 1
    return struct.pack("<III", typ, len(body), len(body)) + bytes(body)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("input", help="overlay.nw or NETWALK.EXE")
    ap.add_argument("--frames", required=True,
                    help="directory containing groupN/frame_XXXX.png or .idx")
    ap.add_argument("--palette", help="startup palette (default movies/palette.pal)")
    ap.add_argument("-o", "--out", default="repacked", help="output directory")
    ap.add_argument("--force", action="store_true",
                    help="rewrite frames even when pixels are unchanged")
    ap.add_argument("--frame-scale", type=int, default=1,
                    help="write native 640*N x 480*N full-surface cells "
                         "(use with NW_RENDER_SCALE=N)")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()
    if args.frame_scale < 1 or args.frame_scale > 8:
        raise FrameError("--frame-scale must be in 1..8")
    frame_w = W * args.frame_scale
    frame_h = H * args.frame_scale

    inp = Path(args.input).resolve()
    if not inp.is_file():
        raise FrameError("input not found: %s" % inp)
    ov = Overlay(str(inp))
    records, gaps = walk_stream(ov)
    groups = scan_frame_groups(ov.data, gaps)
    if not groups:
        raise FrameError("no frame groups found")

    pal_path = Path(args.palette) if args.palette else \
        (inp.parent / "movies" / "palette.pal")
    if pal_path.is_file():
        palette = Palette(pal_path.read_bytes())
    else:
        autopal, _ = scan_animation_palette(ov.data, gaps)
        if autopal is None:
            raise FrameError("no startup palette found; pass --palette")
        palette = Palette(autopal)

    frames_dir = Path(args.frames).resolve()
    edits = []
    replaced = 0
    for gi, group in enumerate(groups):
        for bi, block in enumerate(group["blocks"]):
            src = frames_dir / ("group%d" % gi) / ("frame_%04d.png" % bi)
            if not src.is_file():
                src = frames_dir / ("group%d" % gi) / ("frame_%04d.idx" % bi)
            if not src.is_file():
                continue
            pix = load_frame(src, palette, frame_w, frame_h)
            old_pix = top_down(render_frame_block(block["payload"], W, H))
            old_pix = nearest_scale(old_pix, W, H, frame_w, frame_h)
            if not args.force and pix == old_pix:
                if args.verbose:
                    print("unchanged frame %d/%d" % (gi, bi), file=sys.stderr)
                continue
            old_start = block["offset"]
            old_end = old_start + 12 + block["stored"]
            new_block = make_block(ov.data[old_start:old_start + 12], pix,
                                   frame_w, frame_h)
            edits.append({
                "start": old_start,
                "end": old_end,
                "bytes": new_block,
                "delta": len(new_block) - (old_end - old_start),
                "what": "frame %d/%d" % (gi, bi),
            })
            replaced += 1
            if args.verbose:
                print("replace frame %d/%d at 0x%x (%d -> %d bytes)" %
                      (gi, bi, old_start, old_end - old_start, len(new_block)),
                      file=sys.stderr)

    if not edits:
        raise FrameError("no changed frames found under %s" % frames_dir)

    # Update the enclosing 0x05 delta length for every edited frame.
    for gs, ge, why in gaps:
        inside = [e for e in edits
                  if e["start"] >= gs and e["end"] <= ge and e["delta"]]
        if not inside:
            continue
        off, width = gap_control(ov.data, gs, ge, why)
        old = int.from_bytes(ov.data[off:off + width], "little")
        new = old + sum(e["delta"] for e in inside)
        if new < 0 or new >= (1 << (width * 8)):
            raise FrameError("delta length at 0x%x overflowed" % gs)
        edits.append({
            "start": off,
            "end": off + width,
            "bytes": new.to_bytes(width, "little"),
            "delta": 0,
            "what": "delta length at 0x%x" % gs,
        })

    edits.sort(key=lambda e: (e["start"], e["end"]))
    for a, b in zip(edits, edits[1:]):
        if a["end"] > b["start"]:
            raise FrameError("overlapping edits: %s and %s" %
                             (a["what"], b["what"]))

    entry_abs = ov.dbase + ov.entries[2]
    for e in edits:
        if e["start"] < entry_abs < e["end"]:
            raise FrameError("edit crosses entry 2")
    new_entry2 = ov.entries[2] + sum(e["delta"] for e in edits
                                     if e["end"] <= entry_abs)

    buf = bytearray(ov.data)
    for e in sorted(edits, key=lambda x: x["start"], reverse=True):
        buf[e["start"]:e["end"]] = e["bytes"]

    new_raw = bytes(buf[ov.dbase:len(buf) - TRAILER_SIZE])
    if not (0 <= new_entry2 < len(new_raw)):
        raise FrameError("rebuilt entry 2 is outside the overlay")
    tail = bytearray(ov.data[-TRAILER_SIZE:])
    for i in range(ov.extent + 1):
        off = TRAILER_SIZE - ((ov.extent - i) * 5 + 23 + 5)
        put_u32(tail, off, new_entry2 if i == 2 else ov.entries[i])
    put_u32(tail, 19, ov.dbase)

    out = Path(args.out)
    if out.exists() and not out.is_dir():
        raise FrameError("output is not a directory")
    out.mkdir(parents=True, exist_ok=True)
    (out / "overlay.bin").write_bytes(new_raw)
    (out / "overlay.trailer").write_bytes(tail)
    (out / "overlay.nw").write_bytes(b"\0" * ov.dbase + new_raw + bytes(tail))
    (out / "frame_repack.tsv").write_text(
        "# what\told_start\told_end\tnew_bytes\tdelta\n" +
        "".join("%s\t0x%x\t0x%x\t%d\t%d\n" %
                (e["what"], e["start"], e["end"], len(e["bytes"]), e["delta"])
                for e in sorted(edits, key=lambda x: x["start"])),
        encoding="utf-8")

    print("replaced frames: %d" % replaced)
    print("entry 2: 0x%x -> 0x%x" % (ov.entries[2], new_entry2))
    print("overlay: %d -> %d bytes" % (len(ov.overlay), len(new_raw)))
    print("output: %s" % out)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except FrameError as exc:
        print("nwframe_repack: %s" % exc, file=sys.stderr)
        raise SystemExit(1)
