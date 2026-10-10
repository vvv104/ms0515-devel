"""planes.py - the side walls of the corridor drawn on the host, for the
machine to copy: CRYPT.DAT and the C header that says where in it what
lies.

    planes.py WALL.png OUT.DAT OUT.h [--mkart DIR] [--report]

The view is a cylinder round the party: a column of the screen looks
along one direction for all its rows, a pixel is a quarter of an angle
unit (256 to the turn), a sector of 32 cell columns is a quarter turn
and the 22-column view is the middle of it (columns 5..26).  When the
party steps along a corridor the walls beside it do not change their
angle - every pixel column keeps its height and its rows keep their
texture row - only the texture runs along them.  So the side planes,
at half a cell, one and a half and two and a half from the party's
axis, are drawn here once for each phase of the step, pixel by pixel
from the wall's PNG, and reduced to the screen's cells (mkart.py's way:
two colours a cell); the machine copies the columns a face of a wall
takes.  The left side only: the right is its mirror image, which the
machine draws by turning each cell's pixels round (the bricks are
symmetrical, a lit edge aside), and so the data is half.  Phase 0, the
party on its cell's centre, is drawn for the whole sector, since a turn
scrolls through it; the others for the view only.

CRYPT.DAT: two slots of 8 KB, read into the halves of the bank at
060000 - the primary, which RT-11 left free above the program, and the
extended behind it - and inside a slot the images one after another.
An image: 32 words, the offset
of each sector column's record from the image's start (0: no column),
then the records: a byte top, a byte bottom (window rows, 0..119), and
a word a row, the pattern under the attribute, as video memory has it.
The header gives each image's slot and offset as a table, and the
tables the geometry needs: the pixel column of every face edge for
every phase, the cosine and the tangent of every column, the heights
of the front walls.
"""
import argparse
import math
import struct
import sys
from pathlib import Path

SECTOR = 256                  # pixels in a quarter turn
COLUMNS = SECTOR // 8
VIEW_FIRST, VIEW_LAST = 5, 26  # the view's columns of the sector
ROWS = 120                    # the window's rows
HALF = ROWS // 2              # the horizon: rows above it, rows below
FOCUS = 120.0                 # pixels a cell is tall a cell away
PHASES = 5                    # passes a step takes: the party at k/PHASES (six overflow the slots)
LATERALS = (0.5, 1.5, 2.5)    # the planes drawn, cells from the axis
DEPTH = 16                    # cells a plane is drawn to: past that it is five pixels from the middle
SLOT_SIZE = 8192
SLOT_ADDRESSES = (0o60000, 0o60000)   # bank 3: its primary half, then its extended
SLOT_EXTENDED = (0, 1)


def angle(x):
    """The angle of the centre of sector pixel column x, radians, 0 at
    the party's way, negative to the left."""
    return (x + 0.5 - SECTOR / 2) * (math.pi / 2) / SECTOR


def plane_column(tex, lateral, side, phase, x):
    """The rows of one pixel column of a side plane: [(row, (r, g, b))]
    for the rows the wall covers, or [] - the plane at `lateral` cells
    to the right (side 1) or the left (0), the party phase/PHASES past
    its cell's centre."""
    th = angle(x)
    if (side == 1) != (th > 0) or th == 0:
        return []
    z = lateral / math.tan(abs(th))          # depth of the hit, from the party
    if z > DEPTH:
        return []
    r = lateral / math.sin(abs(th))          # the distance: how tall it stands
    half = FOCUS / (2 * r)                   # rows above and below the horizon
    s = z + phase / PHASES                   # along the wall, cells from the cell's centre
    u = int((s % 1.0) * tex.width) % tex.width
    out = []
    for row in range(ROWS):
        yy = row + 0.5 - HALF
        if abs(yy) < half:
            v = int((yy / half + 1.0) / 2.0 * tex.height)
            out.append((row, tex.px[u, min(v, tex.height - 1)]))
    return out


def wall_cell(pixels, wallInk):
    """Eight palette entries, 0 where there is no wall or the wall is
    black -> (pattern, attribute).  Black must stay black, or the wall's
    edge would creep into the ceiling by a cell's worth of pixels: with
    any black in the cell the paper is black and the ink the commonest
    of the rest, which the others join; a cell of wall alone goes
    mkart's way, two colours of its own.  A cell whose wall pixels are
    all black - a line of mortar at the wall's edge - keeps the wall's
    own ink with no pixels lit, so that the wall's extent on the screen
    is told by the attributes alone, whatever the texture."""
    if 0 not in pixels:
        return cell(pixels, None)
    counts = {}
    for k in pixels:
        if k:
            counts[k] = counts.get(k, 0) + 1
    if not counts:
        inkCode, bright = code(wallInk)
        return 0, inkCode | (0o100 if bright else 0)
    ink = max(counts, key=lambda k: counts[k])
    pattern = 0
    for i, k in enumerate(pixels):
        if k:
            pattern |= 0x80 >> i
    inkCode, bright = code(ink)
    return pattern, inkCode | (0o100 if bright else 0)


def plane_image(tex, nearest, lateral, side, phase, columns):
    """The image of a plane: {column: (top, bottom, [(pattern, attribute)])}."""
    image = {}
    for c in columns:
        pixels = [[None] * 8 for _ in range(ROWS)]   # None: not wall
        rows = set()
        for i in range(8):
            for row, rgb in plane_column(tex, lateral, side, phase, c * 8 + i):
                pixels[row][i] = nearest(rgb)
                rows.add(row)
        if not rows:
            continue
        top, bottom = min(rows), max(rows)
        words = [wall_cell([0 if k is None else k for k in pixels[row]], tex.ink) for row in range(top, bottom + 1)]
        image[c] = (top, bottom, words)
    return image


def encode(image):
    """An image as bytes: the 32 offset words, then the records."""
    offsets = [0] * COLUMNS
    body = bytearray()
    for c in range(COLUMNS):
        if c not in image:
            continue
        top, bottom, words = image[c]
        offsets[c] = COLUMNS * 2 + len(body)
        body += bytes((top, bottom))
        for pattern, attribute in words:
            body += bytes((pattern, attribute))
    if len(body) & 1:
        body += b"\0"
    return struct.pack("<%dH" % COLUMNS, *offsets) + bytes(body)


class Texture:
    def __init__(self, path, nearest):
        from PIL import Image
        im = Image.open(path).convert("RGB")
        self.width, self.height = im.size
        self.px = im.load()
        counts = {}                       # the wall's own colour: its commonest but black
        for y in range(self.height):
            for x in range(self.width):
                k = nearest(self.px[x, y])
                if k:
                    counts[k] = counts.get(k, 0) + 1
        self.ink = max(counts, key=lambda k: counts[k]) if counts else 7


def pack(images):
    """The images into slots, the largest first into the first slot
    with room: [(slot, offset)] in the images' order, and the slots'
    bytes."""
    slots = []
    where = [None] * len(images)
    for i in sorted(range(len(images)), key=lambda i: -len(images[i])):
        data = images[i]
        if len(data) > SLOT_SIZE:
            raise SystemExit("an image larger than a slot")
        for s, slot in enumerate(slots):
            if len(slot) + len(data) <= SLOT_SIZE:
                break
        else:
            slots.append(bytearray())
            s = len(slots) - 1
        where[i] = (s, len(slots[s]))
        slots[s] += data
    if len(slots) > len(SLOT_ADDRESSES):
        raise SystemExit(f"the images take {len(slots)} slots, there are {len(SLOT_ADDRESSES)}")
    return where, slots


def tables():
    """The geometry the machine needs, as C: edge columns, heights,
    cosines and tangents."""
    lines = []
    # edge_px[phase][n][j]: how far from the sector's middle, in pixel
    # columns, the point at lateral j + 0.5 cells and depth n + 0.5 -
    # phase/PHASES stands - the far edge of row n's cells; 128 when at
    # or past the sector's edge, or behind the party.
    lines.append(f"static const unsigned char edge_px[{PHASES}][{DEPTH + 1}][4] = {{")
    for k in range(PHASES):
        lines.append("\t{")
        for n in range(DEPTH + 1):
            d = n + 0.5 - k / PHASES
            vals = []
            for j in range(4):
                if d <= 0:
                    vals.append(SECTOR // 2)
                else:
                    th = math.atan2(j + 0.5, d)
                    px = int(round(th * SECTOR / (math.pi / 2)))
                    vals.append(min(px, SECTOR // 2))
            lines.append("\t\t{" + ", ".join(str(v) for v in vals) + "},")
        lines.append("\t},")
    lines.append("};")
    # front_half[phase][n]: half the height, in rows, of a wall whose
    # face is at depth n - 0.5 - phase/PHASES, straight ahead; 0 when
    # the face is behind the party.
    lines.append(f"static const unsigned char front_half[{PHASES}][{DEPTH + 1}] = {{")
    for k in range(PHASES):
        vals = []
        for n in range(DEPTH + 1):
            d = n - 0.5 - k / PHASES
            vals.append(0 if d <= 0 else min(int(round(FOCUS / (2 * d))), 255))
        lines.append("\t{" + ", ".join(str(v) for v in vals) + "},")
    lines.append("};")
    # col_cos[c], col_tan[c]: of the centre of sector column c, 8.8.
    cosines = [int(round(256 * math.cos(angle(c * 8 + 3.5)))) for c in range(COLUMNS)]
    tangents = [int(round(256 * math.tan(angle(c * 8 + 3.5)))) for c in range(COLUMNS)]
    lines.append(f"static const int col_cos[{COLUMNS}] = {{" + ", ".join(str(v) for v in cosines) + "};")
    lines.append(f"static const int col_tan[{COLUMNS}] = {{" + ", ".join(str(v) for v in tangents) + "};")
    # phase_8[k]: how far past its cell's centre the party is at phase k, 8.8.
    lines.append(f"static const int phase_8[{PHASES}] = {{" +
                 ", ".join(str(int(round(256 * k / PHASES))) for k in range(PHASES)) + "};")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("wall", type=Path)
    ap.add_argument("dat", type=Path)
    ap.add_argument("header", type=Path)
    ap.add_argument("--mkart", type=Path, default=Path(__file__).resolve().parents[4] / "tools",
                    help="the folder of mkart.py (the palette and the cells)")
    ap.add_argument("--report", action="store_true", help="the images' sizes on stdout")
    args = ap.parse_args()
    sys.path.insert(0, str(args.mkart))
    global cell, code
    from mkart import cell, code, nearest

    tex = Texture(args.wall, nearest)
    images, names = [], []
    for phase in range(PHASES):
        columns = range(COLUMNS) if phase == 0 else range(VIEW_FIRST, VIEW_LAST + 1)
        for li, lateral in enumerate(LATERALS):
            data = encode(plane_image(tex, nearest, lateral, 0, phase, columns))
            images.append(data)
            names.append((li, phase))
            if args.report:
                print(f"phase {phase} lateral {lateral}: {len(data)} bytes")
    where, slots = pack(images)
    if args.report:
        print(f"{sum(len(s) for s in slots)} bytes in {len(slots)} slots: " +
              ", ".join(str(len(s)) for s in slots))

    with open(args.dat, "wb") as f:
        for s in slots:
            f.write(bytes(s) + bytes(SLOT_SIZE - len(s)))

    refs = [[None] * PHASES for _ in LATERALS]
    for (li, phase), (slot, offset) in zip(names, where):
        refs[li][phase] = f"{{{slot}, {offset}}}"
    out = [
        "/* planes.h: the side walls drawn by art/planes.py - not for editing.",
        " * A plane image's slot and offset in CRYPT.DAT (plane_ref); the",
        " * geometry tables the view needs. */",
        "",
        f"#define PLANE_PHASES {PHASES}",
        f"#define PLANE_LATERALS {len(LATERALS)}",
        f"#define PLANE_DEPTH {DEPTH}",
        f"#define PLANE_SLOTS {len(slots)}",
        f"#define PLANE_SLOT_BLOCKS {SLOT_SIZE // 512}",
        f"static const unsigned plane_slot_address[{len(SLOT_ADDRESSES)}] = {{" + ", ".join("0%o" % a for a in SLOT_ADDRESSES) + "};",
        f"static const unsigned char plane_slot_extended[{len(SLOT_ADDRESSES)}] = {{" + ", ".join(str(e) for e in SLOT_EXTENDED) + "};",
        "struct plane_ref { unsigned char slot; unsigned offset; };",
        f"static const struct plane_ref plane_refs[{len(LATERALS)}][{PHASES}] = {{",
    ]
    for li in range(len(LATERALS)):
        out.append("\t{" + ", ".join(refs[li]) + "},")
    out.append("};")
    out.append(tables())
    args.header.write_text("\n".join(out) + "\n", newline="\n")


if __name__ == "__main__":
    sys.exit(main())
