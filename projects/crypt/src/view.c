/* view.c - the view from the party's eyes (view.h): the dungeon's
 * edges, and nothing else.
 *
 * Two halves.  The first finds what each pixel column of the sector
 * shows: the cells about the party gone through front to back, row by
 * row away from it, and each face of a wall takes the pixel columns
 * not yet taken, from a table of where every cell corner falls for
 * every phase of a step - in a row the faces towards the party first,
 * then the faces along the axis; a wall along the axis is one face
 * however many cells long, taken when it ends, or before a face that
 * could show past its beginning.  What a face leaves in a cell column
 * is a piece: the pixels it covers as a mask of eight, and which wall.
 *
 * The second draws the edges, in one colour on one: every seam of the
 * world's squares, to the pixel.  A wall along the axis stands 60 / r
 * rows above and below the horizon at r cells, a height every pixel
 * column keeps while the party steps along it: its top and its foot
 * are a pixel in each column, a line on a slant; the seams between
 * its cells, and its ends, stand down it where the corners fall.  A
 * wall across the axis is a frame, its foot the row above the floor's
 * edge it stands on.  The floor's cells have their edges across the
 * axis, a row in each column, and along it, a slant like a wall's
 * foot; the ceiling the same upside down; an edge right under a wall's
 * foot is the foot.  What a wall hides is not drawn; where a wall ends
 * with another farther behind it, the corner is the nearer one's.
 * Nearer than DETAIL pixels to a line drawn no other seam between
 * cells is: far away the cells crowd, and only the first of a crowd
 * shows; a face's own edge gives way only to a line beside it. */

#include <fx.h>
#include <ms0515.h>
#include "geometry.h"
#include "map.h"
#include "view.h"

#define HALF (VIEW_ROWS / 2)		/* the horizon's row in the window */
#define MIDDLE (SECTOR_CELLS * 4)	/* the sector's middle pixel column */
#define MAX_PIECES 4
#define LATERALS PLANE_LATERALS		/* cells beside the axis looked at */
#define DETAIL 4			/* no two lines nearer than this many pixels */
#define VIEW_DEPTH 4			/* rows of cells ahead looked at; past them, black */
#define PAPER (MS_PAPER(7) | MS_INK(0))	/* the one colour, and black for the lines */

enum { kPlane, kFront, kDark };	/* kDark: what lies past VIEW_DEPTH, black */

/* The pixel bits by position, and the masks of the pixels from one on
 * and up to one: tables, since the compiler's shift by a variable
 * count is a trap (toolset README.md). */
static const unsigned char bitAt[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
static const unsigned char fromPixel[9] = {0xFF, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01, 0x00};
static const unsigned char toPixel[9] = {0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFF};

struct piece {
	unsigned char mask, kind;
	unsigned char lateral, mirror;	/* kPlane: the plane, and which side (1: the right) */
	signed char from, through;	/* kPlane: the run's rows; kFront: the cell's row in `through` */
	signed char j;			/* kFront: the cell's lateral */
	unsigned char half;		/* kFront: rows above the horizon */
};

/* The columns of a range, at most the view's width and one more past
 * it (the column after the last drawn, looked at for what goes on past
 * a face's right edge): pieces[c - rangeFirst]. */
static struct piece pieces[VIEW_CELLS + 1][MAX_PIECES];
static unsigned char count[VIEW_CELLS + 1];
static unsigned char covered[VIEW_CELLS + 1];

static int cellX, cellY, phase;
static int fx, fy, rx, ry;		/* the facing: forward and right, in cells */
static const unsigned char (*edgeNow)[PLANE_LATERALS];	/* edge_px[phase] */
static const unsigned char (*crossNow)[SECTOR_CELLS];	/* cross_row[phase] */
static const unsigned char *frontHalfNow;		/* front_half[phase] */
static int rangeFirst, rangeLast, rangeBase, uncovered;
#define IX(c) ((c) - rangeFirst + rangeBase)	/* sector column c's index in the range's tables */
static int turnDir, turnFacing;		/* the turn in progress: the old facing's sector slides */
static struct view turnView;
/* The column drawn, a word a row - the pixels low, the attribute high:
 * its lower half alone, from the horizon down.  The view is the same
 * upside down, the ceiling the floor turned over and a wall's top its
 * foot, so the half is put out twice, down and up (ms_vput_folded). */
static unsigned column[HALF];

static void facing(int f)
{
	static const signed char dx[4] = {1, 0, -1, 0}, dy[4] = {0, 1, 0, -1};

	fx = dx[f & 3];
	fy = dy[f & 3];
	rx = -fy;				/* the right hand of someone facing (fx, fy), y down the map */
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
 * the right, between pixel columns x0 and x1, the cells of rows `from`
 * to `through`.  A column's last piece of the same plane - the run of
 * cells before - grows by these pixels. */
static void sideFace(int x0, int x1, int lateral, int mirror, int from, int through)
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
			if (from < p->from)
				p->from = (signed char)from;
			if (through > p->through)
				p->through = (signed char)through;
			continue;
		}
		p = take(c, m);
		if (!p)
			continue;
		p->kind = kPlane;
		p->lateral = (unsigned char)lateral;
		p->mirror = (unsigned char)mirror;
		p->from = (signed char)from;
		p->through = (signed char)through;
	}
}

/* A wall across the axis: the near face of the cell j to the right
 * (left when j < 0), n rows ahead, between x0 and x1, as tall as its
 * depth allows in each column - down to the row above the floor's
 * edge it stands on, the far edge of row n - 1, where the table has
 * that edge, so that the two are one line. */
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
		if (n - 1 <= CROSS_DEPTH)
			half = crossNow[n - 1][c] - HALF;	/* 255 below the window: a wall past it */
		else
			half = fx_mul8(frontHalfNow[n], col_cos[c] > 255 ? 255 : col_cos[c]);
		p->kind = kFront;
		p->half = (unsigned char)(half < 1 ? 1 : half > 255 ? 255 : half);
		p->through = (signed char)n;
		p->j = (signed char)j;
	}
}

/* The far edge of row n, lateral index L (cells 0.5 + L from the
 * axis), as a pixel column: to the right of the middle, or to the
 * left; row -1 is behind the party, past the sector's edge. */
static int edgeRight(int n, int L)
{
	return n < 0 ? SECTOR_CELLS * 8 : MIDDLE + edgeNow[n][L];
}

static int edgeLeft(int n, int L)
{
	return n < 0 ? 0 : MIDDLE - edgeNow[n][L];
}

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
		sideFace(edgeRight(through, L), edgeRight(from - 1, L), L, 1, from, through);
	else
		sideFace(edgeLeft(from - 1, L), edgeLeft(through, L), L, 0, from, through);
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
			frontFace(MIDDLE + near[j - 1], j < LATERALS ? MIDDLE + near[j] : SECTOR_CELLS * 8, j, n);
		else
			frontFace(-j < LATERALS ? MIDDLE - near[-j] : 0, MIDDLE - near[-j - 1], j, n);
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

/* What no wall nearer than the far edge of row VIEW_DEPTH takes: black
 * from that edge's row on the floor to its mirror on the ceiling, a
 * wall of darkness standing there, as tall as the floor's edge has it
 * in each column. */
static void dark(void)
{
	struct piece *p;
	unsigned char m;
	int c, half;

	for (c = rangeFirst; c <= rangeLast; c++) {
		m = (unsigned char)~covered[IX(c)];
		if (!m)
			continue;
		p = take(c, m);
		if (!p)
			continue;
		half = crossNow[VIEW_DEPTH][c] - HALF;	/* 255 below the window: the whole column */
		p->kind = kDark;
		p->half = (unsigned char)(half < 1 ? 1 : half);
	}
}

/* The pieces of the sector columns first..last found, front to back,
 * and of the column after last, inside the sector. */
static void visibility(int first, int last)
{
	unsigned char walls[2 * LATERALS + 1], before[2 * LATERALS + 1];
	int c, n, j, s, L;

	if (last < SECTOR_CELLS - 1)
		last++;
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
	for (n = 0; n <= VIEW_DEPTH && uncovered > 0; n++) {
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
	if (uncovered > 0)
		dark();
}

/* -- The edges drawn ---------------------------------------------------- */

/* The sector pixel columns of the pixels of column c as the tables
 * have them: the left half's, mirrored for the right. */
static unsigned char tableXs[SECTOR_CELLS][8];
static const unsigned char *tx;		/* the column in hand's */

static void tableX(int c)
{
	static int made;
	int k, i, x;

	if (!made) {				/* the tables made once */
		made = 1;
		for (k = 0; k < SECTOR_CELLS; k++)
			for (i = 0, x = k << 3; i < 8; i++, x++)
				tableXs[k][i] = (unsigned char)(x < MIDDLE ? x : 2 * MIDDLE - 1 - x);
	}
	tx = tableXs[c];
}

/* The column is drawn in rows below the horizon, k = 0..HALF - 1 for
 * window row HALF + k, and so above it (column[]).  The wall in each
 * pixel of the column in hand stands height[i] rows either side of the
 * horizon, at most HALF, 0 where there is none: its foot is row
 * height[i] - 1, and the floor begins at row height[i].  lines: the
 * pixels a line stands down, for the thinning. */
static unsigned char height[8], lines;
static unsigned char fronts;		/* the pixels whose wall is across the axis */
static int minHeight, maxHeight;	/* the least and the greatest of height[] */

/* Pixels `bits` black down rows 0..h - 1, and so up as far. */
static void vline(unsigned bits, int h)
{
	unsigned *w = column, *end = column + h;

	for (; w < end; w++)
		*w |= bits;
}

/* A seam of the wall at sector pixel column x, if it falls in the
 * piece's pixels of column c: a line down the wall there, unless one
 * stands near it already - within DETAIL pixels for a seam between a
 * wall's cells (near[] innerNear), but for a face's own edge, where
 * one wall ends and another shows, only the pixels beside it (near[]
 * edgeNear): an edge is the shape of the place, a seam its detail. */
/* The pixels within DETAIL - 1 of pixel i, i among them. */
static const unsigned char innerNear[8] = {0xF0, 0xF8, 0xFC, 0xFE, 0x7F, 0x3F, 0x1F, 0x0F};
/* The pixels within 1 of pixel i. */
static const unsigned char edgeNear[8] = {0xC0, 0xE0, 0x70, 0x38, 0x1C, 0x0E, 0x07, 0x03};

static void seam(int c, const struct piece *p, int x, const unsigned char *near)
{
	int i;

	if (x < 0 || x >= 2 * MIDDLE || (x >> 3) != c)
		return;
	i = x & 7;
	if (!(p->mask & bitAt[i]) || (lines & near[i]))
		return;
	lines |= bitAt[i];
	vline(bitAt[i], height[i]);
}

/* A piece's face from pixel column *x0 to *x1, its edges, wherever
 * nearer walls hide them. */
static void faceEdges(const struct piece *p, int *x0, int *x1)
{
	const unsigned char *near;
	int L = p->lateral;

	if (p->kind == kPlane) {
		if (p->mirror) {		/* on the right the run's near end is its right edge */
			*x0 = edgeRight(p->through, L);
			*x1 = edgeRight(p->from - 1, L);
		} else {
			*x0 = edgeLeft(p->from - 1, L);
			*x1 = edgeLeft(p->through, L);
		}
		return;
	}
	near = edgeNow[p->through - 1];
	if (p->j == 0) {
		*x0 = MIDDLE - near[0];
		*x1 = MIDDLE + near[0];
	} else if (p->j > 0) {
		*x0 = MIDDLE + near[p->j - 1];
		*x1 = p->j < LATERALS ? MIDDLE + near[p->j] : 2 * MIDDLE;
	} else {
		*x0 = -p->j < LATERALS ? MIDDLE - near[-p->j] : 0;
		*x1 = MIDDLE - near[-p->j - 1];
	}
}

/* Whether a face begins at sector pixel column x as tall as `half`
 * rows above the horizon, or nearly - a piece shows there whose left
 * edge x is, and draws it.  A face farther away may begin on the same
 * ray as a nearer one ends, its corner straight behind: shorter, its
 * edge is not the nearer one's.  Past the sector, taken as so. */
static int begins(int x, int half)
{
	const struct piece *p;
	int c = x >> 3, i, k, x0, x1, h;

	if (x < 0 || x >= 2 * MIDDLE)
		return 1;
	if (c < rangeFirst || c > rangeLast)
		return 0;
	i = IX(c);
	for (k = 0; k < count[i]; k++) {
		p = &pieces[i][k];
		if (p->mask & bitAt[x & 7]) {
			if (p->kind == kDark)
				return 0;
			faceEdges(p, &x0, &x1);
			if (x0 != x)
				return 0;
			h = p->kind == kPlane ? wall_half[p->lateral][x < MIDDLE ? x : 2 * MIDDLE - 1 - x] : p->half;
			return (h > HALF ? HALF : h) + 2 >= half;
		}
	}
	return 0;
}

/* A face's right edge, at pixel x1: a seam at the pixel before it,
 * the face's last, unless a face begins at x1 - whose own left edge
 * the seam is then, drawn once.  A face farther away that shows on
 * past x1 has its edge hidden: the seam is this one's. */
static void rightSeam(int c, const struct piece *p, int x1)
{
	int x = x1 - 1;

	if (x < 0 || (x >> 3) != c || !(p->mask & bitAt[x & 7]))	/* not in this column: begins() spared */
		return;
	if (!begins(x1, height[x & 7]))
		seam(c, p, x, edgeNear);
}

/* The darkness into the column: black over the rows it stands over,
 * taken for the floor's lines as a wall across the axis. */
static void darkness(const struct piece *p)
{
	int i, h = p->half > HALF ? HALF : p->half;

	for (i = 0; i < 8; i++)
		if (p->mask & bitAt[i])
			height[i] = (unsigned char)h;
	vline(p->mask, h);
	fronts |= p->mask;
}

/* A piece's wall into the column: the rows it stands over in each
 * pixel, its top and its foot a pixel each, and its seams - its left
 * edge, the edges between its cells, and its right edge where no wall
 * goes on. */
static void wall(int c, const struct piece *p)
{
	const unsigned char *halves;
	int i, half, n, x, x0, x1, lo, hi, L = p->lateral;

	if (p->kind == kDark) {
		darkness(p);
		return;
	}
	if (p->kind == kFront) {		/* one height across the piece: its foot in one go */
		half = p->half;
		for (i = 0; i < 8; i++)
			if (p->mask & bitAt[i])
				height[i] = (unsigned char)(half > HALF ? HALF : half);
		if (half <= HALF)
			column[half - 1] |= p->mask;	/* the foot, and so the top */
		fronts |= p->mask;
	} else {
		halves = wall_half[L];
		for (i = 0; i < 8; i++) {
			if (!(p->mask & bitAt[i]))
				continue;
			half = halves[tx[i]];
			if (!half)
				continue;
			if (half > HALF) {
				height[i] = HALF;
				continue;
			}
			height[i] = (unsigned char)half;
			column[half - 1] |= bitAt[i];	/* the foot, and so the top */
		}
	}
	faceEdges(p, &x0, &x1);
	if (p->kind == kPlane) {
		/* The seams between the run's cells: the corners come nearer
		 * the sector's middle row by row, so going from the column's
		 * outer side in, those short of the column are passed and the
		 * first past it ends the loop. */
		lo = c << 3;
		hi = lo + 7;
		if (p->mirror) {
			for (n = p->through - 1; n >= p->from; n--) {
				x = MIDDLE + edgeNow[n][L];
				if (x > hi)
					break;
				if (x >= lo)
					seam(c, p, x, innerNear);
			}
		} else {
			for (n = p->from; n < p->through; n++) {
				x = MIDDLE - edgeNow[n][L];
				if (x > hi)
					break;
				if (x >= lo)
					seam(c, p, x, innerNear);
			}
		}
	}
	seam(c, p, x0, edgeNear);
	rightSeam(c, p, x1);
}

/* The floor's edges across the axis in the column, and the ceiling's
 * upside down: the nearest first, each at least DETAIL rows from the
 * one before, in the pixels where no wall stands over the row - nor
 * stands on it: the edge right under the foot of a wall across the
 * axis is that foot.  Under a wall along the axis the edge runs up to
 * the foot, and meets it. */
static void crossEdges(int c)
{
	int n, r, k, i, last = VIEW_ROWS + DETAIL;
	unsigned bits;

	for (n = 1; n < VIEW_DEPTH; n++) {	/* row VIEW_DEPTH's far edge is the darkness's foot */
		r = crossNow[n][c];
		if (r == 255)
			continue;
		if (r > last - DETAIL)
			continue;
		last = r;
		k = r - HALF;
		if (k < minHeight)		/* under every pixel's wall: so are the rest, nearer the horizon */
			break;
		if (k > maxHeight) {
			bits = 0xFF;
		} else {
			bits = 0;
			if (!fronts) {		/* the walls all along the axis: the loop kept short */
				for (i = 0; i < 8; i++)
					if (k >= height[i])
						bits |= bitAt[i];
			} else {
				for (i = 0; i < 8; i++)
					if (k > height[i] || (k == height[i] && !(fronts & bitAt[i])))
						bits |= bitAt[i];
			}
		}
		column[k] |= bits;		/* and so the ceiling's */
	}
}

/* The floor's edges along the axis in the column, and the ceiling's
 * upside down: in each pixel the rows where the planes L + 0.5 aside
 * meet the floor - a wall's foot, were a wall there - the nearest
 * first, each at least DETAIL from the one before and from the wall's
 * foot. */
/* Which rows those are, the thinning done: geometry alone, tabled
 * once - alongTab[c][L][i] the row of plane L's line in pixel i of
 * sector column c, 0 for none; alongMin and alongMax the least and the
 * greatest of a column's rows for the plane.  A plane farther out has
 * its rows nearer the horizon, so a line under a pixel's wall has the
 * farther ones under it too. */
static unsigned char alongTab[SECTOR_CELLS][LATERALS][8];
static unsigned char alongMin[SECTOR_CELLS][LATERALS], alongMax[SECTOR_CELLS][LATERALS];
static int alongMade;

static void alongTables(void)
{
	int c, i, L, half, r, last, x;

	for (c = 0; c < SECTOR_CELLS; c++) {
		for (L = 0; L < LATERALS; L++) {
			alongMin[c][L] = 255;
			alongMax[c][L] = 0;
		}
		for (i = 0; i < 8; i++) {
			x = (c << 3) + i;
			x = x < MIDDLE ? x : 2 * MIDDLE - 1 - x;
			last = VIEW_ROWS + DETAIL;
			for (L = 0; L < LATERALS; L++) {
				half = wall_half[L][x];
				r = HALF + half;
				alongTab[c][L][i] = 0;
				if (!half || r >= VIEW_ROWS || r > last - DETAIL)
					continue;
				last = r;
				r -= HALF;			/* rows below the horizon, from 1 */
				alongTab[c][L][i] = (unsigned char)r;
				if (r < alongMin[c][L])
					alongMin[c][L] = (unsigned char)r;
				if (r > alongMax[c][L])
					alongMax[c][L] = (unsigned char)r;
			}
		}
	}
}

/* A line's pixel at row k below the horizon, and so the ceiling's mirror. */
#define ALONG(k, bit) (column[k] |= (bit))

static void alongEdges(int c)
{
	const unsigned char *rows;
	int i, L, r;

	for (L = 0; L < LATERALS; L++) {
		if (!alongMax[c][L])
			continue;
		if (alongMax[c][L] < minHeight)	/* under every wall's foot: the farther too */
			break;
		rows = alongTab[c][L];
		if (alongMin[c][L] > maxHeight) {	/* over none: all of them */
			for (i = 0; i < 8; i++)
				if ((r = rows[i]) != 0)
					ALONG(r, bitAt[i]);
			continue;
		}
		/* Right under the foot of a wall along the axis the line is
		 * the foot itself; one across the axis it runs up to, and
		 * meets. */
		if (!fronts) {			/* the walls all along the axis: the loop kept short */
			for (i = 0; i < 8; i++)
				if ((r = rows[i]) != 0 && r > height[i])
					ALONG(r, bitAt[i]);
			continue;
		}
		for (i = 0; i < 8; i++)
			if ((r = rows[i]) != 0 && (r > height[i] || (r == height[i] && (fronts & bitAt[i]))))
				ALONG(r, bitAt[i]);
	}
}

/* PROF, a build switch for measuring: 1 finds the pieces and draws
 * nothing, 2 draws but puts nothing on the screen, 3 clears and puts
 * the columns but draws no edges, 4 draws the walls' edges alone. */
#ifndef PROF
#define PROF 0
#endif

/* Sector column c drawn on screen column `screen` from its pieces. */
static void draw(int c, int screen)
{
	const unsigned paper = (unsigned)PAPER << 8;
	unsigned *w = column;
	int row, i;

	if (PROF == 1)
		return;
	for (row = 0; row < HALF; row += 4, w += 4) {	/* four words a turn: the loop's own cost was the half */
		w[0] = paper;
		w[1] = paper;
		w[2] = paper;
		w[3] = paper;
	}
	for (i = 0; i < 8; i++)
		height[i] = 0;
	lines = 0;
	fronts = 0;
	tableX(c);
	if (PROF != 3) {
		for (i = 0; i < count[IX(c)]; i++)
			wall(c, &pieces[IX(c)][i]);
		minHeight = maxHeight = height[0];
		for (i = 1; i < 8; i++) {
			if (height[i] < minHeight)
				minHeight = height[i];
			if (height[i] > maxHeight)
				maxHeight = height[i];
		}
		if (PROF != 4) {
			crossEdges(c);
			alongEdges(c);
		}
	}
	if (PROF != 2)
		ms_vput_folded(screen, VIEW_TOP + HALF, HALF, column);
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
	facing(v->facing);
	if (!alongMade) {			/* the floor's lines along tabled once */
		alongTables();
		alongMade = 1;
	}
}

void view_invalidate(void)
{
}

void view_render(const struct view *v)
{
	int c;

	place(v);
	rangeBase = 0;
	visibility(SECTOR_FIRST, SECTOR_FIRST + VIEW_CELLS - 1);
	for (c = SECTOR_FIRST; c < SECTOR_FIRST + VIEW_CELLS; c++)
		draw(c, VIEW_LEFT + c - SECTOR_FIRST);
}

void view_turn_begin(const struct view *v, int dir)
{
	turnView = *v;
	turnView.phase = 0;
	turnDir = dir;
	turnFacing = (v->facing + dir) & 3;
}

/* A pass of a turn brings in TURN_COLUMNS columns, of the old facing's
 * sector, or of the new one's past its end (or before its beginning),
 * or some of each: the parts, each with its own facing. */
#define TURN_COLUMNS 4
struct part { int s0, s1, screen, facing, base; };

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

/* A part placed: its facing's tables, its columns' pieces at `base`. */
static void placePart(const struct part *p)
{
	struct view v = turnView;

	v.facing = p->facing;
	place(&v);
	rangeFirst = p->s0;
	rangeLast = p->s1 < SECTOR_CELLS - 1 ? p->s1 + 1 : p->s1;	/* as visibility() looked */
	rangeBase = p->base;
}

void view_turn_pass(int pass)
{
	struct part part[2];
	int n, i, c, k = 0;

	if (turnDir > 0)			/* to the right: the picture slides left */
		n = parts(SECTOR_FIRST + TURN_COLUMNS * pass + VIEW_CELLS - TURN_COLUMNS,
			  SECTOR_FIRST + TURN_COLUMNS * pass + VIEW_CELLS - 1, VIEW_LEFT + VIEW_CELLS - TURN_COLUMNS, part);
	else
		n = parts(SECTOR_FIRST - TURN_COLUMNS * pass, SECTOR_FIRST - TURN_COLUMNS * pass + TURN_COLUMNS - 1,
			  VIEW_LEFT, part);
	for (i = 0; i < n; i++) {		/* the pieces found first, each part's in its own slots */
		part[i].base = k;
		placePart(&part[i]);
		visibility(part[i].s0, part[i].s1);
		k += rangeLast - rangeFirst + 1;
	}
	ms_hscroll(VIEW_LEFT, VIEW_LEFT + VIEW_CELLS - 1, VIEW_TOP, VIEW_TOP + VIEW_ROWS - 1,
		   turnDir > 0 ? -TURN_COLUMNS : TURN_COLUMNS);
	for (i = 0; i < n; i++) {		/* then the slide, and the columns drawn right after it */
		placePart(&part[i]);
		for (c = part[i].s0; c <= part[i].s1; c++)
			draw(c, part[i].screen + c - part[i].s0);
	}
}
