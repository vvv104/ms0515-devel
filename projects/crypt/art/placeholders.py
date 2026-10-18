"""placeholders.py - the stand-in art of CRYPT, drawn by code: a
skeleton, four faces drawn pixel by pixel.  Nothing here is meant to last; a PNG of the same
size dropped in its place goes through rt11_devel/toolset/gcc/mkart.py the same way.
Run it here: it writes the PNGs beside itself.  The walls are not
pictures: view.c draws them from geometry.py's tables."""
from pathlib import Path

from PIL import Image, ImageDraw

HERE = Path(__file__).parent
BLACK, WHITE = (0, 0, 0), (255, 255, 255)
BRIGHT_RED = (255, 0, 0)
BRIGHT_YELLOW = (255, 255, 0)
BRIGHT_BLUE, BRIGHT_GREEN = (0, 0, 255), (0, 255, 0)
BRIGHT_CYAN, BRIGHT_MAGENTA = (0, 255, 255), (255, 0, 255)


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


# The faces, drawn pixel by pixel: '.' the background, 'h' the hair,
# 's' the skin, 'k' black, 'r' the lips, 'c' the clothes.  A cell of the
# screen is 8x1 pixels of two colours, both bright or both not but for
# black: the face proper fills the middle two cells of each row, so
# that skin, black and the lips meet there and nothing else, and the
# hair, the ears and the background keep to the cells either side.
MAN = [
    "................................",
    "................................",
    "...........hhhhhhhhhh...........",
    ".........hhhhhhhhhhhhhh.........",
    "........hhhhhhhhhhhhhhhh........",
    ".......hhhhhhhhhhhhhhhhhh.......",
    ".......hhhhhhhhhhhhhhhhhh.......",
    ".......hhhhhhhhhhhhhhhhhh.......",
    ".......hhhhhhhssshhhhhhhh.......",
    ".......hhhsssssssssssssshh......",
    ".......hhsssssssssssssssh.......",
    "........hsskkkksssskkkkss.......",
    "......ssssssssssssssssssss......",
    "......sssskkksssssskkkssss......",
    "......ssssskksssssskkssss.......",
    "......ssssssssssssssssssss......",
    ".......sssssssskssssssssss......",
    "........sssssssksssssssss.......",
    "........sssssskssksssssss.......",
    "........ssssssskkssssssss.......",
    "........ssssssssssssssss........",
    "........sssssrrrrrrsssss........",
    "........ssssssssssssssss........",
    ".........ssssssssssssss.........",
    "..........ssssssssssss..........",
    "............ssssssss............",
    "............ssssssss............",
    ".............ssssss.............",
    "....ccccccccccssssccccccc.......",
    "..ccccccccccccsssscccccccccccc..",
    ".ccccccccccccccssccccccccccccccc",
    "cccccccccccccccccccccccccccccccc",
]

# The bearded one: no sideburn by the brow, where it would meet the
# skin and the black; the mouth black in the beard.
BEARDED = MAN[:]
BEARDED[11] = "........ssskkkksssskkkkss......."
BEARDED[20] = "........sssssshhhhssssss........"
BEARDED[21] = "........hssshhhhhhhhsssh........"
BEARDED[22] = "........hhhhhkkkkkkhhhhh........"
BEARDED[23] = ".........hhhhhhhhhhhhhh........."
BEARDED[24] = "..........hhhhhhhhhhhh.........."
BEARDED[25] = "...........hhhhhhhhhh..........."
BEARDED[26] = "............hhhhhhhh............"


def _woman():
    """Long hair down both sides to the shoulders, the face between."""
    top = ["................................",
           "...........hhhhhhhhhh...........",
           ".........hhhhhhhhhhhhhh.........",
           "........hhhhhhhhhhhhhhhh........",
           ".......hhhhhhhhhhhhhhhhhh.......",
           "......hhhhhhhhhhhhhhhhhhhh......",
           "......hhhhhhhhhhhhhhhhhhhh......",
           ".....hhhhhhhhhhhhhhhhhhhhhh.....",
           ".....hhhhhhhhhhhhhhhhhhhhhh....."]
    middle = ["hhsssssssshhhhhh", "sssssssssssssshh", "sskkksssssskkkss", "ssssssssssssssss",
              "skkkksssssskkkks", "sskkksssssskkkss", "ssssssssssssssss", "ssssssskssssssss",
              "ssssssskssssssss", "ssssssksksssssss", "ssssssssssssssss", "sssssrrrrrrsssss",
              "ssssssrrrrssssss", "ssssssssssssssss", "hssssssssssssssh", "hhsssssssssssshh",
              "hhhhsssssssshhhh", "hhhhhsssssshhhhh", "hhhhhsssssshhhhh", "ccccccsssscccccc",
              "cccccccssccccccc", "cccccccccccccccc", "cccccccccccccccc"]

    def side(y):
        return ".....hhh" if y < 17 else "....hhhh" if y < 24 else "...hhhhh" if y < 28 else "..hhhhhh"
    return top + [side(y) + row + side(y)[::-1] for y, row in enumerate(middle, start=len(top))]


WOMAN = _woman()


def face(name, grid, background, hair, clothes):
    """32x32 from a grid above, the skin bright white, the lips bright
    red; an image for ms_blit.  Every cell is checked to be one
    mkart.py puts down as it is."""
    colours = {".": background, "h": hair, "s": WHITE, "k": BLACK, "r": BRIGHT_RED, "c": clothes}
    im = Image.new("RGB", (32, 32))
    for y, row in enumerate(grid):
        assert len(row) == 32, (name, y)
        for c in range(0, 32, 8):
            used = {colours[k] for k in row[c:c + 8]}
            lit = {max(rgb) for rgb in used} - {0}
            assert len(used) <= 2 and len(lit) <= 1, (name, y, row[c:c + 8])
        for x, k in enumerate(row):
            im.putpixel((x, y), colours[k])
    im.save(HERE / f"{name}.png")


def main():
    skeleton()
    face("face1", MAN, BRIGHT_BLUE, BLACK, BRIGHT_RED)                  # ARNO
    face("face2", BEARDED, BRIGHT_GREEN, BRIGHT_RED, BRIGHT_BLUE)       # OTTO
    face("face3", WOMAN, BRIGHT_MAGENTA, BRIGHT_YELLOW, BRIGHT_CYAN)    # ELSA
    face("face4", WOMAN, BRIGHT_CYAN, BLACK, BRIGHT_MAGENTA)            # VERA


if __name__ == "__main__":
    main()
