#!/usr/bin/env python3
"""Generate every checked-in NetWalk application icon from one artwork source.

Replace assets/icons/icon.png (or assets/icon.png), then run:

    python3 tools/gen_app_icons.py

Outputs:
  platforms/android/app/src/main/res/mipmap-*/ic_launcher.png
  platforms/android/app/src/main/res/mipmap-*/ic_launcher_round.png
  platforms/android/app/src/main/res/drawable-*/ic_launcher_foreground.png
  platforms/android/app/src/main/res/mipmap-anydpi-v26/ic_launcher*.xml
  assets/icons/netwalk.ico
  assets/icons/netwalk-512.png
  src/include/NwAppIcon.h
"""

from __future__ import annotations

from pathlib import Path

from netwalk_icon import (
    CONTENT_SCALE,
    source_path,
    write_c_header,
    write_ico,
    write_png,
)

ROOT = Path(__file__).resolve().parents[1]
RES = ROOT / "platforms/android/app/src/main/res"
ICONS = ROOT / "assets/icons"

DENSITIES = {
    "mdpi": 1.0,
    "hdpi": 1.5,
    "xhdpi": 2.0,
    "xxhdpi": 3.0,
    "xxxhdpi": 4.0,
}

# Android 8+ masks the foreground into an arbitrary shape.  Fit the artwork
# inside the 66dp safe-zone circle (not merely its 72dp bounding box) so a
# circular or squircle launcher mask never clips the monitor corners.
ANDROID_ADAPTIVE_CONTENT_SCALE = 0.44
# Legacy 48dp launcher icons keep the customary 4dp keyline padding.  The
# round variant fits inside the 48dp circle without clipping its corners.
ANDROID_LEGACY_CONTENT_SCALE = 0.82
ANDROID_ROUND_CONTENT_SCALE = 0.70

ADAPTIVE_XML = """<?xml version="1.0" encoding="utf-8"?>
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
    <background android:drawable="@color/ic_launcher_background" />
    <foreground android:drawable="@drawable/ic_launcher_foreground" />
</adaptive-icon>
"""


def generate_android() -> None:
    for density, factor in DENSITIES.items():
        legacy = int(round(48 * factor))
        foreground = int(round(108 * factor))
        write_png(
            RES / f"mipmap-{density}/ic_launcher.png", legacy,
            content_scale=ANDROID_LEGACY_CONTENT_SCALE)
        write_png(
            RES / f"mipmap-{density}/ic_launcher_round.png", legacy,
            content_scale=ANDROID_ROUND_CONTENT_SCALE, round_mask=True)
        write_png(
            RES / f"drawable-{density}/ic_launcher_foreground.png",
            foreground, content_scale=ANDROID_ADAPTIVE_CONTENT_SCALE)

    adaptive = RES / "mipmap-anydpi-v26"
    adaptive.mkdir(parents=True, exist_ok=True)
    (adaptive / "ic_launcher.xml").write_text(ADAPTIVE_XML, encoding="ascii")
    (adaptive / "ic_launcher_round.xml").write_text(ADAPTIVE_XML,
                                                     encoding="ascii")


def main() -> int:
    generate_android()
    write_ico(ICONS / "netwalk.ico")
    write_png(ICONS / "netwalk-512.png", 512, content_scale=CONTENT_SCALE)
    write_c_header(ROOT / "src/include/NwAppIcon.h")
    print(f"Icon source:            {source_path()}")
    print(f"Android launcher icons: {RES}")
    print(f"Desktop/PS5 icon PNG:   {ICONS / 'netwalk-512.png'}")
    print(f"Windows icon:           {ICONS / 'netwalk.ico'}")
    print(f"SDL window icon header: {ROOT / 'src/include/NwAppIcon.h'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
