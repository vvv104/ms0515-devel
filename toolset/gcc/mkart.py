"""mkart.py - a PNG as the C of a texture or an image for the MS 0515.

    mkart.py tex   NAME IN.png OUT.c [--key R,G,B] [--shrink N]
    mkart.py image NAME IN.png OUT.c

The screen has eight colours, bright or not, two to a cell of 8x1
pixels: an attribute byte of ink and paper over a byte of pixels.  Each
pixel of the PNG goes to the nearest of the fifteen colours (black, the
seven at 2/3 and the seven at full), and in every cell the commonest
colour becomes the paper, the next the ink and the rest fall to the
nearer of the two; bright is the ink's.

A `tex` (rt11_devel/toolset/gcc/include/tex.h) is a texture or a sprite
drawn scaled: the PNG's width a multiple of 8, its height up to 255,
written column by column - `const struct tex NAME`.  With --key the
colour given is transparent: a cell of it alone has no pixels, which
tex_sprite leaves unwritten, and the rest of such a cell is paper
black.  With --shrink the PNG is reduced N times each way first, each
texel the mean of its pixels: a texture for the distance, where the
full one would lose its lines between the columns drawn.  An `image`
(ms0515.h) is drawn as it is by ms_blit, written row by row - `const
struct ms_image NAME`.  The C declares nothing but the object; a header
of the project's own says `extern`."""
import argparse
import sys
from pathlib import Path

from PIL import Image

LEVEL = 0xAA                        # the colours at two thirds
COLOURS = [(0, 0, 0)] + [((c >> 1 & 1) * v, (c >> 2 & 1) * v, (c & 1) * v)
                         for v in (LEVEL, 0xFF) for c in range(1, 8)]
# COLOURS[k]: k = 0 black; 1..7 the GRB codes 1..7 at two thirds; 8..14 bright


def nearest(rgb):
    r, g, b = rgb[:3]
    best, at = None, 0
    for k, (cr, cg, cb) in enumerate(COLOURS):
        d = (r - cr) ** 2 + (g - cg) ** 2 + (b - cb) ** 2
        if best is None or d < best:
            best, at = d, k
    return at


def code(k):
    """The GRB code and the bright flag of palette entry k."""
    if k == 0:
        return 0, False
    return (k - 1) % 7 + 1, k > 7


def cell(pixels, key):
    """Eight palette entries (None for the key) -> (pattern, attribute)."""
    counts = {}
    for k in pixels:
        if k is not None:
            counts[k] = counts.get(k, 0) + 1
    ranked = sorted(counts, key=lambda k: -counts[k])
    if not ranked:
        return 0, 0
    paper = ranked[0]
    ink = ranked[1] if len(ranked) > 1 else paper
    if key is not None:
        paper, ink = (0, ranked[0]) if len(ranked) == 1 else (paper, ink)
    pattern = 0
    for i, k in enumerate(pixels):
        if k is None:
            continue
        if k == ink and ink != paper:
            pattern |= 0x80 >> i
        elif k != paper and ink != paper:
            dk = sum((a - b) ** 2 for a, b in zip(COLOURS[k], COLOURS[ink]))
            dp = sum((a - b) ** 2 for a, b in zip(COLOURS[k], COLOURS[paper]))
            if dk < dp:
                pattern |= 0x80 >> i
    inkCode, bright = code(ink)
    paperCode, _ = code(paper)
    attribute = inkCode | (paperCode << 3) | (0o100 if bright else 0)
    return pattern, attribute


def convert(path, key, shrink=1):
    im = Image.open(path).convert("RGB")
    if shrink > 1:
        im = im.resize((im.width // shrink, im.height // shrink), Image.BOX)
    w, h = im.size
    if w % 8 or h > 255:
        raise SystemExit(f"{path}: the width must be a multiple of 8 and the height up to 255")
    keyk = nearest(key) if key else None
    px = im.load()
    patterns = [[0] * (w // 8) for _ in range(h)]
    attributes = [[0] * (w // 8) for _ in range(h)]
    for y in range(h):
        for c in range(w // 8):
            ks = []
            for i in range(8):
                k = nearest(px[c * 8 + i, y])
                ks.append(None if keyk is not None and k == keyk else k)
            patterns[y][c], attributes[y][c] = cell(ks, keyk)
    return w // 8, h, patterns, attributes


def rows(values, per=16):
    out = []
    for i in range(0, len(values), per):
        out.append("\t" + ", ".join("0x%02X" % v for v in values[i:i + per]) + ",")
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("kind", choices=["tex", "image"])
    ap.add_argument("name")
    ap.add_argument("png", type=Path)
    ap.add_argument("out", type=Path)
    ap.add_argument("--key", help="the transparent colour, R,G,B (tex only)")
    ap.add_argument("--shrink", type=int, default=1, help="the PNG reduced N times each way first (tex only)")
    args = ap.parse_args()
    key = tuple(int(v) for v in args.key.split(",")) if args.key else None
    cells, height, patterns, attributes = convert(args.png, key, args.shrink if args.kind == "tex" else 1)
    if args.kind == "tex":
        texels = []
        for c in range(cells):
            for y in range(height):
                texels += [patterns[y][c], attributes[y][c]]
        text = (f"/* {args.name}: {args.png.name} as a texture of {cells} columns by {height} rows,\n"
                f" * a texel a word - the pattern, then the attribute - column by column;\n"
                f" * made by tools/mkart.py - not for editing. */\n\n#include <tex.h>\n\n"
                f"static const unsigned char {args.name}_texels[{cells * height * 2}] = {{\n{rows(texels)}\n}};\n\n"
                f"const struct tex {args.name} = {{{cells}, {height}, {args.name}_texels}};\n")
    else:
        px = [patterns[y][c] for y in range(height) for c in range(cells)]
        at = [attributes[y][c] for y in range(height) for c in range(cells)]
        text = (f"/* {args.name}: {args.png.name} as an image of {cells} cells by {height} rows,\n"
                f" * made by tools/mkart.py - not for editing. */\n\n#include <ms0515.h>\n\n"
                f"static const unsigned char {args.name}_pixels[{cells * height}] = {{\n{rows(px)}\n}};\n\n"
                f"static const unsigned char {args.name}_attributes[{cells * height}] = {{\n{rows(at)}\n}};\n\n"
                f"const struct ms_image {args.name} = {{{cells}, {height}, {args.name}_pixels, {args.name}_attributes}};\n")
    args.out.write_text(text, newline="\n")


if __name__ == "__main__":
    sys.exit(main())
