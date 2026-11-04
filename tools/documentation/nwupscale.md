# nwupscale.py

`nwupscale.py` is a deliberately small nearest-neighbour upscaler for the two
image paths used by the game. It performs no filtering or palette averaging.

## Requirements

- Python 3.8 or newer.
- Pillow for PNG input/output.

```sh
python3 -m pip install -r tools/requirements.txt
```

## Modes

### Sprites

```sh
python3 tools/nwupscale.py sprites /tmp/opencode/nwassets \
    -o /tmp/opencode/nwpack2 --scale 2
NW_RENDER_SCALE=2 NW_DEBUG_ASSET_PACK=/tmp/opencode/nwpack2 \
    ./build/linux-x86_64/NETWALK.EXE
```

The input is an `NW_DEBUG_ASSET_DUMP` directory containing `sprites.tsv` and
`sprites/*.idx`. The output is an `NW_DEBUG_ASSET_PACK` source pack: the
original geometry stays in the manifest, while the extra source-width/height
columns point at the larger art.

### Frames

```sh
python3 tools/nwupscale.py frames extracted/NETWALK \
    -o /tmp/opencode/frames2 --scale 2 --groups 0 --limit 4
```

The input is an extracted asset tree with `overlay.nw` and `movies/`. The tool
upscales the selected movie frames, invokes `nwframe_repack.py`, and writes the
repacked overlay to `<out>/overlay/overlay.nw`; the scaled source frames are
under `<out>/frames/`. Use `NW_RENDER_SCALE=N` with the result.

`--scale` defaults to 2 and must be in the range 1 through 8. `--groups` limits
the animation groups; `--limit` limits the frame index within each group.
`--force` forwards to the frame repacker, and `--verbose` prints the per-frame
work.

## Related documentation

- [`docs/assets/EXTRACTION.md`](../../docs/assets/EXTRACTION.md)
- [`docs/assets/REPACKING.md`](../../docs/assets/REPACKING.md)
- [`nwassets.py`](nwassets.md), [`nwframe_repack.py`](nwframe_repack.md)
