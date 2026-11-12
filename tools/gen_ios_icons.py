#!/usr/bin/env python3
"""Generate plain PNG app icons for a sideloaded iOS bundle.

App Store submissions normally require an asset catalog compiled by Xcode's
actool.  Sideloaded development builds accept plain PNGs referenced from
Info.plist, which is what this tool produces.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

# (base name, pixel size)
ICONS = (
    ("AppIcon60x60@2x", 120),
    ("AppIcon60x60@3x", 180),
    ("AppIcon76x76@2x", 152),
    ("AppIcon83.5x83.5@2x", 167),
)

# iOS masks the icon with a rounded square, so artwork that reaches the edge
# loses its corners.  Keep the artwork a little inside the canvas so it reads
# as a padded icon on the home screen.
CONTENT_SCALE = 0.82


def padded_icon(image, size):
    """Scale the artwork down and center it on the image's corner color."""
    from PIL import Image

    inner = int(round(size * CONTENT_SCALE))
    if inner < 1:
        inner = 1
    artwork = image.resize((inner, inner), Image.LANCZOS)
    background = image.getpixel((0, 0))
    canvas = Image.new("RGB", (size, size), background)
    offset = (size - inner) // 2
    canvas.paste(artwork, (offset, offset))
    return canvas


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args(argv)

    try:
        from PIL import Image
    except ImportError:
        print("gen_ios_icons: Pillow is required", file=sys.stderr)
        return 1

    if not args.source.is_file():
        print(f"gen_ios_icons: source icon not found: {args.source}", file=sys.stderr)
        return 1

    args.out.mkdir(parents=True, exist_ok=True)
    with Image.open(args.source) as image:
        image = image.convert("RGBA")
        # Apple rejects app icons with an alpha channel.
        background = Image.new("RGBA", image.size, (255, 255, 255, 255))
        background.alpha_composite(image)
        rgb = background.convert("RGB")
        for name, size in ICONS:
            icon = padded_icon(rgb, size)
            icon.save(args.out / f"{name}.png", "PNG", optimize=True)

    for name, _ in ICONS:
        print(args.out / f"{name}.png")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
