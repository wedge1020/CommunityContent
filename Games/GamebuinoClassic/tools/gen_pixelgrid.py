"""Generate the pixel-grid presentation overlay used by portVircon32.c.

Direct port of the sibling tinyjoypad_vircon32 project's own (unchecked-in,
one-off ImageMagick-generated) assets/pixelgrid.png - see that project's own
CLAUDE.md ("A pixel-grid presentation overlay...") for the full design
rationale. Re-derived here as a real, checked-in generator script (unlike
the sibling project, which never committed its own) so this asset can be
regenerated if TILE_SCALE or the LCD dimensions ever change.

Produces a transparent-background image, sized to exactly this project's own
game-area size (LCD_WIDTH*TILE_SCALE x LCD_HEIGHT*TILE_SCALE = 588x336), with
opaque black 1px lines at every TILE_SCALE-pixel boundary in both directions
- so each of the original 84x48 LCD pixels reads as its own distinct visible
cell once scaled up 7x, instead of blending into a smooth block. A plain LA
(luminance+alpha) PNG - confirmed matching the sibling project's own asset
format exactly (verified by inspecting its real pixel data: alpha=255 at
every multiple of its own TILE_SCALE, alpha=0 elsewhere, luminance always 0).

Requires Pillow (`pip install pillow`) - unlike gen_column_atlas.py, this
needs a real alpha channel, which plain PPM can't express.
"""

from PIL import Image

LCD_WIDTH = 84
LCD_HEIGHT = 48
TILE_SCALE = 7

IMG_W = LCD_WIDTH * TILE_SCALE
IMG_H = LCD_HEIGHT * TILE_SCALE

if __name__ == "__main__":
    img = Image.new("LA", (IMG_W, IMG_H), (0, 0))
    px = img.load()

    for y in range(IMG_H):
        for x in range(IMG_W):
            if x % TILE_SCALE == 0 or y % TILE_SCALE == 0:
                px[x, y] = (0, 255)

    img.save("assets/pixelgrid.png")
    print("wrote assets/pixelgrid.png (%dx%d)" % (IMG_W, IMG_H))
