#!/usr/bin/env python3
"""Generate the high-resolution mobile control sprites.

The on-screen controls on Android and iOS are drawn from one RGBA atlas rather
than from rectangles and a 5x7 bitmap font.  The atlas holds a compact
(icon-only) and a full (icon plus label) version of every control state, the
translucent overlay used while a button is pressed, and the chrome for the
pull-down card (panel, tab, and chevrons).  The PNG is compiled into the SDL
video unit, so neither platform needs a packaged asset or an extra image
decoder at build time; SDL's built-in PNG loader decodes it once at runtime.

Run after changing a shape or the typography:

    python3 tools/gen_mobile_controls.py

The default output is src/include/NwMobileControls.h.  Pass --preview PATH to
also write a contact sheet for inspection (requires Pillow).
"""

import argparse
import io
import os

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:  # pragma: no cover - build-time helper
    raise SystemExit("this generator requires Pillow")

TILE = 512
DESIGN = 256  # all geometry below is authored in this square
SS = 4 * TILE // DESIGN  # supersampling factor at the final tile size
CANVAS = DESIGN * SS

BG = (24, 43, 50)
BORDER = (68, 194, 168)
CONTENT = (234, 244, 240)
LABEL = (214, 234, 227)

ACTIVE = (66, 189, 165)
ACTIVE_BORDER = (184, 246, 226)
ACTIVE_CONTENT = (11, 27, 33)
ACTIVE_LABEL = (22, 48, 55)

CARD_BORDER = (68, 194, 168, 235)
CARD_FILL = (15, 30, 37, 225)
HANDLE_BORDER = (68, 194, 168, 215)
HANDLE_FILL = (15, 30, 37, 215)
HANDLE_HOT_BORDER = (68, 194, 168, 245)
HANDLE_HOT_FILL = (15, 30, 37, 245)
CHEVRON = (150, 238, 212)

PANEL_MARGIN = 4
# The card and tab use the same rounded language as the buttons.  The radius is
# proportional to the panel's own corner box rather than to its stretched size,
# so the chrome keeps the same look on a phone tab and an iPad card.
PANEL_RADIUS = 56
PANEL_BORDER = 7
# Source pixels covered by each corner of the 9-grid.  The runtime scales the
# corners by NW_MOBILE_PANEL_SCALE, so the panel keeps its proportions while
# the middle stretches to the card or tab size.
PANEL_INSET = (PANEL_MARGIN + PANEL_RADIUS + PANEL_BORDER) * TILE // DESIGN
PANEL_SCALE = 0.2

SPRITES = (
    # name, icon, label, active
    ("exit", "exit", "EXIT", False),
    ("keys", "keys", "KEYS", False),
    ("lock", "unlock", "LOCK", False),
    ("locked", "lock", "LOCKED", True),
    ("left", "mouse_left", "LEFT", False),
    ("right", "mouse_right", "RIGHT", False),
)
COUNT = len(SPRITES)
FULL_OFFSET = COUNT
PRESSED = COUNT * 2
CARD = PRESSED + 1
HANDLE = CARD + 1
HANDLE_HOT = HANDLE + 1
CHEVRON_UP = HANDLE_HOT + 1
CHEVRON_DOWN = CHEVRON_UP + 1
TILES = CHEVRON_DOWN + 1


def s(value):
    return int(round(value * SS))


def chrome(active):
    """Rounded button body in a flat colour with a light border."""
    image = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    mask = Image.new("L", (CANVAS, CANVAS), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        [s(3), s(3), s(DESIGN - 3), s(DESIGN - 3)],
        radius=s(52),
        fill=255,
    )
    image.paste((ACTIVE if active else BG) + (255,), (0, 0), mask)

    draw = ImageDraw.Draw(image)
    border = ACTIVE_BORDER if active else BORDER
    draw.rounded_rectangle(
        [s(4), s(4), s(DESIGN - 4), s(DESIGN - 4)],
        radius=s(51),
        outline=border + (255,),
        width=s(3),
    )
    return image


def _icon_box(full):
    if full:
        size = 100
        return (DESIGN - size) / 2, 46, size
    size = 122
    return (DESIGN - size) / 2, (DESIGN - size) / 2, size


def draw_exit(draw, box, color, width):
    x0, y0, size = box

    def p(x, y):
        return (s(x0 + x * size / 100.0), s(y0 + y * size / 100.0))

    draw.rounded_rectangle(
        [p(6, 10), p(50, 90)],
        radius=s(size * 0.05),
        outline=color,
        width=s(width),
    )
    draw.line([p(38, 50), p(90, 50)], fill=color, width=s(width))
    draw.polygon([p(70, 30), p(93, 50), p(70, 70)], fill=color)


def draw_keys(draw, box, color, width):
    x0, y0, size = box

    def p(x, y):
        return (s(x0 + x * size / 100.0), s(y0 + y * size / 100.0))

    draw.rounded_rectangle(
        [p(4, 26), p(96, 74)],
        radius=s(size * 0.09),
        outline=color,
        width=s(width),
    )
    for row in range(2):
        for column in range(4):
            draw.rounded_rectangle(
                [p(14 + column * 19, 37 + row * 12),
                 p(26 + column * 19, 44 + row * 12)],
                radius=s(size * 0.015),
                fill=color,
            )
    draw.rounded_rectangle(
        [p(30, 60), p(70, 66)],
        radius=s(size * 0.015),
        fill=color,
    )


def draw_lock(draw, box, color, hole, width, open_shackle):
    x0, y0, size = box

    def p(x, y):
        return (s(x0 + x * size / 100.0), s(y0 + y * size / 100.0))

    body_top = 48
    draw.rounded_rectangle(
        [p(20, body_top), p(80, 90)],
        radius=s(size * 0.10),
        fill=color,
    )
    if open_shackle:
        # Shackle lifted on the right, so the opening stays visible.
        draw.arc(
            [p(30, 16), p(76, 62)],
            start=180,
            end=330,
            fill=color,
            width=s(width),
        )
        draw.line([p(72, 34), p(72, 46)], fill=color, width=s(width))
    else:
        draw.arc(
            [p(28, 18), p(72, 62)],
            start=180,
            end=360,
            fill=color,
            width=s(width),
        )
    # Keyhole punched in the body colour.
    draw.ellipse([p(45, 56), p(55, 66)], fill=hole)
    draw.rounded_rectangle(
        [p(47, 64), p(53, 76)],
        radius=s(size * 0.01),
        fill=hole,
    )


def _mouse_mask(box):
    x0, y0, size = box
    mask = Image.new("L", (CANVAS, CANVAS), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        [
            s(x0 + size * 0.24),
            s(y0 + size * 0.08),
            s(x0 + size * 0.76),
            s(y0 + size * 0.94),
        ],
        radius=s(size * 0.26),
        fill=255,
    )
    return mask


def draw_mouse(image, box, color, hole, width, right):
    x0, y0, size = box

    def p(x, y):
        return (s(x0 + x * size / 100.0), s(y0 + y * size / 100.0))

    body = _mouse_mask(box)

    def clipped(source):
        return Image.frombytes(
            "L", (CANVAS, CANVAS),
            bytes(min(a, b) for a, b in
                  zip(source.tobytes(), body.tobytes())),
        )

    # Body outline first, so the filled button can merge into it.
    outline = Image.new("L", (CANVAS, CANVAS), 0)
    ImageDraw.Draw(outline).rounded_rectangle(
        [p(24, 8), p(76, 94)],
        radius=s(size * 0.26),
        outline=255,
        width=s(width),
    )
    image.paste(color + (255,), (0, 0), clipped(outline))

    # Solid button on the selected side.
    active = Image.new("L", (CANVAS, CANVAS), 0)
    adraw = ImageDraw.Draw(active)
    if right:
        adraw.rectangle([p(50, 5), p(90, 44)], fill=255)
    else:
        adraw.rectangle([p(10, 5), p(50, 44)], fill=255)
    image.paste(color + (255,), (0, 0), clipped(active))

    # Dark dividers keep the split readable across the solid half.
    dividers = Image.new("L", (CANVAS, CANVAS), 0)
    ddraw = ImageDraw.Draw(dividers)
    ddraw.line([p(24, 45), p(76, 45)], fill=255, width=s(width * 0.8))
    ddraw.line([p(50, 9), p(50, 45)], fill=255, width=s(width * 0.8))
    image.paste(hole + (255,), (0, 0), clipped(dividers))


def render(name, icon_name, label, active, full):
    image = chrome(active)
    draw = ImageDraw.Draw(image)
    color = ACTIVE_CONTENT if active else CONTENT
    label_color = ACTIVE_LABEL if active else LABEL
    hole = ACTIVE if active else BG
    box = _icon_box(full)

    if icon_name == "exit":
        draw_exit(draw, box, color, 8)
    elif icon_name == "keys":
        draw_keys(draw, box, color, 7)
    elif icon_name in ("lock", "unlock"):
        draw_lock(draw, box, color, hole, 8, icon_name == "unlock")
    elif icon_name in ("mouse_left", "mouse_right"):
        draw_mouse(image, box, color, hole, 7,
                   icon_name == "mouse_right")

    if full:
        font = ImageFont.load_default(size=s(32))
        draw.text(
            (s(DESIGN / 2), s(195)),
            label,
            font=font,
            fill=label_color + (255,),
            anchor="mm",
            stroke_width=s(0.5),
            stroke_fill=label_color + (255,),
        )
    return image.resize((TILE, TILE), Image.LANCZOS)


def pressed_overlay():
    image = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    ImageDraw.Draw(image).rounded_rectangle(
        [s(3), s(3), s(DESIGN - 3), s(DESIGN - 3)],
        radius=s(52),
        fill=(255, 255, 255, 40),
    )
    return image.resize((TILE, TILE), Image.LANCZOS)


def panel(border, fill):
    """A rounded panel used by the 9-grid card and tab chrome."""
    image = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle(
        [s(PANEL_MARGIN), s(PANEL_MARGIN),
         s(DESIGN - PANEL_MARGIN), s(DESIGN - PANEL_MARGIN)],
        radius=s(PANEL_RADIUS),
        fill=border,
    )
    inner = PANEL_MARGIN + PANEL_BORDER
    draw.rounded_rectangle(
        [s(inner), s(inner), s(DESIGN - inner), s(DESIGN - inner)],
        radius=s(PANEL_RADIUS - PANEL_BORDER),
        fill=fill,
    )
    return image.resize((TILE, TILE), Image.LANCZOS)


def chevron(up):
    image = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    if up:
        points = [(80, 160), (176, 160), (128, 104)]
    else:
        points = [(80, 96), (176, 96), (128, 152)]
    draw.polygon([(s(x), s(y)) for x, y in points],
                 fill=CHEVRON + (255,))
    return image.resize((TILE, TILE), Image.LANCZOS)


def atlas():
    sheet = Image.new("RGBA", (TILE * TILES, TILE), (0, 0, 0, 0))
    for index, (name, icon_name, label, active) in enumerate(SPRITES):
        sheet.paste(render(name, icon_name, label, active, False),
                    (index * TILE, 0))
        sheet.paste(render(name, icon_name, label, active, True),
                    ((index + COUNT) * TILE, 0))
    sheet.paste(pressed_overlay(), (PRESSED * TILE, 0))
    sheet.paste(panel(CARD_BORDER, CARD_FILL), (CARD * TILE, 0))
    sheet.paste(panel(HANDLE_BORDER, HANDLE_FILL), (HANDLE * TILE, 0))
    sheet.paste(panel(HANDLE_HOT_BORDER, HANDLE_HOT_FILL),
                (HANDLE_HOT * TILE, 0))
    sheet.paste(chevron(True), (CHEVRON_UP * TILE, 0))
    sheet.paste(chevron(False), (CHEVRON_DOWN * TILE, 0))
    return sheet


def write_header(path):
    sheet = atlas()
    buffer = io.BytesIO()
    sheet.save(buffer, "PNG", optimize=True)
    payload = buffer.getvalue()

    lines = [
        "/* Generated by tools/gen_mobile_controls.py; do not edit by hand. */",
        "#ifndef NW_MOBILE_CONTROLS_H",
        "#define NW_MOBILE_CONTROLS_H",
        "",
        "#define NW_MOBILE_ATLAS_TILE %d" % TILE,
        "#define NW_MOBILE_ATLAS_TILES %d" % TILES,
        "#define NW_MOBILE_SPRITE_FULL_OFFSET %d" % FULL_OFFSET,
        "#define NW_MOBILE_SPRITE_PRESSED %d" % PRESSED,
        "#define NW_MOBILE_PANEL_INSET %d" % PANEL_INSET,
        "#define NW_MOBILE_PANEL_SCALE %.2ff" % PANEL_SCALE,
        "",
        "enum {",
    ]
    for index, (name, _icon, _label, _active) in enumerate(SPRITES):
        lines.append("    NW_MOBILE_SPRITE_%s = %d," % (name.upper(), index))
    lines.append("    NW_MOBILE_SPRITE_CARD = %d," % CARD)
    lines.append("    NW_MOBILE_SPRITE_HANDLE = %d," % HANDLE)
    lines.append("    NW_MOBILE_SPRITE_HANDLE_HOT = %d," % HANDLE_HOT)
    lines.append("    NW_MOBILE_SPRITE_CHEVRON_UP = %d," % CHEVRON_UP)
    lines.append("    NW_MOBILE_SPRITE_CHEVRON_DOWN = %d," % CHEVRON_DOWN)
    lines.append("};")
    lines.append("")
    lines.append(
        "static const unsigned char nw_mobile_controls_png[%d] = {" % len(payload)
    )
    for offset in range(0, len(payload), 16):
        chunk = payload[offset:offset + 16]
        lines.append("    " + ", ".join("0x%02x" % byte for byte in chunk) + ",")
    lines.append("};")
    lines.append("")
    lines.append("#endif /* NW_MOBILE_CONTROLS_H */")
    with open(path, "w", encoding="utf-8") as output:
        output.write("\n".join(lines) + "\n")
    return len(payload)


def write_preview(path):
    sheet = atlas()
    columns = 4
    rows = (TILES + columns - 1) // columns
    margin = 12
    cell = TILE // 2
    preview = Image.new(
        "RGBA",
        (columns * (cell + margin) + margin,
         rows * (cell + margin + 18) + margin),
        (10, 16, 20, 255),
    )
    draw = ImageDraw.Draw(preview)
    font = ImageFont.load_default(size=14)
    for index in range(TILES):
        column = index % columns
        row = index // columns
        x = margin + column * (cell + margin)
        y = margin + row * (cell + margin + 18)
        tile = sheet.crop((index * TILE, 0, (index + 1) * TILE, TILE))
        preview.paste(tile.resize((cell, cell), Image.LANCZOS), (x, y))
        draw.text((x, y + cell + 2), str(index), font=font,
                  fill=(230, 240, 236, 255))
    preview.convert("RGB").save(path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        default=os.path.join("src", "include", "NwMobileControls.h"),
        help="generated C header path",
    )
    parser.add_argument("--preview", help="optional contact-sheet PNG path")
    args = parser.parse_args()
    size = write_header(args.output)
    if args.preview:
        write_preview(args.preview)
    print("wrote %s (%d PNG bytes)" % (args.output, size))


if __name__ == "__main__":
    main()
