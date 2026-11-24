"""Load the character art and prepare the player's wardrobe portraits."""

from pathlib import Path

from PIL import Image

ART = Path(__file__).with_name("art")

PORTRAIT_PALETTE = [
    "#ff00ff", "#161526", "#263249", "#505e83", "#929fb2", "#18555b",
    "#329b93", "#7bcbc1", "#782a51", "#b94772", "#eb7e97", "#8b4c35",
    "#f47b27", "#e3a347", "#ffd29f", "#fff1de",
]


def character_art():
    atlas = Image.open(ART / "animal_portraits.png").convert("RGBA")
    portraits = [atlas.crop((i % 3 * 192, i // 3 * 192, (i % 3 + 1) * 192, (i // 3 + 1) * 192))
                 for i in range(6)]
    finn = Image.open(ART / "finn_portrait.png").convert("RGBA")
    _, top, _, bottom = finn.getbbox()
    top_margin = 8
    bust = finn.crop((0, top, finn.width, bottom))
    bust = bust.resize((finn.width, finn.height - top_margin), Image.Resampling.BOX)
    portraits[4] = Image.new("RGBA", finn.size)
    portraits[4].paste(bust, (0, top_margin))
    return portraits


def player_outfits():
    atlas = Image.open(ART / "wardrobe_portraits.png").convert("RGBA")
    outfits = [
        atlas.crop((i % 4 * 192, i // 4 * 192, (i % 4 + 1) * 192, (i // 4 + 1) * 192))
        for i in range(8)
    ]
    recolor_shirt(outfits[0], light=(245, 224, 184), shadow=(115, 100, 81))
    recolor_shirt(outfits[1], light=(228, 240, 251), shadow=(91, 110, 150))
    return outfits


def recolor_shirt(art, light, shadow):
    """Replace the teal shirt fabric while retaining the vest, tie, and inked seams."""
    pixels = art.load()
    for y in range(111, art.height):
        for x in range(art.width):
            red, green, blue, alpha = pixels[x, y]
            if alpha < 120 or green < 24 or red * 3 > green or not .89 * green <= blue <= 1.27 * green:
                continue
            tone = min(1, max(0, (green - 30) / 92))
            rgb = tuple(round(dark + (bright - dark) * tone) for dark, bright in zip(shadow, light))
            pixels[x, y] = (*rgb, alpha)
