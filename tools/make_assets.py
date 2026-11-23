"""Generate the original indexed pixel art and short sound effects."""
import json
import math
from pathlib import Path
import random
import struct
import wave

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
GRAPHICS = ROOT / "graphics"
AUDIO = ROOT / "audio"

SCENE = [
    "#ff00ff", "#101923", "#142330", "#192b38", "#213643", "#2b4852",
    "#365762", "#53747a", "#9cb7ae", "#e8ead2", "#f9ca70", "#d3904c",
    "#93613c", "#563d35", "#302a2c", "#090e16", "#1e4137", "#235144",
    "#29624e", "#2d6d56", "#31735a", "#34785d", "#398166", "#489476",
    "#61a98b", "#7c5036", "#af7446", "#c78d54", "#78472e", "#0a1519",
    "#152a29", "#234033", "#3e7061", "#a56b42", "#8baf92", "#d9a858",
    "#111e28", "#6fa08e", "#255b48", "#1e4a3d", "#151e2a",
    "#835138", "#b17a50", "#482f29", "#744630", "#b98d59", "#edbb78",
    "#0b111a", "#121c29", "#202d3d", "#2f4255", "#465d6e", "#698490",
    "#9aafad", "#b6bba8", "#dedcc3", "#f6f1d8",
]
CUE = [
    "#ff00ff", "#27201d", "#50332a", "#865538", "#bd8653", "#edd29d",
    "#ae7b4c", "#fff1d2", "#468b7b", "#82c3ad",
]
INK = [
    "#ff00ff", "#10242a", "#223947", "#48777e", "#73b49f", "#b8dfd0", "#f5f0d8",
    "#c4844c", "#ffd27d", "#fff0bb", "#ae5f50", "#f7a88e",
    "#344957", "#60838a", "#62896a", "#1a5c54",
]
BALLS = [
    ("#85908e", "#c7d8cf", "#f6f3dd"),
    ("#9c561f", "#e8a230", "#ffcf59"), ("#1c4983", "#3886cd", "#7ababf"),
    ("#8d303a", "#d24d4b", "#f88370"), ("#563775", "#8b5fa2", "#c997d0"),
    ("#9b4d2d", "#e07b43", "#f5ab63"), ("#23574a", "#398170", "#76b896"),
    ("#692f3d", "#974451", "#c76666"), ("#080a0e", "#171a20", "#323841"),
]

FONT = {
    "A": ("010","101","111","101","101"), "B": ("110","101","110","101","110"),
    "C": ("011","100","100","100","011"), "D": ("110","101","101","101","110"),
    "E": ("111","100","110","100","111"), "F": ("111","100","110","100","100"),
    "G": ("011","100","101","101","011"), "H": ("101","101","111","101","101"),
    "I": ("1","1","1","1","1"), "J": ("001","001","001","101","010"),
    "K": ("101","110","100","110","101"), "L": ("100","100","100","100","111"),
    "M": ("10001","11011","10101","10001","10001"), "N": ("1001","1101","1011","1001","1001"),
    "O": ("010","101","101","101","010"), "P": ("110","101","110","100","100"),
    "Q": ("010","101","101","110","011"), "R": ("110","101","110","101","101"),
    "S": ("011","100","010","001","110"), "T": ("111","010","010","010","010"),
    "U": ("101","101","101","101","111"), "V": ("101","101","101","101","010"),
    "W": ("10001","10001","10101","11011","10001"), "X": ("101","101","010","101","101"),
    "Y": ("101","101","010","010","010"), "Z": ("111","001","010","100","111"),
    "0": ("111","101","101","101","111"), "1": ("010","110","010","010","111"),
    "2": ("110","001","010","100","111"), "3": ("110","001","010","001","110"),
    "4": ("101","101","111","001","001"), "5": ("111","100","110","001","110"),
    "6": ("011","100","111","101","111"), "7": ("111","001","010","010","010"),
    "8": ("111","101","111","101","111"), "9": ("111","101","111","001","110"),
    "+": ("000","010","111","010","000"), "-": ("000","000","111","000","000"),
    "/": ("001","001","010","100","100"), ">": ("100","010","001","010","100"),
    ".": ("0","0","0","0","1"), " ": ("00",) * 5,
}


def indexed(size, colors, base=0):
    image = Image.new("P", size, base)
    palette = [int(color[i:i+2], 16) for color in colors for i in (1, 3, 5)]
    image.putpalette(palette + [0] * (768 - len(palette)))
    return image


def save(image, name, kind, **options):
    image.save(GRAPHICS / f"{name}.bmp")
    data = {"type": kind, **options}
    (GRAPHICS / f"{name}.json").write_text(json.dumps(data) + "\n")


def stamp(draw, label, x, y, color, scale=1, gap=1, centered=True):
    width = sum(len(FONT[ch][0]) * scale + gap for ch in label) - gap
    if centered:
        x -= width // 2
    for char in label:
        for row, line in enumerate(FONT[char]):
            for column, bit in enumerate(line):
                if bit == "1":
                    xx, yy = x + column * scale, y + row * scale
                    draw.rectangle((xx, yy, xx + scale - 1, yy + scale - 1), fill=color)
        x += len(FONT[char][0]) * scale + gap


def shell():
    image = indexed((256, 256), SCENE, 1)
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 0, 239, 159), fill=2)
    for y in range(2, 159, 4):
        for x in range(2 + (y % 8), 238, 8):
            draw.point((x, y), fill=3)
    draw.rectangle((0, 0, 239, 1), fill=15)
    draw.line((0, 158, 239, 158), fill=4)
    draw.line((0, 159, 239, 159), fill=15)
    return image, draw


def table(draw, a, b, c, d):
    draw.rounded_rectangle((a + 1, b + 4, c + 2, d + 3), 9, fill=15)
    draw.rounded_rectangle((a, b, c, d), 9, fill=14, outline=15)
    walnut, highlight, bevel_shadow, wood_grain, brass, brass_light = range(41, 47)
    draw.rounded_rectangle((a + 1, b + 1, c - 1, d - 1), 8, fill=bevel_shadow, outline=13)
    draw.rounded_rectangle((a + 2, b + 1, c - 2, d - 2), 8, fill=walnut, outline=wood_grain)
    draw.line((a + 10, b + 2, c - 10, b + 2), fill=highlight)
    draw.line((a + 10, d - 2, c - 10, d - 2), fill=bevel_shadow)
    draw.line((a + 2, b + 11, a + 2, d - 11), fill=highlight)
    draw.line((c - 2, b + 11, c - 2, d - 11), fill=bevel_shadow)
    for x in range(a + 19, c - 20, 11):
        draw.line((x, b + 4, x + 4, b + 4), fill=wood_grain)
        draw.line((x + 2, d - 4, x + 6, d - 4), fill=wood_grain)
    for y in range(b + 13, d - 13, 13):
        draw.line((a + 5, y, a + 5, y + 5), fill=wood_grain)
        draw.line((c - 4, y + 3, c - 4, y + 7), fill=wood_grain)
    # The cushion noses border the displayed ball edges at the collision limits.
    draw.rounded_rectangle((a + 6, b + 3, c - 5, d - 3), 5, fill=16)
    draw.rectangle((a + 8, b + 5, c - 7, d - 5), fill=21)
    rng = random.Random(741)
    for _ in range((c - a) * (d - b) // 7):
        x, y = rng.randint(a + 8, c - 7), rng.randint(b + 5, d - 5)
        draw.point((x, y), fill=rng.choice((20, 20, 21, 22, 22)))
    for y, color in ((b + 3, 24), (b + 4, 19), (d - 4, 23), (d - 3, 16)):
        draw.line((a + 8, y, c - 7, y), fill=color)
    for x, color in ((a + 6, 23), (a + 7, 19), (c - 6, 23), (c - 5, 16)):
        draw.line((x, b + 5, x, d - 5), fill=color)
    center = (a + c + 1) // 2
    for rail_start, rail_end in ((a, center), (center, c)):
        for quarter in (1, 2, 3):
            x = rail_start + (rail_end - rail_start) * quarter // 4
            for y, toward_cloth in ((b + 2, 1), (d - 2, -1)):
                draw.polygon(((x - 2, y), (x + 2, y), (x, y + 2 * toward_cloth)), fill=brass)
                draw.line((x - 1, y, x + 1, y), fill=brass_light)
    for quarter in (1, 2, 3):
        y = b + (d - b) * quarter // 4
        for x, toward_cloth in ((a + 3, 1), (c - 3, -1)):
            draw.polygon(((x, y - 2), (x, y + 2), (x + 2 * toward_cloth, y)), fill=brass)
            draw.line((x, y - 1, x, y + 1), fill=brass_light)

    for x in (a + 8, center, c - 7):
        for y, toward_cloth in ((b + 5, 1), (d - 5, -1)):
            for side in (-1, 1):
                if (x == a + 8 and side < 0) or (x == c - 7 and side > 0):
                    continue
                shoulder = (x + side * 13, y + toward_cloth)
                tip = (x + side * 5, y - toward_cloth * 2)
                foot = (x + side * 7, y + toward_cloth * 2)
                draw.polygon((shoulder, tip, foot), fill=16)
                draw.line((shoulder, foot), fill=23)
            draw.ellipse((x - 8, y - 6, x + 8, y + 6), fill=16)
            draw.ellipse((x - 7, y - 6, x + 7, y + 5), fill=29, outline=30)
            draw.ellipse((x - 5, y - 5, x + 5, y + 4), fill=15)
            draw.arc((x - 6, y - 4, x + 6, y + 5), 20, 160, fill=7)


def make_board():
    image, draw = shell()
    for x in (8, 128):
        draw.rounded_rectangle((x, 4, x + 103, 33), 4, fill=36, outline=5)
        draw.rounded_rectangle((x + 68, 7, x + 98, 16), 3, fill=3)
        draw.line((x + 5, 18, x + 98, 18), fill=4)
    table(draw, 9, 37, 230, 131)
    draw.point((154, 84), fill=34)
    draw.rounded_rectangle((8, 134, 231, 156), 3, fill=36, outline=5)
    draw.line((13, 144, 226, 144), fill=4)
    save(image, "room", "regular_bg", bpp_mode="bpp_8")


def title_art(image, draw, x, y, radius):
    palette = image.getpalette()[:3 * len(SCENE)]
    shades = {}

    def shade_at(xx, yy, color, coverage):
        amount = min(4, max(0, round(coverage * 4)))
        if not amount:
            return
        if amount == 4:
            draw.point((xx, yy), fill=color)
            return
        previous = image.getpixel((xx, yy))
        key = (previous, color, amount)
        if key not in shades:
            shades[key] = len(palette) // 3
            palette.extend(
                (palette[previous * 3 + i] * (4 - amount) + palette[color * 3 + i] * amount + 2) // 4
                for i in range(3)
            )
        draw.point((xx, yy), fill=shades[key])

    def disc(cx, cy, r, color):
        for yy in range(cy - r - 1, cy + r + 2):
            for xx in range(cx - r - 1, cx + r + 2):
                shade_at(xx, yy, color, r + .6 - math.hypot(xx - cx, yy - cy))

    def rounded_panel(cx, cy, half_width, half_height, r, color):
        for yy in range(cy - half_height, cy + half_height + 1):
            for xx in range(cx - half_width, cx + half_width + 1):
                dx = max(0, abs(xx - cx) - half_width + r)
                dy = max(0, abs(yy - cy) - half_height + r)
                shade_at(xx, yy, color, r + .5 - math.hypot(dx, dy))

    rounded_panel(179, 49, 49, 39, 10, 32)
    rounded_panel(179, 49, 48, 38, 9, 39)
    for yy in range(y + 20, y + 39):
        for xx in range(x - 27, x + 27):
            distance = math.hypot((xx - x - 1) / 26, (yy - y - 29) / 8)
            shade_at(xx, yy, 47, min(.8, (1.0 - distance) * 2.5))

    for dy in range(-radius - 1, radius + 2):
        for dx in range(-radius - 1, radius + 2):
            edge = radius + .6 - math.hypot(dx, dy)
            if edge <= 0:
                continue
            nz = math.sqrt(max(0, radius * radius - dx * dx - dy * dy))
            diffuse = (-0.49 * dx - 0.60 * dy + 0.63 * nz) / radius
            gloss = (-0.41 * dx - 0.49 * dy + 0.77 * nz) / radius
            if gloss > .986:
                tone = 53
            elif gloss > .963:
                tone = 52
            elif diffuse > .80:
                tone = 51
            elif diffuse > .55:
                tone = 50
            elif diffuse > .25:
                tone = 49
            elif diffuse > -.12:
                tone = 48
            else:
                tone = 47
            shade_at(x + dx, y + dy, tone, edge)

    draw.arc((x - radius + 2, y - radius + 2, x + radius - 2, y + radius - 2),
             24, 96, fill=50)
    face_x, face_y = x + 6, y + 5
    disc(face_x, face_y, 15, 47)
    disc(face_x, face_y, 14, 54)
    disc(face_x - 1, face_y - 1, 12, 55)
    disc(face_x - 2, face_y - 2, 11, 56)
    stamp(draw, "8", face_x, face_y - 8, 47, scale=3)
    assert len(palette) <= 768
    image.putpalette(palette + [0] * (768 - len(palette)))


def make_title():
    image, draw = shell()
    draw.rounded_rectangle((8, 7, 231, 91), 5, fill=36, outline=5)
    title_art(image, draw, 179, 45, 32)
    stamp(draw, "8-BALL POOL", 19, 17, 8, gap=2, centered=False)
    stamp(draw, "BREAK", 21, 33, 15, scale=5, gap=3, centered=False)
    stamp(draw, "BREAK", 19, 31, 9, scale=5, gap=3, centered=False)
    stamp(draw, "ROOM", 21, 62, 15, scale=5, gap=3, centered=False)
    stamp(draw, "ROOM", 19, 60, 10, scale=5, gap=3, centered=False)

    draw.rounded_rectangle((8, 96, 231, 112), 3, fill=36, outline=5)
    stamp(draw, "RATING", 15, 102, 8, centered=False)
    draw.line((78, 100, 78, 108), fill=5)
    draw.line((155, 100, 155, 108), fill=5)
    for left, right, color, edge in ((8, 116, 17, 23), (124, 231, 3, 6)):
        draw.rounded_rectangle((left, 119, right, 141), 4, fill=color, outline=edge)
        draw.line((left+7, 120, right-7, 120), fill=edge)
    stamp(draw, "A", 18, 128, 10, centered=False)
    stamp(draw, "RANKED", 34, 128, 9, gap=2, centered=False)
    stamp(draw, "B", 135, 128, 10, centered=False)
    stamp(draw, "LINK CABLE", 150, 128, 9, gap=2, centered=False)
    stamp(draw, "SELECT  CONTROLS", 120, 148, 8, gap=2)
    save(image, "lobby", "regular_bg", bpp_mode="bpp_8")


def make_menu():
    image, draw = shell()
    draw.rounded_rectangle((8, 10, 231, 148), 5, fill=16, outline=5)
    draw.polygon(((171, 12), (228, 12), (228, 145), (111, 145)), fill=39)
    draw.polygon(((220, 12), (228, 12), (228, 145), (162, 145)), fill=38)
    save(image, "menu", "regular_bg", bpp_mode="bpp_8")


def make_panel():
    colors = [SCENE[i] for i in (0, 15, 5, 4, 36, 3, 23, 11, 10, 9)]
    image = indexed((256, 256), colors)
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle((22, 37, 219, 130), 7, fill=1)
    draw.rounded_rectangle((20, 34, 219, 127), 6, fill=2, outline=1)
    draw.rounded_rectangle((22, 36, 217, 125), 5, fill=4, outline=3)
    draw.rounded_rectangle((25, 39, 214, 58), 3, fill=3)
    for x in (31, 208):
        draw.polygon(((x - 2, 48), (x, 46), (x + 2, 48), (x, 50)), fill=7)
    draw.rounded_rectangle((28, 105, 211, 121), 3, fill=3)
    save(image, "panel", "regular_bg", bpp_mode="bpp_4")


def make_sprites():
    for number in range(16):
        shade = BALLS[number if number <= 8 else number - 8]
        colors = ["#ff00ff", "#18312d", *shade, "#829188", "#c7d1b9", "#fff1d5"]
        image = indexed((8, 16), colors)
        draw = ImageDraw.Draw(image)
        # A centered silhouette stays inside the collision circle at contact.
        half_widths = (1, 2, 3, 3, 3, 2, 1)
        for dy, half_width in enumerate(half_widths, -3):
            for dx in range(-half_width, half_width + 1):
                shade = 2 if dx + dy >= 2 else 3 if dx + dy >= -2 else 4
                if number > 8 and abs(dy) >= 2:
                    shade = 6 if dy > 0 else 7
                draw.point((4 + dx, 4 + dy), fill=shade)
        if number == 0 or number > 8:
            draw.point((3, 2), fill=7)
        # The five-pixel scoreboard icon shares the sprite's center.
        draw.ellipse((2, 10, 6, 14), fill=2)
        draw.ellipse((2, 10, 5, 13), fill=3)
        if number > 8:
            draw.line((3, 10, 5, 10), fill=7)
            draw.line((3, 14, 5, 14), fill=6)
        else:
            draw.point((3, 11), fill=4)
        save(image, f"ball_{number}", "sprite", height=8, bpp_mode="bpp_4")

    marks = indexed((8, 8 * 3), INK)
    draw = ImageDraw.Draw(marks)
    draw.ellipse((1, 1, 7, 7), outline=5)
    draw.ellipse((1, 9, 7, 15), outline=11)
    draw.polygon(((2, 19), (4, 17), (6, 19), (4, 21)), fill=8)
    save(marks, "marks", "sprite", height=8, bpp_mode="bpp_4")

    pocket = indexed((16, 16), INK)
    draw = ImageDraw.Draw(pocket)
    for x, dx in ((1, 1), (15, -1)):
        for y, dy in ((2, 1), (14, -1)):
            draw.line((x, y, x + dx * 4, y), fill=4)
            draw.line((x, y, x, y + dy * 2), fill=5)
    draw.line((7, 3, 9, 3), fill=5)
    draw.line((7, 13, 9, 13), fill=5)
    save(pocket, "pocket_marker", "sprite", bpp_mode="bpp_4")

    cursor = indexed((16, 32), INK)
    draw = ImageDraw.Draw(cursor)
    for frame, color in enumerate((5, 11)):
        y = frame * 16
        for x, horizontal in ((2, 1), (14, -1)):
            for dy, vertical in ((2, 1), (14, -1)):
                draw.line((x, y + dy, x + 3 * horizontal, y + dy), fill=color)
                draw.line((x, y + dy, x, y + dy + 3 * vertical), fill=color)
    save(cursor, "placement", "sprite", height=16, bpp_mode="bpp_4")

    stick = indexed((64, 32), CUE)
    draw = ImageDraw.Draw(stick)
    draw.polygon(((0, 14), (20, 14), (58, 15), (63, 15), (63, 17),
                  (58, 17), (20, 18), (0, 18)), fill=1)
    draw.rectangle((2, 15, 23, 17), fill=2)
    draw.line((3, 15, 24, 15), fill=3)
    draw.line((3, 16, 24, 16), fill=4)
    draw.line((8, 16, 19, 16), fill=3)
    draw.line((21, 15, 21, 17), fill=4)
    draw.line((25, 15, 25, 17), fill=5)
    draw.polygon(((27, 15), (56, 15), (57, 17), (27, 17)), fill=6)
    draw.line((26, 16, 57, 16), fill=5)
    draw.line((34, 16, 56, 16), fill=7)
    draw.rectangle((58, 15, 61, 17), fill=7)
    draw.line((62, 15, 62, 17), fill=8)
    draw.line((63, 15, 63, 17), fill=9)
    save(stick, "cue", "sprite", bpp_mode="bpp_4")

    meter = indexed((8, 8 * 9), INK)
    draw = ImageDraw.Draw(meter)
    for fill in range(9):
        y = fill * 8
        draw.rectangle((0, y + 1, 7, y + 6), fill=1)
        draw.rectangle((0, y + 2, 7, y + 5), fill=2)
        draw.line((0, y + 2, 7, y + 2), fill=12)
        if fill:
            draw.rectangle((0, y + 2, fill - 1, y + 5), fill=8)
            draw.line((0, y + 2, fill - 1, y + 2), fill=9)
            draw.line((0, y + 5, fill - 1, y + 5), fill=7)
    save(meter, "meter", "sprite", height=8, bpp_mode="bpp_4")

    avatars = indexed((8, 24), INK)
    draw = ImageDraw.Draw(avatars)
    for profile, color in enumerate((5, 8, 13)):
        y = profile * 8
        draw.rounded_rectangle((0, y, 7, y + 7), 2, fill=2, outline=3)
        draw.rectangle((3, y + 1, 4, y + 3), fill=color)
        draw.polygon(((1, y + 6), (2, y + 5), (5, y + 5), (6, y + 6)), fill=color)
    save(avatars, "avatars", "sprite", height=8, bpp_mode="bpp_4")

    # Butano's ASCII font starts at the first printable character.
    font = indexed((8, 8 * 94), INK)
    draw = ImageDraw.Draw(font)
    for code in range(33, 127):
        char = chr(code)
        if char in FONT:
            stamp(draw, char, 0, (code - 33) * 8 + 1, 6, centered=False)
    save(font, "hud_font", "sprite", width=8, height=8, bpp_mode="bpp_4")
    widths = [len(FONT.get(chr(code), FONT[" "])[0]) for code in range(32, 127)]
    (ROOT / "include" / "font_widths.h").write_text(
        "#pragma once\n#include <cstdint>\n\ninline constexpr int8_t font_widths[] = {\n"
        + "".join("    " + ", ".join(map(str, widths[i:i+16])) + ",\n"
                  for i in range(0, len(widths), 16))
        + "};\n"
    )


def make_sound(name, duration, frequency, noise_amount, decay):
    rate = 16000
    rng = random.Random(name)
    data = []
    for index in range(round(rate * duration)):
        t = index / rate
        tail = math.exp(-t * decay) * min(1, index / 12) * (1 - t / duration)
        tone = math.sin(2 * math.pi * frequency * t * (1 - 2.7 * t))
        value = (tone * (1 - noise_amount) + rng.uniform(-1, 1) * noise_amount) * tail
        data.append(struct.pack("<h", round(value * 24000)))
    with wave.open(str(AUDIO / f"{name}.wav"), "wb") as out:
        out.setparams((1, 2, rate, 0, "NONE", "not compressed"))
        out.writeframes(b"".join(data))


if __name__ == "__main__":
    GRAPHICS.mkdir(exist_ok=True)
    AUDIO.mkdir(exist_ok=True)
    make_board()
    make_title()
    make_menu()
    make_panel()
    make_sprites()
    make_sound("strike", 0.065, 340, 0.53, 58)
    make_sound("click", 0.040, 730, 0.26, 95)
    make_sound("drop", 0.14, 165, 0.42, 25)
    make_sound("confirm", 0.10, 625, 0.02, 19)
