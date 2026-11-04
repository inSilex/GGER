# nwassets.py

`nwassets.py` processes a runtime sprite dump written by a debug build when
`NW_DEBUG_ASSET_DUMP` points at a directory. The dump contains `sprites.tsv`,
indexed sprite files under `sprites/`, and `palette.bin`. The tool does not
launch the game; run the debug build yourself first.

## Requirements

- Python 3.8 or newer.
- Pillow for PNG rendering and the contact sheet. The BMP files are written
  with the standard library.

```sh
python3 -m pip install -r tools/requirements.txt
```

## Commands

### render

```sh
python3 tools/nwassets.py render /tmp/opencode/nwassets
```

Converts every valid sprite in the dump to an 8-bit BMP, also writes PNGs when
Pillow is available, and creates `sheet.png`, a contact sheet for a quick scan.
The default output is `<dump>/render`; use `--out DIR` to choose another
directory.

### pack

```sh
python3 tools/nwassets.py pack /tmp/opencode/nwassets
python3 tools/nwassets.py pack /tmp/opencode/nwassets \
    --out /tmp/opencode/nwpack --scale 2 --only-crc 0955a268
```

Builds an `NW_DEBUG_ASSET_PACK` source pack containing `sprites.tsv` and
nearest-neighbour scaled indexed sprites. The default scale is 4. The pack can
be selected at runtime with `NW_DEBUG_ASSET_PACK` and `NW_RENDER_SCALE`.

Options:

- `--out DIR`: output pack directory; default `<dump>/pack`.
- `--scale N`: integer upscale factor; default 4.
- `--only-crc HEX`: pack only the decoded-pixel CRC32 matching `HEX`.
- `--only-offset HEX`: pack only the logical record offset `HEX`.
- `--no-marker`: omit the test marker block from the generated sprites.

## Related documentation

- [`docs/assets/EXTRACTION.md`](../../docs/assets/EXTRACTION.md)
- [`nwupscale.py`](nwupscale.md)
- [`nwrepack.py`](nwrepack.md)
