/* view.c - the view from the party's eyes (view.h).
 *
 * Two halves.  The first finds what each column of the sector shows:
 * the cells about the party gone through front to back, row by row
 * away from it, and each face of a wall takes the pixel columns not
 * yet taken, from a table of where every cell corner falls for every
 * phase of a step - in a row the faces towards the party first, then
 * the faces along the axis; a wall along the axis is one face however
 * many cells long, taken when it ends, or before a face that could
 * show past its beginning.  What a face leaves in a cell column is a
 * piece: the pixels it covers as a mask of eight, and which wall.
 *
 * The second draws the pieces, from numbers, not pictures.  A wall
 * along the axis stands 60 / r rows above and below the horizon at r
 * cells, a height every pixel column keeps while the party steps
 * along it: its outline, and the three rows of mortar between its
 * four rows of bricks at the quarters of that height, are composed
 * into a layer kept in memory - a column again only when its pieces
 * change - and the layer is copied to the screen each pass.  Over it
 * go what moves: the joints between the bricks, where each falls at
 * this phase, the edges of the floor's cells, and the walls across
 * the axis, which grow as the party comes.  A wall is red with black
 * lines, the floor green, the ceiling black. */

#include <fx.h>
#include <ms0515.h>
#include "geometry.h"
#include "map.h"
#include "view.h"

#define HALF (VIEW_ROWS / 2)		/* the horizon's row in the window */
#define MIDDLE (SECTOR_CELLS * 4)	/* the sector's middle pixel column */
#define MAX_PIECES 4
#define LATERALS PLANE_LATERALS		/* cells beside the axis looked at */
#define BRICK 20			/* a brick's length along a wall, 40ths of a cell: half a cell, twice its height */
#define WALL_ATTR (MS_PAPER(2) | MS_INK(0))
#define FLOOR_ATTR (MS_PAPER(4) | MS_INK(0))
#define CEILING_ATTR 0
#define FAR_ATTR MS_INK(2)
#define FAR_PIXELS 0x55			/* past the depth drawn: a dim bar on the horizon */

enum { kPlane, kFront, kFar };

/* The pixel bits by position, and the masks of the pixels from one on
 * and up to one: tables, since the compiler's shift by a variable
 * count is the third trap (toolset README.md). */
static const unsigned char bitAt[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
static const unsigned char fromPixel[9] = {0xFF, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01, 0x00};
static const unsigned char toPixel[9] = {0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFF};

struct piece {
	unsigned char mask, kind;
	unsigned char lateral, mirror;	/* kPlane: the plane, and which side (1: the right) */
	unsigned char top, bottom;	/* kFront, kFar: window rows, both in */
	unsigned char half;		/* kFront: rows above the horizon */
	signed char n, j;		/* kFront: the cell's row and lateral */
	int z0, z1;			/* kPlane: the face's depths, 40ths of a cell past the party */
};

/* The columns of a range, at most the view's width: pieces[c - rangeFirst]. */
static struct piece pieces[VIEW_CELLS][MAX_PIECES];
static unsigned char count[VIEW_CELLS];
static unsigned char covered[VIEW_CELLS];

/* The layer: the view's columns composed, as the screen keeps them,
 * and the pieces each was composed from. */
static unsigned char layer[VIEW_ROWS][VIEW_CELLS * 2] __attribute__((aligned(2)));	/* words: an even address */
struct plane { unsigned char mask, lateral, mirror; };	/* a wall along the axis, as the layer knows it */
static struct plane layerPieces[VIEW_CELLS][MAX_PIECES];
static unsigned char layerCount[VIEW_CELLS];
static unsigned char floorFrom[VIEW_CELLS];	/* the layer's first floor row, a screen column each */
/* A column's rows of mortar and of floor lines while it is composed:
 * the pixels of each, and which rows were touched, to clear them after
 * - a clear of the whole is a byte at a time, dearer than the column. */
static unsigned char mortar[VIEW_ROWS], lines[VIEW_ROWS];
static unsigned char mortarRows[MAX_PIECES * 8 * 3], lineRows[LATERALS * 8];
static int mortarN, lineN;

static void setMortar(int row, unsigned char bit)
{
	if (!mortar[row])
		mortarRows[mortarN++] = (unsigned char)row;
	mortar[row] |= bit;
}

static void setLine(int row, unsigned char bit)
{
	if (!lines[row])
		lineRows[lineN++] = (unsigned char)row;
	lines[row] |= bit;
}

static void clearRows(void)
{
	while (mortarN > 0)
		mortar[mortarRows[--mortarN]] = 0;
	while (lineN > 0)
		lines[lineRows[--lineN]] = 0;
}

static int cellX, cellY, phase;
static int fx, fy, rx, ry;		/* the facing: forward and right, in cells */
static int rangeFirst, rangeLast, rangeBase, uncovered;
#define IX(c) ((c) - rangeFirst + rangeBase)	/* sector column c's index in the range's tables */
static int turnDir, turnFacing;		/* the turn in progress: the old facing's sector slides */
static struct view turnView;

static void facing(int f)
{
	static const signed char dx[4] = {1, 0, -1, 0}, dy[4] = {0, 1, 0, -1};

	fx = dx[f];
	fy = dy[f];
	rx = -fy;			/* the right hand of someone facing (fx, fy), y down the map */
	ry = fx;
}

/* The cells of row n, j = -LATERALS..LATERALS to the right (left when
 * j < 0): wall or not, into walls[j + LATERALS].  A cell past the
 * level's edge is wall. */
static int rowX, rowY;			/* the leftmost cell of the row in hand */

static void rowWalls(unsigned char *walls)
{
	int x = rowX, y = rowY, j;

	for (j = 0; j < 2 * LATERALS + 1; j++, x += rx, y += ry)
		walls[j] = (unsigned char)(x < 0 || y < 0 || x >= MAP_SIDE || y >= MAP_SIDE || map[(y << 5) + x]);
	rowX += fx;				/* on to the next row */
	rowY += fy;
}

/* -- The pieces ------------------------------------------------------- */

/* The pixels of column c inside [x0, x1) not taken yet, as a mask. */
static unsigned char span(int c, int x0, int x1)
{
	unsigned char m = 0xFF;
	int lo = c << 3, hi = lo + 8;

	if (x0 > lo)
		m &= fromPixel[x0 - lo];
	if (x1 < hi)
		m &= toPixel[x1 - lo];
	return m & ~covered[IX(c)];
}

static void cover(int c, unsigned char mask)
{
	int i = IX(c);

	covered[i] |= mask;
	if (covered[i] == 0xFF)
		uncovered--;
}

static struct piece *take(int c, unsigned char mask)
{
	struct piece *p;
	int i = IX(c);

	cover(c, mask);
	if (count[i] >= MAX_PIECES)
		return 0;
	p = &pieces[i][count[i]++];
	p->mask = mask;				/* the rest is the taker's to set */
	return p;
}

/* The column's last piece taken - the farthest so far - or 0. */
static struct piece *last(int c)
{
	int i = IX(c);

	return count[i] ? &pieces[i][count[i] - 1] : 0;
}

static void farPiece(int c, unsigned char mask)
{
	struct piece *p = take(c, mask);

	if (p) {
		p->kind = kFar;
		p->top = HALF - 1;
		p->bottom = HALF;
	}
}

/* The columns a face of [x0, x1) touches inside the range. */
static int columns(int x0, int x1, int *c0, int *c1)
{
	if (x1 <= x0)
		return 0;
	*c0 = x0 >> 3;
	*c1 = (x1 - 1) >> 3;
	if (*c0 < rangeFirst)
		*c0 = rangeFirst;
	if (*c1 > rangeLast)
		*c1 = rangeLast;
	return *c0 <= *c1;
}

/* A wall along the axis, `lateral` planes out on the left or (mirror)
 * the right, between pixel columns x0 and x1, from depth z0 to z1 in
 * 40ths of a cell past the party.  A column's last piece of the same
 * plane - the run of cells before - grows by these pixels. */
static void sideFace(int x0, int x1, int lateral, int mirror, int z0, int z1)
{
	struct piece *p;
	unsigned char m;
	int c, c0, c1;

	if (!columns(x0, x1, &c0, &c1))
		return;
	for (c = c0; c <= c1; c++) {
		m = span(c, x0, x1);
		if (!m)
			continue;
		p = last(c);
		if (p && p->kind == kPlane && p->lateral == lateral && p->mirror == mirror) {
			cover(c, m);
			p->mask |= m;
			if (z0 < p->z0)
				p->z0 = z0;
			if (z1 > p->z1)
				p->z1 = z1;
			continue;
		}
		p = take(c, m);
		if (!p)
			continue;
		p->kind = kPlane;
		p->lateral = (unsigned char)lateral;
		p->mirror = (unsigned char)mirror;
		p->z0 = z0;
		p->z1 = z1;
	}
}

/* A wall across the axis: the near face of the cell j to the right
 * (left when j < 0), n rows ahead, between x0 and x1, as tall as its
 * depth allows in each column. */
static void frontFace(int x0, int x1, int j, int n)
{
	struct piece *p;
	unsigned char m;
	int c, c0, c1, half;

	if (!columns(x0, x1, &c0, &c1))
		return;
	for (c = c0; c <= c1; c++) {
		m = span(c, x0, x1);
		if (!m)
			continue;
		p = take(c, m);
		if (!p)
			continue;
		half = fx_mul8(front_half[phase][n], col_cos[c]);
		if (half < 1)
			half = 1;
		if (half > 255)
			half = 255;
		p->kind = kFront;
		p->half = (unsigned char)half;
		p->n = (signed char)n;
		p->j = (signed char)j;
		p->top = (unsigned char)(half > HALF ? 0 : HALF - half);
		p->bottom = (unsigned char)(half > HALF ? VIEW_ROWS - 1 : HALF + half - 1);
	}
}

/* The far edge of row n, lateral index L (cells 0.5 + L from the
 * axis), as a pixel column: to the right of the middle, or to the
 * left; row -1 is behind the party, past the sector's edge. */
#define EDGE_RIGHT(n, L) ((n) < 0 ? SECTOR_CELLS * 8 : MIDDLE + edge_px[phase][n][L])
#define EDGE_LEFT(n, L) ((n) < 0 ? 0 : MIDDLE - edge_px[phase][n][L])
/* The far edge of row n in 40ths of a cell past the party. */
#define DEPTH40(n) (((n) * 40 + 20) - phase * (40 / PLANE_PHASES))

/* A wall along the axis is taken as one face, however many cells
 * long: the run of wall cells at lateral index L on side s (0 left, 1
 * right) from row runFrom[s][L] on, taken when it ends, or when a
 * face farther out on the same side is about to be taken, since that
 * one may show past the run's beginning and must come after the run's
 * nearer part. */
static signed char runFrom[2][LATERALS];

static void flush(int s, int L, int through)
{
	int from = runFrom[s][L];

	if (from < 0)
		return;
	runFrom[s][L] = -1;
	if (s)
		sideFace(EDGE_RIGHT(through, L), EDGE_RIGHT(from - 1, L), L, 1, DEPTH40(from - 1), DEPTH40(through));
	else
		sideFace(EDGE_LEFT(from - 1, L), EDGE_LEFT(through, L), L, 0, DEPTH40(from - 1), DEPTH40(through));
}

static void flushAll(int through)
{
	int s, L;

	for (s = 0; s < 2; s++)
		for (L = 0; L < LATERALS; L++)
			flush(s, L, through);
}

/* The faces of row n towards the party: the cells that are wall with
 * floor before them. */
static void frontFaces(int n, const unsigned char *walls, const unsigned char *before)
{
	const unsigned char *near = edge_px[phase][n - 1];	/* the row's near edge */
	int j, flushed = 0;

	for (j = -LATERALS; j <= LATERALS; j++) {
		if (!walls[LATERALS + j] || before[LATERALS + j])
			continue;
		if (!flushed) {			/* the runs so far, nearer than this row, first */
			flushAll(n - 1);
			flushed = 1;
		}
		if (j == 0)
			frontFace(MIDDLE - near[0], MIDDLE + near[0], j, n);
		else if (j > 0)
			frontFace(MIDDLE + near[j - 1], MIDDLE + near[j], j, n);
		else
			frontFace(MIDDLE - near[-j], MIDDLE - near[-j - 1], j, n);
	}
}

/* The faces of row n along the axis: the runs begun, grown or ended.
 * A wall's face towards the axis shows only when the cell between is
 * floor. */
static void sideFaces(int n, const unsigned char *walls)
{
	int s, L, j, inner, c;

	for (s = 0; s < 2; s++)
		for (L = 0; L < LATERALS; L++) {
			j = s ? L + 1 : -(L + 1);
			inner = s ? j - 1 : j + 1;
			if (walls[LATERALS + j] && !walls[LATERALS + inner]) {
				for (c = 0; c < L; c++)		/* the runs nearer the axis, this row too */
					if (runFrom[s][c] >= 0)
						flush(s, c, n);
				if (runFrom[s][L] < 0)
					runFrom[s][L] = (signed char)n;
			} else if (runFrom[s][L] >= 0) {
				flush(s, L, n - 1);
			}
		}
}

/* The pieces of the sector columns first..last found, front to back. */
static void visibility(int first, int last)
{
	unsigned char walls[2 * LATERALS + 1], before[2 * LATERALS + 1];
	int c, n, j, s, L;

	rangeFirst = first;
	rangeLast = last;
	for (c = first; c <= last; c++) {
		count[IX(c)] = 0;
		covered[IX(c)] = 0;
	}
	uncovered = last - first + 1;
	for (s = 0; s < 2; s++)
		for (L = 0; L < LATERALS; L++)
			runFrom[s][L] = -1;
	rowX = cellX - LATERALS * rx;
	rowY = cellY - LATERALS * ry;
	rowWalls(before);
	for (n = 0; n <= PLANE_DEPTH && uncovered > 0; n++) {
		if (n > 0) {
			rowWalls(walls);
			frontFaces(n, walls, before);
		} else {
			for (j = 0; j < 2 * LATERALS + 1; j++)
				walls[j] = before[j];
		}
		sideFaces(n, walls);
		for (j = 0; j < 2 * LATERALS + 1; j++)
			before[j] = walls[j];
	}
	flushAll(n - 1);
	for (c = first; c <= last; c++)
		if (covered[IX(c)] != 0xFF)
			farPiece(c, (unsigned char)~covered[IX(c)]);
}

/* -- The layer: a column composed from its pieces --------------------- */

/* The sector pixel column of pixel i of column c as the tables have
 * it: the left half's, mirrored for the right. */
#define TABLE_X(c, i) (((c) << 3) + (i) < MIDDLE ? ((c) << 3) + (i) : 2 * MIDDLE - 1 - ((c) << 3) - (i))

/* A plane piece's rows into the column being composed: the words of
 * rows the wall covers written, the outline's pixels black, the
 * mortar rows noted.  The tops of the mask's pixels run one way
 * (nearer pixels taller), so the wall's pixels of a row are a prefix
 * or a suffix of the mask, grown pixel by pixel down the rows. */
static void composePlane(const struct piece *p, int c, unsigned char *dst)
{
	unsigned char top[8], bit[8], inside = 0;
	int n = 0, in = 0, out = 0, i, x, row, half;
	unsigned char *q;

	/* The mask's pixels with a wall, nearest first: the order they
	 * enter the wall down the rows, and leave it. */
	for (i = 0; i < 8; i++) {
		int k = p->mirror ? 7 - i : i;

		if (!(p->mask & bitAt[k]))
			continue;
		x = TABLE_X(c, k);
		if (wall_top[p->lateral][x] == 255)
			continue;
		top[n] = wall_top[p->lateral][x];
		bit[n++] = bitAt[k];
		half = wall_half[p->lateral][x];
		if (half > 1 && HALF - half / 2 >= 0)
			setMortar(HALF - half / 2, bitAt[k]);
		setMortar(HALF, bitAt[k]);
		if (half > 1 && HALF + half / 2 < VIEW_ROWS)
			setMortar(HALF + half / 2, bitAt[k]);
	}
	if (!n)
		return;
	q = dst + (top[0] << 1);
	out = n - 1;				/* the farthest leaves first: the last in */
	for (row = top[0]; row <= VIEW_ROWS - 1 - top[0]; row++, q += 2) {
		while (in < n && top[in] == row)
			inside |= bit[in++];
		while (out >= 0 && VIEW_ROWS - top[out] == row)	/* a pixel's last row is 119 - its first */
			inside &= (unsigned char)~bit[out--];
		q[0] = (unsigned char)((q[1] == WALL_ATTR ? q[0] & ~inside : ~inside) | (mortar[row] & inside));
		q[1] = WALL_ATTR;
	}
}

/* A wall across the axis, or the far bar: its rows all the mask's
 * pixels, the brick rows at the quarters of its height. */
/* A wall across the axis, or the far bar, drawn on the screen over
 * the layer: these change as the party steps, so they are not in the
 * layer.  The rows the mask's pixels, the brick rows at the quarters
 * of its height; a joint where the brick along the face changes from
 * the column before - a cell column at a time, which is the picture's
 * grain anyway. */
static int lastBrick[2];		/* the brick seen in the column before, by row parity */

static void frontColumn(const struct piece *p, int c, int screen)
{
	unsigned char *v = ms_row(VIEW_TOP + p->top) + (screen << 1), *w;
	int row, half = p->half, rows = p->bottom - p->top + 1, parity, brick, u8, d8, tan;
	int edges[5];

	if (p->kind == kFar) {
		if (p->mask == 0xFF)
			ms_vfill(screen, VIEW_TOP + p->top, VIEW_TOP + p->bottom, FAR_PIXELS, WALL_ATTR);
		else
			for (row = 0; row < rows; row++, v += MS_STRIDE) {
				v[0] = (unsigned char)((v[0] & ~p->mask) | (FAR_PIXELS & p->mask));
				v[1] = WALL_ATTR;
			}
		return;
	}
	if (p->mask == 0xFF) {
		ms_vfill(screen, VIEW_TOP + p->top, VIEW_TOP + p->bottom, 0, WALL_ATTR);
	} else {
		for (row = 0, w = v; row < rows; row++, w += MS_STRIDE) {
			w[0] = (unsigned char)(w[0] & ~p->mask);
			w[1] = WALL_ATTR;
		}
	}
	if (half < 4)
		return;
	edges[0] = HALF - half;
	edges[1] = HALF - half / 2;
	edges[2] = HALF;
	edges[3] = HALF + half / 2;
	edges[4] = HALF + half;
	for (row = 1; row < 4; row++)
		if (edges[row] >= 0 && edges[row] < VIEW_ROWS)
			ms_row(VIEW_TOP + edges[row])[(screen << 1)] |= p->mask;
	/* the joints: where along the face the column looks, in bricks */
	d8 = (p->n << 8) - 128 - phase_8[phase];
	tan = col_tan[c] < 0 ? -col_tan[c] : col_tan[c];
	u8 = fx_mul8(tan > 255 ? 255 : tan, d8);
	if (col_tan[c] < 0)
		u8 = -u8;
	u8 -= (p->j << 8) - 128;
	for (parity = 0; parity < 2; parity++) {
		brick = (u8 + (parity ? 32 : 0)) >> 6;
		if (brick != lastBrick[parity] && (p->mask & 0x80)) {
			for (row = parity; row < 4; row += 2) {
				int y0 = edges[row] < 0 ? -1 : edges[row];
				int y1 = edges[row + 1] > VIEW_ROWS ? VIEW_ROWS : edges[row + 1];

				w = ms_row(VIEW_TOP + y0 + 1) + (screen << 1);
				for (y0++; y0 < y1; y0++, w += MS_STRIDE)
					*w |= 0x80;
			}
		}
		lastBrick[parity] = brick;
	}
}

/* The edges of the floor's cells along the axis, at every plane's
 * foot: where a wall at that plane would end, the line lies - the
 * same tables. */
static void floorLines(int c)
{
	int L, i, x, half;

	for (L = 0; L < LATERALS; L++)
		for (i = 0; i < 8; i++) {
			x = TABLE_X(c, i);
			half = wall_half[L][x];
			if (half && HALF + half < VIEW_ROWS)
				setLine(HALF + half, bitAt[i]);
		}
}

/* Column c of the sector composed at dst, a word a row: the ceiling,
 * the floor with its lines, the pieces over them.  Where the floor
 * begins under them comes back. */
static unsigned char column[VIEW_ROWS][2] __attribute__((aligned(2)));

static int compose(int c, unsigned char *dst)
{
	unsigned char *q = dst;
	int row, i;

	floorLines(c);
	for (row = 0; row < VIEW_ROWS; row++, q += 2) {
		q[0] = row < HALF ? 0 : lines[row];
		q[1] = row < HALF ? CEILING_ATTR : FLOOR_ATTR;
	}
	for (i = 0; i < count[IX(c)]; i++)
		if (pieces[IX(c)][i].kind == kPlane)
			composePlane(&pieces[IX(c)][i], c, dst);
	for (row = VIEW_ROWS - 1, q = dst + (row << 1); row >= HALF; row--, q -= 2)
		if (q[1] == WALL_ATTR)
			break;
	clearRows();
	return row + 1;				/* the first floor row under the walls along the axis */
}

/* A column composed, at `from`, into the layer's slot. */
static void layerStore(int slot, const unsigned char *from)
{
	unsigned char *to = layer[0] + (slot << 1);
	int row;

	for (row = 0; row < VIEW_ROWS; row++, to += VIEW_CELLS * 2, from += 2) {
		to[0] = from[0];
		to[1] = from[1];
	}
}

/* -- What moves: the joints, the floor's edges across ---------------- */

/* The brick joints of a plane piece in column c: along the wall a
 * brick is BRICK 40ths of a cell, the second and fourth rows of
 * bricks laid half a brick on.  A joint is a black line between two
 * mortar rows in the pixel column where its depth falls; where the
 * bricks are narrower than two pixels, none. */
static void joints(const struct piece *p, int c, int screen)
{
	static const int lateral40[LATERALS] = {20, 60, 100};
	int L40 = lateral40[p->lateral], lastX = -10;
	int tc = p->mirror ? SECTOR_CELLS - 1 - c : c;	/* the column in the tables, the left half's */
	int z0 = p->z0, z1 = p->z1, zMin = col_zmin[p->lateral][tc], zMax = col_zmax[p->lateral][tc];
	int z, x, i, q, parity, half, y0, y1, row, edges[5];
	unsigned char bit, *v;

	/* The depths this column's pixels see on the plane: the joints
	 * between those alone.  A column that spans more than two bricks
	 * of the wall sees them four pixels wide at most: none there. */
	if (zMax < 0 || zMax - zMin > 2 * BRICK)
		return;
	if (z1 > zMax)
		z1 = zMax;
	if (zMin <= L40)
		zMin = L40 + 1;			/* nearer than that is past the sector's edge */
	/* The joints lie half a brick apart along the wall, those of the
	 * first and third rows of bricks and those of the second and
	 * fourth by turns; the face begins on a cell's edge, where the
	 * first row has a joint.  The first at the column's near depth or
	 * past it: so many half-bricks on from the face's beginning. */
	if (zMin > z0) {
		i = fx_div8(zMin - z0 + BRICK / 2 - 1, BRICK / 2);
		z = z0 + i * (BRICK / 2);
		parity = i & 1;
	} else {
		z = z0;
		parity = 0;
	}
	for (; z <= z1; z += BRICK / 2, parity ^= 1) {
		q = fx_div8(L40 << 8, z);	/* the tangent, 256ths */
		x = p->mirror ? MIDDLE + atan_px[q] - 1 : MIDDLE - atan_px[q];
		if ((x >> 3) != c)
			continue;
		if (x - lastX < 2 && x - lastX > -2)
			continue;
		lastX = x;
		i = x & 7;
		bit = bitAt[i];
		if (!(p->mask & bit))
			continue;
		half = wall_half[p->lateral][TABLE_X(c, i)];
		if (half < 4)
			continue;
		/* the rows of mortar, and the two brick rows of this parity
		 * between them: 0 and 2, or 1 and 3 */
		edges[0] = HALF - half;
		edges[1] = HALF - half / 2;
		edges[2] = HALF;
		edges[3] = HALF + half / 2;
		edges[4] = HALF + half;
		for (row = parity; row < 4; row += 2) {
			y0 = edges[row] < 0 ? -1 : edges[row];
			y1 = edges[row + 1] > VIEW_ROWS ? VIEW_ROWS : edges[row + 1];
			v = ms_row(VIEW_TOP + y0 + 1) + (screen << 1);
			for (y0++; y0 < y1; y0++, v += MS_STRIDE)
				*v |= bit;
		}
	}
}

/* The edges of the floor's cells across the axis in column c: a line
 * where each stands at this phase, down to the nearest wall's foot. */
static void floorEdges(int c, int screen, int from)
{
	const unsigned char *at = &cross_row[phase][1][c - SECTOR_FIRST];	/* row n's entry, a row's worth on */
	int n, row;
	unsigned char *v;

	for (n = 1; n <= CROSS_DEPTH; n++, at += VIEW_CELLS) {
		row = *at;
		if (row == 255)
			continue;
		if (row < from)
			break;
		v = ms_row(VIEW_TOP + row) + (screen << 1);
		v[0] = 0xFF;
	}
}

/* What moves in column c, over the layer on screen column `screen`:
 * the joints of the walls along the axis, the walls across it, the
 * floor's edges down to the nearest wall's foot (`from`, the layer's,
 * or lower under a wall across). */
static void moving(int c, int screen, int from)
{
	const struct piece *p;
	int i;

	for (i = 0; i < count[IX(c)]; i++) {
		p = &pieces[IX(c)][i];
		if (p->kind == kPlane) {
			joints(p, c, screen);
		} else {
			frontColumn(p, c, screen);
			if (p->bottom + 1 > from)
				from = p->bottom + 1;
		}
	}
	floorEdges(c, screen, from);
}

/* -- The passes ------------------------------------------------------- */

static void place(const struct view *v)
{
	cellX = v->cellX;
	cellY = v->cellY;
	phase = v->phase;
	facing(v->facing);
}

/* Whether the column's walls along the axis are those its layer was
 * composed from: the same planes under the same masks.  The depths
 * change with every phase of a step and the joints alone use them;
 * the walls across the axis are drawn over the layer each pass. */
static int layerCurrent(int c, int screen)
{
	const struct piece *p;
	const struct plane *q = layerPieces[screen];
	int i, k = 0;

	for (i = 0; i < count[IX(c)]; i++) {
		p = &pieces[IX(c)][i];
		if (p->kind != kPlane)
			continue;
		if (k >= layerCount[screen] || q[k].mask != p->mask || q[k].lateral != p->lateral || q[k].mirror != p->mirror)
			return 0;
		k++;
	}
	return k == layerCount[screen];
}

/* The column composed into the layer's slot, with the planes it was
 * composed from and where its floor begins. */
static void layerRemember(int c, int slot, int floor, const unsigned char *composed)
{
	const struct piece *p;
	int i, k = 0;

	layerStore(slot, composed);
	for (i = 0; i < count[IX(c)]; i++) {
		p = &pieces[IX(c)][i];
		if (p->kind == kPlane) {
			layerPieces[slot][k].mask = p->mask;
			layerPieces[slot][k].lateral = p->lateral;
			layerPieces[slot][k].mirror = p->mirror;
			k++;
		}
	}
	layerCount[slot] = (unsigned char)k;
	floorFrom[slot] = (unsigned char)floor;
}

static void layerColumn(int c, int screen)
{
	int floor = compose(c, column[0]);

	layerRemember(c, screen, floor, column[0]);
}

void view_invalidate(void)
{
	int c;

	for (c = 0; c < VIEW_CELLS; c++)
		layerCount[c] = 255;
}

/* PROF, a build switch for measuring: 1 finds the pieces and paints
 * nothing, 2 paints the pieces found once and finds no more, 3 leaves
 * out what moves, 4 the layer's copy. */
#ifndef PROF
#define PROF 0
#endif

void view_render(const struct view *v)
{
	static int found;
	int c;

	if (!found)
		view_invalidate();
	place(v);
	rangeBase = 0;
	if (PROF != 2 || !found)
		visibility(SECTOR_FIRST, SECTOR_FIRST + VIEW_CELLS - 1);
	found = 1;
	if (PROF == 1)
		return;
	for (c = SECTOR_FIRST; c < SECTOR_FIRST + VIEW_CELLS; c++)
		if (!layerCurrent(c, c - SECTOR_FIRST))
			layerColumn(c, c - SECTOR_FIRST);
	if (PROF != 4)
		ms_put(VIEW_LEFT, VIEW_TOP, VIEW_CELLS, VIEW_ROWS, layer);
	if (PROF == 3)
		return;
	lastBrick[0] = lastBrick[1] = -1;
	for (c = SECTOR_FIRST; c < SECTOR_FIRST + VIEW_CELLS; c++)
		moving(c, VIEW_LEFT + c - SECTOR_FIRST, floorFrom[c - SECTOR_FIRST]);
}

void view_turn_begin(const struct view *v, int dir)
{
	turnView = *v;
	turnView.phase = 0;
	turnDir = dir;
	turnFacing = (v->facing + dir) & 3;
}

/* Sector columns s0..s1 of the old facing - past its end, or before
 * its beginning, the new facing's sector - on screen columns from
 * `screen`.  A column of the new facing that the view will show when
 * the turn is over goes into the layer on its way, so that the layer
 * is whole then; the others pass through a column of their own. */
/* A pass of a turn brings in TURN_COLUMNS columns, of the old facing's
 * sector, or of the new one's past its end (or before its beginning),
 * or some of each: the parts, each with its own facing, found and
 * composed before the picture slides, so that the slide and the new
 * columns go up together. */
#define TURN_COLUMNS 4
struct part { int s0, s1, screen, base, facing; };
static unsigned char brought[TURN_COLUMNS][VIEW_ROWS][2] __attribute__((aligned(2)));
static unsigned char broughtFloor[TURN_COLUMNS];

/* Sector columns s0..s1, as counted on the old facing's sector, on the
 * screen from `screen`: one or two parts. */
static int parts(int s0, int s1, int screen, struct part *out)
{
	int n = 0;

	if (s0 < 0 && s1 >= 0) {
		n = parts(s0, -1, screen, out);
		return n + parts(0, s1, screen - s0, out + n);
	}
	if (s0 < SECTOR_CELLS && s1 >= SECTOR_CELLS) {
		n = parts(s0, SECTOR_CELLS - 1, screen, out);
		return n + parts(SECTOR_CELLS, s1, screen + SECTOR_CELLS - s0, out + n);
	}
	out->facing = turnView.facing;
	if (s0 >= SECTOR_CELLS) {
		s0 -= SECTOR_CELLS;
		s1 -= SECTOR_CELLS;
		out->facing = turnFacing;
	} else if (s0 < 0) {
		s0 += SECTOR_CELLS;
		s1 += SECTOR_CELLS;
		out->facing = turnFacing;
	}
	out->s0 = s0;
	out->s1 = s1;
	out->screen = screen;
	return 1;
}

static void placePart(const struct part *p)
{
	struct view v = turnView;

	v.facing = p->facing;
	place(&v);
	rangeFirst = p->s0;
	rangeLast = p->s1;
	rangeBase = p->base;
}

void view_turn_pass(int pass)
{
	struct part part[2];
	int n, i, c, k = 0, slot;

	if (turnDir > 0)			/* to the right: the picture slides left */
		n = parts(SECTOR_FIRST + TURN_COLUMNS * pass + VIEW_CELLS - TURN_COLUMNS,
			  SECTOR_FIRST + TURN_COLUMNS * pass + VIEW_CELLS - 1, VIEW_LEFT + VIEW_CELLS - TURN_COLUMNS, part);
	else
		n = parts(SECTOR_FIRST - TURN_COLUMNS * pass, SECTOR_FIRST - TURN_COLUMNS * pass + TURN_COLUMNS - 1,
			  VIEW_LEFT, part);
	/* found and composed first, each part's pieces in its own slots */
	for (i = 0; i < n; i++) {
		part[i].base = k;
		placePart(&part[i]);
		visibility(part[i].s0, part[i].s1);
		for (c = part[i].s0; c <= part[i].s1; c++, k++) {
			broughtFloor[k] = (unsigned char)compose(c, brought[k][0]);
			slot = part[i].facing == turnFacing && c >= SECTOR_FIRST && c < SECTOR_FIRST + VIEW_CELLS
				? c - SECTOR_FIRST : -1;
			if (slot >= 0)
				layerRemember(c, slot, broughtFloor[k], brought[k][0]);
		}
	}
	/* then the slide, the columns put up, and what moves on them */
	ms_hscroll(VIEW_LEFT, VIEW_LEFT + VIEW_CELLS - 1, VIEW_TOP, VIEW_TOP + VIEW_ROWS - 1,
		   turnDir > 0 ? -TURN_COLUMNS : TURN_COLUMNS);
	for (i = 0, k = 0; i < n; i++) {
		placePart(&part[i]);
		lastBrick[0] = lastBrick[1] = -1;
		for (c = part[i].s0; c <= part[i].s1; c++, k++) {
			ms_put(part[i].screen + c - part[i].s0, VIEW_TOP, 1, VIEW_ROWS, brought[k]);
			moving(c, part[i].screen + c - part[i].s0, broughtFloor[k]);
		}
	}
}
