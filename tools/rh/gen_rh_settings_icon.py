#!/usr/bin/env python3
# Romhack: the Randomizer Settings key item's icon (owner: a "New Nintendo 3DS XL"): an open Metallic Blue New 3DS XL
# with lit screens, circle pad, D-pad, C-stick and the coloured ABXY buttons. 32x32, 16 colours, index 0 transparent,
# like the game's other item icons. Writes data/graphics/item/rh_randomizer_settings.png.
# Usage: python3 tools/rh/gen_rh_settings_icon.py [preview.png]
import sys

from PIL import Image

PAL = [
    (0, 255, 255),      # 0 transparent
    (24, 24, 40),       # 1 outline
    (40, 64, 144),      # 2 body shadow
    (64, 104, 200),     # 3 body
    (128, 168, 240),    # 4 body highlight
    (48, 48, 56),       # 5 bezel
    (88, 168, 216),     # 6 screen
    (184, 232, 248),    # 7 screen glare
    (176, 176, 184),    # 8 grey
    (104, 104, 112),    # 9 grey shadow
    (232, 64, 56),      # 10 A red
    (240, 200, 48),     # 11 B yellow
    (80, 120, 248),     # 12 X blue
    (72, 184, 88),      # 13 Y green
    (248, 248, 248),    # 14 white
    (0, 0, 0),          # 15 unused
]


def draw():
    px = [[0] * 32 for _ in range(32)]

    def rect(x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                px[y][x] = c

    def shell(x0, y0, x1, y1):
        # rounded body with outline, highlight on the top / left edge, shadow on the bottom / right
        rect(x0 + 1, y0, x1 - 1, y1, 1)
        rect(x0, y0 + 1, x1, y1 - 1, 1)
        rect(x0 + 1, y0 + 1, x1 - 1, y1 - 1, 3)
        rect(x0 + 2, y0 + 1, x1 - 2, y0 + 1, 4)
        rect(x0 + 1, y0 + 2, x0 + 1, y1 - 2, 4)
        rect(x0 + 2, y1 - 1, x1 - 1, y1 - 1, 2)
        rect(x1 - 1, y0 + 2, x1 - 1, y1 - 1, 2)

    # (about 24x24, centred in the 32x32 icon like the game's other key items)
    # top lid: wide 3D screen in a black bezel, camera above it
    shell(4, 4, 27, 14)
    rect(6, 6, 25, 12, 5)
    rect(8, 7, 23, 11, 6)
    for i in range(3):                          # diagonal glare
        rect(9 + i, 7 + i, 10 + i, 7 + i, 7)
    rect(15, 5, 16, 5, 9)
    # hinge
    rect(5, 15, 26, 15, 1)
    rect(6, 15, 25, 15, 2)
    # bottom half: touch screen, controls
    shell(4, 16, 27, 27)
    rect(11, 17, 20, 24, 5)
    rect(12, 18, 19, 23, 6)
    rect(13, 18, 14, 18, 7)
    px[19][13] = 7
    # circle pad (top left) and D-pad (below it)
    rect(6, 18, 8, 20, 9)
    rect(6, 18, 7, 19, 8)
    rect(7, 23, 7, 25, 1)
    rect(6, 24, 8, 24, 1)
    # C-stick nub and ABXY (X top blue, Y left green, A right red, B bottom yellow)
    px[17][25] = 9
    for (x, y, c) in ((23, 18, 12), (21, 20, 13), (25, 20, 10), (23, 22, 11)):
        rect(x, y, x + 1, y + 1, c)
    # start / select
    rect(13, 26, 14, 26, 9)
    rect(17, 26, 18, 26, 9)
    return px


def main():
    px = draw()
    im = Image.new('P', (32, 32))
    flat = [v for c in PAL for v in c]
    im.putpalette(flat)
    im.putdata([c for row in px for c in row])
    im.save('data/graphics/item/rh_randomizer_settings.png', transparency=0)
    if len(sys.argv) > 1:
        im.convert('RGB').resize((256, 256), Image.NEAREST).save(sys.argv[1])


if __name__ == '__main__':
    main()
