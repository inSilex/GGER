#!/usr/bin/env python3
"""nwrepack.py -- rebuild the NETWALK VM overlay with replaced image records.

The shipped overlay is a byte stream, not a flat archive.  Image records carry
their own 4-byte ``width,height`` header and may be raw or LZSS blocks.  Records
are concatenated with VM control bytes, and the stream ends with a 38-byte
index/trailer.  Replacing an image can therefore change a record's length, shift
everything after it, and move the top-level index entry.

This tool:

  * parses the same records as nwextract.py;
  * replaces image records from a pack directory (files named like the
    extractor's images/ output) or explicit --replace mappings;
  * accepts RGB replacement PNGs (e.g. ComfyUI upscales) and maps them onto
    the game's 768-byte palette, either in memory or, with --reindex, by
    rewriting the pack PNGs as indexed images;
  * supports replacement images of any size: the default fits them to the
    original record geometry (nearest-neighbour), --exact keeps the new size;
  * re-encodes image blocks (all-literal LZSS for LZSS records, raw otherwise);
  * updates embedded-image gaps and the VM skip/delta lengths that enclose them;
  * rebuilds the index entry 2 and the 38-byte trailer;
  * writes overlay.bin, overlay.trailer and overlay.nw.

Usage:
    python3 tools/nwrepack.py extracted/NETWALK/overlay.nw \\
        --pack edited-png -o repacked

    python3 tools/nwrepack.py NETWALK.EXE \\
        --replace images/s43_45x45_abc.png=edited/foo.png -o repacked

    # Convert an upscaled (RGB) pack to indexed PNGs in place, then repack:
    python3 tools/nwrepack.py extracted/NETWALK/overlay.nw \\
        --pack extracted_upscaled/NETWALK --reindex -o repacked

    # Only convert the PNGs, without repacking:
    python3 tools/nwrepack.py --pack extracted_upscaled/NETWALK --reindex

The output is an external-loadable overlay.nw.  It has the original dbase
padding, so use it with the SDL external-overlay build.
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import struct
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from nwextract import (  # noqa: E402
    Overlay,
    crc32,
    flattened,
    image_from_block,
    palette_matches,
    scan_embedded_images,
    to_indexed,
    walk_stream,
)

TRAILER_SIZE = 38
TRAILER_MAGIC = b"=VS="
PNG_RE = re.compile(r"s([0-9a-fA-F]{2})_(\d+)x(\d+)_([0-9a-fA-F]{8})\.png$")

# High-resolution sidecar embedded in the overlay, immediately before the
# original 38-byte trailer.  The section is ignored by the VM stream reader;
# the SDL renderer looks it up by descriptor stream offset.
HR_MAGIC = b"NWHRPL01"
HR_VERSION = 1
HR_FOOTER_SIZE = 32
HR_ENTRY_SIZE = 24
HR_TABLE_HEADER_SIZE = 4
HR_FOOTER = struct.Struct("<8sIIIIII")
HR_ENTRY = struct.Struct("<IIHHHHII")

# Synthetic descriptor offset for the full-screen background.  Real stream
# offsets are below the 5 MB overlay size, so this cannot collide with a
# sprite record.  The engine asks for it through NwAssetGetHighResBackground.
HR_BACKGROUND_OFFSET = 0x4E574247


class RepackError(Exception):
    pass


def u32(b: bytes) -> int:
    return struct.unpack_from("<I", b, 0)[0]


def put_u32(buf: bytearray, off: int, value: int) -> None:
    if value < 0 or value > 0xFFFFFFFF:
        raise RepackError("uint32 out of range: %d" % value)
    struct.pack_into("<I", buf, off, value)


def load_source(path: Path, trailer_path: Path | None) -> Overlay:
    data = path.read_bytes()
    if data[-4:] == TRAILER_MAGIC:
        return Overlay(str(path), data)
    if trailer_path is None:
        trailer_path = path.with_name("overlay.trailer")
    if not trailer_path.is_file():
        raise RepackError(
            "raw overlay needs a 38-byte trailer; pass --trailer or place "
            "overlay.trailer next to %s" % path)
    tail = trailer_path.read_bytes()
    if len(tail) != TRAILER_SIZE or tail[-4:] != TRAILER_MAGIC:
        raise RepackError("not a 38-byte =VS= trailer: %s" % trailer_path)
    dbase = u32(tail[19:23])
    if dbase == 0:
        raise RepackError("trailer dbase is zero; use an overlay.nw")
    return Overlay(str(path), b"\0" * dbase + data + tail)


def parse_records(ov: Overlay):
    records, gaps = walk_stream(ov)
    main_count = len(records)
    embedded = scan_embedded_images(ov.data, gaps, records)
    records.extend(embedded)
    return records, gaps, main_count


def read_image_rows(path: Path) -> dict[int, dict]:
    rows: dict[int, dict] = {}
    if not path.is_file():
        return rows
    with path.open("r", encoding="utf-8") as f:
        header = f.readline()
        for line in f:
            if not line.strip() or line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t")
            if len(fields) < 10:
                continue
            try:
                tag = int(fields[2], 0)
                rec = int(fields[0])
            except ValueError:
                continue
            rows[tag] = {
                "record": rec,
                "state": fields[1],
                "tag_offset": tag,
                "stream_offset": int(fields[3], 0) if fields[3] else 0,
                "stored": int(fields[4], 0) if fields[4] else 0,
                "decoded": int(fields[5], 0) if fields[5] else 0,
                "width": int(fields[6]) if fields[6].isdigit() else 0,
                "height": int(fields[7]) if fields[7].isdigit() else 0,
                "crc": int(fields[8], 0) if fields[8] else 0,
                "idx": fields[9] if len(fields) > 9 else "",
                "png": fields[10],
                "source": fields[11] if len(fields) > 11 else "main",
            }
    return rows


def find_manifest(input_path: Path, pack: Path | None, explicit: Path | None):
    if explicit is not None:
        return explicit
    candidates = []
    if pack is not None:
        candidates += [pack / "images.tsv", pack.parent / "images.tsv"]
    candidates += [input_path.parent / "images.tsv",
                   input_path.parent.parent / "images.tsv"]
    for p in candidates:
        if p.is_file():
            return p
    return None


def resolve_pack(pack: Path) -> Path:
    """Accept an asset root, or a directory that contains exactly one.

    A normal extraction is extracted/NETWALK/, so users often pass
    extracted/ (or extracted_upscaled/) instead of the NETWALK folder.
    """
    def looks_like_tree(path: Path) -> bool:
        return (path / "images.tsv").is_file() or (path / "palettes").is_dir()

    if looks_like_tree(pack):
        return pack
    netwalk = pack / "NETWALK"
    if netwalk.is_dir() and looks_like_tree(netwalk):
        return netwalk
    try:
        matches = [p for p in pack.iterdir()
                   if p.is_dir() and looks_like_tree(p)]
    except OSError:
        return pack
    if len(matches) == 1:
        return matches[0]
    return pack


def find_palette(input_path: Path | None, pack: Path | None,
                 explicit: Path | None):
    if explicit is not None:
        if not explicit.is_file():
            raise RepackError("palette not found: %s" % explicit)
        return explicit.read_bytes()[:768]
    candidates = []
    if pack is not None:
        candidates += [pack / "palettes" / "main.pal",
                       pack.parent / "palettes" / "main.pal"]
    if input_path is not None:
        candidates += [input_path.parent / "palettes" / "main.pal",
                       input_path.parent.parent / "palettes" / "main.pal"]
    for p in candidates:
        if p.is_file():
            data = p.read_bytes()
            if len(data) >= 768:
                return data[:768]
    raise RepackError("no 768-byte palette found; pass --palette "
                      "(usually extracted/NETWALK/palettes/main.pal)")


def palette_for_png(png: Path, pack: Path, main_palette: bytes) -> bytes:
    """Pick the palette that belongs to *png* inside an extracted tree."""
    sibling = png.with_suffix(".pal")
    if sibling.is_file():
        data = sibling.read_bytes()
        if len(data) >= 768:
            return data[:768]
    relative = png.relative_to(pack)
    if relative.parts and relative.parts[0] == "movies":
        movie_palette = pack / "movies" / "palette.pal"
        if movie_palette.is_file():
            data = movie_palette.read_bytes()
            if len(data) >= 768:
                return data[:768]
    return main_palette


def reindex_png(path: Path, palette: bytes, dither: bool = False) -> bool:
    """Rewrite *path* in place as an indexed PNG. True if it changed."""
    from PIL import Image
    try:
        im = Image.open(path)
    except OSError as exc:
        raise RepackError("cannot read image %s: %s" % (path, exc)) from None
    try:
        if palette_matches(im, palette):
            return False
        indexed = to_indexed(im, palette, dither=dither)
    finally:
        im.close()

    temporary = path.with_name(path.name + ".tmp")
    try:
        indexed.save(temporary, "PNG")
        os.replace(temporary, path)
    except OSError:
        try:
            temporary.unlink()
        except OSError:
            pass
        raise
    return True


def reindex_pack(pack: Path, main_palette: bytes, dither: bool = False):
    """Convert every PNG in *pack* to indexed mode, using its own palette."""
    changed = kept = 0
    for png in sorted(pack.rglob("*.png")):
        if reindex_png(png, palette_for_png(png, pack, main_palette), dither):
            changed += 1
        else:
            kept += 1
    return changed, kept


def pack_file(pack: Path | None, rel: str) -> Path | None:
    rel = rel.replace("\\", "/")
    if pack is None:
        p = Path(rel)
        return p if p.is_file() else None
    candidates = [pack / rel, pack / Path(rel).name]
    if rel.startswith("images/"):
        candidates.append(pack / "images" / Path(rel).name)
    for p in candidates:
        if p.is_file():
            return p
    return None


def find_highres_background(pack: Path, scale: int):
    """Return (png, original_width, original_height) for the pack background.

    Extracted trees put the single 640x480 screen background in a
    ``background/`` directory; the filename carries its original geometry and
    the sibling ``.idx`` file proves that geometry.  Any other tree layout is
    ignored rather than guessed at.
    """
    from PIL import Image

    candidates = []
    for sub in ("background", "backgrounds"):
        d = pack / sub
        if d.is_dir():
            candidates.extend(sorted(d.glob("*.png")))
    if not candidates:
        candidates.extend(sorted(pack.glob("background*.png")))

    for png in candidates:
        match = re.search(r"(\d+)x(\d+)", png.stem)
        if match is None:
            continue
        ow, oh = int(match.group(1)), int(match.group(2))
        if ow <= 0 or oh <= 0:
            continue
        idx = png.with_suffix(".idx")
        if idx.is_file() and idx.stat().st_size != ow * oh:
            continue
        with Image.open(png) as im:
            sw, sh = im.size
        if sw < ow or sh < oh:
            continue
        if scale and (sw != ow * scale or sh != oh * scale):
            continue
        return png, ow, oh
    return None


class PaletteMapper:
    def __init__(self, palette: bytes):
        self.palette = palette[:768]

    def image_indices(self, path: Path, size: tuple[int, int]):
        """Return palette indices for *path*, fitted to *size* if needed."""
        from PIL import Image
        im = Image.open(path)
        try:
            if palette_matches(im, self.palette):
                # Already indexed with the game palette: keep the exact indices.
                if im.size != size:
                    im = im.resize(size, Image.Resampling.NEAREST)
                return bytes(flattened(im))
            im = im.convert("RGB")
            if im.size != size:
                im = im.resize(size, Image.Resampling.NEAREST)
            return bytes(flattened(to_indexed(im, self.palette)))
        finally:
            im.close()


def encode_block(data: bytes, lzss: bool) -> bytes:
    """Encode one VM block, excluding the next stream tag byte."""
    if lzss:
        comp = bytearray()
        for pos in range(0, len(data), 8):
            chunk = data[pos:pos + 8]
            comp.append((1 << len(chunk)) - 1)
            comp.extend(chunk)
        max_len = max(len(comp), len(data), 1)
        width = max(1, (max_len.bit_length() + 7) // 8)
        if width > 4:
            raise RepackError("LZSS block is too large")
        tag = 0x80 | 0x04 | (width - 1)
        return (bytes([tag]) +
                len(comp).to_bytes(width, "little") +
                len(data).to_bytes(width, "little") +
                bytes(comp))

    comp = data
    if len(comp) <= 0x1F:
        return bytes([0x80 | 0x40 | len(comp)]) + comp
    width = max(1, (len(comp).bit_length() + 7) // 8)
    if width > 4:
        raise RepackError("raw block is too large")
    tag = 0x80 | (width - 1)
    return bytes([tag]) + len(comp).to_bytes(width, "little") + comp


DISK_TAG = b"ksiD"          # 0x4469736b little-endian


def block_comp_len(block: bytes, lzss: bool) -> int:
    if not lzss:
        return 0
    width = (block[0] & 3) + 1
    return int.from_bytes(block[1:1 + width], "little")


def descriptor_sites(data: bytes):
    """Yield (disk_tag_offset, old_offset, stored) for inline Disk records."""
    pos = 0
    while True:
        pos = data.find(DISK_TAG, pos)
        if pos < 4 or pos + 12 > len(data):
            return
        yield pos, int.from_bytes(data[pos + 4:pos + 8], "little"), \
            int.from_bytes(data[pos + 8:pos + 12], "little")
        pos += 4


def gap_control(data: bytes, gap_start: int, gap_end: int, why: str):
    """Return (offset, width) of the byte count that skips this gap."""
    if why == "opcode":
        off = gap_start - 4
        if off < 0:
            raise RepackError("bad opcode gap at 0x%x" % gap_start)
        return off, 4
    if why == "delta":
        for p in range(gap_start - 5, gap_start):
            if p < 0:
                continue
            tag = data[p]
            if (tag & 0xEC) != 0xEC:
                continue
            width = (tag & 3) + 1
            if p + 1 + width == gap_start:
                return p + 1, width
        raise RepackError("could not locate delta field before 0x%x" % gap_start)
    raise RepackError("unknown gap kind %r" % why)


def record_image(rec):
    # walk_stream() does not run classify(), so use the state/data shape here.
    if rec.state not in (0x43, 0x14):
        return None
    img = image_from_block(rec.data)
    if img is None:
        return None
    w, h, pixels = img
    if len(rec.data) < 4 + w * h:
        return None
    return w, h, pixels, rec.data[4 + w * h:]


def resolve_old(spec: str, records, image_rows):
    """Resolve --replace OLD to a list of records."""
    try:
        idx = int(spec, 0)
    except ValueError:
        idx = None
    if idx is not None:
        found = [r for r in records if r.index == idx]
        if found:
            return found
        found = [r for r in records if r.tag_offset == idx]
        if found:
            return found
        raise RepackError("no record index or tag offset %d" % idx)
    if spec.lower().startswith("0x"):
        try:
            tag = int(spec, 16)
        except ValueError:
            tag = None
        if tag is not None:
            return [r for r in records if r.tag_offset == tag]
    name = Path(spec).name
    m = PNG_RE.search(name)
    if m:
        state = int(m.group(1), 16)
        w = int(m.group(2))
        h = int(m.group(3))
        crc = int(m.group(4), 16)
        found = []
        for r in records:
            img = record_image(r)
            if (r.state == state and img and img[0] == w and img[1] == h and
                    crc32(img[2]) == crc):
                found.append(r)
        if found:
            return found
    for tag, row in image_rows.items():
        if Path(row["png"]).name == name:
            return [r for r in records if r.tag_offset == tag]
    raise RepackError("could not resolve replacement source %r" % spec)


def build_replacements(records, gaps, args, input_path: Path,
                       pack: Path | None):
    manifest_path = find_manifest(input_path, pack,
                                  Path(args.images_tsv).resolve()
                                  if args.images_tsv else None)
    image_rows = read_image_rows(manifest_path) if manifest_path else {}
    by_tag = {r.tag_offset: r for r in records}

    # record -> replacement PNG
    repl: dict[object, Path] = {}
    if pack is not None:
        if manifest_path:
            for tag, row in image_rows.items():
                src = pack_file(pack, row["png"])
                if src is None:
                    continue
                rec = by_tag.get(tag)
                if rec is not None:
                    repl[rec] = src
        else:
            # Fall back to the extractor's filename convention.
            for src in pack.rglob("*.png"):
                m = PNG_RE.search(src.name)
                if not m:
                    continue
                state = int(m.group(1), 16)
                w = int(m.group(2))
                h = int(m.group(3))
                crc = int(m.group(4), 16)
                for rec in records:
                    img = record_image(rec)
                    if (rec.state == state and img and
                            img[0] == w and img[1] == h and
                            crc32(img[2]) == crc):
                        repl[rec] = src

    for spec in args.replace:
        if "=" not in spec:
            raise RepackError("--replace wants OLD=NEW, got %r" % spec)
        old, new = spec.split("=", 1)
        src = Path(new)
        if not src.is_file():
            raise RepackError("replacement image not found: %s" % src)
        for rec in resolve_old(old, records, image_rows):
            repl[rec] = src

    if not repl:
        raise RepackError("no replacement images found; use --pack or --replace")
    return repl


def output_names(out: Path):
    """Return (directory, nw_name, bin_name, trailer_name, report_name)."""
    suffix = out.suffix.lower()
    if suffix in (".nw", ".bin") and not (out.exists() and out.is_dir()):
        output_dir = out.parent if str(out.parent) else Path(".")
        stem = out.stem
        nw_name = out.name if suffix == ".nw" else stem + ".nw"
        bin_name = out.name if suffix == ".bin" else stem + ".bin"
        trailer_name = stem + ".trailer"
        report_name = stem + ".tsv"
    else:
        output_dir = out
        nw_name, bin_name = "overlay.nw", "overlay.bin"
        trailer_name, report_name = "overlay.trailer", "repack.tsv"
    return output_dir, nw_name, bin_name, trailer_name, report_name


def build_highres_overlay(args, input_path: Path, pack: Path) -> int:
    """Embed a high-resolution asset section without touching the VM stream.

    The overlay's records, index entry 2, dbase and heap offsets stay byte
    identical.  This is what makes a 4x overlay safe: the VM keeps its original
    640x480 geometry, while the SDL blitter samples the high-resolution pixels
    from the appended table when NW_RENDER_SCALE is greater than one.
    """
    manifest_path = find_manifest(
        input_path, pack,
        Path(args.images_tsv).resolve() if args.images_tsv else None)
    if manifest_path is None or not manifest_path.is_file():
        raise RepackError("--highres needs an images.tsv (use --images-tsv)")
    rows = read_image_rows(manifest_path)
    if not rows:
        raise RepackError("no image rows in %s" % manifest_path)
    if args.scale < 0:
        raise RepackError("--scale must be zero or positive")

    main_palette = find_palette(
        input_path, pack,
        Path(args.palette).resolve() if args.palette else None)
    mapper = PaletteMapper(main_palette)

    ov = load_source(input_path,
                     Path(args.trailer).resolve() if args.trailer else None)
    records, _gaps = walk_stream(ov)
    main_count = len(records)

    from PIL import Image

    entries = []
    blobs = []
    report_rows = []
    data_offset = 0
    skipped = 0
    sources = {"main": 0, "embedded": 0}
    scales = set()
    for tag, row in sorted(rows.items()):
        w = int(row["width"])
        h = int(row["height"])
        off = int(row.get("stream_offset", 0))
        crc = int(row.get("crc", 0))
        png = pack_file(pack, row["png"])
        if png is None:
            skipped += 1
            if args.verbose:
                print("skip missing high-res image: %s" % row["png"],
                      file=sys.stderr)
            continue
        if w <= 0 or h <= 0:
            skipped += 1
            continue
        if off <= 0 or off >= len(ov.overlay):
            skipped += 1
            if args.verbose:
                print("skip bad stream offset 0x%x for %s" % (off, png),
                      file=sys.stderr)
            continue
        with Image.open(png) as im:
            sw, sh = im.size
        if sw <= 0 or sh <= 0 or sw < w or sh < h:
            raise RepackError("bad source size %dx%d for %s (%dx%d)" %
                              (sw, sh, png, w, h))
        if args.scale:
            if sw != w * args.scale or sh != h * args.scale:
                raise RepackError(
                    "%s is %dx%d, expected %dx%d for --scale %d" %
                    (png, sw, sh, w * args.scale, h * args.scale, args.scale))
        if sw % w == 0 and sh % h == 0 and sw // w == sh // h:
            scales.add(sw // w)
        else:
            scales.add(0)
        pixels = mapper.image_indices(png, (sw, sh))
        if len(pixels) != sw * sh:
            raise RepackError("source pixel count mismatch for %s" % png)
        entries.append((off, crc, w, h, sw, sh, data_offset, len(pixels)))
        blobs.append(bytes(pixels))
        report_rows.append(row["png"])
        data_offset += len(pixels)
        key = row.get("source", "main")
        sources[key] = sources.get(key, 0) + 1

    bg_png = None
    bg_size = None
    bg_hi_size = None
    bg_info = find_highres_background(pack, args.scale)
    if bg_info is not None:
        bg_png, bw, bh = bg_info
        with Image.open(bg_png) as im:
            bsw, bsh = im.size
        if bsw <= 0 or bsh <= 0 or bsw < bw or bsh < bh:
            raise RepackError("bad background size %dx%d for %s (%dx%d)" %
                              (bsw, bsh, bg_png, bw, bh))
        if args.scale and (bsw != bw * args.scale or bsh != bh * args.scale):
            raise RepackError(
                "%s is %dx%d, expected %dx%d for --scale %d" %
                (bg_png, bsw, bsh, bw * args.scale, bh * args.scale,
                 args.scale))
        bg_pixels = mapper.image_indices(bg_png, (bsw, bsh))
        if len(bg_pixels) != bsw * bsh:
            raise RepackError("background pixel count mismatch for %s" % bg_png)
        entries.append((HR_BACKGROUND_OFFSET, 0, bw, bh, bsw, bsh,
                        data_offset, len(bg_pixels)))
        blobs.append(bytes(bg_pixels))
        report_rows.append(str(bg_png.relative_to(pack))
                           if bg_png.is_relative_to(pack) else bg_png.name)
        data_offset += len(bg_pixels)
        bg_size = (bw, bh)
        bg_hi_size = (bsw, bsh)
        if bsw % bw == 0 and bsh % bh == 0 and bsw // bw == bsh // bh:
            scales.add(bsw // bw)
        else:
            scales.add(0)

    if not entries:
        raise RepackError("no usable high-resolution images in %s" %
                          manifest_path)

    table_len = HR_TABLE_HEADER_SIZE + HR_ENTRY_SIZE * len(entries)
    pad = (-table_len) & 3
    table = bytearray(struct.pack("<I", len(entries)))
    table.extend(b"\0" * pad)
    # Data offsets are relative to the section start, which also keeps the
    # table valid when overlay.bin is materialized with dbase padding.
    entries = [(off, crc, w, h, sw, sh,
                table_len + pad + doff, dlen)
               for (off, crc, w, h, sw, sh, doff, dlen) in entries]
    for e in entries:
        table.extend(HR_ENTRY.pack(*e))
    section = bytes(table) + b"".join(blobs)
    if len(section) != HR_TABLE_HEADER_SIZE + pad + HR_ENTRY_SIZE * len(entries) + data_offset:
        raise RepackError("internal high-res section size mismatch")

    footer = HR_FOOTER.pack(HR_MAGIC, HR_VERSION, len(section), len(table),
                            len(entries), 0, 0)
    if len(footer) != HR_FOOTER_SIZE:
        raise RepackError("internal high-res footer size mismatch")

    # The original VM stream's final record overreads five bytes into the
    # 38-byte trailer.  Preserve that copy immediately after the stream, then
    # put the high-res section behind it, and finish with a fresh trailer for
    # the external loader.  The appended section is therefore invisible both
    # to the VM and to walk_stream().
    tail = bytes(ov.data[-TRAILER_SIZE:])
    new_overlay = bytes(ov.overlay) + tail + section + footer

    out = Path(args.out)
    output_dir, nw_name, bin_name, trailer_name, report_name = output_names(out)
    if output_dir.exists() and not output_dir.is_dir():
        raise RepackError("output path exists and is not a directory: %s" %
                          output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / bin_name).write_bytes(new_overlay)
    (output_dir / trailer_name).write_bytes(tail)
    (output_dir / nw_name).write_bytes(
        b"\0" * ov.dbase + new_overlay + tail)

    report = output_dir / report_name
    with report.open("w", encoding="utf-8") as f:
        f.write("# highres\t1\n")
        f.write("# offset\tw\th\tsw\tsh\tcrc\tdata_offset\tdata_len\tpng\n")
        for e, png_name in zip(entries, report_rows):
            f.write("%08x\t%d\t%d\t%d\t%d\t%08x\t%d\t%d\t%s\n" %
                    (e[0], e[2], e[3], e[4], e[5], e[1], e[6], e[7],
                     png_name))

    # The VM stream must still parse to exactly the same records.  The section
    # lives after the stream's terminating tag and is not visible to the VM.
    check = Overlay(str(output_dir / nw_name))
    check_records, _check_gaps = walk_stream(check)
    if len(check_records) != main_count:
        raise RepackError("high-res overlay changed the main record count: "
                          "%d -> %d" % (main_count, len(check_records)))

    scale_text = ("scale=%s" % ",".join(str(s) for s in sorted(scales))
                  if scales else "scale=mixed")
    image_count = len(entries) - (1 if bg_png is not None else 0)
    print("high-res images: %d (%s; %d skipped)" %
          (image_count, scale_text, skipped))
    if bg_png is not None:
        print("high-res background: %dx%d -> %dx%d (%s)" %
              (bg_size[0], bg_size[1], bg_hi_size[0], bg_hi_size[1],
               bg_png.name))
    print("vm stream: %d bytes (unchanged, %d records)" %
          (len(ov.overlay), main_count))
    print("high-res section: %d bytes" % len(section))
    print("overlay: %d -> %d bytes" %
          (len(ov.data), len(b"\0" * ov.dbase + new_overlay + tail)))
    print("output: %s (%s is the external-loadable stream)" %
          (output_dir, nw_name))
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("input", nargs="?",
                    help="NETWALK.EXE, overlay.nw, or raw overlay.bin "
                         "(optional with --reindex)")
    ap.add_argument("-o", "--out", default="repacked",
                    help="output directory, or a .nw/.bin file path "
                         "(default: repacked/)")
    ap.add_argument("--pack", help="directory with replacement PNGs (or a full "
                                   "extracted/NETWALK asset tree; the directory "
                                   "above NETWALK also works)")
    ap.add_argument("--images-tsv", help="override images.tsv mapping")
    ap.add_argument("--palette", help="768-byte main.pal")
    ap.add_argument("--trailer", help="38-byte trailer for a raw overlay.bin")
    ap.add_argument("--replace", action="append", default=[],
                    metavar="OLD=NEW",
                    help="replace record/tag/old-PNG with NEW (repeatable)")
    ap.add_argument("--reindex", action="store_true",
                    help="rewrite every PNG in --pack as an indexed PNG using "
                         "its palette, in place; with no INPUT, stop after that")
    ap.add_argument("--dither", action="store_true",
                    help="Floyd-Steinberg dithering when reindexing (default: off)")
    ap.add_argument("--exact", action="store_true",
                    help="keep replacement dimensions instead of fitting to the "
                         "original record geometry (experimental)")
    ap.add_argument("--highres", action="store_true",
                    help="embed the pack's high-resolution pixels in the overlay "
                         "without changing the VM stream; use with "
                         "NW_RENDER_SCALE>1 (recommended for upscaled art)")
    ap.add_argument("--scale", type=int, default=0,
                    help="with --highres, require source dimensions to be exactly "
                         "width*scale by height*scale (default: accept any larger "
                         "source)")
    ap.add_argument("--force", action="store_true",
                    help="re-encode even when pixels are unchanged")
    ap.add_argument("--lzss", action="store_true",
                    help="write LZSS image blocks (default: raw blocks)")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    pack = Path(args.pack).resolve() if args.pack else None
    input_path = Path(args.input).resolve() if args.input else None
    if pack is not None:
        requested_pack = pack
        pack = resolve_pack(pack)
        if pack != requested_pack:
            print("using pack tree: %s" % pack)

    if args.reindex:
        if pack is None or not pack.is_dir():
            raise RepackError("--reindex needs --pack DIR")
        main_palette = find_palette(
            input_path, pack,
            Path(args.palette).resolve() if args.palette else None)
        changed, kept = reindex_pack(pack, main_palette, args.dither)
        print("reindexed %d PNG(s), %d already indexed: %s" %
              (changed, kept, pack))
        if input_path is None:
            return 0

    if input_path is None:
        raise RepackError("input is required unless --reindex is used with --pack")
    if not input_path.is_file():
        raise RepackError("input not found: %s" % input_path)
    if args.highres:
        if pack is None or not pack.is_dir():
            raise RepackError("--highres needs --pack DIR")
        if args.exact:
            raise RepackError("--highres and --exact are different overlay modes; "
                              "choose one")
        return build_highres_overlay(args, input_path, pack)
    trailer_path = Path(args.trailer).resolve() if args.trailer else None
    ov = load_source(input_path, trailer_path)
    records, gaps, main_count = parse_records(ov)
    repl = build_replacements(records, gaps, args, input_path, pack)

    palette = PaletteMapper(find_palette(
        input_path, pack,
        Path(args.palette).resolve() if args.palette else None))

    infos = {}
    base_edits = []
    replaced = 0
    for rec, src in sorted(repl.items(), key=lambda kv: kv[0].tag_offset):
        img = record_image(rec)
        if img is None:
            if args.verbose:
                print("skip non-image record %d state %02x" %
                      (rec.index, rec.state), file=sys.stderr)
            continue
        ow, oh, opix, trailing = img
        from PIL import Image
        im = Image.open(src).convert("RGB")
        if args.exact:
            tw, th = im.size
        else:
            tw, th = ow, oh
        if tw == 0 or th == 0 or tw > 4096 or th > 4096:
            raise RepackError("bad replacement size %dx%d for %s" %
                              (tw, th, src))
        pixels = palette.image_indices(src, (tw, th))
        if not args.force and tw == ow and th == oh and pixels == opix:
            if args.verbose:
                print("unchanged: %s" % src, file=sys.stderr)
            continue
        new_data = struct.pack("<HH", tw, th) + pixels + trailing
        # Raw blocks avoid LZSS scratch-space pressure in the fixed sprite
        # heap.  --lzss keeps the original class for experiments.
        use_lzss = args.lzss
        block = encode_block(new_data, use_lzss)
        start = rec.tag_offset
        end = rec.end - 1          # next tag belongs to the following stream byte
        if end <= start:
            raise RepackError("bad record range at 0x%x" % start)
        old_len = end - start
        edit = {
            "start": start,
            "end": end,
            "bytes": block,
            "delta": len(block) - old_len,
            "what": "record %d" % rec.index,
        }
        base_edits.append(edit)
        infos[rec] = {
            "block": block,
            "data": new_data,
            "use_lzss": use_lzss,
            "comp_len": block_comp_len(block, use_lzss),
            "header_len": len(block) - len(new_data),
            "tw": tw,
            "th": th,
            "old_w": ow,
            "old_h": oh,
        }
        replaced += 1
        if args.verbose:
            print("replace rec %d %s %dx%d -> %dx%d, %d -> %d bytes" %
                  (rec.index, src.name, ow, oh, tw, th, old_len, len(block)),
                  file=sys.stderr)

    if not base_edits:
        raise RepackError("nothing changed")

    # Inline "Disk" records in 0x06 opcode blocks hold absolute stream offsets
    # to embedded images.  Re-encoding a preceding image moves those targets,
    # so patch every descriptor and re-encode its opcode block.  Opcode blocks
    # may themselves move later images, hence the small fixed-point loop.
    old_desc_target = {}
    for rec in records:
        img = record_image(rec)
        if img is None:
            continue
        old_desc_target[rec.payload_offset + (0 if rec.lzss else 4)] = rec

    desc_records = [r for r in records
                    if r.state == 0x06 and DISK_TAG in r.data]
    desc_edits = {}
    for iteration in range(12):
        all_edits = base_edits + list(desc_edits.values())

        def shifted(pos: int) -> int:
            return pos + sum(e["delta"] for e in all_edits if e["start"] < pos)

        def descriptor_fields(rec):
            info = infos.get(rec)
            if info is not None:
                new_header = info["header_len"]
                new_payload = shifted(rec.tag_offset) + new_header
                use_lzss = info["use_lzss"]
                return (info["tw"], info["th"],
                        new_payload + (0 if use_lzss else 4),
                        info["comp_len"] if use_lzss else 0)
            img = record_image(rec)
            if img is None:
                return None
            new_payload = shifted(rec.tag_offset) + (rec.payload_offset - rec.tag_offset)
            return (img[0], img[1],
                    new_payload + (0 if rec.lzss else 4),
                    rec.comp_len if rec.lzss else 0)

        new_desc_edits = {}
        for rec in desc_records:
            data = bytearray(rec.data)
            changed = False
            for pos, old_off, old_stored in descriptor_sites(bytes(data)):
                target = old_desc_target.get(old_off)
                if target is None:
                    continue
                fields = descriptor_fields(target)
                if fields is None:
                    continue
                w, h, new_off, new_stored = fields
                if (struct.unpack_from("<HH", data, pos - 4) != (w, h) or
                        struct.unpack_from("<I", data, pos + 4) != new_off or
                        struct.unpack_from("<I", data, pos + 8) != new_stored):
                    struct.pack_into("<HH", data, pos - 4, w, h)
                    struct.pack_into("<I", data, pos + 4, new_off)
                    struct.pack_into("<I", data, pos + 8, new_stored)
                    changed = True
            if changed:
                block = encode_block(bytes(data), False)
                end = rec.end - 1
                edit = {
                    "start": rec.tag_offset,
                    "end": end,
                    "bytes": block,
                    "delta": len(block) - (end - rec.tag_offset),
                    "what": "descriptor block %d" % rec.index,
                }
                new_desc_edits[rec] = edit
            if args.verbose and changed:
                print("relocate descriptors in rec %d (0x%x), %d -> %d bytes" %
                      (rec.index, rec.tag_offset,
                       rec.end - 1 - rec.tag_offset, len(block)),
                      file=sys.stderr)

        old_sig = {r: (e["bytes"], e["start"], e["end"])
                   for r, e in desc_edits.items()}
        new_sig = {r: (e["bytes"], e["start"], e["end"])
                   for r, e in new_desc_edits.items()}
        desc_edits = new_desc_edits
        if old_sig == new_sig:
            break
    else:
        raise RepackError("descriptor relocation did not converge")

    edits = base_edits + list(desc_edits.values())

    # If a replaced record is inside an opcode/delta skip, update its byte count.
    for gs, ge, why in gaps:
        inside = [e for e in edits
                  if e["start"] >= gs and e["end"] <= ge and e["delta"]]
        if not inside:
            continue
        off, width = gap_control(ov.data, gs, ge, why)
        old = int.from_bytes(ov.data[off:off + width], "little")
        new = old + sum(e["delta"] for e in inside)
        if new < 0 or new >= (1 << (width * 8)):
            raise RepackError("gap at 0x%x overflowed its %d-byte count" %
                              (gs, width))
        edits.append({
            "start": off,
            "end": off + width,
            "bytes": new.to_bytes(width, "little"),
            "delta": 0,
            "what": "gap length at 0x%x" % gs,
        })

    edits.sort(key=lambda e: (e["start"], e["end"]))
    for a, b in zip(edits, edits[1:]):
        if a["end"] > b["start"]:
            raise RepackError("overlapping edits: %s and %s" %
                              (a["what"], b["what"]))

    entry_abs = ov.dbase + ov.entries[2]
    for e in edits:
        if e["start"] < entry_abs < e["end"]:
            raise RepackError("edit crosses index entry 2 at 0x%x" % entry_abs)
    new_entry2 = ov.entries[2] + sum(
        e["delta"] for e in edits if e["end"] <= entry_abs)

    buf = bytearray(ov.data)
    for e in sorted(edits, key=lambda x: x["start"], reverse=True):
        buf[e["start"]:e["end"]] = e["bytes"]

    new_raw_end = len(buf) - TRAILER_SIZE
    new_overlay = bytes(buf[ov.dbase:new_raw_end])
    if new_entry2 < 0 or new_entry2 >= len(new_overlay):
        raise RepackError("rebuilt entry 2 is outside the overlay")

    tail = bytearray(ov.data[-TRAILER_SIZE:])
    for i in range(ov.extent + 1):
        off = TRAILER_SIZE - ((ov.extent - i) * 5 + 23 + 5)
        value = ov.entries[i]
        if i == 2:
            value = new_entry2
        put_u32(tail, off, value)
    put_u32(tail, 19, ov.dbase)       # dbase stays at the original value

    out = Path(args.out)
    suffix = out.suffix.lower()
    if suffix in (".nw", ".bin") and not (out.exists() and out.is_dir()):
        # Treat -o as a file: keep the companion files next to it, named
        # after the same stem (repacked.nw -> repacked.bin/.trailer/.tsv).
        output_dir = out.parent if str(out.parent) else Path(".")
        stem = out.stem
        nw_name = out.name if suffix == ".nw" else stem + ".nw"
        bin_name = out.name if suffix == ".bin" else stem + ".bin"
        trailer_name = stem + ".trailer"
        report_name = stem + ".tsv"
    else:
        output_dir = out
        nw_name, bin_name = "overlay.nw", "overlay.bin"
        trailer_name, report_name = "overlay.trailer", "repack.tsv"

    if output_dir.exists() and not output_dir.is_dir():
        raise RepackError("output path exists and is not a directory: %s" %
                          output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / bin_name).write_bytes(new_overlay)
    (output_dir / trailer_name).write_bytes(tail)
    (output_dir / nw_name).write_bytes(
        b"\0" * ov.dbase + new_overlay + bytes(tail))

    report = output_dir / report_name
    with report.open("w", encoding="utf-8") as f:
        f.write("# what\told_start\told_end\tnew_bytes\tdelta\n")
        for e in sorted(edits, key=lambda x: x["start"]):
            f.write("%s\t0x%x\t0x%x\t%d\t%d\n" %
                    (e["what"], e["start"], e["end"],
                     len(e["bytes"]), e["delta"]))

    # Reparse the rebuilt stream.  This catches a bad index/trailer immediately.
    check = Overlay(str(output_dir / nw_name))
    check_records, check_gaps = walk_stream(check)
    if len(check_records) != main_count:
        raise RepackError("rebuilt main record count changed: %d -> %d" %
                          (main_count, len(check_records)))

    print("replaced records: %d" % replaced)
    print("entry 2: 0x%x -> 0x%x" % (ov.entries[2], new_entry2))
    print("overlay: %d -> %d bytes" % (len(ov.overlay), len(new_overlay)))
    print("output: %s (%s is the external-loadable stream)" %
          (output_dir, nw_name))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RepackError as exc:
        print("nwrepack: %s" % exc, file=sys.stderr)
        raise SystemExit(1)
