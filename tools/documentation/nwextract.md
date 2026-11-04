# nwextract.py

`nwextract.py` reads the VM data stream appended to the original Windows
`NETWALK.EXE`. By default it writes only the overlay files needed by local and
PS5 builds. `--assets` additionally extracts decoded records, images,
background, sound effects, music tracks, palettes, and animation-frame
previews for upscaling.

It is an offline extractor: it does not run the game and does not need a debug
build. The original executable is not included in this repository.

## Requirements

- Python 3.8 or newer.
- Pillow only when asset extraction with PNG output is requested. `--assets
  --no-png` still writes the raw indexed image files.

Install the Python requirements from the repository root:

```sh
python3 -m pip install -r tools/requirements.txt
```

## Usage

```sh
# Overlay only (default): overlay.bin, overlay.nw, overlay.trailer.
python3 tools/nwextract.py /path/to/NETWALK.EXE -o extracted/NETWALK

# Also decode the asset tree for inspection or upscaling.
python3 tools/nwextract.py /path/to/NETWALK.EXE -o extracted/NETWALK --assets
python3 tools/nwextract.py /path/to/NETWALK.EXE -o extracted/NETWALK \
    --assets --no-png
python3 tools/nwextract.py /path/to/NETWALK.EXE -o extracted/NETWALK \
    --assets --frame-scale 2

# Install the generated overlay used by local and PS5 staging builds.
cp extracted/NETWALK/overlay.nw assets/overlay.nw
```

`assets/overlay.nw` is generated and ignored by Git. The Docker release wrapper
can perform the extraction automatically with
`--extract-assets /path/to/NETWALK.EXE`.

Important options:

- `-o, --out DIR`: output directory; default `extracted/NETWALK`.
- `--assets, --extract-assets`: also decode records, images, audio, palettes,
  metadata, and animation frames.
- `--arg VALUE`: `DispatchPendingCommandLoop` argument; only used with
  `--assets`; default `0`.
- `--no-png`: with `--assets`, skip PNG rendering and write only `.idx` image
  data.
- `--frame-scale N`: with `--assets`, scale extracted animation previews; this
  does not change the game resolution.
- `--verbose`: print extra parsing progress.

## Outputs

The default output tree includes:

- `overlay.bin`: exact appended data stream without the trailer;
- `overlay.nw`: external-loadable stream with database padding and trailer;
- `overlay.trailer`: original 38-byte trailer.

With `--assets`, the tree also includes:

- `data/` with the decoded tagged records and manifest;
- `images/` with indexed `.idx` files and PNG previews;
- `background/` with the 640x480 background and palette;
- `audio/` with WAV effects and MIDI track data;
- `movies/` with startup/animation frame previews and palettes.

The generated asset tree is source data for `nwrepack.py`,
`nwframe_repack.py`, and `nwupscale.py`. It is not part of the shipping build
and should not be committed.

## Related documentation

- [`docs/assets/OFFLINE_EXTRACTOR.md`](../../docs/assets/OFFLINE_EXTRACTOR.md)
- [`docs/assets/EXTRACTION.md`](../../docs/assets/EXTRACTION.md)
- [`docs/assets/REPACKING.md`](../../docs/assets/REPACKING.md)
