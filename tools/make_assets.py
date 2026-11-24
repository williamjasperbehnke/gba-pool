"""Generate the original indexed pixel art and short sound effects."""
import json
import math
from pathlib import Path
import random
import struct
import wave

from PIL import Image, ImageChops, ImageColor, ImageDraw

if __package__:
    from .pool_portraits import PORTRAIT_PALETTE, character_art, player_outfits
else:
    from pool_portraits import PORTRAIT_PALETTE, character_art, player_outfits

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
# Match the native close-view table materials in src/aim_table.cpp.
TABLE_COLORS = [
    (9, 5, 5), (15, 8, 6), (22, 13, 8), (28, 19, 12),
    (7, 16, 12), (9, 20, 14), (9, 7, 7), (3, 4, 5), (16, 10, 8),
    (20, 11, 7), (30, 24, 15), (7, 17, 13),
]
(APRON_SHADE, APRON, WOOD, WOOD_LIGHT, NEAR_CUSHION, FAR_CUSHION, POCKET_WALL,
 POCKET_THROAT, LEATHER, TRIM, SIGHT, FELT) = range(len(SCENE), len(SCENE) + len(TABLE_COLORS))
TABLE_PALETTE = SCENE + ["#" + "".join(f"{round(channel * 255 / 31):02x}" for channel in rgb)
                        for rgb in TABLE_COLORS]

CUE = [
    "#ff00ff", "#27201d", "#50332a", "#865538", "#bd8653", "#edd29d",
    "#ae7b4c", "#fff1d2", "#3a656d", "#9bcdc6",
    "#684233", "#e6b26a", "#344752", "#fffbe6",
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
    "<": ("001","010","100","010","001"), "%": ("11001","11010","00100","01011","10011"),
    ".": ("0","0","0","0","1"), ",": ("00","00","00","01","10"),
    "'": ("1","1","0","0","0"), "?": ("110","001","010","000","010"),
    " ": ("00",) * 5,
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


def shell(colors=SCENE):
    image = indexed((256, 256), colors, 1)
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 0, 239, 159), fill=2)
    for y in range(2, 159, 4):
        for x in range(2 + (y % 8), 238, 8):
            draw.point((x, y), fill=3)
    draw.rectangle((0, 0, 239, 1), fill=15)
    draw.line((0, 158, 239, 158), fill=4)
    draw.line((0, 159, 239, 159), fill=15)
    return image, draw


def table_projection(x, y, z=0):
    # Match pool::project in pool_view.h for the fixed table mesh.
    depth = 160 - (.28 * (y - 84) + .96 * z)
    return round(120 + (x - 120) * 149 / depth), round(78 + (.96 * (y - 84) - .28 * z) * 149 / depth)


def perspective_table(image, draw):
    def face(vertices, fill):
        draw.polygon([table_projection(*point) for point in vertices], fill=fill)

    def quad(x0, y0, x1, y1, height, color):
        face(((x0, y0, height), (x1, y0, height), (x1, y1, height), (x0, y1, height)), color)

    draw.ellipse((12, 112, 227, 132), fill=15)
    face(((6, 136, 3), (234, 136, 3), (234, 136, -15), (6, 136, -15)), APRON)
    face(((6, 136, -5), (234, 136, -5), (234, 136, -9), (6, 136, -9)), TRIM)
    draw.line((table_projection(6, 136, -14), table_projection(234, 136, -14)), fill=APRON_SHADE)

    # A solid deck supports the rail joins and keeps the pocket trim inside the frame.
    quad(6, 32, 234, 136, 0, WOOD)
    quad(13, 39, 227, 129, 0, FELT)
    rng = random.Random(741)
    for _ in range(1200):
        x, y = rng.randint(17, 223), rng.randint(42, 126)
        draw.point(table_projection(x, y), fill=rng.choice((NEAR_CUSHION, FELT, FELT, FAR_CUSHION)))

    for edge, inner, nose, cushion in ((32, 40, 43, FAR_CUSHION), (136, 128, 125, NEAR_CUSHION)):
        quad(6, min(edge, inner), 234, max(edge, inner), 3, WOOD)
        draw.line((table_projection(6, edge, 3), table_projection(234, edge, 3)), fill=WOOD_LIGHT)
        face(((14, inner, 3), (226, inner, 3), (226, nose, 0), (14, nose, 0)), cushion)
        for first in (26, 130):
            for x in range(first + 15, first + 74, 26):
                inward = 1 if edge == 32 else -1
                grain_y = edge + inward * (2 - (x & 1))
                draw.line((table_projection(x - 11, grain_y, 3), table_projection(x + 4, grain_y, 3)),
                          fill=APRON if x & 2 else WOOD_LIGHT)
                draw.point(table_projection(x, edge + inward * 5, 3), fill=SIGHT)
    for outer, inner, nose in ((6, 14, 17), (234, 226, 223)):
        quad(min(outer, inner), 32, max(outer, inner), 136, 3, WOOD)
        draw.line((table_projection(outer, 32, 3), table_projection(outer, 136, 3)), fill=WOOD_LIGHT)
        face(((inner, 40, 3), (nose, 40, 0), (nose, 128, 0), (inner, 128, 3)), NEAR_CUSHION)
        for y in (62, 84, 106):
            inward = 1 if outer == 6 else -1
            grain_x = outer + inward * (2 + y % 3)
            draw.line((table_projection(grain_x, y - 7, 3), table_projection(grain_x, y + 4, 3)),
                      fill=WOOD_LIGHT if y == 84 else APRON)
            draw.point(table_projection(outer + inward * 5, y, 3), fill=SIGHT)

    for pocket in range(6):
        paint_pocket(image, pocket)


POCKET_CENTERS = ((17, 42), (120, 42), (223, 42), (17, 126), (120, 126), (223, 126))


def pocket_rings(pocket):
    x, y = POCKET_CENTERS[pocket]
    dx = (1, 0, -1)[pocket % 3]
    dy = 1 if pocket < 3 else -1

    def world(u, v):
        if dx:
            return (x + dx * (1 + ((u + v) * 181 + 128) // 256),
                    y + dy * (1 + ((v - u) * 181 + 128) // 256))
        return x + u, y + dy * v

    width = 9 if dx else 11
    rings = []
    for w, depth in ((width, 7), (width - 1, 6), (width - 2, 5)):
        shape = ((-2, -depth), (2, -depth), (w - 3, 2 - depth), (w - 1, 4 - depth), (w, 1),
                 (w - 1, depth - 2), (w - 3, depth), (3 - w, depth), (1 - w, depth - 2),
                 (-w, 1), (1 - w, 4 - depth), (3 - w, 2 - depth))
        rings.append([world(u, v) for u, v in shape])
    return rings


def paint_pocket(image, pocket, called=False, offset=(0, 0)):
    rim, mouth, throat = pocket_rings(pocket)
    ox, oy = offset

    def polygon(ring, z):
        return [(x - ox, y - oy) for x, y in (table_projection(x, y, z) for x, y in ring)]

    draw = ImageDraw.Draw(image)
    outer = polygon(rim, 3)
    draw.polygon(outer, fill=8 if called else LEATHER)
    if not called:
        draw.polygon(outer[4:10], fill=FAR_CUSHION if pocket < 3 else NEAR_CUSHION)
    if called:
        draw.line(outer + outer[:1], fill=9)
    else:
        draw.line((outer[0], outer[1]), fill=TRIM)
    opening = polygon(mouth, 3)
    draw.polygon(opening, fill=7 if called else POCKET_WALL)
    mask = Image.new("1", image.size)
    ImageDraw.Draw(mask).polygon(opening, fill=1)
    floor = Image.new("1", image.size)
    ImageDraw.Draw(floor).polygon(polygon(throat, 0), fill=1)
    image.paste(1 if called else POCKET_THROAT, mask=ImageChops.logical_and(mask, floor))


def make_pocket_art():
    sheet = indexed((32, 6 * 32), INK)
    centers = []
    for pocket, center in enumerate(POCKET_CENTERS):
        x, y = table_projection(*center, 3)
        centers.append((x, y))
        tile = indexed((32, 32), INK)
        paint_pocket(tile, pocket, called=True, offset=(x - 16, y - 16))
        sheet.paste(tile, (0, pocket * 32))
    save(sheet, "called_pockets", "sprite", height=32, bpp_mode="bpp_4")

    lines = ["#pragma once", "", "#include <cstdint>", "", "namespace pool", "{",
             "inline constexpr uint8_t overview_pocket_centers[6][2] = {"]
    lines.extend(f"    {{ {x}, {y} }}," for x, y in centers)
    lines.extend(("};", "}", ""))
    (ROOT / "include" / "pocket_art.h").write_text(chr(10).join(lines))


def pool_room(draw):
    draw.rectangle((0, 0, 239, 63), fill=2)
    draw.rectangle((0, 64, 239, 83), fill=3)
    draw.line((0, 84, 239, 84), fill=4)
    draw.rectangle((0, 85, 239, 159), fill=1)


def make_match_hud(compact=False):
    image = indexed((256, 256), [SCENE[i] for i in (0, 1, 5, 3, 4)])
    draw = ImageDraw.Draw(image)
    bottom = 16 if compact else 33
    for x0, x1 in ((3, 91), (96, 143), (148, 236)):
        card_bottom = 26 if not compact and x0 == 96 else bottom
        draw.rounded_rectangle((x0, 2, x1, card_bottom), 3, fill=3, outline=2)
        if not compact:
            if x0 == 96:
                draw.rounded_rectangle((x0 + 2, 4, x1 - 2, 15), 2, fill=1)
            else:
                draw.rounded_rectangle((x0 + 2, 3, x0 + 22, 20), 2, fill=1)
                draw.rounded_rectangle((x0 + 24, 3, x1 - 2, 20), 2, fill=1)
    save(image, "match_hud_compact" if compact else "match_hud", "regular_bg", bpp_mode="bpp_4")


def make_board(overview=True):
    colors = TABLE_PALETTE if overview else SCENE
    image = indexed((256, 256), colors, 1)
    draw = ImageDraw.Draw(image)
    pool_room(draw)
    if overview:
        perspective_table(image, draw)
        draw.point(table_projection(154, 84), fill=34)
    if overview:
        save(image, "room", "regular_bg", bpp_mode="bpp_8")
    else:
        colors = [0] + sorted(color for _, color in image.getcolors() if color)
        assert len(colors) <= 16
        indices = [0] * 256
        for index, color in enumerate(colors):
            indices[color] = index
        stage = image.point(indices)
        old_palette = image.getpalette()
        stage_palette = [channel for color in colors for channel in old_palette[color * 3:color * 3 + 3]]
        stage.putpalette(stage_palette + [0] * (768 - len(stage_palette)))
        save(stage, "aim_stage", "regular_bg", bpp_mode="bpp_4")


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
    draw.rounded_rectangle((8, 7, 231, 86), 5, fill=36, outline=5)
    draw.line((15, 7, 224, 7), fill=6)
    draw.rounded_rectangle((130, 12, 225, 81), 6, fill=39, outline=32)
    draw.rounded_rectangle((133, 15, 222, 78), 3, fill=16)
    draw.polygon(((138, 18), (186, 18), (210, 74), (161, 74)), fill=39)
    title_art(image, draw, 179, 43, 29)

    stamp(draw, "8-BALL POOL", 19, 17, 8, gap=2, centered=False)
    draw.line((19, 27, 116, 27), fill=5)
    stamp(draw, "BREAK", 21, 34, 15, scale=4, gap=3, centered=False)
    stamp(draw, "BREAK", 19, 32, 9, scale=4, gap=3, centered=False)
    stamp(draw, "ROOM", 21, 59, 15, scale=4, gap=4, centered=False)
    stamp(draw, "ROOM", 19, 57, 10, scale=4, gap=4, centered=False)

    draw.rounded_rectangle((8, 91, 231, 105), 3, fill=36, outline=5)
    stamp(draw, "RATING", 15, 96, 8, centered=False)
    draw.line((78, 94, 78, 102), fill=5)
    draw.line((155, 94, 155, 102), fill=5)
    for top in (110, 129):
        for left, right in ((8, 117), (123, 231)):
            draw.rounded_rectangle((left, top, right, top + 15), 3, fill=36, outline=5)
    save(image, "lobby", "regular_bg", bpp_mode="bpp_8")


def portrait_sprite(art, size, colors):
    source = art.resize((size, size), Image.Resampling.BOX)
    rgb = [ImageColor.getrgb(color) for color in colors]
    sprite = indexed(source.size, colors)
    sprite.putdata([
        min(range(1, len(rgb)), key=lambda i:
            sum((component - target)**2 for component, target in zip(pixel, rgb[i])))
        if pixel[3] >= 130 else 0
        for pixel in source.getdata()
    ])
    return sprite


def make_rival_portraits():
    art = character_art()[:5]
    frames = indexed((64, 64 * (len(art) - 1)), PORTRAIT_PALETTE)
    icons = indexed((16, 16 * (len(art) - 1)), PORTRAIT_PALETTE)
    finn_palette = PORTRAIT_PALETTE[:5] + [
        "#802d3c", "#53232d", "#9e252d", "#cb3535", "#ed5b54", "#e18289",
        "#c2c8d1", "#88522e", "#e3a347", "#ffdda7", "#fff1de",
    ]
    circle_colors = (6, 4, 13, 9, 5)
    for slot, portrait in enumerate(art):
        is_finn = slot == len(art) - 1
        palette = finn_palette if is_finn else PORTRAIT_PALETTE
        card = indexed((64, 64), palette)
        draw = ImageDraw.Draw(card)
        draw.rounded_rectangle((0, 0, 63, 63), 5, fill=1, outline=3)
        draw.rounded_rectangle((2, 2, 61, 61), 4, fill=2)
        draw.line((6, 0, 57, 0), fill=4)
        draw.ellipse((9, 7, 54, 52), fill=circle_colors[slot])
        inset = portrait_sprite(portrait, 56, palette)
        mask = Image.new("1", inset.size)
        mask.putdata([255 if pixel else 0 for pixel in inset.tobytes()])
        card.paste(inset, (4, 4), mask)
        icon = portrait_sprite(portrait, 16, palette)
        if is_finn:
            save(card, "finn_portrait", "sprite", height=64, bpp_mode="bpp_4")
            save(icon, "finn_icon", "sprite", height=16, bpp_mode="bpp_4")
        else:
            frames.paste(card, (0, 64 * slot))
            icons.paste(icon, (0, 16 * slot))
    save(frames, "rival_portraits", "sprite", height=64, bpp_mode="bpp_4")
    save(icons, "rival_icons", "sprite", height=16, bpp_mode="bpp_4")


def make_player_portraits():
    default = character_art()[-1]
    outfits = player_outfits()
    avatars = indexed((16, 16 * 3), INK)
    cosmetics = indexed((16, 16 * len(outfits)), INK)
    wardrobe = indexed((64, 64 * (len(outfits) + 1)), PORTRAIT_PALETTE)
    for frame, jersey in enumerate((15, 7, 13)):
        icon = portrait_sprite(default, 16, INK)
        colors = list(range(256))
        colors[15] = jersey
        avatars.paste(icon.point(colors), (0, 16 * frame))
    for frame, outfit in enumerate(outfits):
        cosmetics.paste(portrait_sprite(outfit, 16, INK), (0, 16 * frame))
        wardrobe.paste(portrait_sprite(outfit, 54, PORTRAIT_PALETTE), (5, 64 * frame + 5))
    wardrobe.paste(portrait_sprite(default, 54, PORTRAIT_PALETTE), (5, 64 * len(outfits) + 5))
    save(avatars, "avatars", "sprite", height=16, bpp_mode="bpp_4")
    save(cosmetics, "cosmetic_avatars", "sprite", height=16, bpp_mode="bpp_4")
    save(wardrobe, "wardrobe_portraits", "sprite", height=64, bpp_mode="bpp_4")


def make_menu():
    image, draw = shell()
    draw.rounded_rectangle((10, 10, 229, 149), 6, fill=36, outline=4)
    save(image, "menu", "regular_bg", bpp_mode="bpp_8")


def make_panel(paused=False):
    colors = [SCENE[i] for i in (0, 15, 5, 4, 36, 3, 23, 17)]
    image = indexed((256, 256), colors)
    draw = ImageDraw.Draw(image)
    if paused:
        draw.rounded_rectangle((50, 36, 192, 144), 7, fill=1)
        draw.rounded_rectangle((48, 33, 191, 141), 6, fill=4, outline=2)
        draw.rounded_rectangle((57, 39, 182, 53), 3, fill=5)
        for y in (65, 82, 114, 130):
            draw.rounded_rectangle((60, y - 6, 179, y + 6), 3, fill=5)
    else:
        draw.rounded_rectangle((23, 22, 220, 146), 7, fill=1)
        draw.rounded_rectangle((21, 19, 218, 143), 6, fill=4, outline=2)
        draw.rounded_rectangle((27, 25, 212, 42), 3, fill=5)
        draw.rounded_rectangle((28, 121, 211, 138), 3, fill=5)
    save(image, "pause_panel" if paused else "panel", "regular_bg", bpp_mode="bpp_4")


def blend(first, second, ratio):
    channels = (round(int(first[i:i+2], 16) * (1 - ratio) + int(second[i:i+2], 16) * ratio)
                for i in (1, 3, 5))
    return "#" + "".join(f"{channel:02x}" for channel in channels)


def draw_cue_preview(item):
    art = indexed((64, 64), CUE)
    draw = ImageDraw.Draw(art)

    def point(distance, across):
        return round(7 + distance * .70 + across * .71), round(56 - distance * .70 + across * .71)

    def section(lo, hi, left, right, color):
        draw.polygon((point(lo, -left), point(hi, -right), point(hi, right), point(lo, left)), fill=color)

    # Long tapered maple shaft, slim grip, ivory ferrule and chalked leather tip.
    section(1, 70, 2.9, 1.0, 1)
    section(3, 69, 2.3, .75, 5)
    draw.line((point(4, -1.4), point(67, -.4)), fill=7)
    section(2, 24, 2.5, 1.8, 12 if item else 10)
    draw.line((point(3, -1.5), point(23, -1.0)), fill=4)
    section(1, 3, 2.7, 2.4, 1)
    section(24, 26, 1.8, 1.7, 11)
    section(67, 70, 1.0, .95, 13)
    section(70, 72, .95, .75, 8)
    draw.point(point(71, -.5), fill=9)

    if item == 1:  # Bamboo binding and nodes.
        for at in (8, 13, 18, 35, 47, 57):
            section(at, at + 1, 1.8 if at < 20 else .8, 1.6 if at < 20 else .8, 11)
    elif item == 2:  # Frost Tip's silver diamonds.
        for at in (8, 15, 21):
            draw.polygon((point(at - 2, 0), point(at, -1.6), point(at + 2, 0), point(at, 1.6)), fill=13)
        draw.line((point(30, 0), point(62, 0)), fill=11)
    elif item == 3:  # Copper feather inlay.
        draw.line((point(5, 1), point(17, -1), point(30, 0)), fill=11, width=2)
        draw.line((point(12, 1), point(24, -.5)), fill=7)
    elif item == 4:  # Tentacle grip.
        draw.line((point(6, .7), point(25, -.4)), fill=11, width=2)
        for at in (8, 14, 20):
            draw.point(point(at, -.6), fill=13)
    elif item == 5:  # Ivory with a teal wave at the joint.
        draw.line((point(4, 0), point(12, -1), point(21, 0), point(28, 1)), fill=4, width=2)
        for at in (10, 16, 22):
            draw.point(point(at, -.7), fill=13)
    return art


def make_sprites():
    for number in range(16):
        shade = BALLS[number if number <= 8 else number - 8]
        low, mid, high = shade if number else ("#68837c", "#a3b8aa", "#e2e6d2")
        gloss = "#464b54" if number == 8 else blend(high, "#fff4d9", .26)
        if number == 0:
            gloss = "#fff9e7"
        colors = ["#ff00ff", "#18312d", *shade, "#a4b5ab", "#d5ded0", "#eee9d6",
                  blend(low, "#101823", .46), low, mid, high, gloss, "#687d76", "#fff7e3"]
        image = indexed((16, 16), colors)
        draw = ImageDraw.Draw(image)
        for y in range(3, 13):
            for x in range(3, 13):
                dx = (x - 7.5) / 5.0
                dy = (y - 7.5) / 5.0
                depth = 1 - dx * dx - dy * dy
                if depth < 0:
                    continue
                light = .78 * math.sqrt(depth) - .30 * dx - .45 * dy
                ink = 8 + sum(light > level for level in (.08, .33, .60, .83))
                stripe_axis = .34 * dx + .86 * dy + .39 * math.sqrt(depth)
                if number > 8 and abs(stripe_axis) > .46:
                    ink = (13, 5, 6, 7, 14)[ink - 8]
                draw.point((x, y), fill=ink)
        save(image, f"ball_{number}", "sprite", bpp_mode="bpp_4")

    marks = indexed((8, 8), INK)
    draw = ImageDraw.Draw(marks)
    draw.line((2, 3, 6, 3), fill=15)
    draw.line((1, 4, 7, 4), fill=15)
    draw.line((2, 4, 6, 4), fill=1)
    draw.line((3, 5, 5, 5), fill=15)
    save(marks, "marks", "sprite", height=8, bpp_mode="bpp_4")

    active = indexed((64, 32 * 4), INK)
    for mode, bottom in enumerate((31, 14)):
        border = indexed((89, 32), INK)
        draw = ImageDraw.Draw(border)
        draw.rounded_rectangle((0, 0, 88, bottom), 3, outline=8)
        draw.line((5, 0, 83, 0), fill=9)
        active.paste(border.crop((0, 0, 45, 32)), (0, mode * 64))
        active.paste(border.crop((45, 0, 89, 32)), (0, mode * 64 + 32))
    save(active, "active_player", "sprite", height=32, bpp_mode="bpp_4")

    spin_ball = indexed((16, 16), INK)
    draw = ImageDraw.Draw(spin_ball)
    for y in range(1, 15):
        for x in range(1, 15):
            dx, dy = (x - 7.5) / 7, (y - 7.5) / 7
            depth = 1 - dx * dx - dy * dy
            if depth >= 0:
                light = math.sqrt(depth) - .22 * dx - .27 * dy
                draw.point((x, y), fill=4 + int(light > .23) + int(light > .56))
    save(spin_ball, "spin_ball", "sprite", bpp_mode="bpp_4")

    spin_dot = indexed((8, 8), INK)
    draw = ImageDraw.Draw(spin_dot)
    draw.rectangle((3, 3, 4, 4), fill=10)
    draw.point((3, 3), fill=11)
    save(spin_dot, "spin_dot", "sprite", bpp_mode="bpp_4")

    signatures = indexed((64, 32 * 6), CUE)
    draw = ImageDraw.Draw(signatures)
    for style in range(6):
        y = style * 32 + 16
        draw.polygon(((0, y - 3), (18, y - 3), (41, y - 2), (62, y - 1), (63, y),
                      (62, y + 1), (43, y + 2), (18, y + 3), (0, y + 3)), fill=1)
        draw.polygon(((2, y - 2), (28, y - 2), (58, y - 1), (61, y - 1),
                      (61, y + 1), (42, y + 1), (24, y + 2), (2, y + 2)), fill=6)
        draw.line((3, y - 1, 26, y - 1, 40, y, 59, y), fill=5)
        draw.line((28, y - 2, 58, y - 1), fill=7)
        draw.rectangle((2, y - 2, 18, y + 2), fill=10 if style == 0 else 12)
        draw.line((2, y - 2, 18, y - 2), fill=4)
        draw.line((18, y - 2, 18, y + 2), fill=11)
        draw.line((21, y - 2, 21, y + 2), fill=3)
        draw.line((58, y - 1, 61, y - 1), fill=13)
        draw.line((58, y, 61, y), fill=7)
        draw.line((62, y - 1, 62, y + 1), fill=8)
        draw.point((63, y), fill=9)
        if style == 0:
            draw.line((5, y, 15, y), fill=3)
        elif style == 1:
            for x in (7, 12, 17, 34, 45):
                draw.line((x, y - 1, x, y + 1), fill=11)
        elif style == 2:
            for x in (7, 13, 29):
                draw.polygon(((x - 1, y), (x, y - 1), (x + 1, y), (x, y + 1)), fill=13)
            draw.line((31, y, 47, y), fill=11)
        elif style == 3:
            draw.line((4, y + 1, 15, y - 1, 26, y), fill=11)
            draw.line((12, y + 1, 22, y), fill=7)
        elif style == 4:
            for x in (5, 10, 15):
                draw.rectangle((x, y - 1, x + 2, y + 1), fill=11)
                draw.point((x + 1, y), fill=13)
        else:
            draw.line((24, y - 1, 57, y), fill=13)
            for x in (5, 10, 15):
                draw.line((x, y - 1, x - 2, y + 1), fill=11)
    save(signatures, "cue", "sprite", height=32, bpp_mode="bpp_4")

    cue_previews = indexed((64, 64 * 6), CUE)
    for item in range(6):
        cue_previews.paste(draw_cue_preview(item), (0, 64 * item))
    save(cue_previews, "cue_previews", "sprite", height=64, bpp_mode="bpp_4")

    gear_frame = indexed((64, 64), INK)
    mat = ImageDraw.Draw(gear_frame)
    mat.rounded_rectangle((0, 0, 63, 63), 4, fill=2, outline=13)
    mat.rounded_rectangle((3, 3, 60, 60), 3, fill=1, outline=12)
    mat.line((7, 3, 56, 3), fill=3)
    save(gear_frame, "gear_frame", "sprite", bpp_mode="bpp_4")

    close_cue = indexed((8, 32 * 64 * 6), CUE)
    draw = ImageDraw.Draw(close_cue)
    for style in range(6):
        for section in range(2):
            for length in range(1, 33):
                top = (style * 64 + section * 32 + length - 1) * 32
                for pixel_row in range(length):
                    row = section * 32 + pixel_row
                    y = top + pixel_row
                    if row <= 1:
                        draw.line((3, y, 5, y), fill=8)
                        draw.point((4, y), fill=9)
                    elif row < 7:
                        draw.line((3, y, 5, y), fill=6)
                        draw.point((4, y), fill=13)
                        draw.point((5, y), fill=7)
                    else:
                        left = 3 if row < 15 else 2 if row < 33 else 1
                        right = 5 if row < 24 else 6 if row < 45 else 7
                        draw.line((left, y, right, y), fill=3)
                        draw.line((left + 1, y, right - 1, y), fill=6)
                        draw.point((3, y), fill=7)
                        draw.point((4, y), fill=13 if style in (2, 5) else 5)
                        if row >= 43:
                            draw.line((left + 1, y, right - 1, y), fill=10 if style == 0 else 12)
                            draw.point((3, y), fill=4)
                        if row in (39, 42) or (style == 1 and row in (17, 27, 35)):
                            draw.line((left + 1, y, right - 1, y), fill=11)
                        if style in (2, 3, 4, 5) and row >= 46 and row % 6 in (0, 1):
                            draw.point((4, y), fill=13 if style in (2, 4) else 11)
    save(close_cue, "close_cue", "sprite", height=32, bpp_mode="bpp_4")

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

    make_player_portraits()

    # Each width shares the same tail position on screen, under the rival portrait.
    widths = range(52, 121, 4)
    bubble_parts = indexed((64, 64 * len(widths)), INK)
    for frame, width in enumerate(widths):
        bubble = indexed((128, 32), INK)
        draw = ImageDraw.Draw(bubble)
        tail = 160 - min(144, 234 - width)
        draw.rounded_rectangle((0, 4, width - 1, 16), 3, fill=1, outline=3)
        draw.line((3, 15, width - 4, 15), fill=2)
        draw.polygon(((tail - 4, 4), (tail, 0), (tail + 4, 4)), fill=1)
        draw.line((tail - 4, 4, tail, 0, tail + 4, 4), fill=3)
        bubble_parts.paste(bubble.crop((0, 0, 64, 32)), (0, frame * 64))
        bubble_parts.paste(bubble.crop((64, 0, 128, 32)), (0, frame * 64 + 32))
    save(bubble_parts, "taunt_chip", "sprite", height=32, bpp_mode="bpp_4")

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
    make_pocket_art()
    make_board(overview=False)
    make_match_hud()
    make_match_hud(compact=True)
    make_title()
    make_menu()
    make_rival_portraits()
    make_panel()
    make_panel(paused=True)
    make_sprites()
    make_sound("strike", 0.065, 340, 0.53, 58)
    make_sound("click", 0.040, 730, 0.26, 95)
    make_sound("drop", 0.14, 165, 0.42, 25)
    make_sound("confirm", 0.10, 625, 0.02, 19)
    make_sound("reject", 0.11, 225, 0.08, 25)
