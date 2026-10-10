"""placeholders.py - the stand-in art of CRYPT, drawn by code: a red brick
wall, a skeleton, four faces.  Nothing here is meant to last; a PNG of
the same size dropped in its place goes through tools/mkart.py the same
way.  Run it here: it writes the PNGs beside itself."""
from pathlib import Path

from PIL import Image, ImageDraw

HERE = Path(__file__).parent
BLACK, WHITE = (0, 0, 0), (255, 255, 255)
RED, BRIGHT_RED = (170, 0, 0), (255, 0, 0)
YELLOW, BRIGHT_YELLOW = (170, 170, 0), (255, 255, 0)
BLUE, GREEN, CYAN, MAGENTA = (0, 0, 170), (0, 170, 0), (0, 170, 170), (170, 0, 170)


def wall():
    """128x32: four rows of bricks 32 wide and 8 high, the odd rows
    shifted by half a brick, mortar a pixel wide; a texture of 16
    columns of 8 pixels by 32 texels (tex.h)."""
    im = Image.new("RGB", (128, 32), BLACK)
    d = ImageDraw.Draw(im)
    for row in range(4):
        shift = 16 if row % 2 else 0
        for x in range(-16, 128, 32):
            x0, y0 = x + shift, row * 8
            d.rectangle([x0, y0, x0 + 30, y0 + 6], fill=RED)
            d.line([x0, y0, x0 + 30, y0], fill=BRIGHT_RED)      # the lit top edge
            d.line([x0, y0, x0, y0 + 6], fill=BRIGHT_RED)
    im.save(HERE / "wall.png")


def skeleton():
    """32x32 on black, which is the key colour: a skull, ribs, arms and
    legs in white."""
    im = Image.new("RGB", (32, 32), BLACK)
    d = ImageDraw.Draw(im)
    d.ellipse([11, 1, 20, 10], fill=WHITE)                       # the skull
    d.rectangle([13, 6, 14, 7], fill=BLACK)                       # the eyes
    d.rectangle([17, 6, 18, 7], fill=BLACK)
    d.rectangle([13, 9, 18, 10], fill=WHITE)                      # the jaw
    d.line([15, 11, 15, 22], fill=WHITE, width=2)                 # the spine
    for y in (13, 16, 19):                                        # the ribs
        d.line([9, y, 22, y], fill=WHITE)
    d.line([9, 12, 4, 20], fill=WHITE)                            # the arms
    d.line([22, 12, 27, 20], fill=WHITE)
    d.line([13, 22, 10, 31], fill=WHITE, width=2)                 # the legs
    d.line([18, 22, 21, 31], fill=WHITE, width=2)
    im.save(HERE / "skeleton.png")


def face(name, hair, background, skin):
    """32x32: a head of `skin` with `hair` on `background`, eyes and a
    mouth; an image for ms_blit."""
    im = Image.new("RGB", (32, 32), background)
    d = ImageDraw.Draw(im)
    d.ellipse([7, 6, 24, 27], fill=skin)                          # the head
    d.ellipse([7, 2, 24, 14], fill=hair)                          # the hair
    d.rectangle([9, 11, 22, 14], fill=skin)                       # the brow under it
    d.rectangle([11, 15, 13, 16], fill=BLACK)                     # the eyes
    d.rectangle([18, 15, 20, 16], fill=BLACK)
    d.line([13, 22, 18, 22], fill=RED)                            # the mouth
    im.save(HERE / f"{name}.png")


def main():
    wall()
    skeleton()
    face("face1", BLACK, BLUE, BRIGHT_YELLOW)
    face("face2", RED, GREEN, YELLOW)
    face("face3", YELLOW, MAGENTA, BRIGHT_YELLOW)
    face("face4", WHITE, CYAN, YELLOW)


if __name__ == "__main__":
    main()
