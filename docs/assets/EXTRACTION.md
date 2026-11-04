# Asset extraction and high-resolution overrides

Status: extraction, external override packs, and embedded high-resolution
overlays are implemented and tested (SDL build).

The shipped `NETWALK.EXE` carries a 4.9 MB appended overlay. It is **not** a
flat archive. The game treats its own module file as a serialized VM stream;
`DispatchPendingCommandLoop` executes opcodes and synthesizes image descriptors
in memory, then stamps them with the `Disk` tag and a logical stream offset.
That is why a naive scan for `Disk`, `RIFF`, or `BMP` signatures finds no image
table in the overlay.

The extraction path therefore uses the engine itself: the SDL build can dump
every sprite after the engine has decoded it, and can replace that sprite from
an external pack before the renderer sees it. The manifest is keyed by decoded
content, not by a hard-coded NetWalk offset, so another game built from the same
engine can use the same format and the same loader.

## 1. Extraction

The dump hooks are compiled only in a debug build (`NW_DEBUG_TOOLS=ON`). Set
`NW_DEBUG_ASSET_DUMP` to a directory (relative to the game's working directory
or a Wine `Z:` path) before starting the game:

```sh
cmake --preset linux-x86_64-debug
cmake --build --preset linux-x86_64-debug
NW_DEBUG_ASSET_DUMP=assets ./build/linux-x86_64-debug/NETWALK.EXE
```

`assets/.gitignore` keeps these runtime dumps and other generated asset trees
out of Git.

`LoadDiskSpriteImageFromStream` writes each decoded sprite as raw 8-bit palette
indices:

```
assets/sprites.tsv
assets/sprites/o<offset>_<w>x<h>.idx
assets/palette.bin
```

`palette.bin` is a sequence of 776-byte records:

```
00 "PAL" 00 03 <count:u16> 256 * (R,G,B)
```

The palette dump is in the SDL `SetLogicalPaletteDrawGrid` and is inert unless
`NW_DEBUG_ASSET_DUMP` is set.

`sprites.tsv` rows are tab-separated:

```
offset  width  height  stored  crc32  file  [source_width source_height]
```

The first six fields are produced by the dumper; the optional two fields are
used by an override pack. `offset` is an absolute file position and therefore
changes when the rebuilt PE changes size. `crc32` is computed over the decoded
pixels (`width * height` bytes) and is the stable identity.

Only sprites loaded through `LoadDiskSpriteImageFromStream` are captured. This
is the path used by the menu buttons, text strips, playfield tiles, cursor/UI
sprites, and other `Disk` records. Palettes, the tile-map stream (`kind3`
blocks), the VM bytecode, and audio are separate record types; they are not
misrepresented as sprites. The same hook pattern can be added for them later.

## 2. Override pack

Set `NW_DEBUG_ASSET_PACK` to a directory containing `sprites.tsv` and the referenced raw
files:

```sh
NW_DEBUG_ASSET_PACK=pack wine ./netwalk.exe
```

The loader looks up an entry by `crc32` first and `offset` second, and only
when the original `width`/`height` match. A file that has the same dimensions is
copied directly. If the row appends `source_width source_height`, the file is
larger (or smaller) and is sampled with nearest-neighbour to the original
geometry. Nearest-neighbour is intentional: the pixels are palette indices, so
averaging them would produce an index that is not a colour in the game palette.

The game layout, blitter, and 8bpp framebuffer stay unchanged. This is a
replacement pipeline, not yet a higher-resolution renderer: a 4x source is
reduced to the sprite's existing on-screen size.

`NW_DEBUG_ASSET_LOG` optionally names a text log. Each applied override writes:

```
sprite off=... orig=WxH crc=... file=... src=SWxSH
```

Both environment variables are read only in debug builds. Release builds
compile the hooks out and ignore them.

### Embedded high-resolution overlay

For a complete upscaled tree, `nwrepack.py --highres` embeds the
high-resolution pixels in the overlay itself, so no `NW_DEBUG_ASSET_PACK` sidecar is
needed:

```sh
python3 tools/nwrepack.py extracted/NETWALK/overlay.nw \
    --pack /path/to/extracted_upscaled/NETWALK \
    --highres --scale 4 -o .tmp/highres4
```

Run a desktop build with `NW_RENDER_SCALE=4` (the default).  The VM stream, `dbase`, record
offsets, index entry 2, and 32-bit guest layout are unchanged; the SDL blitter
samples the appended 4x sprites and background.  At `NW_RENDER_SCALE=1` the
embedded overrides are ignored, so the same overlay is pixel-identical to the
original.  `NW_DEBUG_ASSET_PACK` still takes priority when set; the embedded section is a
fallback when no external manifest is loaded.  The section also carries the
640x480 screen background (`background/background_640x480_*.png`) at 4x under
the synthetic `0x4E574247` offset.  The engine discovers it automatically from
the external overlay path; no background file is needed next to the binary.

The overlay layout and footer format are documented in
[`REPACKING.md`](REPACKING.md#high-resolution-section).  `--exact` is not a
resolution switch: it changes inline descriptor sizes and shifts the VM heap,
so use `--highres` for 4x art.  Movies are intentionally not included.

## 3. Tooling

The runtime dump is intentionally driven by the game itself. Launch a debug
build with `NW_DEBUG_ASSET_DUMP` set, visit the menu and playfield screens you
want, then quit:

```sh
cd build/linux-x86_64-debug
NW_DEBUG_ASSET_DUMP=/tmp/opencode/nwassets ./NETWALK.EXE
```

The dump appears under that directory. `tools/nwassets.py` processes an
existing dump without launching the game:

```sh
# Render .idx files to BMP/PNG and a contact sheet.
python3 tools/nwassets.py render /tmp/opencode/nwassets

# Build a 4x override pack with a visible marker block.
python3 tools/nwassets.py pack /tmp/opencode/nwassets

# Build a pack from one decoded-pixel CRC (the NOVICE label here).
python3 tools/nwassets.py pack /tmp/opencode/nwassets \
    --out /tmp/opencode/nwpack_novice --only-crc 0955a268
```

`--no-marker` produces an exact nearest-neighbour source instead of the test
marker. `--only-offset` is the offset-keyed equivalent of `--only-crc`. The
pack is activated by `NW_DEBUG_ASSET_PACK` as described above; `NW_RENDER_SCALE`
selects the desktop output scale.

For a reproducible asset tree, prefer the offline extractor. `nwextract.py`
recovers the same sprite set without a running game, and `nwupscale.py sprites`
builds a nearest-neighbour pack from an `NW_DEBUG_ASSET_DUMP` directory:

```sh
python3 tools/nwextract.py NETWALK.EXE -o extracted/NETWALK --assets
python3 tools/nwupscale.py sprites /tmp/opencode/nwassets \
    -o /tmp/opencode/nwpack2 --scale 2
```

## 4. Why this is engine-generic

- The extractor is a hook on the engine's decoded-sprite boundary, not a
  NetWalk-specific image table parser.
- Pack identity is a CRC32 of decoded pixels plus the original geometry, so it
  survives PE size/dbase changes and shared/duplicate sprites.
- The manifest is plain text and the payload is raw indexed pixels; no external
  codec library or SDL_image dependency is added.
- Scaling dimensions are data, so the same loader can accept equal-size
  replacements, 2x/4x sources, or smaller sources.

A future pass can add the same dump/override hooks for `ReadLzssOrRawTaggedRecordBlock`
(audio, bytecode, and other raw blocks) and for palettes used by those blocks.

High-resolution source packs are now used by the native render path.  Set
`NW_RENDER_SCALE=N` and either use an external `NW_DEBUG_ASSET_PACK=<pack>` or the
embedded section produced by `nwrepack.py --highres`; the scaled sprite
blitter samples the full source image and draws it into the
`640*N x 480*N` framebuffer.  The original 640x480 manifest geometry remains
the draw rectangle, so game layout does not move.  `tools/nwupscale.py
sprites` builds a basic nearest-neighbour pack from an existing `NW_DEBUG_ASSET_DUMP`
directory.

## 5. Offline extraction without the runtime hooks

The runtime dump remains the oracle, but it is no longer required to get the
assets.  `tools/nwextract.py` walks the appended VM stream directly:

```sh
python3 tools/nwextract.py /path/to/NETWALK.EXE -o extracted/NETWALK --assets
cp extracted/NETWALK/overlay.nw assets/overlay.nw
```

The copy installs the overlay used by local builds and PS5 staging.
`assets/overlay.nw` is generated and ignored by Git; the Docker wrapper can
perform the same extraction with `--extract-assets /path/to/NETWALK.EXE`.

With `--assets` it writes the exact overlay, every parsed record, all unique
images (raw `.idx` plus PNG previews), the 640x480 background and its palette,
sound-effect WAVs,
raw MIDI tracks plus standard `.mid` wrappers, and every remaining decoded
block.  It also scans the payloads skipped by opcode jumps, which is where the
menu animation frames live; the runtime oracle's 76 unique decoded sprite CRCs
are all present in the offline output.

The format details, output layout, and validation results are in
[`OFFLINE_EXTRACTOR.md`](OFFLINE_EXTRACTOR.md).  `extracted/` is generated and
git-ignored.
