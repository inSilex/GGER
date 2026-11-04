# Repacking the overlay

`tools/nwrepack.py` replaces image records in the extracted VM stream and
rebuilds the index/trailer.  It is the write-side companion to
[`OFFLINE_EXTRACTOR.md`](OFFLINE_EXTRACTOR.md).

The overlay is not a flat archive.  Image records are interleaved with opcode
streams, some images live inside skipped opcode payloads, and both the
top-level index entry and inline `Disk` descriptors contain absolute stream
offsets.  Changing an image's length therefore moves everything after it.  The
repacker handles those relocations; it does not merely overwrite bytes in place.

## Quick start

```sh
# 1. Extract once (--assets also emits images.tsv and the PNG tree).
python3 tools/nwextract.py /path/to/NETWALK.EXE -o extracted/NETWALK --assets

# 2. Edit or replace PNGs.  A full extracted tree is fine; unchanged pixels are
#    skipped automatically.
python3 tools/nwrepack.py extracted/NETWALK/overlay.nw \
    --pack edited_assets -o repacked

# 3. Use the rebuilt external stream.
cp repacked/overlay.nw build/linux-x86_64/
```

`--pack` may point at a complete `extracted/NETWALK` tree, a flat directory of
the extractor's PNG filenames, or a directory containing only edited PNGs.  An
image is matched by its extractor filename (state/dimensions/CRC) and by the
`images.tsv` record map.

For a one-off change, use `--replace` instead:

```sh
python3 tools/nwrepack.py extracted/NETWALK/overlay.nw \
    --replace images/s14_121x77_7bb9810f.png=edited/sound.png \
    -o repacked
```

`OLD` may be an extractor PNG name, a record index, a tag offset (`0x...`), or
a PNG path.  `--replace` is repeatable.

The source may be `NETWALK.EXE`, an `overlay.nw`, or a raw `overlay.bin`
(use `--trailer overlay.trailer` when the sidecar is not named by convention).
The palette defaults to the extractor's `palettes/main.pal`; pass `--palette`
if the output tree is somewhere else.

## Image sizes

The default is **fit**: the replacement PNG can be any size, but it is
nearest-neighbour resized to the original record geometry.  This keeps the
engine's 640x480 layout and the sprite coordinates unchanged.  A 2x or 4x
source image is therefore accepted, but the rendered size remains the original
one.  This is the safe mode for redrawn/high-resolution source art.

`--exact` keeps the replacement's dimensions and rewrites the image header and
any inline `Disk` descriptor (`width`, `height`, `stored`, offset).  The engine
still has a fixed 640x480 canvas and hard-coded placement, so a larger sprite
can overlap or clip neighbouring elements.  `--exact` is useful for
experimentation and for assets whose layout is also being changed; it is not a
general resolution switch.

For runtime high-resolution replacement without touching the stream, there are
two paths:

- **Embedded high-resolution overlay (supported).**  `--highres` appends a
  self-describing section to the rebuilt overlay.  The VM stream, `dbase`,
  index entry 2, record offsets, and 32-bit guest heap layout stay
  byte-identical; the SDL blitter samples the full-size pixels when
  `NW_RENDER_SCALE=N`.  This is the recommended path for a complete upscaled
  asset tree:

  ```sh
  python3 tools/nwrepack.py extracted/NETWALK/overlay.nw \
      --pack /path/to/extracted_upscaled/NETWALK \
      --highres --scale 4 -o repacked-highres
  ```

  Run the SDL build with `NW_RENDER_SCALE=4` (the default).  At
  `NW_RENDER_SCALE=1` the embedded overrides are ignored, so the same overlay
  behaves exactly like the original.

- **External `NW_DEBUG_ASSET_PACK` pack.**  The loader still accepts a directory with
  `sprites.tsv` and raw indexed files; `NW_DEBUG_ASSET_PACK` takes priority over the
  embedded section.  This is useful for small isolated packs and for the
  `tools/nwupscale.py sprites` helper.

`--exact` is not a resolution switch.  It changes inline `Disk` descriptor
sizes, which shifts the VM heap layout; use `--highres` for 2x/3x/4x art.

## What is rebuilt

- **Record blocks.**  Replaced images are written as raw blocks by default.
  Raw blocks avoid the fixed LZSS scratch arena and are accepted by the same
  record decoder as the shipped raw records.  `--lzss` asks for LZSS output
  (all-literal, primarily for experiments).
- **Skipped-payload counts.**  Embedded images live inside the payload skipped
  by main-stream opcode `0x06` (and, in principle, delta gaps).  When their
  block length changes, the enclosing 4-byte count is adjusted.
- **Inline `Disk` descriptors.**  Opcode `0x43` copies a 16-byte descriptor
  from the opcode block:

  ```text
  +0  uint16 width
  +2  uint16 height
  +4  "Disk" (0x4469736b)
  +8  uint32 absolute stream offset
  +12 uint32 stored length (0 means raw pixels; otherwise LZSS source bytes)
  ```

  Every descriptor in a `0x06` opcode block (all 147 inline descriptors in this
  build) is remapped to the new image payload, and the descriptor's
  dimensions/stored length are updated.  The containing block is re-encoded,
  which can move later images again; the repacker runs a fixed-point relocation
  loop until the layout is stable.
- **Index entry 2.**  The stream start at `overlay + 0x3f8c4b` is shifted by
  every earlier insert/delete and written back into the 38-byte trailer.
  Entry 1 stays at zero.
- **Trailer.**  `dbase` is kept at the original `0x12800`, so the external
  stream behaves exactly like the spliced module.  The repacker writes:

  ```text
  repacked/overlay.bin       rebuilt raw stream
  repacked/overlay.trailer   rebuilt 38-byte trailer
  repacked/overlay.nw        0x12800 zero padding + overlay.bin + trailer
  repacked/repack.tsv        every edit and its byte delta
  ```

### High-resolution section

`--highres` is the supported mode for a complete 2x/3x/4x asset tree.  It does
not rewrite any image record.  It appends a self-describing section after a
preserved copy of the original trailer, then writes a fresh trailer for the
external loader:

```text
0x12800 zero padding (dbase)
raw VM stream (byte-identical)
original 38-byte trailer copy   (the final VM record overreads 5 bytes)
high-res section
32-byte footer (NWHRPL01, version 1)
fresh 38-byte trailer
```

The section begins with a `uint32` entry count, a 4-byte-aligned table of
24-byte entries, and then the raw indexed pixel blobs.  Each entry stores the
original stream offset, decoded-pixel CRC, original width/height, source
width/height, and the section-relative data offset and length.  `--scale N`
requires every source to be exactly `N` times its original dimensions; without
`--scale`, any larger source is accepted and sampled.  The full-screen
`background/background_640x480_*.png` is included under the synthetic offset
`0x4E574247`; with `--scale N` it must be `640*N x 480*N`.  Movie frames are
not included.

The engine finds the footer immediately before the final trailer, so the
section works in `overlay.nw` and in the materialized `overlay.bin` +
`overlay.trailer` pair.  It never changes `dbase`, the VM stream, index entry
2, record offsets, or the 32-bit guest heap layout.

For PS5, stage the high-resolution pack as the external overlay and create the
mountable image in the same build:

```sh
NW_PS5_OVERLAY=.tmp/highres4/overlay.nw \
platforms/ps5/tools/build-native.sh --ffpkg
```

The overlay remains a separate file inside the UFS2 image; `--ffpkg` refuses
`--no-overlay`.  If the pack is stored as `overlay.bin` plus
`overlay.trailer`, set `NW_PS5_OVERLAY` and `NW_PS5_OVERLAY_TRAILER` together.
See [../PS5.md](../PS5.md#package-as-ffpkg).

## Validation

The rebuilt stream was run for the full timed intro and into the menu; the
640x480 menu is pixel-identical to the original even after a full rebuild of
all 1,182 image records.  A visible test replacing the SOUND button with a
242x154 checkerboard (default fit) showed only the button area changing, with
no crash.

The 4x embedded overlay used for the current validation is 58,625,103 bytes
and carries 1,182 sprites plus a 2560x1920 background (section 53,645,052
bytes).  With the SDL build and `NW_RENDER_SCALE=4`:

- A background-only patch is pixel-identical to the upscaled background PNG.
- Gameplay rotates a tile and exits cleanly through the EXIT button.
- `NW_DEBUG_ASSET_LOG` reports 4x sources for board and UI sprites (`45x45 -> 180x180`,
  `137x77 -> 548x308`, and similar).
- At `NW_RENDER_SCALE=1` the same overlay is pixel-identical to the original
  overlay at the menu.
- At scales 2 and 3 the background samples match the scale-4 high-resolution
  source at the expected positions.

A repack necessarily changes the stream bytes the VM trace hashes (record
bodies, descriptor offsets, and lengths), so use the trace as a startup/liveness
smoke test rather than expecting the original `b81f…` hash.  The untouched external stream retains that gate.

Headless smoke test:

```sh
cmake --preset linux-x86_64-debug
cmake --build --preset linux-x86_64-debug
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy NW_NO_AUDIO=1 \
  NW_DEBUG_VM_TRACE=/tmp/vm-repacked.txt timeout 6 \
  build/linux-x86_64-debug/NETWALK.EXE
sha256sum /tmp/vm-repacked.txt
```

By default every desktop build loads `overlay.nw` from beside the executable.
Replace the staged file or point the build at another overlay with
`-DNW_OVERLAY_ASSET=...`. For a self-contained desktop build, append the
overlay with [`nwembed.py`](../../tools/documentation/nwembed.md), or pass
`--embed-overlay` to the Docker release wrapper. An external overlay still takes
precedence over an embedded copy at runtime.

## Where is the startup animation?

There is **no RIFF/AVI/MPEG container in the overlay**, and the apparent
`00 00 01 B3` start code at overlay offset `0x2af726` is inside a frame-cell
block, not a standalone video: `ffprobe`/`ffmpeg` reject the extracted range.
The built-in startup animation is a sequence of **frame-cell command blocks**
inside the large `0x05` delta payload.

The extractor renders and preserves it under `movies/`:

```text
movies/group0/frame_0000.png ... 200 frames at overlay 0x21db94
movies/group1/frame_0000.png ...  62 frames at overlay 0x2f8a01
movies/group2/frame_0000.png ...  30 frames at overlay 0x37dfaa
movies/palette.pal              startup palette block
movies/frames.tsv               offsets, lengths, CRC, .bin/.png
movies/README.txt
```

`nwextract.py --assets --frame-scale 2` writes 2x preview PNGs
(nearest-neighbour).
For an editable native-resolution set, use `nwupscale.py frames` (or
`nwframe_repack.py --frame-scale 2`).  The repacker emits full-surface cells in
physical coordinates (`1280x960` for scale 2); the SDL compositor recognizes
that size and writes the pixels directly into the scaled framebuffer.  Use the
same `NW_RENDER_SCALE` value at runtime.

Each block starts with:

```text
uint32 type = 1
uint32 stored_bytes
uint32 raw_bytes
stored_bytes of LZSS or raw frame-command data
```

The first group's first block is LZSS-coded; the rest of the chain in this
build is raw.  The decoded stream is consumed by
`ComposeFrameCellsFromStreamRecords` as a list of cell rectangles, not by a
media decoder.  The game reads each block through `ReadStreamBlockRawOrLzss`;
the outer delta payload is the byte range `0x21d1c5..0x3f8c4a`.

`MoviePlayback.c` still contains an optional external `<appdir>\intro.AVI`
MCI path, but that is a separate feature and was not the source of the
built-in startup animation.  The SDL build's
`TryPlayStartupMovieWithWindowFallback` returns immediately, which is why the
current SDL/Linux executable goes straight to the game screen instead of
running the Win32 startup/child-window animation.  The frame renderer is now
available offline; making the SDL build play the startup sequence is a
separate window/message-pump port.

## Replacing rendered frames

`tools/nwframe_repack.py` takes edited frames from a directory with the
same `groupN/frame_XXXX.png` layout:

```sh
# edit extracted/NETWALK/movies/group0/frame_0004.png
python3 tools/nwframe_repack.py extracted/NETWALK/overlay.nw \
    --frames extracted/NETWALK/movies -o frame_repacked
```

Each changed frame is written as a full-surface cell command (640x480,
plain-delta decoder), the enclosing `0x05` delta length is adjusted, and
entry 2/trailer are rebuilt.  `--frame-scale N` instead writes a native
`640*N x 480*N` cell and accepts edited sources at that size; a smaller source
is nearest-scaled up.  Use the same `NW_RENDER_SCALE=N` when launching, for
example:

```sh
python3 tools/nwupscale.py frames extracted/NETWALK \
    -o /tmp/opencode/frames2 --scale 2 --groups 0 --limit 4
NW_RENDER_SCALE=2 ./NETWALK.EXE
```

Replacing many frames with full-surface blocks makes the external overlay
large, so prefer editing only the frames you need.  `--force` rewrites every
frame in the directory.
