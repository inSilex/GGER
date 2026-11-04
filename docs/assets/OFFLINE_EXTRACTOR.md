# Offline overlay extractor

`tools/nwextract.py` reads the shipped executable's appended VM data stream.
By default it writes only `overlay.bin`, `overlay.nw`, and `overlay.trailer`.
Pass `--assets` to also decode every asset record it can identify.  It does not
run the game and does not need Wine, SDL, or the `NW_DEBUG_ASSET_DUMP` hooks.

```sh
# Overlay only (the default):
python3 tools/nwextract.py /path/to/NETWALK.EXE -o extracted/NETWALK

# Overlay plus the complete decoded asset tree:
python3 tools/nwextract.py /path/to/NETWALK.EXE -o extracted/NETWALK --assets
```

The executable is read-only.  The output directory is replaced on each run.
With `--assets`, `--no-png` skips Pillow-based PNG rendering but still writes
raw `.idx` files, and `--arg` changes the initial
`DispatchPendingCommandLoop` argument (the default `0` is the shipped game's
path).

## Output layout

With `--assets`, the output contains:

```
overlay.bin       exact appended stream (dbase through, but not including, trailer)
overlay.nw        external-loadable stream: 0x12800 zero padding + overlay.bin + trailer
overlay.trailer   38-byte original trailer for overlay.bin
manifest.tsv      every parsed record, including duplicate records
images/           unique 8-bit images: .idx palette indices + .png preview
background/       640x480 background: .idx, .pal, .png
audio/            *.wav sound effects, *.midtrack raw music, *.mid wrappers
palettes/         main.pal (background palette) and color tables
data/             every other unique decoded block/script/table/grid record
metadata/         save/screen/map header records
movies/           rendered startup/menu frames: .png, groupN/, frames.tsv
images.tsv        image record -> dimensions, stored length, CRC, files
background.tsv    background record -> files
audio.tsv         sound/music record -> files
data.tsv          non-image/non-audio decoded records
palettes.tsv      palette/color-table records
summary.json/txt  counts, entry table, notes
README.txt        human-readable copy of this layout
```

Without `--assets`, only the three overlay files at the top of that list are
written.  `.idx` files are palette indices, not RGB.  Use
`palettes/main.pal` (768 bytes, RGB triples) or the background's `.pal`.
PNGs are rendered with that palette.
The palette comes from the 768 bytes trailing the 640x480 background payload.

## Repacking

`tools/nwrepack.py` consumes this output (or the original executable)
and rebuilds `overlay.bin`, `overlay.trailer`, and `overlay.nw` after image
replacement.  It relocates the index entry, inline `Disk` descriptors, and
skipped-payload lengths.  Extract with `--assets` for this workflow; the
default overlay-only run has no image tree.  See [`REPACKING.md`](REPACKING.md)
for the workflow, the `--pack`/`--replace` options, image-size handling, and the
startup-frame location.

`tools/nwframe_repack.py` is the companion for the rendered startup/
menu frames under `movies/`; it writes edited frames back as full-surface cell
commands and rebuilds the delta length and index/trailer.  Pass
`--frame-scale N` (and launch with `NW_RENDER_SCALE=N`) for native
`640*N x 480*N` frame cells.  `tools/nwupscale.py` wraps the basic
nearest-neighbour upscale for both frame sets and sprite packs.

## Stream format

The executable ends with a `=VS=` trailer.  The trailer holds the index extent,
the data base (`dbase`), and one 5-byte entry per index:

```
entry i: uint32 offset from dbase, uint8 tag == i
```

The shipped image uses `dbase=0x12800`, extent 2, and a 38-byte trailer.
`overlay.bin` is `[dbase, filesize-38)`.  Entry 0 is not a stream in this build;
the walker starts at entry 1 and switches to entry 2 on the first tag `0x00`.

## External overlay loading

The desktop and PS5 builds load the stream from `overlay.nw` next to the
executable. This is intended for editing the stream without relinking:

```text
build/linux-x86_64/NETWALK.EXE
build/linux-x86_64/overlay.nw       # preferred
```

`overlay.nw` is `overlay.bin` with `0x12800` zero bytes prepended and the
38-byte trailer appended.  The padding is required: inline `Disk` image records
carry file offsets that the engine rebases only when `dbase` is greater than
`0x12800`.  Keeping the original `dbase` makes the external stream behave like
the original module.

A raw `overlay.bin` is also accepted.  If `overlay.trailer` is next to it, that
trailer is used; otherwise the build falls back to the original NETWALK trailer
and a temporary stream is materialized in the system temp directory.  For an
overlay whose record sizes or offsets changed, ship a matching `overlay.nw` or
`overlay.trailer`; the fallback trailer only describes the shipped layout.

The normal CMake build stages `assets/overlay.nw` beside the executable. That
file is generated from the original executable and is ignored by Git; see
[BUILD.md](../BUILD.md#assets) or run the Docker wrapper with
`--extract-assets`. To use a different stream, point `NW_OVERLAY_ASSET` at it
when configuring:

```sh
cmake --preset linux-x86_64 -DNW_OVERLAY_ASSET=/path/to/overlay.nw
cmake --build --preset linux-x86_64
```

The PS5 equivalent is `NW_PS5_NATIVE_OVERLAY`.

Desktop builds can instead carry the overlay inside the executable. The
runtime's fallback reads the trailer from the module file itself, so
[`nwembed.py`](../../tools/documentation/nwembed.md) appends the stream and
rewrites the trailer's `dbase` to the append offset. An external `overlay.nw`
still takes precedence. Docker exposes the same mode as `--embed-overlay` for
Linux and Windows; the PS5 title keeps a separate overlay.

Records are framed by a tag byte.  `DecodeTaggedRecordLengthToOutParam` is used
for lengths and deltas:

| tag bits | length encoding |
|---|---|
| bit 7 clear | value is the tag itself |
| bit 7 set, bit 5 clear | low 5 bits are a byte width; read that many little-endian bytes |
| bit 7 set, bit 5 set | 1–4 low bits select width; 2-bit field extends it; bit 4 selects subtract/add bias |

The bias tables are the engine's `nw_data_window+0x1f8` and `+0x20c` values.
They are reproduced in the script because they are part of the engine image,
not the overlay.

A block tag has bit 7 set.  For the common forms:

- `tag & 0x40` set: short raw block, length is `tag & 0x1f`.
- `tag & 0x40` clear, `tag & 0x0c == 0`: raw block, length is the tagged value.
- `tag & 0x40` clear, `tag & 0x0c != 0`: LZSS block; stored and uncompressed
  lengths are both present, followed by compressed bytes.

LZSS uses eight flags per control byte, LSB first.  A set flag copies one
literal byte.  A clear flag reads a 16-bit little-endian back-reference:

```
distance = ((word >> 12) << 8) | (word & 0xff)
length   = ((word >> 8) & 0xf) + 2   # do/while => length + 1 bytes copied
```

The decompressor copies byte-at-a-time, so overlapping runs work.

Main-stream opcodes `0x01`–`0x05` are length/delta commands.  Opcode `0x06`
skips a 4-byte distance, then executes one block.  The skipped payload is
opaque to the main walker.  The extractor records those ranges and scans them
for complete LZSS image blocks; menu animation frames and the missing
137x77 `e86eacf3` frame live there.  Only exact geometry/decompressed-length
matches are accepted, so compressed bytes do not produce false images.

## Asset classes

| state | interpretation |
|---|---|
| `0x43` | `uint16 width`, `uint16 height`, then `width*height` palette indices; LZSS block typically includes the 4-byte header |
| `0x14` | same 4-byte image header when `len == 4 + width*height`; otherwise grid/map data |
| `0x18` | 640x480 background; `uint32 width/height` at +8/+12, pixels at +24, 768-byte palette at end |
| `0x42` | 768-byte color table |
| `0x51` | sound effect: 4-byte payload length + unsigned 8-bit mono PCM |
| `0x52` | music: 4-byte payload length + running-status MIDI event track |
| `0x06` | opcode stream/script block; skipped payloads may embed image blocks |
| `0x0f`, `0x10`, `0x11`, `0x19` | engine headers/metadata |
| other | raw decoded data, grid, script, table, or config |

Sound effects use the engine's waveOut ring format: 11025 Hz, mono, 8-bit
unsigned.  The `.wav` files have a standard RIFF header; the exact payload is
also inside `overlay.bin` and the manifest's offsets.

Music tracks begin after a 4-byte length.  They are not a standard MIDI file:
the events use running status and a 10 ms delta unit, and a status byte `0xFF`
is the engine's end marker.  `.midtrack` preserves the exact bytes.  `.mid` is
a format-0 standard MIDI file with the same event/delta bytes, a tempo meta
event (1,000,000 us per quarter), 100 ticks per quarter (10 ms per tick), and a
real end-of-track event.

## Validation

The extraction was checked against the committed runtime sprite dump:

- 4,666 main-stream records, 27 skipped payload ranges (2,355,107 bytes),
  147 embedded image records.
- 1,182 image records, 723 unique `(width, height, decoded-pixel CRC32)` images.
- All 86 runtime sprite rows / 76 unique decoded images are present, with
  matching dimensions, decoded-pixel CRCs, and stored lengths.
- Known LZSS frames match both the offline output and the runtime dump:
  `0955a268` (213x42) and `e86eacf3` (137x77, embedded).
- The 640x480 background is the only full-screen bitmap.  There is no
  RIFF/AVI/WAVE/`MThd`/`MTrk` container in the stream.  The visual startup/menu
  content is the background plus sprite and menu frames; the startup animation
  itself is a sequence of frame-cell command blocks in the delta payload.
  Those blocks are preserved under `movies/` (3 groups, 292 blocks); they are
  not a standard video file.  See
  [`REPACKING.md`](REPACKING.md#where-is-the-startup-animation).

## Cross-build notes

The script validates the trailer and `dbase`, but the bias tables and the
state-to-asset interpretation are specific to this engine generation.  For a
different build, first confirm the trailer/entry table and the `0x43` image
header, then extend the class table if new states appear.  The manifest keeps
every record, so unknown blocks are still preserved under `data/`.
