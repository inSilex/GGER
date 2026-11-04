# nwframe_repack.py

`nwframe_repack.py` replaces the frame-cell startup/menu animation in an
external overlay. This animation is stored as a sequence of frame blocks, not
as AVI/MPEG video:

```text
uint32 type
uint32 stored_bytes
uint32 raw_bytes
stored_bytes of frame-cell command data
```

The tool accepts edited 640x480 PNG or indexed frames, rebuilds the surrounding
delta payload and index/trailer, and can emit native-resolution frame cells for
`NW_RENDER_SCALE>1`.

## Requirements

- Python 3.8 or newer.
- Pillow for PNG frame input.

```sh
python3 -m pip install -r tools/requirements.txt
```

## Usage

```sh
python3 tools/nwframe_repack.py extracted/NETWALK/overlay.nw \
    --frames edited-frames -o repacked

python3 tools/nwframe_repack.py NETWALK.EXE \
    --frames edited-frames --frame-scale 2 -o repacked
```

Input may be `overlay.nw` or `NETWALK.EXE`. `--frames` must contain
`groupN/frame_XXXX.png` or `.idx` files in the extractor's layout. The default
output directory is `repacked/`.

## Important options

- `--palette PATH`: startup palette; defaults to the extracted
  `movies/palette.pal` when available.
- `--frame-scale N`: write native `640*N x 480*N` full-surface cells. Use
  `NW_RENDER_SCALE=N` when running the result.
- `--force`: rewrite frames even when the pixels are unchanged.
- `--verbose`: print per-frame work.

The current animation frames can be regenerated with `nwextract.py --assets`;
see [`nwupscale.py frames`](nwupscale.md) for a one-step upscale-and-repack
wrapper.

## Related documentation

- [`docs/assets/REPACKING.md`](../../docs/assets/REPACKING.md)
- [`nwextract.py`](nwextract.md)
- [`nwupscale.py`](nwupscale.md)
