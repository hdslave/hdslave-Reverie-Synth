"""Bake the glass panel artwork at the native size of Reverie's SP-16 screen."""

# /// script
# dependencies = ["pillow==13.0.0", "resvg-py==0.5.0"]
# ///
from io import BytesIO
from math import cos, radians, sin
from pathlib import Path

import resvg_py
from bake_screen_art import array
from PIL import Image, ImageColor, ImageDraw, ImageFont, ImageOps

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
OUTPUT = ROOT / "build" / "sp16_art.h"
SIZE = (780, 285)
BODY = 115
DESKTOP = ROOT / "mac-clap" / "assets"
LANCZOS = Image.Resampling.LANCZOS


def round_mask(size, radius):
    scale = 3
    hi = Image.new("L", (size[0] * scale, size[1] * scale))
    ImageDraw.Draw(hi).rounded_rectangle((0, 0, hi.width - 1, hi.height - 1), radius * scale, fill=255)
    return hi.resize(size, LANCZOS)


def gradient(size, stops):
    """A vertical gradient with explicit stops for the horizon reflected in glass."""
    strip = Image.new("RGBA", (1, size[1]))
    colors = [(at, ImageColor.getcolor(hex_color, "RGBA")) for at, hex_color in stops]
    segment = 0
    for y in range(size[1]):
        t = y / (size[1] - 1)
        while segment + 2 < len(colors) and t > colors[segment + 1][0]:
            segment += 1
        a, ca = colors[segment]
        b, cb = colors[segment + 1]
        f = min(1, max(0, (t - a) / (b - a)))
        strip.putpixel((0, y), tuple(round(v + (w - v) * f) for v, w in zip(ca, cb)))
    return strip.resize(size)


PEARL = [(0, "#faffffdf"), (0.38, "#e3fbfcca"), (0.75, "#b7dfe7db"), (1, "#e4fdf8df")]
BLUE = [(0, "#fbffff"), (0.35, "#c0e9f8"), (0.72, "#8bcadd"), (1, "#e6ffff")]
GREEN = [(0, "#fbffed"), (0.35, "#dcf5bd"), (0.72, "#a7d38d"), (1, "#efffce")]
AMBER = [(0, "#fffcdf"), (0.35, "#ffdc8b"), (0.72, "#edab37"), (1, "#ffeb95")]
OCEAN = [(0, "#87e6ff"), (0.35, "#3687b8"), (0.72, "#104c7b"), (1, "#138bb0")]
LIME = [(0, "#edffc1"), (0.35, "#bdec79"), (0.72, "#91cc47"), (1, "#e6ff9b")]


def glass(size, radius, stops=PEARL, rim="#3b879c"):
    face = Image.new("RGBA", size, rim)
    inner = gradient((size[0] - 2, size[1] - 2), stops)
    # An off-axis window reflection bends across each surface; typography sits
    # below the brightest glint on the large windows.
    reflection = Image.new("RGBA", inner.size)
    iw, ih = inner.size
    ImageDraw.Draw(reflection).ellipse(
        (-iw * 0.19, -ih * 1.65, iw * 1.15, ih * (0.27 if ih > 40 else 0.50)),
        fill=(255, 255, 255, 38 if ih > 40 else 75),
    )
    inner.alpha_composite(reflection)
    face.paste(inner, (1, 1), round_mask(inner.size, radius - 1))
    draw = ImageDraw.Draw(face)
    draw.line((radius, 1, size[0] - radius - 1, 1), fill="#ffffff")
    face.putalpha(Image.composite(face.getchannel("A"), Image.new("L", size), round_mask(size, radius)))
    return face


def inset_picture(scene, xy, source, size, radius, centering=(0.5, 0.5)):
    outer = Image.new("RGBA", size, "#2e788c")
    mask = round_mask(size, radius)
    outer.putalpha(mask)
    scene.alpha_composite(outer, xy)
    inside = ImageOps.fit(source, (size[0] - 4, size[1] - 4), LANCZOS, centering=centering)
    scene.paste(inside, (xy[0] + 2, xy[1] + 2), round_mask(inside.size, radius - 2))


def jewel(diameter, color, symbol=None):
    """Spherical enamel icons with a curved white reflection and a rim light."""
    scale = 4
    n = diameter * scale
    tint = ImageColor.getrgb(color)
    icon = Image.new("RGBA", (n, n))
    for y in range(n):
        for x in range(n):
            u, v = (2 * x + 1) / n - 1, (2 * y + 1) / n - 1
            rr = u * u + v * v
            if rr >= 1:
                continue
            z = (1 - rr) ** 0.5
            shade = 0.43 + 0.48 * z + 0.15 * v
            edge = max(0, 1 - abs(rr - 0.83) * 10) * 0.46
            reflection = max(0, 1 - ((u + 0.17) / 0.87) ** 2 - ((v + 0.56) / 0.45) ** 2) ** 0.35
            rgb = [int(min(255, c * shade + (254 - c * shade) * max(edge, reflection * 0.8))) for c in tint]
            icon.putpixel((x, y), (*rgb, 255))
    if symbol:
        d = ImageDraw.Draw(icon)

        def points(coords):
            return [(round(x * n), round(y * n)) for x, y in coords]

        def line(coords):
            d.line(
                points(coords),
                fill="#ffffff",
                width=max(2, round(n * 0.075)),
                joint="curve",
            )

        if symbol == "keys":
            for x in [0.32, 0.5, 0.68]:
                line([(x, 0.36), (x, 0.69)])
        elif symbol == "orbit":
            d.arc(
                (n * 0.24, n * 0.34, n * 0.76, n * 0.67),
                0,
                360,
                fill="white",
                width=round(n * 0.065),
            )
            d.ellipse((n * 0.50, n * 0.24, n * 0.67, n * 0.41), fill="white")
        elif symbol == "wave":
            line([(0.18 + i * 0.64 / 16, 0.52 - 0.18 * sin(i * 6.283 / 16)) for i in range(17)])
        elif symbol == "filter":
            line([(0.2, 0.43), (0.48, 0.43), (0.60, 0.36), (0.69, 0.59), (0.80, 0.68)])
        elif symbol == "env":
            line([(0.19, 0.68), (0.39, 0.28), (0.51, 0.49), (0.67, 0.49), (0.80, 0.68)])
        elif symbol == "echo":
            for j in range(3):
                line(
                    [
                        (0.30 + 0.20 * j, 0.31 + 0.07 * j),
                        (0.30 + 0.20 * j, 0.70 - 0.07 * j),
                    ]
                )
    return icon.resize((diameter, diameter), LANCZOS)


def scene():
    water = ImageOps.fit(Image.open(DESKTOP / "aqua-glass.jpg"), SIZE, LANCZOS).convert("RGBA")
    water = Image.blend(water, Image.new("RGBA", SIZE, "#d7f4f5"), 0.19)
    photo = Image.open(DESKTOP / "desktop-horizon.jpg").convert("RGBA")
    # The wordmark has clear sky behind it. At the right the whole bubble,
    # clouds and green island fit between the logo baseline and the toolbar.
    sky = photo.resize((320, 104), LANCZOS).crop((0, 17, 320, 104))
    base = Image.new("RGBA", (780, 87))
    base.paste(sky.crop((0, 0, 1, 87)).resize((460, 87)), (0, 0))
    base.paste(sky, (460, 0))
    shade = Image.new("RGBA", (780, 1))
    shade.putdata([(0, 44, 91, round(105 * max(0, 1 - x / 460))) for x in range(780)])
    base.alpha_composite(shade.resize(base.size))
    # A short staff dissolves into the sky. Its path and notes are original.
    scale = 3
    score = Image.new("RGBA", (780 * scale, 87 * scale))
    d = ImageDraw.Draw(score)

    def staff_y(x):
        t = (x - 190) / 310
        return 42 - 41 * t + 9 * sin(5 * t)

    for k in range(5):
        for x in range(195, 486):
            opacity = int(80 * min(1, (x - 195) / 65, (486 - x) / 70))
            d.line(
                (
                    x * scale,
                    (staff_y(x) + k * 2.2) * scale,
                    (x + 1) * scale,
                    (staff_y(x + 1) + k * 2.2) * scale,
                ),
                fill=(198, 249, 255, opacity),
                width=2,
            )
    for x, k in ((257, 0), (274, 3), (418, 1), (442, 3)):
        y = staff_y(x) + k * 2.2
        d.ellipse(
            ((x - 2) * scale, (y - 1) * scale, (x + 2) * scale, (y + 1.6) * scale),
            fill=(221, 255, 255, 136),
        )
        d.line(
            ((x + 1.7) * scale, y * scale, (x + 1.7) * scale, (y - 7) * scale),
            fill=(221, 255, 255, 112),
            width=2,
        )
    base.alpha_composite(score.resize(base.size, LANCZOS))
    water.alpha_composite(base, (0, 0))
    # The toolbar sits where the sky meets the glass body.
    bar = glass((768, 31), 15, PEARL, "#48869c")
    water.alpha_composite(bar, (6, 55))
    mist = gradient((780, 29), [(0, "#daf3f2df"), (1, "#e9ffff00")])
    water.alpha_composite(mist, (0, 86))
    mark = Image.open(BytesIO(resvg_py.svg_to_bytes(svg_path=str(ASSETS / "wordmark.svg"), zoom=5)))
    alpha = mark.getchannel("A").resize((178, 31), LANCZOS)
    water.paste((1, 58, 100, 255), (17, 4), alpha)
    water.paste((251, 255, 255, 255), (16, 3), alpha)
    return water


ICONS = [
    ("keys", "#0082c8"),
    ("orbit", "#009d9c"),
    ("wave", "#0677bf"),
    ("filter", "#349866"),
    ("env", "#2677bd"),
    ("echo", "#058a9e"),
]


def tabs(header, active):
    for i, (symbol, tint) in enumerate(ICONS):
        x = 12 + 127 * i
        if active:
            header.alpha_composite(glass((115, 26), 13, LIME, "#4d8b51"), (x + 3, 58))
        header.alpha_composite(jewel(22, tint, symbol), (x + 13, 60))


def pad_tile(stops, tint):
    tile = glass((112, 30), 13, stops)
    tile.alpha_composite(jewel(24, tint), (3, 3))
    return tile


def chord_body(base):
    im = base.copy()
    for row in range(4):
        tile = pad_tile(GREEN, "#48992f") if row < 2 else pad_tile(BLUE, "#087fb7")
        for col in range(4):
            im.alpha_composite(tile, (15 + 121 * col, 123 + 37 * row))
    size = (265, 146)
    pane = ImageOps.fit(
        Image.open(DESKTOP / "desktop-horizon.jpg"),
        size,
        LANCZOS,
        centering=(0.36, 0.50),
    ).convert("RGBA")
    pane.alpha_composite(
        gradient(
            size,
            [
                (0, "#00375994"),
                (0.51, "#003d5850"),
                (0.73, "#014566dc"),
                (1, "#013c5bed"),
            ],
        )
    )
    inset_picture(im, (502, 122), pane, size, 19)
    return im


def motion_body(base):
    im = base.copy()
    globe = motion_lens().convert("RGBA")
    globe.putalpha(round_mask(globe.size, 20))
    im.alpha_composite(globe, (13, 121))
    im.alpha_composite(glass((588, 149), 18), (178, 121))
    draw = ImageDraw.Draw(im)
    for row in range(4):
        y = 122 + 37 * row
        if row:
            draw.line((191, y, 750, y), fill="#a5cdd3")
            draw.line((192, y + 1, 749, y + 1), fill="#f3ffff")
        im.alpha_composite(jewel(24, "#0588b9" if row < 2 else "#329a63"), (188, y + 6))
    return im


def graph_body(base):
    im = base.copy()
    sky = Image.open(DESKTOP / "aqua-glass.jpg")
    size = (372, 126)
    for i, x in enumerate((13, 394)):
        face = ImageOps.fit(sky if i == 0 else ImageOps.mirror(sky), size, LANCZOS).convert("RGBA")
        face.alpha_composite(gradient(size, [(0, "#143e5de8"), (0.56, "#00415de7"), (1, "#002c4bcd")]))
        d = ImageDraw.Draw(face)
        for n in range(1, 12):
            gx = 3 + (size[0] - 6) * n // 12
            d.line((gx, 8, gx, 11), fill="#8dbac8")
            if n % 2 == 0:
                for gy in range(15, size[1] - 8, 4):
                    d.point((gx, gy), fill="#527c90")
        inset_picture(im, (x, 144), face, size, 15)
    return im


def fx_body(base):
    im = base.copy()
    fish = Image.open(ASSETS / "aquarium.jpg")
    inset_picture(im, (13, 121), fish, (204, 148), 23, centering=(0.5, 0.26))
    im.alpha_composite(glass((530, 147), 20), (234, 122))
    d = ImageDraw.Draw(im)
    for i, tint in enumerate(("#087eaf", "#148d92", "#458d57")):
        center = (322 + 174 * i, 195)
        x, y = center
        if i:
            d.line((x - 87, 140, x - 87, 250), fill="#a5cad2")
        im.alpha_composite(jewel(68, tint), (x - 34, y - 34))
        for k in range(17):
            a = radians(135 + 270 * k / 16)
            d.line(
                (x + 37 * cos(a), y + 37 * sin(a), x + 40 * cos(a), y + 40 * sin(a)),
                fill="#608e9f",
                width=1,
            )
    return im


def sound_body(base):
    im = base.copy()
    im.alpha_composite(glass((754, 146), 20), (13, 121))
    d = ImageDraw.Draw(im)
    for col in range(5):
        x = 13 + col * 152
        if col:
            d.line((x - 3, 134, x - 3, 253), fill="#9fc7d1")
        for row in range(1, 4):
            y = 122 + row * 35
            d.line((x + 9, y, x + 137, y), fill="#aed4d7")
    return im


def motion_lens():
    photo = Image.open(ASSETS / "sp16-motion-glass.jpg").convert("RGB")
    # Use the desktop bake's measured rim and preserve the bezel independently.
    # At this size the rim is a 118 px circle centered on (75, 67).
    source_x = (0, 118, 1121, photo.width)
    source_y = (0, 46, 1060, photo.height)
    dest_x = (0, 16, 134, 150)
    dest_y = (0, 8, 126, 150)
    tile = Image.new("RGB", (150, 150))
    for row in range(3):
        for col in range(3):
            box = (source_x[col], source_y[row], source_x[col + 1], source_y[row + 1])
            size = (dest_x[col + 1] - dest_x[col], dest_y[row + 1] - dest_y[row])
            tile.paste(
                photo.resize(size, Image.Resampling.LANCZOS, box=box),
                (dest_x[col], dest_y[row]),
            )
    return tile


def pixels(out, name, im):
    # Opaque plates copy straight into the framebuffer. Only the small control
    # sprites use per-pixel alpha at runtime.
    values = [
        f"0x{(a << 24) | (r << 16) | (g << 8) | b:08x}"
        for r, g, b, a in im.convert("RGBA").get_flattened_data()
    ]
    array(out, "uint32_t", name, values)


base = scene()
header = base.copy()
tabs(header, False)
OUTPUT.parent.mkdir(exist_ok=True)
with OUTPUT.open("w") as out:
    out.write(
        "// Generated by tools/bake_sp16_art.py.\n#pragma once\n#include <cstdint>\nnamespace reverie::sp16Art {\n"
    )
    pixels(out, "header", header.crop((0, 0, 780, BODY)).convert("RGB"))
    active = base.copy()
    tabs(active, True)
    pixels(out, "selectedTabs", active.crop((0, 56, 780, 86)).convert("RGB"))
    for name, paint in [
        ("chords", chord_body),
        ("motion", motion_body),
        ("graphs", graph_body),
        ("fx", fx_body),
        ("sounds", sound_body),
    ]:
        pixels(out, name, paint(base).crop((0, BODY, 780, 285)).convert("RGB"))
    pixels(out, "padDown", pad_tile(AMBER, "#c36b0d"))
    pixels(out, "soundButton", glass((90, 25), 12, OCEAN))
    pixels(out, "soundFocus", glass((143, 29), 14, LIME))
    pixels(out, "bankFocus", glass((100, 21), 10, BLUE))
    for size in (11, 14, 18, 23):
        font = ImageFont.truetype(ASSETS / "Barlow-SemiBold.ttf", size)
        widths, offsets, pixels = [], [], []
        for ch in map(chr, range(32, 127)):
            width = max(1, round(font.getlength(ch)), font.getbbox(ch)[2])
            mask = Image.new("L", (width, size + 3))
            ImageDraw.Draw(mask).text((0, 0), ch, font=font, fill=255)
            offsets.append(len(pixels))
            widths.append(width)
            pixels.extend(mask.get_flattened_data())
        array(out, "uint8_t", f"width{size}", list(map(str, widths)))
        array(out, "uint16_t", f"offset{size}", list(map(str, offsets)))
        array(out, "uint8_t", f"mask{size}", list(map(str, pixels)))
    out.write("}\n")
