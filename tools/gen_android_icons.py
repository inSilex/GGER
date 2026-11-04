#!/usr/bin/env python3
"""Generate the small monochrome Android control icons.

The icons are compiled into the SDL video unit as 32x32 bit masks so the APK
does not need a separate image decoder or packaged PNG files.  Re-run this
script after changing a shape:

    python3 tools/gen_android_icons.py

The default output is src/include/NwAndroidIcons.h.  Pass --preview DIR to also
write a 256x256 PNG preview of each icon (requires Pillow).
"""

import argparse
import math
import os

W = 32
H = 32


def blank():
    return [[0] * W for _ in range(H)]


def put(g, x, y, value=1):
    if 0 <= x < W and 0 <= y < H:
        g[y][x] = value


def rect(g, x0, y0, x1, y1, value=1):
    for y in range(max(0, y0), min(H, y1 + 1)):
        for x in range(max(0, x0), min(W, x1 + 1)):
            g[y][x] = value


def frame(g, x0, y0, x1, y1, thickness=2, value=1):
    rect(g, x0, y0, x1, y0 + thickness - 1, value)
    rect(g, x0, y1 - thickness + 1, x1, y1, value)
    rect(g, x0, y0, x0 + thickness - 1, y1, value)
    rect(g, x1 - thickness + 1, y0, x1, y1, value)


def ellipse_outline(g, cx, cy, rx, ry, thickness, value=1):
    """Draw a one-bit ellipse outline using pixel centers."""
    inner_rx = max(rx - thickness, 0.1)
    inner_ry = max(ry - thickness, 0.1)
    for y in range(H):
        for x in range(W):
            dx = x + 0.5 - cx
            dy = y + 0.5 - cy
            if ((dx * dx) / (rx * rx) + (dy * dy) / (ry * ry) <= 1.0 and
                    ((dx * dx) / (inner_rx * inner_rx) +
                     (dy * dy) / (inner_ry * inner_ry)) >= 1.0):
                g[y][x] = value


def line(g, x0, y0, x1, y1, thickness=2, value=1):
    dx = x1 - x0
    dy = y1 - y0
    steps = max(abs(dx), abs(dy)) * 4 + 1
    radius = thickness / 2.0
    for i in range(steps + 1):
        x = x0 + dx * i / steps
        y = y0 + dy * i / steps
        for yy in range(math.floor(y - radius), math.ceil(y + radius) + 1):
            for xx in range(math.floor(x - radius), math.ceil(x + radius) + 1):
                if (xx - x) ** 2 + (yy - y) ** 2 <= (radius + 0.01) ** 2:
                    put(g, xx, yy, value)


def triangle(g, p0, p1, p2, value=1):
    def edge(ax, ay, bx, by, cx, cy):
        return (ax - cx) * (by - cy) - (bx - cx) * (ay - cy)

    min_x = min(p[0] for p in (p0, p1, p2))
    max_x = max(p[0] for p in (p0, p1, p2))
    min_y = min(p[1] for p in (p0, p1, p2))
    max_y = max(p[1] for p in (p0, p1, p2))
    for y in range(max(0, min_y), min(H, max_y + 1)):
        for x in range(max(0, min_x), min(W, max_x + 1)):
            b0 = edge(x, y, *p0, *p1) < 0
            b1 = edge(x, y, *p1, *p2) < 0
            b2 = edge(x, y, *p2, *p0) < 0
            if b0 == b1 == b2:
                put(g, x, y, value)


def arc(g, cx, cy, radius, thickness, start_deg, end_deg, value=1):
    steps = 720
    radius2 = thickness / 2.0
    for i in range(steps + 1):
        angle = math.radians(start_deg + (end_deg - start_deg) * i / steps)
        x = cx + radius * math.cos(angle)
        y = cy + radius * math.sin(angle)
        for yy in range(math.floor(y - radius2), math.ceil(y + radius2) + 1):
            for xx in range(math.floor(x - radius2), math.ceil(x + radius2) + 1):
                if (xx - x) ** 2 + (yy - y) ** 2 <= (radius2 + 0.01) ** 2:
                    put(g, xx, yy, value)


def exit_icon():
    g = blank()
    frame(g, 3, 4, 18, 27, 3)
    rect(g, 16, 9, 20, 22, 0)
    line(g, 14, 16, 26, 16, 3)
    triangle(g, (26, 9), (31, 16), (26, 23))
    return g


def keyboard_icon():
    g = blank()
    frame(g, 2, 8, 29, 23, 2)
    for y in (12, 16):
        for column in range(5):
            rect(g, 5 + column * 5, y, 7 + column * 5, y + 1)
    rect(g, 10, 20, 21, 21)
    return g


def lock_icon():
    g = blank()
    # Closed padlock: a U-shaped shackle joined to a solid body with a
    # keyhole cut out of the body.
    arc(g, 16, 13, 7, 3, 180, 360)
    rect(g, 9, 12, 11, 17)
    rect(g, 21, 12, 23, 17)
    rect(g, 7, 15, 25, 28)
    rect(g, 15, 18, 18, 21, 0)
    rect(g, 16, 21, 17, 24, 0)
    return g


def unlock_icon():
    g = blank()
    # Open padlock: the shackle is rotated off the body so the opening is
    # visible at a glance, while the body and keyhole match lock_icon().
    arc(g, 16, 13, 7, 3, 180, 300)
    rect(g, 9, 12, 11, 17)
    line(g, 20, 7, 20, 11, 3)
    rect(g, 7, 15, 25, 28)
    rect(g, 15, 18, 18, 21, 0)
    rect(g, 16, 21, 17, 24, 0)
    return g


def mouse_icon(right):
    g = blank()
    # Oval mouse body with the selected top button filled and the other button
    # outlined.  The central scroll wheel keeps the two controls distinct even
    # at the smallest integer scale used by the Android control bar.
    ellipse_outline(g, 15.5, 16.0, 9.0, 12.5, 2.2)

    left_button = (10, 7, 13, 13)
    right_button = (18, 7, 21, 13)
    active, inactive = (right_button, left_button) if right else (
        left_button, right_button)
    rect(g, *active)
    frame(g, *inactive, 1)

    rect(g, 15, 7, 16, 11)
    return g


def mouse_left_icon():
    return mouse_icon(False)


def mouse_right_icon():
    return mouse_icon(True)


ICONS = (
    ("exit", exit_icon),
    ("keyboard", keyboard_icon),
    ("lock", lock_icon),
    ("unlock", unlock_icon),
    ("mouse_left", mouse_left_icon),
    ("mouse_right", mouse_right_icon),
)


def write_header(path):
    lines = [
        "/* Generated by tools/gen_android_icons.py; do not edit by hand. */",
        "#ifndef NW_ANDROID_ICONS_H",
        "#define NW_ANDROID_ICONS_H",
        "",
        "#include <stdint.h>",
        "",
        "#define NW_ANDROID_ICON_SIZE 32",
        "",
    ]
    for name, builder in ICONS:
        grid = builder()
        lines.append("static const uint32_t nw_android_icon_%s[NW_ANDROID_ICON_SIZE] = {" % name)
        for row in grid:
            value = 0
            for x, bit in enumerate(row):
                if bit:
                    value |= 1 << x
            lines.append("    0x%08xu," % value)
        lines.append("};")
        lines.append("")
    lines.append("#endif /* NW_ANDROID_ICONS_H */")
    with open(path, "w", encoding="utf-8") as output:
        output.write("\n".join(lines) + "\n")


def write_previews(directory):
    try:
        from PIL import Image
    except ImportError:
        raise SystemExit("--preview requires Pillow")
    os.makedirs(directory, exist_ok=True)
    for name, builder in ICONS:
        grid = builder()
        image = Image.new("RGBA", (W, H), (16, 24, 30, 255))
        pixels = image.load()
        for y in range(H):
            for x in range(W):
                if grid[y][x]:
                    pixels[x, y] = (255, 255, 255, 255)
        image.resize((256, 256), Image.NEAREST).save(
            os.path.join(directory, "icon-%s.png" % name)
        )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        default=os.path.join("src", "include", "NwAndroidIcons.h"),
        help="generated C header path",
    )
    parser.add_argument("--preview", help="optional directory for PNG previews")
    args = parser.parse_args()
    write_header(args.output)
    if args.preview:
        write_previews(args.preview)


if __name__ == "__main__":
    main()
