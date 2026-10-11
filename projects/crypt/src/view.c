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
 * along it.  Its outline and the three rows of mortar between its
 * four rows of bricks at the quarters of that height, the walls
 * across the axis with theirs, the floor and the ceiling, are composed
 * into a layer kept in memory - a column again only when its walls
 * change, and when a wall across the axis grows as the party comes
 * only the rows it takes or leaves - and the layer is copied to the
 * screen each pass.  Over it go what moves: the joints between the
 * bricks, where each falls at this phase, and the edges of the floor's
 * cells across.  A wall is red with black lines, the floor green, the
 * ceiling black; a cell row part wall and part floor is red and
 * green, with no black to spare. */

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
/* The detail kept: no two lines nearer than DETAIL pixels.  A wall
 * whose brick rows would be shorter than that has no mortar but the
 * horizon's, no joints and no corner lines, and is a fill; joints
 * nearer than that along a wall are left out; the floor's lines along
 * the axis too.  Else the far walls crowd into black. */
#define DETAIL 4
#define DETAILED(half) ((half) >= 2 * DETAIL)	/* a wall tall enough for its lines */
#define WALL_ATTR (MS_PAPER(2) | MS_INK(0))
#define FLOOR_ATTR (MS_PAPER(4) | MS_INK(0))
#define CEILING_ATTR 0
#define FAR_ATTR MS_INK(2)
#define FAR_PIXELS 0x55			/* past the depth drawn: a dim bar on the horizon */

enum { kPlane, kFront, kFar };

/* The compiler inlines a function called twice into both callers; the
 * program's room runs out before its time does, so the bigger ones
 * are kept as functions. */
#define NOINLINE __attribute__((noinline))

/* The pixel bits by position, and the masks of the pixels from one on
 * and up to one: tables, since the compiler's shift by a variable
 * count is the third trap (toolset README.md). */
static const unsigned char bitAt[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
static const unsigned char fromPixel[9] = {0xFF, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01, 0x00};
static const unsigned char toPixel[9] = {0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFF};

struct piece {
	unsigned char mask, kind;
	union {				/* by the kind, to keep the pieces short */
		struct {
			unsigned char lateral, mirror;	/* kPlane: the plane, and which side (1: the right) */
			unsigned char corners;	/* the pixels where the wall's corners stand */
			int z0, z1;		/* the face's depths, 40ths of a cell past the party */
		};
		struct {
			unsigned char top, bottom;	/* kFront, kFar: window rows, both in */
			unsigned char half;	/* kFront: rows above the horizon */
			signed char n, j;	/* kFront: the cell's row and lateral */
			unsigned char corner;	/* kFront: the pixels where the wall's corners stand */
		};
	};
};

/* The columns of a range, at most the view's width: pieces[c - rangeFirst]. */
static struct piece pieces[VIEW_CELLS][MAX_PIECES];
static unsigned char count[VIEW_CELLS];
static unsigned char covered[VIEW_CELLS];

/* The layer: the view's columns composed, a word a row as the screen
 * keeps them, a column at a time: a column goes up whole, with what
 * moves drawn into it and taken out again after. */
static unsigned char layer[VIEW_CELLS][VIEW_ROWS][2] __attribute__((aligned(2)));	/* words: an even address */
/* A column's rows of mortar and of floor lines while it is composed:
 * the pixels of each, and which rows were touched, to clear them after
 * - a clear of the whole is a byte at a time, dearer than the column. */
unsigned char view_mortar[VIEW_ROWS], view_lines[VIEW_ROWS];	/* rows.s reads them */
static unsigned char mortarRows[MAX_PIECES * 8 * 3], lineRows[LATERALS * 8];
static int mortarN, lineN;

static void setMortar(int row, unsigned char bit)
{
	if (!view_mortar[row])
		mortarRows[mortarN++] = (unsigned char)row;
	view_mortar[row] |= bit;
}

static void setLine(int row, unsigned char bit)
{
	if (!view_lines[row])
		lineRows[lineN++] = (unsigned char)row;
	view_lines[row] |= bit;
}

static void clearRows(void)
{
	while (mortarN > 0)
		view_mortar[mortarRows[--mortarN]] = 0;
	while (lineN > 0)
		view_lines[lineRows[--lineN]] = 0;
}

static int cellX, cellY, phase;
static int fx, fy, rx, ry;		/* the facing: forward and right, in cells */
/* The phase's rows of the tables, taken once a pass: the multiplying
 * of indexes is the compiler's weak point. */
static const unsigned char (*edgeNow)[PLANE_LATERALS + 1];	/* edge_px[phase] */
static const unsigned char (*crossNow)[VIEW_CELLS];		/* cross_row[phase] */
static const unsigned char *frontHalfNow;			/* front_half[phase] */
static int phaseNow8;						/* phase_8[phase] */
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
		walls[j] = (unsigned char)(x < 0 || y < 0 || x >= MAP_SIDE || y >= MAP_SIDE || MAP_WALL(map_rows[y][x]));
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

/* A face's corners - a line stands down each - are its left edge
 * (CORNER_LEFT, pixel x0) and its right (CORNER_RIGHT, pixel x1 - 1)
 * when the wall ends there: the pixels of those in column c, under
 * the mask the face takes. */
#define CORNER_LEFT 1
#define CORNER_RIGHT 2

static unsigned char cornerPixels(int c, int x0, int x1, int corners, unsigned char m)
{
	unsigned char bits = 0;

	if ((corners & CORNER_LEFT) && (x0 >> 3) == c)
		bits |= bitAt[x0 & 7];
	if ((corners & CORNER_RIGHT) && ((x1 - 1) >> 3) == c)
		bits |= bitAt[(x1 - 1) & 7];
	return bits & m;
}

/* A wall along the axis, `lateral` planes out on the left or (mirror)
 * the right, between pixel columns x0 and x1, from depth z0 to z1 in
 * 40ths of a cell past the party.  A column's last piece of the same
 * plane - the run of cells before - grows by these pixels. */
NOINLINE static void sideFace(int x0, int x1, int lateral, int mirror, int z0, int z1, int corners)
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
			p->corners |= cornerPixels(c, x0, x1, corners, m);
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
		p->corners = cornerPixels(c, x0, x1, corners, m);
		p->z0 = z0;
		p->z1 = z1;
	}
}

/* A wall across the axis: the near face of the cell j to the right
 * (left when j < 0), n rows ahead, between x0 and x1, as tall as its
 * depth allows in each column.  Its left edge is a corner of the wall
 * (CORNER_LEFT) unless the cell beside shows a face across too, the
 * right the same: a line stands there. */
NOINLINE static void frontFace(int x0, int x1, int j, int n, int corners)
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
		p->corner = cornerPixels(c, x0, x1, corners, m);
		half = fx_mul8(frontHalfNow[n], col_cos[c]);
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
#define EDGE_RIGHT(n, L) ((n) < 0 ? SECTOR_CELLS * 8 : MIDDLE + edgeNow[n][L])
#define EDGE_LEFT(n, L) ((n) < 0 ? 0 : MIDDLE - edgeNow[n][L])
/* The far edge of row n in 40ths of a cell past the party. */
#define DEPTH40(n) (((n) * 40 + 20) - phase * (40 / PLANE_PHASES))

/* A wall along the axis is taken as one face, however many cells
 * long: the run of wall cells at lateral index L on side s (0 left, 1
 * right) from row runFrom[s][L] on, taken when it ends, or when a
 * face farther out on the same side is about to be taken, since that
 * one may show past the run's beginning and must come after the run's
 * nearer part. */
static signed char runFrom[2][LATERALS];

/* Whether the cell n rows ahead and j to the right is wall. */
NOINLINE static int wallAt(int n, int j)
{
	int x = cellX, y = cellY;

	x += fx > 0 ? n : fx < 0 ? -n : 0;
	y += fy > 0 ? n : fy < 0 ? -n : 0;
	x += rx > 0 ? j : rx < 0 ? -j : 0;
	y += ry > 0 ? j : ry < 0 ? -j : 0;
	return map_at(x, y);
}

static void flush(int s, int L, int through)
{
	int from = runFrom[s][L], j = s ? L + 1 : -(L + 1), corners = 0;
	int near, far;

	if (from < 0)
		return;
	runFrom[s][L] = -1;
	/* the run's ends are the wall's corners where floor lies before
	 * it or past it; the party's own row has no near end, the depth
	 * drawn no far one */
	near = from > 0 && !wallAt(from - 1, j);
	far = through < PLANE_DEPTH && !wallAt(through + 1, j);
	if (s) {
		corners = (far ? CORNER_LEFT : 0) | (near ? CORNER_RIGHT : 0);
		sideFace(EDGE_RIGHT(through, L), EDGE_RIGHT(from - 1, L), L, 1, DEPTH40(from - 1), DEPTH40(through), corners);
	} else {
		corners = (near ? CORNER_LEFT : 0) | (far ? CORNER_RIGHT : 0);
		sideFace(EDGE_LEFT(from - 1, L), EDGE_LEFT(through, L), L, 0, DEPTH40(from - 1), DEPTH40(through), corners);
	}
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
	const unsigned char *near = edgeNow[n - 1];	/* the row's near edge */
	int j, k, flushed = 0, corners;

	for (j = -LATERALS; j <= LATERALS; j++) {
		if (!walls[LATERALS + j] || before[LATERALS + j])
			continue;
		if (!flushed) {			/* the runs so far, nearer than this row, first */
			flushAll(n - 1);
			flushed = 1;
		}
		/* a corner at an edge whose neighbour shows no face across:
		 * floor there, or a wall along the axis running past */
		corners = 0;
		k = LATERALS + j - 1;
		if (j > -LATERALS && (!walls[k] || before[k]))
			corners |= CORNER_LEFT;
		k = LATERALS + j + 1;
		if (j < LATERALS && (!walls[k] || before[k]))
			corners |= CORNER_RIGHT;
		if (j == 0)
			frontFace(MIDDLE - near[0], MIDDLE + near[0], j, n, corners);
		else if (j > 0)
			frontFace(MIDDLE + near[j - 1], MIDDLE + near[j], j, n, corners);
		else
			frontFace(MIDDLE - near[-j], MIDDLE - near[-j - 1], j, n, corners);
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

/* A cell row some of whose pixels are wall and the rest floor has two
 * colours for three things: the wall's red is its paper, the floor's
 * green its ink, and the black of the lines waits for a row that is
 * wall throughout - else the floor came out black in triangles under
 * the wall's foot, where its slanting edge crosses a cell. */
#define MIXED_ATTR (MS_PAPER(2) | MS_INK(4))

/* The sector pixel column of pixel i of column c as the tables have
 * it: the left half's, mirrored for the right. */
#define TABLE_X(c, i) (((c) << 3) + (i) < MIDDLE ? ((c) << 3) + (i) : 2 * MIDDLE - 1 - ((c) << 3) - (i))

/* The rows of mortar between the brick rows, at the quarters of the
 * wall's height: a wall along the axis in sector pixel column x,
 * `lateral` planes out, noted for pixel `bit`. */
static void planeMortar(int lateral, int x, unsigned char bit)
{
	int half = wall_half[lateral][x];

	setMortar(HALF, bit);
	if (!DETAILED(half))
		return;
	if (HALF - half / 2 >= 0)
		setMortar(HALF - half / 2, bit);
	if (HALF + half / 2 < VIEW_ROWS)
		setMortar(HALF + half / 2, bit);
}

/* The rows of a wall across the axis `half` rows tall that are lines:
 * its top and its foot, where it meets the ceiling and the floor, and
 * the three rows of mortar between its brick rows - none but the
 * horizon's when it is too short for the detail.  Into rows[], those
 * in the window; how many comes back. */
static int frontRows(int half, int *rows)
{
	int n = 0;

	rows[n++] = HALF;
	if (!DETAILED(half))
		return n;
	if (HALF - half >= 0)
		rows[n++] = HALF - half;
	if (HALF - half / 2 >= 0)
		rows[n++] = HALF - half / 2;
	if (HALF + half / 2 < VIEW_ROWS)
		rows[n++] = HALF + half / 2;
	if (HALF + half - 1 < VIEW_ROWS)
		rows[n++] = HALF + half - 1;
	return n;
}

/* Those of a wall across the axis, for its pixels. */
static void frontMortar(const struct piece *p)
{
	int rows[5], n, i;

	if (p->kind != kFront)
		return;
	n = frontRows(p->half, rows);
	for (i = 0; i < n; i++)
		setMortar(rows[i], p->mask);
}

/* The mortar of all the column's pieces noted. */
static void noteMortar(int c)
{
	const struct piece *p = pieces[IX(c)];
	int i, k, n = count[IX(c)];

	for (i = 0; i < n; i++, p++) {
		if (p->kind != kPlane) {
			frontMortar(p);
			continue;
		}
		for (k = 0; k < 8; k++)
			if ((p->mask & bitAt[k]) && wall_top[p->lateral][TABLE_X(c, k)] != 255)
				planeMortar(p->lateral, TABLE_X(c, k), bitAt[k]);
	}
}

/* Pixels `wall` of `rows` rows from the word at q, `stride` bytes
 * apart, the first being window row `row`, are wall, besides what
 * each row had: the fill, with the mortar's pixels where noted.  The
 * rest of a row is black above the horizon, the ink; below it green,
 * the ink again with the attribute for it, unless the row is wall
 * throughout.  A mixed row shows no mortar, so its pixels say exactly
 * what is wall.  In assembler (rows.s): the compiler made fifty
 * instructions of a row. */
void wall_rows(unsigned char *q, int stride, int row, int rows, int wall);

/* A plane piece's rows into the column being composed.  The tops of
 * the mask's pixels run one way (nearer pixels taller), so the wall's
 * pixels of a row are a prefix or a suffix of the mask, grown pixel by
 * pixel down the rows. */
static void composePlane(const struct piece *p, int c, unsigned char *dst)
{
	unsigned char top[8], bit[8], inside = 0;
	int n = 0, in = 0, out, i, x, row, next;
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
		planeMortar(p->lateral, x, bitAt[k]);
	}
	if (!n)
		return;
	q = dst + (top[0] << 1);
	out = n - 1;				/* the farthest leaves first: the last in */
	for (row = top[0]; row < VIEW_ROWS - top[0]; q += (next - row) << 1, row = next) {
		while (in < n && top[in] == row)
			inside |= bit[in++];
		while (out >= 0 && VIEW_ROWS - top[out] == row)	/* a pixel's last row is 119 - its first */
			inside &= (unsigned char)~bit[out--];
		next = VIEW_ROWS - top[0];		/* the run to the next pixel's entering or leaving */
		if (in < n && top[in] < next)
			next = top[in];
		if (out >= 0 && VIEW_ROWS - top[out] < next)
			next = VIEW_ROWS - top[out];
		wall_rows(q, 2, row, next - row, inside);
	}
}

/* A wall across the axis, or the far bar, into the column: its rows
 * all the mask's pixels, the mortar at the quarters of its height.
 * These are in the layer too, though they grow as the party steps,
 * so that the floor never shows where they stand. */
static void composeFront(const struct piece *p, unsigned char *dst)
{
	frontMortar(p);
	wall_rows(dst + (p->top << 1), 2, p->top, p->bottom - p->top + 1, p->mask);
}

/* The far bar's dither, over everything. */
static void composeFar(const struct piece *p, unsigned char *dst)
{
	unsigned char *q = dst + (p->top << 1);
	int row;

	for (row = p->top; row <= p->bottom; row++, q += 2)
		q[0] |= FAR_PIXELS & p->mask;
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
			if (DETAILED(half) && HALF + half < VIEW_ROWS)
				setLine(HALF + half, bitAt[i]);
		}
}

/* Where column c's walls end below the horizon, from its pieces: the
 * first row that is floor throughout, and the last that is wall
 * throughout, HALF - 1 for none.  Every wall pixel's rows run from
 * above the horizon to its foot, so the rows wall throughout are
 * those above every foot, if every pixel has one. */
struct extent { unsigned char floor, through; };

static void extent(int c, struct extent *e)
{
	const struct piece *p = pieces[IX(c)];
	unsigned char covered = 0;
	int i, k, top, foot, n = count[IX(c)], floor = HALF - 1, through = VIEW_ROWS - 1;

	for (i = 0; i < n; i++, p++) {
		if (p->kind != kPlane) {
			covered |= p->mask;
			if (p->bottom > floor)
				floor = p->bottom;
			if (p->bottom < through)
				through = p->bottom;
			continue;
		}
		for (k = 0; k < 8; k++) {
			if (!(p->mask & bitAt[k]))
				continue;
			top = wall_top[p->lateral][TABLE_X(c, k)];
			if (top == 255)
				continue;
			covered |= bitAt[k];
			foot = VIEW_ROWS - 1 - top;
			if (foot > floor)
				floor = foot;
			if (foot < through)
				through = foot;
		}
	}
	e->floor = (unsigned char)(floor + 1);
	e->through = (unsigned char)(covered == 0xFF ? through : HALF - 1);
}

/* Column c of the sector composed at dst, a word a row: the ceiling,
 * the floor with its lines, the pieces over them. */
void column_base(unsigned char *q);	/* rows.s: the ceiling and the floor with its lines */

static void compose(int c, unsigned char *dst)
{
	const struct piece *p = pieces[IX(c)];
	int i, n = count[IX(c)];

	floorLines(c);
	column_base(dst);
	for (i = 0; i < n; i++)
		if (p[i].kind == kPlane)
			composePlane(&p[i], c, dst);
		else
			composeFront(&p[i], dst);
	for (i = 0; i < n; i++)
		if (p[i].kind == kFar)
			composeFar(&p[i], dst);
	clearRows();
}

/* One row of column c composed on its own at q, from its pieces: for
 * the rows a wall across the axis leaves as the party steps back.  The
 * floor's lines are to be noted before. */
NOINLINE static void composeRow(int c, int row, unsigned char *q)
{
	const struct piece *p = pieces[IX(c)];
	unsigned char wall = 0, mort = 0, far = 0;
	int i, k, x, top, half, n = count[IX(c)];

	q[0] = (unsigned char)(row < HALF ? 0 : view_lines[row]);
	q[1] = row < HALF ? CEILING_ATTR : FLOOR_ATTR;
	for (i = 0; i < n; i++, p++) {
		if (p->kind != kPlane) {
			if (row < p->top || row > p->bottom)
				continue;
			wall |= p->mask;
			if (p->kind == kFar) {
				far |= p->mask;
			} else {
				int rows[5], m = frontRows(p->half, rows), r;

				for (r = 0; r < m; r++)
					if (rows[r] == row)
						mort |= p->mask;
			}
			continue;
		}
		for (k = 0; k < 8; k++) {
			if (!(p->mask & bitAt[k]))
				continue;
			x = TABLE_X(c, k);
			top = wall_top[p->lateral][x];
			if (top == 255 || row < top || row > VIEW_ROWS - 1 - top)
				continue;
			wall |= bitAt[k];
			half = wall_half[p->lateral][x];
			if (row == HALF || (DETAILED(half) && (row == HALF - half / 2 || row == HALF + half / 2)))
				mort |= bitAt[k];
		}
	}
	if (!wall)
		return;
	if (wall == 0xFF || row < HALF) {
		q[0] = (unsigned char)(~wall | mort);
		q[1] = WALL_ATTR;
	} else {
		q[0] = (unsigned char)~wall;
		q[1] = MIXED_ATTR;
	}
	q[0] |= FAR_PIXELS & far;
}

/* PROF, a build switch for measuring: 1 finds the pieces and paints
 * nothing, 2 paints the pieces found once and finds no more, 3 leaves
 * out what moves, 4 the columns' going up, 5 the joints, 6 the layer's
 * keeping up (the columns go up as they were). */
#ifndef PROF
#define PROF 0
#endif

/* -- What moves: the joints, the floor's edges across ---------------- */

/* What moves is drawn into the column before it goes up and taken out
 * after, so that the column goes up whole: the joints as runs of rows
 * with a pixel set, the floor's edges as rows made black, noted for
 * the taking out.  The bit turned down a run of rows, once to draw
 * and once to take out: rows.s. */
#define MAX_RUNS 32			/* a column's joints and corners, their runs between the mortar rows */
struct run { unsigned char row, rows, bit; };
static struct run runs[MAX_RUNS];
static unsigned char edgeRows[CROSS_DEPTH], edgeWas[CROSS_DEPTH];
static int runN, edgeN;
void column_flip(unsigned char *q, int rows, int bit);

static void addRun(int from, int to, unsigned char bit)
{
	if (to <= from || runN >= MAX_RUNS)
		return;
	runs[runN].row = (unsigned char)from;
	runs[runN].rows = (unsigned char)(to - from);
	runs[runN++].bit = bit;
}

/* The rows of mortar of a wall `half` rows above and below the
 * horizon, and its top and foot: the brick rows lie between. */
static void brickRows(int half, int *edges)
{
	edges[0] = HALF - half;
	edges[1] = HALF - half / 2;
	edges[2] = HALF;
	edges[3] = HALF + half / 2;
	edges[4] = HALF + half;
}

/* A line down pixel `bit` in the brick rows `first`, `first` + step
 * and so on of a wall with those edges, to `through`. */
static void lineDown(const int *edges, int first, int step, int through, unsigned char bit)
{
	int row;

	for (row = first; row < 4; row += step)
		addRun(edges[row] < 0 ? 0 : edges[row] + 1,
		       edges[row + 1] > through ? through + 1 : edges[row + 1], bit);
}

/* The pixel of column c, as the tables have it (tc), that sees depth z
 * on the plane: the last from the nearest on whose centre is not
 * past it. */
static int pixelAt(const int *zs, int c, int mirror, int z)
{
	int i = 0;

	while (i < 7 && zs[i + 1] <= z)
		i++;
	return (c << 3) + (mirror ? 7 - i : i);
}

/* The brick joints of a plane piece in column c, and its corners.
 * Along the wall a brick is BRICK 40ths of a cell, the second and
 * fourth rows of bricks laid half a brick on.  A joint is a black line
 * between two mortar rows in the pixel column where its depth falls -
 * the last pixel, from the nearest on, whose centre is not past it;
 * where the bricks are narrower than two pixels, none, and none
 * within a pixel of the face's ends, whose lines are the corners'.  A
 * row part wall and part floor has no black: the lines stop at
 * `through`, the last row wall throughout. */
NOINLINE static void joints(const struct piece *p, int c, int through)
{
	static const int lateral40[LATERALS] = {20, 60, 100};
	const int *depth = wall_z[p->lateral];
	int L40 = lateral40[p->lateral], lastX = -10, xEnd = -10;
	int tc = p->mirror ? SECTOR_CELLS - 1 - c : c;	/* the column in the tables, the left half's */
	int z0 = p->z0, z1 = p->z1, zMin = col_zmin[p->lateral][tc], zMax = col_zmax[p->lateral][tc];
	int z, x, i, k, parity, half, edges[5], zs[8];
	unsigned char bit;

	for (k = 0; p->corners && k < 8; k++) {	/* the corners: a line the wall's height */
		if (!(p->corners & bitAt[k]))
			continue;
		half = wall_half[p->lateral][tc * 8 + (p->mirror ? 7 - k : k)];
		if (!DETAILED(half))
			continue;
		brickRows(half, edges);
		lineDown(edges, 0, 1, through, bitAt[k]);
	}
	/* The depths this column's pixels see on the plane: the joints
	 * between those alone.  A column that spans more than two bricks
	 * of the wall sees them four pixels wide at most: none there. */
	if (zMax < 0 || zMax - zMin > 2 * BRICK)
		return;
	for (i = 0; i < 8; i++)			/* the pixels' depths, the nearest first */
		zs[i] = depth[tc * 8 + i];
	if (z0 >= zMin)				/* the face's ends, where they fall in this column */
		lastX = pixelAt(zs, c, p->mirror, z0);
	if (z1 <= zMax)
		xEnd = pixelAt(zs, c, p->mirror, z1);
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
	for (i = 0; z <= z1; z += BRICK / 2, parity ^= 1) {
		while (i < 7 && zs[i + 1] <= z)
			i++;
		k = p->mirror ? 7 - i : i;
		x = (c << 3) + k;
		if ((x - lastX < DETAIL && x - lastX > -DETAIL) || (x - xEnd < DETAIL && x - xEnd > -DETAIL))
			continue;
		lastX = x;
		bit = bitAt[k];
		if (!(p->mask & bit))
			continue;
		half = wall_half[p->lateral][tc * 8 + i];
		if (!DETAILED(half))
			continue;
		brickRows(half, edges);		/* the two brick rows of this parity: 0 and 2, or 1 and 3 */
		lineDown(edges, parity, 2, through, bit);
	}
}

/* Where along a wall across the axis, n rows ahead and j cells to the
 * right, sector column c looks: 256ths of a cell from the face's left
 * end. */
static int along(int c, int n, int j)
{
	int d8 = (n << 8) - 128 - phaseNow8;
	int tan = col_tan[c] < 0 ? -col_tan[c] : col_tan[c];
	int u8 = fx_mul8(tan > 255 ? 255 : tan, d8);

	if (col_tan[c] < 0)
		u8 = -u8;
	return u8 - ((j << 8) - 128);
}

/* The joints of a wall across the axis: where the brick along the face
 * changes from the column before, a line down the column's first
 * pixel - a cell column at a time, which is the picture's grain
 * anyway - the brick rows by turns, a brick half a cell as along the
 * axis; and a line the wall's height where a corner of it stands. */
NOINLINE static void frontJoints(const struct piece *p, int c, int through)
{
	int half = p->half, parity, u8, before;
	int edges[5];

	if (p->kind != kFront || !DETAILED(half))
		return;
	brickRows(half, edges);
	edges[4]--;				/* the foot's row is a line: the joints stop above it */
	if (p->corner)
		lineDown(edges, 0, 1, through, p->corner);
	if (!(p->mask & 0x80) || c == 0)
		return;
	u8 = along(c, p->n, p->j);
	before = along(c - 1, p->n, p->j);
	for (parity = 0; parity < 2; parity++)
		if (((u8 + (parity ? 64 : 0)) >> 7) != ((before + (parity ? 64 : 0)) >> 7))
			lineDown(edges, parity, 2, through, 0x80);
}

/* The edges of the floor's cells across the axis in column c: a line
 * where each stands at this phase, down to the nearest wall's foot. */
static void floorEdges(int c, int from)
{
	const unsigned char *at = &crossNow[1][c - SECTOR_FIRST];	/* row n's entry, a row's worth on */
	int n, row;

	edgeN = 0;
	for (n = 1; n <= CROSS_DEPTH; n++, at += VIEW_CELLS) {
		row = *at;
		if (row == 255)
			continue;
		if (row < from)
			break;
		edgeRows[edgeN++] = (unsigned char)row;
	}
}

/* Column c, composed at col, put up on screen column `screen` with
 * what moves on it: the joints of the walls along the axis and across
 * it, down to the last row wall throughout, and the floor's edges from
 * the first row floor throughout - drawn into the column, which goes
 * up whole, and taken out of it again. */
static void show(int c, unsigned char *col, int screen, const struct extent *e)
{
	const struct piece *p;
	int i;

	runN = 0;
	for (i = 0; i < count[IX(c)] && PROF != 5; i++) {
		p = &pieces[IX(c)][i];
		if (p->kind == kPlane)
			joints(p, c, e->through);
		else
			frontJoints(p, c, e->through);
	}
	floorEdges(c, e->floor);
	if (PROF != 3) {
		for (i = 0; i < runN; i++)
			column_flip(col + (runs[i].row << 1), runs[i].rows, runs[i].bit);
		for (i = 0; i < edgeN; i++) {
			edgeWas[i] = col[edgeRows[i] << 1];
			col[edgeRows[i] << 1] = 0xFF;
		}
	}
	if (PROF != 4)
		ms_vput(screen, VIEW_TOP, VIEW_ROWS, col);
	if (PROF != 3) {
		for (i = 0; i < edgeN; i++)
			col[edgeRows[i] << 1] = edgeWas[i];
		for (i = 0; i < runN; i++)
			column_flip(col + (runs[i].row << 1), runs[i].rows, runs[i].bit);
	}
}

/* -- The passes ------------------------------------------------------- */

static void place(const struct view *v)
{
	cellX = v->cellX;
	cellY = v->cellY;
	phase = v->phase;
	edgeNow = edge_px[phase];
	crossNow = cross_row[phase];
	frontHalfNow = front_half[phase];
	phaseNow8 = phase_8[phase];
	facing(v->facing);
}

/* What a column of the layer was composed from: its pieces, the pixels
 * and kind of each, the plane or the rows, and where its floor begins.
 * The depths change with every phase of a step and the joints alone
 * use them. */
struct known { unsigned char mask, kind, a, b, half; };
static struct known known[VIEW_CELLS][MAX_PIECES];
static unsigned char knownCount[VIEW_CELLS];
static struct extent extents[VIEW_CELLS];

/* Whether column c's layer column was composed from its pieces: 0 when
 * it was; 1 when from the same walls along the axis, those across
 * having grown or shrunk since - rows to bring up to date; 2 when
 * from other walls - a column to compose again. */
static int layerCompare(int c, int slot)
{
	const struct piece *p = pieces[IX(c)];
	const struct known *k = known[slot];
	int i, n = count[IX(c)], moved = 0;

	if (n != knownCount[slot])
		return 2;
	for (i = 0; i < n; i++, p++, k++) {
		if (p->kind != k->kind || p->mask != k->mask)
			return 2;
		if (p->kind == kPlane) {
			if (p->lateral != k->a || p->mirror != k->b)
				return 2;
		} else if (p->top != k->a || p->bottom != k->b || (p->kind == kFront && p->half != k->half)) {
			moved = 1;		/* the height alone moves when the wall overflows the window */
		}
	}
	return moved;
}

static void layerNote(int c, int slot)
{
	const struct piece *p = pieces[IX(c)];
	struct known *k = known[slot];
	int i, n = count[IX(c)];

	for (i = 0; i < n; i++, p++, k++) {
		k->mask = p->mask;
		k->kind = p->kind;
		if (p->kind == kPlane) {
			k->a = p->lateral;
			k->b = p->mirror;
			k->half = 0;
		} else {
			k->a = p->top;
			k->b = p->bottom;
			k->half = p->half;
		}
	}
	knownCount[slot] = (unsigned char)n;
	extent(c, &extents[slot]);
}

/* The column composed into the layer's slot, and noted. */
NOINLINE static void layerColumn(int c, int slot)
{
	compose(c, layer[slot][0]);
	layerNote(c, slot);
}

/* The three mortar rows of a wall across the axis `half` rows tall,
 * or none: into rows[]. */
static int mortarRowsOf(int half, int *rows)
{
	return frontRows(half, rows);
}

/* The walls across the axis in column c grown or shrunk since its
 * layer column was composed: the rows they took made wall, the rows
 * they left composed again, the mortar moved; the rest of the column
 * as it was.  Then the column noted again. */
NOINLINE static void layerFronts(int c, int slot)
{
	const struct piece *p = pieces[IX(c)];
	const struct known *k = known[slot];
	unsigned char (*col)[2] = layer[slot], *q;
	int i, j, m, n = count[IX(c)], row, rows[5], noted = 0;

	/* The rows taken need the other pieces' mortar, where one makes a
	 * row wall throughout; the rows left need the floor's lines: the
	 * one when the column has other pieces, the other when a wall
	 * shrank. */
	for (i = 0; i < n; i++, p++, k++) {
		if (p->kind != kFront || (p->top == k->a && p->bottom == k->b && p->half == k->half))
			continue;
		if (!noted && n > 1) {
			noteMortar(c);
			noted = 1;
		}
		/* the old mortar rows still inside the wall: the fill again */
		m = mortarRowsOf(k->half, rows);
		for (j = 0; j < m; j++)
			if (rows[j] >= p->top && rows[j] <= p->bottom)
				col[rows[j]][0] &= (unsigned char)~p->mask;
		/* the rows taken */
		if (p->top < k->a)
			wall_rows(col[p->top], 2, p->top, k->a - p->top, p->mask);
		if (p->bottom > k->b)
			wall_rows(col[k->b + 1], 2, k->b + 1, p->bottom - k->b, p->mask);
		/* the rows left */
		if (p->top > k->a || p->bottom < k->b) {
			if (noted < 2) {
				floorLines(c);
				noted = 2;
			}
			for (row = k->a; row < p->top; row++)
				composeRow(c, row, col[row]);
			for (row = p->bottom + 1; row <= k->b; row++)
				composeRow(c, row, col[row]);
		}
		/* the new mortar rows, where the row is wall throughout */
		m = mortarRowsOf(p->half, rows);
		for (j = 0; j < m; j++) {
			if (rows[j] < 0 || rows[j] >= VIEW_ROWS)
				continue;
			q = col[rows[j]];
			if (q[1] == WALL_ATTR)
				q[0] |= p->mask;
		}
	}
	clearRows();
	layerNote(c, slot);
}

void view_invalidate(void)
{
	int c;

	for (c = 0; c < VIEW_CELLS; c++)
		knownCount[c] = 255;
}

void view_render(const struct view *v)
{
	static int found;
	int c, slot, how;

	if (!found)
		view_invalidate();
	place(v);
	rangeBase = 0;
	if (PROF != 2 || !found)
		visibility(SECTOR_FIRST, SECTOR_FIRST + VIEW_CELLS - 1);
	found = 1;
	if (PROF == 1)
		return;
	for (c = SECTOR_FIRST, slot = 0; c < SECTOR_FIRST + VIEW_CELLS; c++, slot++) {
		how = PROF == 6 && knownCount[slot] != 255 ? 0 : layerCompare(c, slot);
		if (how == 2)
			layerColumn(c, slot);
		else if (how == 1)
			layerFronts(c, slot);
		show(c, layer[slot][0], VIEW_LEFT + slot, &extents[slot]);
	}
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
static unsigned char *broughtColumn[TURN_COLUMNS];	/* each column's place: the layer's, or brought[] */
static struct extent broughtExtent[TURN_COLUMNS];

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
			slot = part[i].facing == turnFacing && c >= SECTOR_FIRST && c < SECTOR_FIRST + VIEW_CELLS
				? c - SECTOR_FIRST : -1;
			broughtColumn[k] = slot >= 0 ? layer[slot][0] : brought[k][0];
			compose(c, broughtColumn[k]);
			extent(c, &broughtExtent[k]);
			if (slot >= 0)
				layerNote(c, slot);
		}
	}
	/* then the slide, and the columns put up with what moves on them */
	ms_hscroll(VIEW_LEFT, VIEW_LEFT + VIEW_CELLS - 1, VIEW_TOP, VIEW_TOP + VIEW_ROWS - 1,
		   turnDir > 0 ? -TURN_COLUMNS : TURN_COLUMNS);
	for (i = 0, k = 0; i < n; i++) {
		placePart(&part[i]);
		for (c = part[i].s0; c <= part[i].s1; c++, k++)
			show(c, broughtColumn[k], part[i].screen + c - part[i].s0, &broughtExtent[k]);
	}
}
