#!/usr/bin/env python3
"""Generate the "GRAY LCD column" texture atlas used by portVircon32.c
whenever the runtime gbRealGrayColor toggle is on (see gamebuinoShim.h's
own Configuration section).

Same 256-tile, one-tile-per-byte-value technique as gen_column_atlas.py
(see that script's own header comment for the full column-atlas design),
but producing a SECOND atlas meant to be drawn as an extra pass on top of
the first: "lit" bits are drawn as a real opaque mid-gray instead of black,
and "off" bits are fully TRANSPARENT (not opaque white) - unlike the main
atlas, where off-bits don't need transparency since md_beginFrame() already
clears the whole screen to white first. This atlas is drawn selectively,
only over the specific pixels gbGrayBuffer[] marks as real GB_GRAY (a
strict subset of whatever the main atlas already drew as black for that
same byte), so its own off-bits must be transparent to avoid painting over
already-correct black pixels drawn by the first pass.

Requires Pillow (`pip install pillow`) - needs a real alpha channel, which
plain PPM can't express (see gen_pixelgrid.py's own header comment for the
same reasoning).

Output: assets/columns_gray.png
"""

from PIL import Image

SCALE = 7            # 1 LCD pixel -> SCALE x SCALE physical pixels
GRID_COLS = 16        # tiles per row in the atlas (16x16 = 256 tiles)
GRID_ROWS = 16
TILE_W = SCALE
TILE_H = 8 * SCALE    # a "page" byte is always 8 pixels tall regardless of screen height

IMG_W = GRID_COLS * TILE_W
IMG_H = GRID_ROWS * TILE_H

# A genuine mid-gray, distinct from both BLACK and WHITE at a glance on the
# small physical LCD area this ends up scaled into - adjust here if a
# lighter/darker dither shade is ever wanted; no other file needs updating.
GRAY = (150, 150, 150, 255)
TRANSPARENT = (0, 0, 0, 0)


def main():
    img = Image.new("RGBA", (IMG_W, IMG_H), TRANSPARENT)
    px = img.load()

    for value in range(256):
        grid_x = value % GRID_COLS
        grid_y = value // GRID_COLS
        tile_origin_x = grid_x * TILE_W
        tile_origin_y = grid_y * TILE_H

        for bit in range(8):
            lit = (value >> bit) & 1
            color = GRAY if lit else TRANSPARENT
            row_origin_y = tile_origin_y + bit * SCALE

            for dy in range(SCALE):
                y = row_origin_y + dy
                for dx in range(SCALE):
                    x = tile_origin_x + dx
                    px[x, y] = color

    img.save("assets/columns_gray.png")
    print("wrote assets/columns_gray.png (%dx%d)" % (IMG_W, IMG_H))


if __name__ == "__main__":
    main()
