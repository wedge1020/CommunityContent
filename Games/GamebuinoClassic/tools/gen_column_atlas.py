#!/usr/bin/env python3
"""Generate the 256-tile "LCD column" texture atlas used by portVircon32.c.

Same technique as the sibling tinyjoypad_vircon32 project's own
tools/gen_column_atlas.py: the real Gamebuino Classic PCD8544 (Nokia 5110)
display addresses its framebuffer one byte per (column, page), each byte
packing 8 vertical pixels of a single 1px-wide column (bit 0 = top pixel,
bit 7 = bottom pixel) - the same convention SSD1306 uses, just a smaller
84x48/6-page canvas instead of 128x64/8-page. There are only 256 possible
byte values, so this script pre-renders all 256 possible column patterns
once, at build time, already scaled up to the final on-screen pixel size.
portVircon32.c slices this image into 256 texture regions (ids 0-255,
matching the byte value each depicts) via define_region_matrix(), and turns
each md_drawColumn(value) call into a single select_region(value) +
draw_region_at() GPU blit.

**A "lit" bit is drawn BLACK, not WHITE** - a real fix, not the original
design: this script started as a copy of the sibling tinyjoypad_vircon32
project's own generator, which correctly draws "lit" as WHITE because
TinyJoypad's own target is a genuine self-illuminating SSD1306 OLED (lit
pixels emit light against a dark, unlit background). The PCD8544/Nokia
5110 this project actually targets is a reflective/transmissive LCD, the
opposite physical convention: the background is naturally light, and a
"set" pixel is dark ink drawn on top of it - already documented elsewhere
in this project (gamebuinoShim.c's own `gbColor = 1; // 1 = black/on`
comment) but never actually corrected here until a direct user report
("colors seem inverted... background is normally white by default") led
to checking this file specifically. Every game's own draw calls are
unaffected (a game only ever decides which bits are 1 vs 0 - this is
purely which color each of those two states is rendered as), so this is a
one-file, no-gameplay-logic fix that corrects the on-screen look of the
LCD emulation for all 12 games at once.

Output: tools/atlas.ppm (regenerate assets/columns.png from it with
`magick tools/atlas.ppm assets/columns.png`).
"""

import struct

SCALE = 7           # 1 LCD pixel -> SCALE x SCALE physical pixels
GRID_COLS = 16       # tiles per row in the atlas (16x16 = 256 tiles)
GRID_ROWS = 16
TILE_W = SCALE
TILE_H = 8 * SCALE   # a "page" byte is always 8 pixels tall regardless of screen height

IMG_W = GRID_COLS * TILE_W
IMG_H = GRID_ROWS * TILE_H

WHITE = (255, 255, 255)
BLACK = (0, 0, 0)


def main():
    pixels = bytearray(IMG_W * IMG_H * 3)

    for value in range(256):
        grid_x = value % GRID_COLS
        grid_y = value // GRID_COLS
        tile_origin_x = grid_x * TILE_W
        tile_origin_y = grid_y * TILE_H

        for bit in range(8):
            lit = (value >> bit) & 1
            color = BLACK if lit else WHITE
            row_origin_y = tile_origin_y + bit * SCALE

            for dy in range(SCALE):
                y = row_origin_y + dy
                row_start = y * IMG_W * 3
                for dx in range(SCALE):
                    x = tile_origin_x + dx
                    idx = row_start + x * 3
                    pixels[idx] = color[0]
                    pixels[idx + 1] = color[1]
                    pixels[idx + 2] = color[2]

    with open("tools/atlas.ppm", "wb") as f:
        f.write(("P6\n%d %d\n255\n" % (IMG_W, IMG_H)).encode("ascii"))
        f.write(bytes(pixels))

    print("wrote tools/atlas.ppm (%dx%d)" % (IMG_W, IMG_H))


if __name__ == "__main__":
    main()
