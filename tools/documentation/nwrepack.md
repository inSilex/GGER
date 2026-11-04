# nwrepack.py

`nwrepack.py` rebuilds an external `overlay.nw` after replacing image records
inside the recovered VM stream. It understands the same record framing as
`nwextract.py`, including raw and LZSS image blocks, embedded-image gaps,
skip/delta lengths, index entry 2, and the 38-byte trailer.

## Requirements

- Python 3.8 or newer.
- Pillow for PNG replacement images and `--reindex`.

```sh
python3 -m pip install -r tools/requirements.txt
```

## Usage

```sh
# Replace images from a pack directory.
python3 tools/nwrepack.py extracted/NETWALK/overlay.nw \
    --pack edited-png -o repacked

# Replace one image explicitly.
python3 tools/nwrepack.py extracted/NETWALK/overlay.nw \
    --replace images/s43_45x45_abc.png=edited/foo.png -o repacked

# Convert an RGB upscale pack to indexed PNGs in place, then repack it.
python3 tools/nwrepack.py extracted/NETWALK/overlay.nw \
    --pack extracted_upscaled/NETWALK --reindex -o repacked

# Only rewrite the pack's PNGs and stop.
python3 tools/nwrepack.py --pack extracted_upscaled/NETWALK --reindex
```

The input may be `NETWALK.EXE`, an extracted `overlay.nw`, or a raw
`overlay.bin` with `--trailer`. The output is a complete external overlay
(`overlay.nw`, plus raw pieces where requested) with the original dbase
padding.

## Important options

- `--pack DIR`: directory containing replacement images named like the
  extractor output, or a full extracted asset tree.
- `--images-tsv PATH`, `--palette PATH`, `--trailer PATH`: override extracted
  metadata for unusual layouts.
- `--replace OLD=NEW`: replace a record, tag, or old PNG; repeatable.
- `--reindex`: rewrite every PNG in `--pack` as an indexed PNG. With no input,
  stop after the rewrite.
- `--dither`: Floyd-Steinberg dithering during `--reindex`.
- `--exact`: keep replacement dimensions instead of fitting the original
  record geometry. This is experimental.
- `--highres`: embed larger pixels without changing the VM layout; use with
  `NW_RENDER_SCALE>1`.
- `--scale N`: with `--highres`, require exactly `original_width*N` by
  `original_height*N` sources.
- `--force`: re-encode records even when the pixels are unchanged.
- `--lzss`: write LZSS image blocks; the default is raw blocks.

## Related documentation

- [`docs/assets/REPACKING.md`](../../docs/assets/REPACKING.md)
- [`docs/assets/EXTRACTION.md`](../../docs/assets/EXTRACTION.md)
- [`nwextract.py`](nwextract.md), [`nwupscale.py`](nwupscale.md)
