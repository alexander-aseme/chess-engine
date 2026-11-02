#!/usr/bin/env python3
"""Generate assets/pieces.png: a 6x2 sprite atlas of chess pieces.

Layout (each cell CELL x CELL pixels):
    row 0: white  K Q R B N P
    row 1: black  K Q R B N P

The glyphs come from the DejaVu Sans font (free license), drawn with a
filled body and an outline so both colors read well on light and dark
squares. Swap this out for hand-drawn or SVG-rendered pieces any time;
the game only cares about the atlas layout above.

Usage: python3 tools/make_piece_atlas.py [path/to/DejaVuSans.ttf]
Requires Pillow (pip install pillow).
"""
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

CELL = 128
GLYPHS = "♚♛♜♝♞♟"  # filled K Q R B N P

FONT_CANDIDATES = [
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/Library/Fonts/DejaVuSans.ttf",
    str(Path.home() / "Library/Fonts/DejaVuSans.ttf"),
]


def find_font():
    if len(sys.argv) > 1:
        return sys.argv[1]
    for p in FONT_CANDIDATES:
        if Path(p).exists():
            return p
    sys.exit("DejaVuSans.ttf not found; pass its path as an argument")
    return None


def main():
    font = ImageFont.truetype(find_font(), int(CELL * 0.86))
    atlas = Image.new("RGBA", (CELL * 6, CELL * 2), (0, 0, 0, 0))
    draw = ImageDraw.Draw(atlas)
    styles = [
        ((250, 248, 240, 255), (30, 30, 30, 255)),  # white: light body, dark outline
        ((35, 35, 40, 255), (235, 235, 235, 255)),  # black: dark body, light outline
    ]
    for row, (fill, stroke) in enumerate(styles):
        for col, ch in enumerate(GLYPHS):
            cx, cy = col * CELL + CELL / 2, row * CELL + CELL / 2
            draw.text((cx, cy + CELL * 0.02), ch, font=font, fill=fill,
                      stroke_width=max(2, CELL // 40), stroke_fill=stroke,
                      anchor="mm")
    out = Path(__file__).resolve().parent.parent / "assets" / "pieces.png"
    atlas.save(out)
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
