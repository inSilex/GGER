#!/usr/bin/env python3
"""nwextract.py -- extract the appended NETWALK data stream.

By default this writes only the overlay files needed by a recompiled build:

  * overlay.bin       exact appended data stream (raw, no trailer)
  * overlay.nw        external-loadable stream: dbase padding + overlay + trailer
  * overlay.trailer   original 38-byte trailer

Pass --assets to also decode the stream into an asset tree for upscaling:
every tagged record (data/, manifest.tsv), images (images/, .idx + .png),
the 640x480 background and palette (background/), waveOut sound effects
(audio/*.wav), MIDI tracks (audio/*.midtrack and .mid), palettes, metadata,
and startup/animation frame previews.

The parser is intentionally data-driven.  Record framing comes from the
stream's own tag bytes; the only game-specific constants are the standard
length-bias tables used by this engine's tagged-record decoder.

Usage:
    python3 tools/nwextract.py NETWALK.EXE -o extracted/NETWALK
    python3 tools/nwextract.py NETWALK.EXE -o out --assets
    python3 tools/nwextract.py NETWALK.EXE -o out --assets --no-png
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import struct
import sys
import wave
import zlib
from collections import Counter, OrderedDict

# ---------------------------------------------------------------------------
# Engine constants
# ---------------------------------------------------------------------------

# The stream's tagged-length decoder uses two 4-entry bias tables at
# nw_data_window+0x1f8 and +0x20c.  They are part of the engine image, not the
# overlay; these are the values from the 32-bit data window used by NETWALK.
BIAS_SUB = [
    0xFFFFFC00, 0xFFFFFFFF, 0xFFFBFC00, 0xFFFFFBFF,
    0xFBFBFC00, 0xFFFBFBFF, 0x00000080, 0x0000047F,
]
BIAS_ADD = [
    0xFBFBFC00, 0xFFFBFBFF, 0x00000080, 0x0000047F,
    0x00000480, 0x0004047F, 0x00040480, 0x0404047F,
]

# The engine's MIDI delta unit is 10 ms.  A conventional MIDI file with 100
# ticks/quarter and a 1,000,000 us quarter gives exactly 10 ms per tick.
MIDI_DIVISION = 100
MIDI_TEMPO_US = 1_000_000
SFX_RATE = 11025


# ---------------------------------------------------------------------------
# Stream reader and block framing
# ---------------------------------------------------------------------------

class StreamError(Exception):
    pass


class Stream:
    def __init__(self, data: bytes, pos: int):
        self.data = data
        self.pos = pos
        self.tag = 0

    def read(self, n: int) -> bytes:
        if n < 0 or self.pos < 0 or self.pos + n > len(self.data):
            raise StreamError("read %d byte(s) at 0x%x past end (0x%x)"
                              % (n, self.pos, len(self.data)))
        b = self.data[self.pos:self.pos + n]
        self.pos += n
        return b

    def fetch(self) -> int:
        self.tag = self.read(1)[0]
        return self.tag

    def seek(self, pos: int) -> None:
        if pos < 0 or pos > len(self.data):
            raise StreamError("seek 0x%x past end (0x%x)" % (pos, len(self.data)))
        self.pos = pos


def le(b: bytes) -> int:
    return int.from_bytes(b, "little")


def decode_length(st: Stream) -> int:
    """Port of DecodeTaggedRecordLengthToOutParam."""
    st.fetch()
    tag = st.tag
    if not (tag & 0x80):
        value = tag
    elif not (tag & 0x20):
        width = tag & 0x1F
        value = le(st.read(width))
    else:
        width = ((tag >> 2) & 3) + 1
        value = le(st.read(width))
        value += (tag & 3) << ((width * 8) & 0x1F)
        if tag & 0x10:
            value = (BIAS_SUB[width * 2] - value) & 0xFFFFFFFF
        else:
            value = (value + BIAS_ADD[width * 2]) & 0xFFFFFFFF
    st.fetch()
    return value


def lzss_decompress(src: bytes, max_out: int = 64 * 1024 * 1024) -> bytes:
    """Port of DecompressLzssBlockToBuffer (12-bit distance, 4-bit count)."""
    out = bytearray()
    i = 0
    while i < len(src) and len(out) < max_out:
        flags = src[i]
        i += 1
        for _ in range(8):
            if i >= len(src) or len(out) >= max_out:
                break
            if flags & 1:
                out.append(src[i])
                i += 1
            else:
                if i + 2 > len(src):
                    break
                br = src[i] | (src[i + 1] << 8)
                i += 2
                dist = ((br >> 12) << 8) | (br & 0xFF)
                count = ((br >> 8) & 0x0F) + 2
                if dist == 0 or dist > len(out):
                    raise StreamError("invalid LZSS distance %d at output %d"
                                      % (dist, len(out)))
                # The engine's do/while runs count+1 times.
                for _ in range(count + 1):
                    out.append(out[-dist])
            flags >>= 1
    return bytes(out)


class Record:
    __slots__ = ("index", "state", "tag", "tag_offset", "payload_offset",
                 "header_len", "comp_len", "uncomp_len", "data", "end",
                 "lzss", "kind", "file", "pixel_crc", "image_offset",
                 "image_stored", "asset_offset", "source")

    def __init__(self, **kw):
        for k in self.__slots__:
            setattr(self, k, kw.get(k))


def read_block(st: Stream, state: int, why: str = "") -> Record:
    start = st.pos
    st.fetch()
    tag = st.tag
    if not (tag & 0x80):
        raise StreamError("block tag 0x%02x at 0x%x lacks the high bit"
                          % (tag, start))

    if tag & 0x40:
        comp = tag & 0x1F
        header_len = 1
        data = st.read(comp)
        uncomp = comp
        is_lzss = False
    else:
        width = (tag & 3) + 1
        comp = le(st.read(width))
        header_len = 1 + width
        if (tag & 0x0C) == 0:
            data = st.read(comp)
            uncomp = comp
            is_lzss = False
        else:
            uncomp = le(st.read(width))
            header_len += width
            compressed = st.read(comp)
            data = lzss_decompress(compressed, uncomp + 4096)
            is_lzss = True

    st.fetch()
    rec = Record(index=0, state=state, tag=tag, tag_offset=start,
                 payload_offset=start + header_len, header_len=header_len,
                 comp_len=comp, uncomp_len=uncomp, data=data, end=st.pos,
                 lzss=is_lzss, kind="data", file=None, pixel_crc=None,
                 image_offset=None, image_stored=None, asset_offset=None,
                 source="main")
    return rec


def read_delta(st: Stream, gaps: list | None = None) -> int:
    st.fetch()
    tag = st.tag
    if (tag & 0xEC) != 0xEC:
        raise StreamError("delta tag 0x%02x at 0x%x" % (tag, st.pos - 1))
    n = (tag & 3) + 1
    delta = le(st.read(n))
    gap_start = st.pos
    st.pos += delta
    if gaps is not None:
        gaps.append((gap_start, st.pos, "delta"))
    st.fetch()
    return delta


# ---------------------------------------------------------------------------
# Overlay trailer
# ---------------------------------------------------------------------------

class Overlay:
    def __init__(self, path: str, data: bytes | None = None):
        self.path = path
        self.data = open(path, "rb").read() if data is None else data
        self.size = len(self.data)
        if self.size < 38 or self.data[-4:] != b"=VS=":
            raise SystemExit("%s: no '=VS=' data trailer at end of file" % path)
        self.payload_len, self.spare, self.version = struct.unpack_from(
            "<III", self.data, self.size - 12)
        payload = self.data[self.size - 23:self.size - 12]
        self.extent, self.dbase = struct.unpack_from("<II", payload, 0)
        if self.dbase <= 0 or self.dbase >= self.size - 38:
            raise SystemExit("%s: implausible dbase 0x%x" % (path, self.dbase))
        self.entries = {}
        for i in range(self.extent + 1):
            off = self.size - ((self.extent - i) * 5 + 23 + 5)
            eo = struct.unpack_from("<I", self.data, off)[0]
            tag = self.data[off + 4]
            if tag != i:
                raise SystemExit("%s: entry %d has tag %d" % (path, i, tag))
            self.entries[i] = eo
        self.overlay_start = self.dbase
        self.overlay_end = self.size - 38
        self.overlay = self.data[self.overlay_start:self.overlay_end]


# ---------------------------------------------------------------------------
# Main stream walk
# ---------------------------------------------------------------------------

def walk_stream(ov: Overlay, arg: int = 0, verbose: bool = False):
    records = []
    gaps = []
    st = Stream(ov.data, ov.dbase)

    def check_entry(index: int) -> None:
        if index not in ov.entries:
            raise StreamError("entry %d is outside the index" % index)
        st.seek(ov.dbase + ov.entries[index])
        st.fetch()

    # The engine starts at indexed record 1.
    check_entry(1)
    edi = 1
    entry_index = arg + 2
    state = 0
    local10 = local14 = local18 = local1c = 0
    steps = 0

    def add(state_: int, why: str) -> Record:
        r = read_block(st, state_, why)
        r.index = len(records)
        records.append(r)
        return r

    while True:
        steps += 1
        if steps > 1_000_000:
            raise StreamError("stream walk exceeded one million steps")
        tag = st.tag

        if tag == 0x00:
            if edi == 1:
                edi = entry_index
                check_entry(edi)
                continue
            break

        if tag == 0x01:
            local14 = decode_length(st)
            continue
        if tag == 0x02:
            local18 = decode_length(st)
            continue
        if tag == 0x03:
            local1c = decode_length(st)
            continue
        if tag == 0x05:
            read_delta(st, gaps)
            continue
        if tag == 0x06:
            # InterpretOpcodeStreamUntilEnd: 4-byte skip, tag 7, then one block.
            # The skipped payload is opaque to the main walker but may contain
            # sprite images referenced by the opcode stream.  Remember it.
            dist = le(st.read(4))
            gap_start = st.pos
            gap_end = gap_start + dist
            gaps.append((gap_start, gap_end, "opcode"))
            st.seek(gap_end)
            st.fetch()
            if st.tag != 7:
                raise StreamError("expected tag 7 after opcode skip, got 0x%02x at 0x%x"
                                  % (st.tag, st.pos - 1))
            add(0x06, "opcode-stream")
            continue
        if tag == 0xFF:
            st.fetch()
            continue

        if tag == 0x04:
            if state == 0x0F:
                add(state, "case04/0f")
                continue
            if state == 0x10:
                add(state, "case04/10")
                continue
            if state == 0x11:
                # InitializeGridArenaFromMapHeader reads its own header block.
                add(state, "case04/11")
                continue
            # 0x12, 0x13, 0x18, ... fall through to run_opcode.
            add(state, "run")
            continue

        if tag & 0x80:
            state = tag & 0x7F
            st.fetch()
            continue

        local10 = decode_length(st)

    return records, gaps


# ---------------------------------------------------------------------------
# Embedded records in skipped opcode/delta payloads
# ---------------------------------------------------------------------------

def scan_embedded_images(data: bytes, gaps, records, verbose: bool = False):
    """Find image blocks stored inside payloads the main walker skips.

    Some opcode streams carry a small descriptor followed by a complete LZSS
    image block.  The main loop jumps over the whole payload, so those images
    would otherwise be missed.  Only exact, self-consistent image blocks are
    accepted; arbitrary bytes in a compressed payload do not pass the geometry
    and decompressed-length checks.
    """
    known = {r.tag_offset for r in records}
    extra = []
    for gap_start, gap_end, why in gaps:
        if gap_start < 0 or gap_end > len(data) or gap_end <= gap_start:
            continue
        for off in range(gap_start, gap_end):
            if not (data[off] & 0x80):
                continue
            try:
                st = Stream(data, off)
                rec = read_block(st, 0x43)
            except Exception:
                continue
            if rec.end - 1 > gap_end or rec.tag_offset in known:
                continue
            if not rec.lzss:
                continue
            img = image_from_block(rec.data)
            if img is None:
                continue
            w, h, pix = img
            if rec.uncomp_len != 4 + w * h or len(rec.data) != 4 + w * h:
                continue
            # Tiny 1x1 placeholders exist in the main stream, but an embedded
            # opcode payload is expected to carry a real sprite frame.
            if w * h < 16:
                continue
            rec.index = len(records) + len(extra)
            rec.source = "embedded"
            rec.kind = "image"
            rec.pixel_crc = crc32(pix)
            rec.asset_offset = rec.payload_offset
            rec.image_offset = rec.payload_offset
            rec.image_stored = rec.comp_len
            extra.append(rec)
            if verbose:
                print("embedded image at 0x%x (%dx%d, %d stored bytes, gap %s)"
                      % (off, w, h, rec.comp_len, why), file=sys.stderr)
    return extra


# ---------------------------------------------------------------------------
# Frame-cell animation blocks
# ---------------------------------------------------------------------------

def scan_animation_palette(data: bytes, gaps):
    """Return (palette, offset) for the 768-byte palette used by the frames.

    It is a type-2 LZSS block immediately before the first frame chain.
    """
    for gap_start, gap_end, why in gaps:
        if why != "delta":
            continue
        for p in range(gap_start, gap_end - 12):
            typ, stored, raw = struct.unpack_from("<III", data, p)
            if typ == 2 and raw == 768 and 0 < stored <= gap_end - p - 12:
                try:
                    out = lzss_decompress(data[p + 12:p + 12 + stored], raw + 64)[:raw]
                except Exception:
                    continue
                if len(out) == 768:
                    return out, p
    return None, None


def _grid_add_run(buf, dst, stream, n):
    for _ in range(n):
        buf[dst] = (buf[dst] + stream[0]) & 0xff
        dst += 1
        stream = stream[1:]
    return dst, stream


def _grid_fill_run(buf, dst, stream, n):
    value = stream[0]
    stream = stream[1:]
    for _ in range(n):
        buf[dst] = (buf[dst] + value) & 0xff
        dst += 1
    return dst, stream


def _apply_grid_delta(buf, stream, dst, rows, cols, decoder, rewind):
    p = 0
    d = dst
    if decoder == 0:
        for _ in range(rows):
            for _ in range(cols):
                buf[d] = (buf[d] + stream[p]) & 0xff
                p += 1
                d += 1
            d -= rewind
        return p
    if decoder == 1:
        for _ in range(rows):
            budget = cols
            while True:
                lead = stream[p]
                p += 1
                if lead & 0x80:
                    code = lead & 0x3f
                    if not lead & 0x40:
                        n = code + 1
                        budget -= n
                        d, _ = _grid_add_run(buf, d, stream[p:p + n], n)
                        p += n
                        if budget:
                            continue
                        break
                    if code == 0:
                        d += budget
                        break
                    if code == 1:
                        d, _ = _grid_add_run(buf, d, stream[p:p + budget], budget)
                        p += budget
                        break
                    n = code + 1
                    budget -= n
                    d, _ = _grid_fill_run(buf, d, stream[p:p + 1], n)
                    p += 1
                    if budget:
                        continue
                    break
                n = lead + 1
                d += n
                budget -= n
                if budget:
                    continue
                break
            d -= rewind
        return p
    if decoder == 2:
        for _ in range(rows):
            budget = cols
            while True:
                lead = stream[p]
                p += 1
                if lead & 0x80:
                    n = (lead & 0x3f) + 1
                    if not lead & 0x40:
                        d += n
                        budget -= n
                        if budget:
                            continue
                        break
                    budget -= n
                    d, _ = _grid_add_run(buf, d, stream[p:p + n], n)
                    p += n
                    if budget:
                        continue
                    break
                if lead == 0:
                    d += budget
                    break
                if lead == 1:
                    d, _ = _grid_add_run(buf, d, stream[p:p + budget], budget)
                    p += budget
                    break
                n = lead + 1
                budget -= n
                d, _ = _grid_fill_run(buf, d, stream[p:p + 1], n)
                p += 1
                if budget:
                    continue
                break
            d -= rewind
        return p
    for _ in range(rows):
        budget = cols
        while True:
            lead = stream[p]
            p += 1
            if lead & 0x80:
                code = lead & 0x3f
                if not lead & 0x40:
                    n = code + 1
                    d += n
                    budget -= n
                    if budget:
                        continue
                    break
                if code == 0:
                    d += budget
                    break
                if code == 1:
                    d, _ = _grid_add_run(buf, d, stream[p:p + budget], budget)
                    p += budget
                    break
                n = code + 1
                budget -= n
                d, _ = _grid_fill_run(buf, d, stream[p:p + 1], n)
                p += 1
                if budget:
                    continue
                break
            n = lead + 1
            budget -= n
            d, _ = _grid_add_run(buf, d, stream[p:p + n], n)
            p += n
            if budget:
                continue
            break
        d -= rewind
    return p


def render_frame_block(payload: bytes, width: int = 640, height: int = 480,
                       fill: int = 0):
    """Render one frame-cell command stream to a bottom-up 8-bit buffer."""
    buf = bytearray([fill]) * (width * height)
    p = 0
    while p < len(payload):
        flags = payload[p]
        p += 1
        x = y = w = h = 0
        if not flags & 0x40:
            x = payload[p]; p += 1
            if flags & 0x04:
                x |= payload[p] << 8; p += 1
            y = payload[p]; p += 1
            if flags & 0x08:
                y |= payload[p] << 8; p += 1
            w = payload[p]; p += 1
            if flags & 0x10:
                w |= payload[p] << 8; p += 1
            h = payload[p]; p += 1
            if flags & 0x20:
                h |= payload[p] << 8; p += 1
        else:
            w, h = width, height
        if x < 0 or y < 0 or x + w > width or y + h > height:
            break
        dst = (height - 1 - y) * width + x
        used = _apply_grid_delta(buf, payload[p:], dst, h, w,
                                 flags & 3, width + w)
        p += used
        if flags & 0x80:
            break
    return buf


def scan_frame_groups(data: bytes, gaps):
    """Find the 12-byte frame-block chains in delta payloads.

    The startup/menu animation is not AVI/MPEG.  It is a sequence of
    ``[uint32 type=1][uint32 stored][uint32 raw]`` blocks followed by a
    command stream for ComposeFrameCellsFromStreamRecords.  The first block is
    LZSS-coded when stored != raw; the rest in this build are raw.  Return one
    group per contiguous chain.
    """
    groups = []
    for gap_start, gap_end, why in gaps:
        if why != "delta":
            continue
        pos = gap_start
        while pos + 12 <= gap_end:
            if data[pos:pos + 4] != b"\x01\x00\x00\x00":
                pos += 1
                continue
            stored, raw = struct.unpack_from("<II", data, pos + 4)
            if stored == 0 or stored > gap_end - pos - 12 or raw == 0:
                pos += 1
                continue
            blocks = []
            p = pos
            while p + 12 <= gap_end and data[p:p + 4] == b"\x01\x00\x00\x00":
                c, u = struct.unpack_from("<II", data, p + 4)
                if c == 0 or p + 12 + c > gap_end:
                    break
                payload = data[p + 12:p + 12 + c]
                decoded = payload
                compressed = c != u
                if compressed:
                    try:
                        decoded = lzss_decompress(payload, u + 4096)[:u]
                    except Exception:
                        compressed = False
                        decoded = payload
                blocks.append({
                    "offset": p,
                    "stored": c,
                    "raw": u,
                    "compressed": compressed,
                    "payload": decoded,
                })
                p += 12 + c
            if len(blocks) >= 5:
                groups.append({"start": pos, "end": p, "blocks": blocks})
                pos = p
            else:
                pos += 1
    return groups


# ---------------------------------------------------------------------------
# Classification / assets
# ---------------------------------------------------------------------------

def crc32(b: bytes) -> int:
    return zlib.crc32(b) & 0xFFFFFFFF


def image_from_block(data: bytes):
    """Return (w, h, pixels) for the engine's 4-byte w/h image header."""
    if len(data) < 5:
        return None
    w, h = struct.unpack_from("<HH", data, 0)
    if w == 0 or h == 0 or w > 4096 or h > 4096:
        return None
    need = w * h
    if len(data) < 4 + need:
        return None
    return w, h, data[4:4 + need]


def classify(rec: Record, bg_palette: bytes | None):
    data = rec.data
    if rec.state == 0x43:
        img = image_from_block(data)
        if img is not None:
            rec.kind = "image"
            rec.pixel_crc = crc32(img[2])
            # Stream offset of the pixel payload, matching LoadDiskSpriteImageFromStream.
            last = rec.uncomp_len if rec.lzss else rec.comp_len
            ebx = (last - 4) & 0xFFFFFFFF
            if rec.lzss:
                ebx = rec.comp_len
            rec.image_offset = (rec.end - ebx - 1) & 0xFFFFFFFF
            rec.image_stored = rec.comp_len if rec.lzss else 0
            rec.asset_offset = rec.image_offset
            return
    if rec.state == 0x14:
        img = image_from_block(data)
        if img is not None and len(data) == 4 + img[0] * img[1]:
            rec.kind = "image"
            rec.pixel_crc = crc32(img[2])
            rec.asset_offset = rec.payload_offset + 4
            return
    if rec.state == 0x18 and len(data) >= 24:
        w, h = struct.unpack_from("<II", data, 8)
        if 0 < w <= 4096 and 0 < h <= 4096 and len(data) >= 24 + w * h:
            rec.kind = "background"
            rec.pixel_crc = crc32(data[24:24 + w * h])
            rec.asset_offset = rec.payload_offset + 24
            return
    if rec.state == 0x51 and len(data) >= 4:
        n = struct.unpack_from("<I", data, 0)[0]
        if n == len(data) - 4 and n > 0:
            rec.kind = "sfx"
            rec.asset_offset = rec.payload_offset + 4
            return
    if rec.state == 0x52 and len(data) >= 4:
        n = struct.unpack_from("<I", data, 0)[0]
        if n == len(data) - 4:
            rec.kind = "midi"
            rec.asset_offset = rec.payload_offset + 4
            return
    if len(data) == 768 and rec.state == 0x42:
        rec.kind = "palette"
        return
    if rec.state == 0x0F:
        rec.kind = "metadata"
    elif rec.state == 0x10:
        rec.kind = "metadata"
    elif rec.state == 0x11:
        rec.kind = "metadata"
    elif rec.state == 0x19:
        rec.kind = "metadata"
    elif rec.state == 0x2B:
        rec.kind = "grid"
    elif rec.state == 0x14:
        rec.kind = "grid"
    elif rec.state == 0x13:
        rec.kind = "script"
    elif rec.state == 0x39:
        rec.kind = "table"
    elif rec.state == 0x61:
        rec.kind = "config"
    elif rec.state == 0x06:
        rec.kind = "script"
    elif rec.state == 0x43:
        rec.kind = "data"


def write_wav(path: str, payload: bytes, rate: int = SFX_RATE) -> None:
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(1)
        w.setframerate(rate)
        w.writeframes(payload)


def midi_vlv(data: bytes, i: int):
    val = 0
    while i < len(data):
        b = data[i]
        i += 1
        val = (val << 7) | (b & 0x7F)
        if not (b & 0x80):
            break
    return val, i


def midi_track_to_file(payload: bytes, out_path: str) -> bool:
    """Wrap the game's running-status MIDI track in a standard SMF.

    The engine treats a 0xFF status byte as its end marker.  The surrounding
    bytes are ordinary MIDI channel messages with delta-time VLV fields, so a
    format-0 SMF is enough to make the track playable and inspectable.
    """
    end = len(payload)
    i = 0
    running = 0
    try:
        while i < len(payload):
            status = payload[i]
            if status > 0x7F:
                i += 1
            else:
                status = running
            running = status
            if status in (0xF0, 0xF7):
                ln, i = midi_vlv(payload, i)
                i += ln
                continue
            if status == 0xFF:
                end = i
                break
            high = status & 0xF0
            if high in (0x80, 0x90, 0xA0, 0xB0, 0xE0):
                i += 2
            elif high in (0xC0, 0xD0):
                i += 1
            else:
                return False
            _, i = midi_vlv(payload, i)
    except Exception:
        return False

    # Keep the original event/delta bytes exactly, add one explicit tempo
    # (10 ms per tick) and replace the engine's 0xFF terminator with a real
    # MIDI end-of-track event.  The delta immediately before 0xFF belongs to
    # that end event and is already present in payload[:end].
    track = bytearray()
    track += b"\x00\xFF\x51\x03" + MIDI_TEMPO_US.to_bytes(3, "big")
    track += payload[:end]
    track += b"\xFF\x2F\x00"
    header = b"MThd" + struct.pack(">IHHH", 6, 0, 1, MIDI_DIVISION)
    with open(out_path, "wb") as f:
        f.write(header)
        f.write(b"MTrk" + struct.pack(">I", len(track)) + bytes(track))
    return True


def render_png(idx_path: str, png_path: str, w: int, h: int,
               palette: bytes | None) -> bool:
    try:
        from PIL import Image
    except Exception:
        return False
    pix = open(idx_path, "rb").read()
    if len(pix) != w * h:
        return False
    im = Image.frombytes("P", (w, h), pix)
    if palette is not None and len(palette) >= 768:
        im.putpalette(palette[:768])
    else:
        im.putpalette(bytes(range(256)) * 3)
    im.save(png_path)
    return True


def palette_image(palette: bytes):
    """Return a 1x1 P-mode image carrying the 768-byte palette."""
    from PIL import Image
    im = Image.new("P", (1, 1))
    im.putpalette(palette[:768])
    return im


def palette_matches(im, palette: bytes) -> bool:
    """True if *im* is P-mode and already uses exactly this palette."""
    if im.mode != "P":
        return False
    current = im.getpalette()
    return bool(current) and bytes(current[:768]) == palette[:768]


def flattened(im):
    """Pixel data accessor that works before and after Pillow 12."""
    getter = getattr(im, "get_flattened_data", None)
    return getter() if getter is not None else im.getdata()


def to_indexed(im, palette: bytes, dither: bool = False):
    """Map an RGB (or foreign-palette) image onto *palette* and return mode P.

    Colors are mapped to their nearest palette entry in Pillow's C code, which
    keeps this fast enough for full-size 4x upscales.  P-mode input that already
    uses this palette is passed through untouched so indices stay exact.
    """
    from PIL import Image
    if palette_matches(im, palette):
        out = im.copy()
    else:
        mode = Image.Dither.FLOYDSTEINBERG if dither else Image.Dither.NONE
        out = im.convert("RGB").quantize(
            palette=palette_image(palette), dither=mode)
    out.putpalette(palette[:768])
    return out


def extract(ov: Overlay, out_dir: str, no_png: bool = False,
            arg: int = 0, verbose: bool = False,
            frame_scale: float = 1.0, assets: bool = False) -> dict:
    if os.path.isdir(out_dir):
        shutil.rmtree(out_dir)
    os.makedirs(out_dir, exist_ok=True)

    # The exact appended stream.
    open(os.path.join(out_dir, "overlay.bin"), "wb").write(ov.overlay)

    # An external-loadable stream that mirrors the original module layout:
    # dbase zero padding, then the overlay, then the original 38-byte trailer.
    # Keeping dbase at 0x12800 matters: inline "Disk" image records carry file
    # offsets that the engine only rebases when dbase is larger than 0x12800.
    external_tail = bytearray(ov.data[-38:])
    open(os.path.join(out_dir, "overlay.trailer"), "wb").write(external_tail)
    with open(os.path.join(out_dir, "overlay.nw"), "wb") as f:
        f.write(b"\0" * ov.dbase)
        f.write(ov.overlay)
        f.write(external_tail)

    if not assets:
        return {
            "input": os.path.abspath(ov.path),
            "file_size": ov.size,
            "overlay_start": ov.overlay_start,
            "overlay_size": len(ov.overlay),
            "dbase": ov.dbase,
            "assets": False,
        }

    for sub in ("images", "background", "audio", "palettes", "data", "metadata"):
        os.makedirs(os.path.join(out_dir, sub), exist_ok=True)

    records, gaps = walk_stream(ov, arg=arg, verbose=verbose)
    if verbose:
        print("walked %d record(s), %d skipped payload range(s), %d bytes"
              % (len(records), len(gaps), sum(e - s for s, e, _ in gaps)),
              file=sys.stderr)
    embedded = scan_embedded_images(ov.data, gaps, records, verbose=verbose)
    records.extend(embedded)
    if verbose and embedded:
        print("found %d embedded image(s)" % len(embedded), file=sys.stderr)

    # Find the background palette first; images use it.
    bg_palette = None
    for rec in records:
        if rec.state == 0x18 and len(rec.data) >= 24 + 768:
            w, h = struct.unpack_from("<II", rec.data, 8)
            if 0 < w <= 4096 and 0 < h <= 4096 and len(rec.data) >= 24 + w * h + 768:
                bg_palette = rec.data[-768:]
                break

    for rec in records:
        classify(rec, bg_palette)

    # Startup/menu animation is a sequence of frame-cell command blocks in the
    # large delta payload.  It is not a standard video container.
    frame_groups = scan_frame_groups(ov.data, gaps)
    anim_palette, anim_palette_offset = scan_animation_palette(ov.data, gaps)
    frame_rows = []
    frames_root = os.path.join(out_dir, "movies")
    os.makedirs(frames_root, exist_ok=True)
    if anim_palette is not None:
        open(os.path.join(frames_root, "palette.pal"), "wb").write(anim_palette)
    for gi, group in enumerate(frame_groups):
        group_dir = os.path.join(frames_root, "group%d" % gi)
        os.makedirs(group_dir, exist_ok=True)
        for bi, block in enumerate(group["blocks"]):
            rel = "movies/group%d/frame_%04d.bin" % (gi, bi)
            path = os.path.join(out_dir, rel)
            open(path, "wb").write(block["payload"])
            png_rel = "movies/group%d/frame_%04d.png" % (gi, bi)
            frame_w, frame_h = 640, 480
            top_down = None
            if len(block["payload"]) >= 1:
                frame = render_frame_block(block["payload"], frame_w, frame_h)
                top_down = b"".join(frame[y * frame_w:(y + 1) * frame_w]
                                   for y in range(frame_h - 1, -1, -1))
                if not no_png:
                    try:
                        from PIL import Image
                        im = Image.frombytes("P", (frame_w, frame_h), top_down)
                        if anim_palette is not None:
                            im.putpalette(anim_palette)
                        else:
                            im.putpalette(bytes(range(256)) * 3)
                        if frame_scale != 1.0:
                            nw = max(1, int(round(frame_w * frame_scale)))
                            nh = max(1, int(round(frame_h * frame_scale)))
                            im = im.resize((nw, nh), Image.Resampling.NEAREST)
                        im.save(os.path.join(out_dir, png_rel))
                    except Exception:
                        png_rel = None
            if png_rel is None:
                png_rel = "-"
            frame_rows.append((gi, bi, block["offset"] - ov.dbase,
                               block["stored"], block["raw"],
                               int(block["compressed"]),
                               crc32(block["payload"]), rel, png_rel,
                               frame_w, frame_h))
    with open(os.path.join(frames_root, "frames.tsv"), "w", encoding="utf-8") as f:
        f.write("# group\tindex\toverlay_offset\tstored\traw\tcompressed\tcrc32\tfile\tpng\twidth\theight\n")
        for row in frame_rows:
            f.write("\t".join(str(x) for x in row) + "\n")
    with open(os.path.join(frames_root, "README.txt"), "w", encoding="utf-8") as f:
        f.write("Embedded startup/menu animation frame blocks\n")
        f.write("===========================================\n\n")
        f.write("These are NOT AVI/MPEG files.  Each block is:\n\n")
        f.write("  uint32 type=1, uint32 stored_bytes, uint32 raw_bytes,\n")
        f.write("  followed by stored_bytes of LZSS or raw frame data.  The .bin\n")
        f.write("  files here are the decoded command streams consumed by\n")
        f.write("  ComposeFrameCellsFromStreamRecords (a list of cell rectangles).\n\n")
        f.write("The .idx/.png files are the rendered 640x480 frame images; the\n")
        f.write("palette.pal is the startup palette block.  frames.tsv records\n")
        f.write("the exact overlay offset, stored/raw lengths, LZSS flag, CRC, and\n")
        f.write("rendered image for every block.  The game screen itself remains\n")
        f.write("640x480; --frame-scale only changes the extracted preview size.\n")

    # Categorized, deduplicated outputs.
    seen = {}
    image_rows = []
    sfx_rows = []
    midi_rows = []
    raw_rows = []
    palette_rows = []
    bg_rows = []

    def uniq_path(sub: str, name: str, data: bytes, rec: Record,
                  key=None) -> str:
        if key is None:
            key = (sub, crc32(data), len(data))
        if key in seen:
            return seen[key]
        path = os.path.join(sub, name)
        open(os.path.join(out_dir, path), "wb").write(data)
        seen[key] = path
        return path

    for rec in records:
        if rec.kind == "image":
            w, h, pix = image_from_block(rec.data)
            crc = crc32(pix)
            name = "s%02x_%dx%d_%08x" % (rec.state, w, h, crc)
            idx = uniq_path("images", name + ".idx", pix, rec,
                            key=("image", w, h, crc))
            png = None
            if not no_png:
                png_rel = os.path.splitext(idx)[0] + ".png"
                png_path = os.path.join(out_dir, png_rel)
                if not os.path.exists(png_path):
                    if not render_png(os.path.join(out_dir, idx), png_path,
                                      w, h, bg_palette):
                        png_path = None
                if png_path is not None and os.path.exists(png_path):
                    png = png_rel
            rec.file = idx
            image_rows.append((rec.index, rec.state, rec.tag_offset,
                               rec.payload_offset, rec.comp_len if rec.lzss else 0,
                               rec.uncomp_len, w, h, crc, idx, png,
                               rec.source or "main"))
        elif rec.kind == "background":
            w, h = struct.unpack_from("<II", rec.data, 8)
            pix = rec.data[24:24 + w * h]
            pal = rec.data[-768:] if len(rec.data) >= 24 + w * h + 768 else bg_palette
            name = "background_%dx%d_%08x" % (w, h, crc32(pix))
            idx = uniq_path("background", name + ".idx", pix, rec)
            palpath = None
            if pal is not None:
                palpath = os.path.join(out_dir, "background", name + ".pal")
                open(palpath, "wb").write(pal)
            png = None
            if not no_png:
                png_path = os.path.join(out_dir, "background", name + ".png")
                if render_png(os.path.join(out_dir, idx), png_path, w, h, pal):
                    png = "background/" + name + ".png"
            rec.file = idx
            bg_rows.append((rec.index, rec.tag_offset, w, h, crc32(pix), idx, palpath, png))
        elif rec.kind == "sfx":
            n = struct.unpack_from("<I", rec.data, 0)[0]
            payload = rec.data[4:4 + n]
            name = "sfx_%08x_%d.wav" % (crc32(payload), len(payload))
            key = ("audio", "wav", crc32(payload), len(payload))
            if key not in seen:
                wav_path = os.path.join(out_dir, "audio", name)
                write_wav(wav_path, payload)
                seen[key] = "audio/" + name
            rec.file = seen[key]
            sfx_rows.append((rec.index, rec.tag_offset, len(payload), crc32(payload),
                             rec.file))
        elif rec.kind == "midi":
            n = struct.unpack_from("<I", rec.data, 0)[0]
            payload = rec.data[4:4 + n]
            base = "music_%08x_%d" % (crc32(payload), len(payload))
            rawpath = "audio/" + base + ".midtrack"
            open(os.path.join(out_dir, rawpath), "wb").write(payload)
            mid = "audio/" + base + ".mid"
            ok = midi_track_to_file(payload, os.path.join(out_dir, mid))
            rec.file = rawpath
            midi_rows.append((rec.index, rec.tag_offset, len(payload),
                              crc32(payload), rawpath, mid if ok else None))
        elif rec.kind == "palette":
            name = "table_%08x_%d.bin" % (crc32(rec.data), len(rec.data))
            path = uniq_path("palettes", name, rec.data, rec)
            rec.file = path
            palette_rows.append((rec.index, rec.tag_offset, len(rec.data),
                                 crc32(rec.data), path))
        elif rec.kind == "metadata":
            name = "s%02x_%08x_%d.bin" % (rec.state, crc32(rec.data), len(rec.data))
            path = uniq_path("metadata", name, rec.data, rec)
            rec.file = path
            raw_rows.append((rec.index, rec.state, rec.tag_offset, rec.kind,
                             len(rec.data), crc32(rec.data), path))
        else:
            name = "s%02x_%08x_%d.bin" % (rec.state, crc32(rec.data), len(rec.data))
            path = uniq_path("data", name, rec.data, rec)
            rec.file = path
            raw_rows.append((rec.index, rec.state, rec.tag_offset, rec.kind,
                             len(rec.data), crc32(rec.data), path))

    # Save the background palette under palettes/main.pal too.
    if bg_palette is not None:
        open(os.path.join(out_dir, "palettes", "main.pal"), "wb").write(bg_palette)

    # Full manifest: one row per parsed record, including duplicates.
    with open(os.path.join(out_dir, "manifest.tsv"), "w", encoding="utf-8") as f:
        f.write("# index\tstate\ttag_offset\tpayload_offset\tcomp\tuncomp\t"
                "decoded\tcrc32\tkind\tasset_offset\tfile\tsource\n")
        for rec in records:
            f.write("%d\t%02x\t0x%08x\t0x%08x\t%d\t%d\t%d\t%08x\t%s\t%s\t%s\t%s\n"
                    % (rec.index, rec.state, rec.tag_offset, rec.payload_offset,
                       rec.comp_len, rec.uncomp_len, len(rec.data), crc32(rec.data),
                       rec.kind,
                       ("0x%08x" % rec.asset_offset) if rec.asset_offset is not None else "-",
                       rec.file or "-", rec.source or "main"))

    # Human-readable asset manifests.
    with open(os.path.join(out_dir, "images.tsv"), "w", encoding="utf-8") as f:
        f.write("# record\tstate\ttag_offset\tstream_offset\tstored\tdecoded\t"
                "width\theight\tpixel_crc\tidx\tpng\tsource\n")
        for row in image_rows:
            f.write("\t".join(str(x) for x in row) + "\n")
    with open(os.path.join(out_dir, "background.tsv"), "w", encoding="utf-8") as f:
        f.write("# record\ttag_offset\twidth\theight\tpixel_crc\tidx\tpalette\tpng\n")
        for row in bg_rows:
            f.write("\t".join(str(x) for x in row) + "\n")
    with open(os.path.join(out_dir, "audio.tsv"), "w", encoding="utf-8") as f:
        f.write("# kind\trecord\ttag_offset\tbytes\tcrc32\tfile\tmidi\n")
        for row in sfx_rows:
            f.write("sfx\t" + "\t".join(str(x) for x in row) + "\n")
        for row in midi_rows:
            f.write("midi\t" + "\t".join(str(x) for x in row) + "\n")
    with open(os.path.join(out_dir, "palettes.tsv"), "w", encoding="utf-8") as f:
        f.write("# record\ttag_offset\tbytes\tcrc32\tfile\n")
        for row in palette_rows:
            f.write("\t".join(str(x) for x in row) + "\n")
    with open(os.path.join(out_dir, "data.tsv"), "w", encoding="utf-8") as f:
        f.write("# record\tstate\ttag_offset\tkind\tbytes\tcrc32\tfile\n")
        for row in raw_rows:
            f.write("\t".join(str(x) for x in row) + "\n")

    counts = Counter(rec.kind for rec in records)
    unique_images = len({(r[6], r[7], r[8]) for r in image_rows})
    summary = {
        "input": os.path.abspath(ov.path),
        "file_size": ov.size,
        "overlay_start": ov.overlay_start,
        "overlay_size": len(ov.overlay),
        "dbase": ov.dbase,
        "assets": True,
        "extent": ov.extent,
        "entries": {str(k): {"offset": v} for k, v in ov.entries.items()},
        "records": len(records),
        "main_records": sum(1 for r in records if (r.source or "main") == "main"),
        "embedded_records": sum(1 for r in records if r.source == "embedded"),
        "skipped_payload_ranges": len(gaps),
        "skipped_payload_bytes": sum(e - s for s, e, _ in gaps),
        "record_kinds": dict(sorted(counts.items())),
        "images": len(image_rows),
        "unique_images": unique_images,
        "backgrounds": len(bg_rows),
        "sfx": len(sfx_rows),
        "midi_tracks": len(midi_rows),
        "palettes": len(palette_rows),
        "frame_groups": len(frame_groups),
        "frame_blocks": len(frame_rows),
        "palette_source": "background trailing 768 bytes" if bg_palette else "none",
        "notes": [
            "The appended stream is VM-record data, not a flat file archive.",
            "Images are raw 8-bit palette indices; PNGs use the background palette.",
            "SFX are 8-bit unsigned mono at 11025 Hz (the engine's waveOut format).",
            "MIDI tracks use running status and 10 ms delta units; .mid wrappers use 100 ticks/quarter at 1,000,000 us.",
            "No RIFF/AVI/WAVE/MThd container was present in this stream; the 640x480 background is the only full-screen bitmap.",
            "The startup/menu animation is a sequence of frame-cell command blocks in the delta payload; movies/ preserves them.",
        ],
    }
    with open(os.path.join(out_dir, "summary.json"), "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2)
        f.write("\n")
    with open(os.path.join(out_dir, "summary.txt"), "w", encoding="utf-8") as f:
        f.write("input: %s\n" % summary["input"])
        f.write("overlay: 0x%x..0x%x (%d bytes)\n"
                % (ov.overlay_start, ov.overlay_end, len(ov.overlay)))
        f.write("records: %d (%d main, %d embedded)\n"
                % (len(records), summary["main_records"], summary["embedded_records"]))
        f.write("skipped payloads: %d ranges, %d bytes\n"
                % (len(gaps), summary["skipped_payload_bytes"]))
        for k, v in sorted(counts.items()):
            f.write("  %-12s %d\n" % (k, v))
        f.write("images: %d (%d unique)\n" % (len(image_rows), unique_images))
        f.write("backgrounds: %d\n" % len(bg_rows))
        f.write("sfx: %d\n" % len(sfx_rows))
        f.write("midi tracks: %d\n" % len(midi_rows))
        f.write("frame groups: %d (%d blocks)\n" % (len(frame_groups), len(frame_rows)))
        if bg_palette:
            f.write("palette: background trailing 768 bytes\n")

    with open(os.path.join(out_dir, "README.txt"), "w", encoding="utf-8") as f:
        f.write("NETWALK appended-stream extraction\n")
        f.write("==================================\n\n")
        f.write("Generated by tools/nwextract.py from:\n  %s\n\n" % summary["input"])
        f.write("The game does not use a flat archive.  A VM byte stream is appended\n"
                "to the PE after dbase=0x%x; overlay.bin is that exact byte range.\n"
                "The walker follows the engine's tagged-record framing, decodes\n"
                "raw and LZSS blocks, and also scans payloads skipped by opcode\n"
                "jumps for complete embedded image blocks.\n\n"
                % ov.dbase)
        f.write("Layout\n------\n")
        f.write("  overlay.bin       exact appended data stream (raw, no trailer)\n")
        f.write("  overlay.nw        external-loadable stream: dbase padding + overlay + trailer\n")
        f.write("  overlay.trailer   38-byte original trailer for overlay.bin\n")
        f.write("  manifest.tsv      one row per parsed record (including duplicates)\n")
        f.write("  images/           unique 8-bit images: .idx raw indices + .png\n")
        f.write("  background/       640x480 background: .idx, .pal, .png\n")
        f.write("  audio/            *.wav sound effects and *.midtrack / *.mid music\n")
        f.write("  palettes/         main.pal (from the background) and color tables\n")
        f.write("  data/             every other unique decoded block/script/table\n")
        f.write("  metadata/         engine headers (save/screen/map records)\n")
        f.write("  movies/           rendered startup/menu frame PNGs + frames.tsv\n")
        f.write("  images.tsv        image record -> dimensions, CRC, .idx/.png\n")
        f.write("  background.tsv    background record -> .idx/.pal/.png\n")
        f.write("  audio.tsv         sound effect and music record -> output file\n")
        f.write("  data.tsv          non-image/non-audio decoded blocks\n")
        f.write("  palettes.tsv      palette/color-table blocks\n")
        f.write("  summary.json/txt  counts and format notes\n\n")
        f.write("Notes\n-----\n")
        f.write("  * overlay.nw is the file to place next to the executable for\n")
        f.write("    external-overlay builds; overlay.bin plus overlay.trailer also works.\n")
        f.write("  * .idx files are palette indices, not RGB.  Use main.pal with them.\n")
        f.write("  * PNGs are rendered with the background's trailing VGA palette.\n")
        f.write("  * SFX are unsigned 8-bit mono PCM at 11025 Hz.\n")
        f.write("  * The .mid wrappers keep the original event/delta bytes and add\n"
                "    a 1,000,000 us tempo at 100 ticks/quarter (10 ms per tick).\n")
        f.write("  * No RIFF/AVI/WAVE/MThd container exists in this stream.  The\n"
                "    640x480 background and the sprite/menu frames are the visual\n"
                "    intro content; there is no separate movie file to extract.\n")
    return summary


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("exe", help="NETWALK.EXE or another build with the =VS= data trailer")
    ap.add_argument("-o", "--out", default="extracted/NETWALK",
                    help="output directory (default: extracted/NETWALK)")
    ap.add_argument("--assets", "--extract-assets", dest="assets",
                    action="store_true",
                    help="also extract decoded records, images, audio, "
                         "palettes, metadata, and animation frames")
    ap.add_argument("--arg", type=int, default=0,
                    help="DispatchPendingCommandLoop argument (only used with --assets; default: 0)")
    ap.add_argument("--no-png", action="store_true",
                    help="with --assets, do not render PNGs (raw .idx files are still written)")
    ap.add_argument("--frame-scale", type=float, default=1.0,
                    help="with --assets, scale extracted frame PNG previews (default 1.0); "
                         "the game screen remains 640x480")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    ov = Overlay(args.exe)
    summary = extract(ov, args.out, no_png=args.no_png, arg=args.arg,
                      verbose=args.verbose, frame_scale=args.frame_scale,
                      assets=args.assets)
    if args.assets:
        print("extracted overlay and assets: %d record(s), %d unique image(s), "
              "%d sfx, %d midi track(s)"
              % (summary["records"], summary["unique_images"],
                 summary["sfx"], summary["midi_tracks"]))
    else:
        print("extracted overlay: %d bytes (dbase %d)" %
              (summary["overlay_size"], summary["dbase"]))
    print("output: %s" % os.path.abspath(args.out))


if __name__ == "__main__":
    main()
