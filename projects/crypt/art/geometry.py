"""geometry.py - the tables of the view's geometry, as a C header.

    geometry.py OUT.h

The view is a cylinder round the party: a column of the screen looks
along one direction for all its rows, a pixel is a quarter of an angle
unit (256 to the turn), a sector of 32 cell columns is a quarter turn
and the 22-column view is the middle of it (columns 5..26).  A wall
stands 60 / r rows above and below the horizon at r cells.  Nothing
here is a picture: a wall is drawn on the machine from these numbers -
its outline and its brick rows from the height of the wall in every
pixel column, which never changes as the party steps along it, and
the joints between the bricks from where each falls, which does.

The tables:
  wall_top[L][x], wall_half[L][x], wall_z[L][x]  for the plane half a
        cell + L from the party's axis, on the left: in sector pixel
        column x (0..127, the sector's left half), the wall's first
        row in the window, its half-height in rows, and the depth of
        the point seen in 40ths of a cell past the party; 255, 0 and
        32767 where there is no wall (ahead, or past DEPTH cells).
        The right side is the mirror image: column 255 - x.
  atan_px[r]  how far from the sector's middle, in pixel columns, the
        direction at a tangent of r / 256 lies: a point at lateral l
        and depth d is at atan_px[256 l / d].
  edge_px[k][n][j]  the same for every cell corner: lateral j + 0.5,
        depth n + 0.5 - k/PHASES - the far edge of row n's cells at
        phase k; 128 past the sector's edge.
  front_half[k][n]  half the height of a wall across the axis at depth
        n - 0.5 - k/PHASES, straight ahead.
  col_zmin[L][c], col_zmax[L][c]  the least and greatest of wall_z over
        the eight pixels of sector column c (the left half's columns).
  cross_row[k][n][c]  the window row where the edge of row n's cells
        crosses the floor in view column c at phase k, to CROSS_DEPTH
        cells; 255 below the window.
  col_cos[c], col_tan[c]  of the centre of sector column c, 8.8.
  phase_8[k]  how far past its cell's centre the party is at phase k.
"""
import math
import sys
from pathlib import Path

SECTOR = 256                  # pixels in a quarter turn
COLUMNS = SECTOR // 8
VIEW_FIRST, VIEW_CELLS = 5, 22
ROWS = 120                    # the window's rows
HALF = ROWS // 2              # the horizon: rows above it, rows below
FOCUS = 120.0                 # pixels a cell is tall a cell away
PHASES = 5                    # passes a step takes: the party at k/PHASES
LATERALS = 3                  # planes beside the axis: 0.5, 1.5, 2.5 cells out
DEPTH = 16                    # cells a wall is drawn to
CROSS_DEPTH = 4               # cells the floor's edges across are drawn to: past that they crowd into a block


def angle(x):
    """The angle of the centre of sector pixel column x, radians, 0 at
    the party's way, negative to the left."""
    return (x + 0.5 - SECTOR / 2) * (math.pi / 2) / SECTOR


def px(theta):
    """A direction as pixel columns from the sector's middle, at most 128."""
    return min(int(round(theta * SECTOR / (math.pi / 2))), SECTOR // 2)


def table(name, kind, values):
    """A C array, nested lists as nested braces: its declaration for
    the header and its definition for the C file, since the assembler
    reads the tables too."""
    def text(v):
        if isinstance(v, list):
            return "{" + ", ".join(text(e) for e in v) + "}"
        return str(v)
    dims = ""
    v = values
    while isinstance(v, list):
        dims += f"[{len(v)}]"
        v = v[0]
    return (f"extern const {kind} {name}{dims};", f"const {kind} {name}{dims} = {text(values)};")


def walls():
    tops, halves, depths = [], [], []
    for L in range(LATERALS):
        lateral = L + 0.5
        top, half, depth = [], [], []
        for x in range(SECTOR // 2):
            th = angle(x)
            z = lateral / math.tan(-th)
            if z > DEPTH:
                top.append(255)
                half.append(0)
                depth.append(32767)
                continue
            h = FOCUS / (2 * lateral / math.sin(-th))      # rows above the horizon
            t = int(math.floor(HALF - 0.5 - h)) + 1          # the first row whose centre is inside
            top.append(max(t, 0))
            half.append(min(int(round(h)), 255))
            depth.append(int(round(40 * z)))
        tops.append(top)
        halves.append(half)
        depths.append(depth)
    return tops, halves, depths


def edges():
    out = []
    for k in range(PHASES):
        rows = []
        for n in range(DEPTH + 1):
            d = n + 0.5 - k / PHASES
            rows.append([SECTOR // 2 if d <= 0 else px(math.atan2(j + 0.5, d)) for j in range(4)])
        out.append(rows)
    return out


def fronts():
    out = []
    for k in range(PHASES):
        row = []
        for n in range(DEPTH + 1):
            d = n - 0.5 - k / PHASES
            row.append(0 if d <= 0 else min(int(round(FOCUS / (2 * d))), 255))
        out.append(row)
    return out


def spans(depths):
    """For every sector column, the least and greatest depth its eight
    pixels see on each plane - the left half's columns, the right half
    mirrored by the machine."""
    least, most = [], []
    for L in range(LATERALS):
        lo, hi = [], []
        for c in range(COLUMNS // 2):
            zs = [depths[L][c * 8 + i] for i in range(8) if depths[L][c * 8 + i] != 32767]
            lo.append(min(zs) if zs else 32767)
            hi.append(max(zs) if zs else -1)
        least.append(lo)
        most.append(hi)
    return least, most


def crosses():
    out = []
    for k in range(PHASES):
        rows = []
        for n in range(CROSS_DEPTH + 1):
            d = n + 0.5 - k / PHASES
            cols = []
            for c in range(VIEW_FIRST, VIEW_FIRST + VIEW_CELLS):
                if d <= 0:
                    cols.append(255)
                    continue
                r = d / math.cos(angle(c * 8 + 3.5))         # the distance along the column's way
                row = HALF + int(round(FOCUS / (2 * r)))
                cols.append(row if row < ROWS else 255)
            rows.append(cols)
        out.append(rows)
    return out


def main():
    out = Path(sys.argv[1])
    tops, halves, depths = walls()
    least, most = spans(depths)
    tables = [
        table("wall_top", "unsigned char", tops),
        table("wall_half", "unsigned char", halves),
        table("wall_z", "int", depths),
        table("col_zmin", "int", least),
        table("col_zmax", "int", most),
        table("atan_px", "unsigned char", [px(math.atan(r / 256)) for r in range(257)]),
        table("edge_px", "unsigned char", edges()),
        table("front_half", "unsigned char", fronts()),
        table("cross_row", "unsigned char", crosses()),
        table("col_cos", "int", [int(round(256 * math.cos(angle(c * 8 + 3.5)))) for c in range(COLUMNS)]),
        table("col_tan", "int", [int(round(256 * math.tan(angle(c * 8 + 3.5)))) for c in range(COLUMNS)]),
        table("phase_8", "int", [int(round(256 * k / PHASES)) for k in range(PHASES)]),
    ]
    lines = [
        "/* geometry.h: the view's tables, made by art/geometry.py - not for editing. */",
        "",
        f"#define PLANE_PHASES {PHASES}",
        f"#define PLANE_LATERALS {LATERALS}",
        f"#define PLANE_DEPTH {DEPTH}",
        f"#define CROSS_DEPTH {CROSS_DEPTH}",
    ] + [declaration for declaration, _ in tables]
    out.write_text("\n".join(lines) + "\n", newline="\n")
    source = ["/* geometry.c: the view's tables, made by art/geometry.py - not for editing. */", ""]
    source += [definition for _, definition in tables]
    out.with_suffix(".c").write_text("\n".join(source) + "\n", newline="\n")


if __name__ == "__main__":
    sys.exit(main())
