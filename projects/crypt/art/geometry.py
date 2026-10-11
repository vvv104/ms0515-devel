"""geometry.py - the tables of the view's geometry, as a C header.

    geometry.py OUT.h

The view is a cylinder round the party: a column of the screen looks
along one direction for all its rows, a pixel is a quarter of an angle
unit (256 to the turn), a sector of 32 cell columns is a quarter turn
and the 22-column view is the middle of it (columns 5..26).  A wall
stands 60 / r rows above and below the horizon at r cells along the
column's way.  A cell column of the screen is drawn from the one ray
down its middle: what that ray meets, the walls' heights and the
floor's edges, are tables of the column's angle.

The tables:
  col_sin[c], col_cos[c], col_tan[c]  of the middle of sector column c,
        8.8: the sine's magnitude, the cosine, the signed tangent.
  front_half[k][n]  half the height of a wall across the axis at depth
        n - 0.5 - k/PHASES, straight ahead, in rows; by the cosine for
        a column aside.
  edge_px[k][n][j]  how far from the sector's middle, in pixel columns,
        the corner of a cell falls: lateral j + 0.5, depth
        n + 0.5 - k/PHASES - the far edge of row n's cells at phase k;
        128 past the sector's edge.
  cross_row[k][n][c]  the window row where the far edge of row n's
        cells crosses the floor in sector column c at phase k, to
        CROSS_DEPTH cells; 255 below the window.
  phase_8[k]  how far past its cell's centre the party is at phase k,
        256ths of a cell.
"""
import math
import sys
from pathlib import Path

SECTOR = 256                  # pixels in a quarter turn
COLUMNS = SECTOR // 8
ROWS = 120                    # the window's rows
HALF = ROWS // 2              # the horizon: rows above it, rows below
FOCUS = 120.0                 # pixels a cell is tall a cell away
PHASES = 5                    # passes a step takes: the party at k/PHASES
LATERALS = 4                  # cells beside the axis whose corners are tabled
DEPTH = 16                    # cells a wall is drawn to
CROSS_DEPTH = 8               # cells the floor's edges across are tabled to


def angle(x):
    """The angle of the centre of sector pixel column x, radians, 0 at
    the party's way, negative to the left."""
    return (x + 0.5 - SECTOR / 2) * (math.pi / 2) / SECTOR


def middle(c):
    """The angle of the middle of sector column c."""
    return angle(c * 8 + 3.5)


def px(theta):
    """A direction as pixel columns from the sector's middle, at most 128."""
    return min(int(round(theta * SECTOR / (math.pi / 2))), SECTOR // 2)


def table(name, kind, values):
    """A C array, nested lists as nested braces."""
    def text(v):
        if isinstance(v, list):
            return "{" + ", ".join(text(e) for e in v) + "}"
        return str(v)
    dims = ""
    v = values
    while isinstance(v, list):
        dims += f"[{len(v)}]"
        v = v[0]
    return f"static const {kind} {name}{dims} = {text(values)};"


def walls():
    """wall_half[L][x]: half the height, in rows, of a wall L + 0.5 cells
    aside in sector pixel column x of the left half, 0 past DEPTH or
    straight ahead; the right half is the mirror image, column
    255 - x."""
    out = []
    for L in range(LATERALS):
        lateral = L + 0.5
        half = []
        for x in range(SECTOR // 2):
            th = angle(x)
            if lateral / math.tan(-th) > DEPTH:                # the plane's cell ahead is past the depth drawn
                half.append(0)
                continue
            r = lateral / math.sin(-th)                        # along the ray to the plane
            half.append(max(1, min(int(round(FOCUS / (2 * r))), 255)))
        out.append(half)
    return out


def edges():
    out = []
    for k in range(PHASES):
        rows = []
        for n in range(DEPTH + 1):
            d = n + 0.5 - k / PHASES
            rows.append([SECTOR // 2 if d <= 0 else px(math.atan2(j + 0.5, d)) for j in range(LATERALS)])
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


def crosses():
    out = []
    for k in range(PHASES):
        rows = []
        for n in range(CROSS_DEPTH + 1):
            d = n + 0.5 - k / PHASES
            cols = []
            for c in range(COLUMNS):
                if d <= 0:
                    cols.append(255)
                    continue
                r = d / math.cos(middle(c))                   # the distance along the column's way
                row = HALF + int(round(FOCUS / (2 * r)))
                cols.append(row if row < ROWS else 255)
            rows.append(cols)
        out.append(rows)
    return out


def main():
    out = Path(sys.argv[1])
    lines = [
        "/* geometry.h: the view's tables, made by art/geometry.py - not for editing. */",
        "",
        f"#define PLANE_PHASES {PHASES}",
        f"#define PLANE_LATERALS {LATERALS}",
        f"#define PLANE_DEPTH {DEPTH}",
        f"#define CROSS_DEPTH {CROSS_DEPTH}",
        table("col_sin", "int", [int(round(256 * abs(math.sin(middle(c))))) for c in range(COLUMNS)]),
        table("col_cos", "int", [int(round(256 * math.cos(middle(c)))) for c in range(COLUMNS)]),
        table("col_tan", "int", [int(round(256 * math.tan(middle(c)))) for c in range(COLUMNS)]),
        table("wall_half", "unsigned char", walls()),
        table("front_half", "unsigned char", fronts()),
        table("edge_px", "unsigned char", edges()),
        table("cross_row", "unsigned char", crosses()),
        table("phase_8", "int", [int(round(256 * k / PHASES)) for k in range(PHASES)]),
    ]
    out.write_text("\n".join(lines) + "\n", newline="\n")


if __name__ == "__main__":
    sys.exit(main())
