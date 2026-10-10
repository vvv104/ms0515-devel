/* view.c - the view from the party's eyes (view.h).
 *
 * The cells about the party are gone through front to back, row by
 * row away from it, and each face of a wall takes the pixel columns
 * of the sector not yet taken, from a table of where every cell
 * corner falls for every phase of a step: in a row, the faces towards
 * the party first, then the faces along the axis; a wall along the
 * axis is one face however many cells long, taken when it ends, or
 * before a face that could show past its beginning.  What a face
 * leaves in a cell column is a piece: the pixels it covers as a mask
 * of eight and where its rows come from - a column drawn on the host
 * for a wall along the axis, a texture column for a wall across it.
 * The pieces of a column are then painted farthest first, the
 * farthest with its ceiling above and floor below, the nearer ones
 * over it under their masks; a nearer wall is always the taller, so
 * what a far piece paints outside its mask is painted over.  The
 * floor is a plain dither for now: the edges of its cells, which the
 * first version drew as lines, cost a multiply a line a column, more
 * than the walls; they will come back from a table. */

#include <fx.h>
#include <ms0515.h>
#include <tex.h>
#include "art.h"
#include "data.h"
#include "map.h"
#include "planes.h"
#include "view.h"

#define HALF (VIEW_ROWS / 2)		/* the horizon's row in the window */
#define BOTTOM (VIEW_TOP + VIEW_ROWS - 1)
#define MIDDLE (SECTOR_CELLS * 4)	/* the sector's middle pixel column */
#define MAX_PIECES 4
#define LATERALS 3			/* cells beside the axis looked at: the planes drawn */
#define FLOOR 0xAA, MS_INK(4)
#define FAR_PIXELS 0x55			/* past the depth drawn: a dim bar on the horizon */
#define FAR_INK MS_INK(2)

enum { kPlane, kFront, kFar };

struct piece {
	unsigned char mask, kind, slot;
	unsigned char top, bottom;	/* window rows, both in */
	unsigned char mirror;		/* kPlane: the plane's other side - its pixels turned round */
	unsigned char mip, texCol;	/* kFront: the texture and its column */
	unsigned char half;		/* kFront: rows above the horizon, maybe past the window */
	const unsigned char *words;	/* kPlane: a word a row from top */
};

/* The columns of a range, at most the view's width: pieces[c - rangeFirst]. */
static struct piece pieces[VIEW_CELLS][MAX_PIECES];
static unsigned char count[VIEW_CELLS];
static unsigned char covered[VIEW_CELLS];
/* What each screen column of the view shows: the rows its ceiling and
 * floor were last painted to, so that a pass repaints them only when
 * the wall's rows change - along a corridor they never do. */
static unsigned char shownTop[VIEW_CELLS], shownBottom[VIEW_CELLS];
#define UNSHOWN 255
static const struct tex *const mips[3] = {&wall, &wall2, &wall4};

static int cellX, cellY, phase;
static int fx, fy, rx, ry;		/* the facing: forward and right, in cells */
static int rangeFirst, rangeLast, uncovered;
#define IX(c) ((c) - rangeFirst)	/* sector column c's index in the range's tables */
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
 * level's edge is wall.  The row's cells lie a stride apart in the
 * level, so one lookup each. */
static void rowWalls(int n, unsigned char *walls)
{
	int x = cellX + n * fx - LATERALS * rx, y = cellY + n * fy - LATERALS * ry;
	int j;

	for (j = 0; j < 2 * LATERALS + 1; j++, x += rx, y += ry)
		walls[j] = (unsigned char)(x < 0 || y < 0 || x >= MAP_SIDE || y >= MAP_SIDE || map[(y << 5) + x]);
}

/* The pixels of column c inside [x0, x1) not taken yet, as a mask. */
static unsigned char span(int c, int x0, int x1)
{
	unsigned char m = 0xFF;
	int lo = c << 3, hi = lo + 8;

	if (x0 > lo)
		m &= (unsigned char)(0xFF >> (x0 - lo));
	if (x1 < hi)
		m &= (unsigned char)(0xFF << (hi - x1));
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
	p->mask = mask;
	return p;
}

/* The column's last piece taken - the farthest so far - or 0. */
static struct piece *last(int c)
{
	int i = IX(c);

	return count[i] ? &pieces[i][count[i] - 1] : 0;
}

static void grow(int c, struct piece *p, unsigned char mask)
{
	cover(c, mask);
	p->mask |= mask;
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

/* A wall along the axis, `lateral` planes out on the left, or on the
 * right (`mirror`: the left's image turned round), between the pixel
 * columns x0 and x1: its columns from the plane drawn on the host, at
 * this phase. */
static void sideFace(int x0, int x1, int lateral, int mirror)
{
	const struct plane_ref *ref = &plane_refs[lateral][phase];
	const unsigned *offsets;
	const unsigned char *record;
	struct piece *p;
	unsigned char m;
	unsigned offset;
	int c, c0, c1;

	if (!columns(x0, x1, &c0, &c1))
		return;
	data_select(ref->slot);
	offsets = (const unsigned *)data_at(ref->slot, ref->offset);
	for (c = c0; c <= c1; c++) {
		m = span(c, x0, x1);
		if (!m)
			continue;
		offset = offsets[mirror ? SECTOR_CELLS - 1 - c : c];
		if (offset == 0) {		/* the plane runs past the depth drawn here */
			farPiece(c, m);
			continue;
		}
		record = (const unsigned char *)offsets + offset;
		/* The same plane's last piece in the column - the row before
		 * along a straight wall - grows by these pixels: one piece
		 * for the wall, however many cells long. */
		p = last(c);
		if (p && p->kind == kPlane && p->words == record + 2 && p->mirror == mirror) {
			grow(c, p, m);
			continue;
		}
		p = take(c, m);
		if (!p)
			continue;
		p->kind = kPlane;
		p->slot = ref->slot;
		p->mirror = (unsigned char)mirror;
		p->top = record[0];
		p->bottom = record[1];
		p->words = record + 2;
	}
}

/* A wall across the axis: the near face of the cell j to the right and
 * n ahead, between x0 and x1: texture columns, as tall as its depth
 * allows in each column, the texture's column by where the column's
 * ray meets the face. */
static void frontFace(int x0, int x1, int j, int n)
{
	int d8 = (n << 8) - 128 - phase_8[phase];	/* the face's depth, 8.8 */
	int faceLeft = (j << 8) - 128;			/* its left edge, lateral 8.8 */
	struct piece *p;
	unsigned char m;
	int c, c0, c1, half, u8, level;

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
		level = half >= 30 ? 0 : half >= 15 ? 1 : 2;
		/* Along the face, 8.8: the depth times the column's tangent,
		 * which is under one but at the sector's edges - eight steps. */
		u8 = (col_tan[c] < 0 ? -fx_mul8(-col_tan[c] > 255 ? 255 : -col_tan[c], d8)
				     : fx_mul8(col_tan[c] > 255 ? 255 : col_tan[c], d8)) - faceLeft;
		if (half > 255)
			half = 255;
		p->kind = kFront;
		p->mip = (unsigned char)level;
		p->texCol = (unsigned char)((u8 >> (4 + level)) & (15 >> level));
		p->half = (unsigned char)half;
		p->top = (unsigned char)(half > HALF ? 0 : HALF - half);
		p->bottom = (unsigned char)(half > HALF ? VIEW_ROWS - 1 : HALF + half - 1);
	}
}

/* The far edge of row n, lateral index L (cells 0.5 + L from the
 * axis), as a pixel column: to the right of the middle, or to the
 * left; row -1 is behind the party, past the sector's edge. */
#define EDGE_RIGHT(n, L) ((n) < 0 ? SECTOR_CELLS * 8 : MIDDLE + edge_px[phase][n][L])
#define EDGE_LEFT(n, L) ((n) < 0 ? 0 : MIDDLE - edge_px[phase][n][L])

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
		sideFace(EDGE_RIGHT(through, L), EDGE_RIGHT(from - 1, L), L, 1);
	else
		sideFace(EDGE_LEFT(from - 1, L), EDGE_LEFT(through, L), L, 0);
}

static void flushAll(int through)
{
	int s, L;

	for (s = 0; s < 2; s++)
		for (L = 0; L < LATERALS; L++)
			flush(s, L, through);
}

/* The pieces of the sector columns first..last found, front to back. */
static void visibility(int first, int last)
{
	const unsigned char *near;
	unsigned char walls[2 * LATERALS + 1], before[2 * LATERALS + 1];
	int c, n, j, s, L, inner;

	rangeFirst = first;
	rangeLast = last;
	for (c = 0; c <= last - first; c++) {
		count[c] = 0;
		covered[c] = 0;
	}
	uncovered = last - first + 1;
	for (s = 0; s < 2; s++)
		for (L = 0; L < LATERALS; L++)
			runFrom[s][L] = -1;
	/* Row by row: the cells of the row looked up once, those of the
	 * row before kept.  A wall's face towards the party shows only
	 * when the cell before it is floor; its face towards the axis only
	 * when the cell between is floor. */
	rowWalls(0, before);
	for (n = 0; n <= PLANE_DEPTH && uncovered > 0; n++) {
		if (n > 0)
			rowWalls(n, walls);
		else
			for (j = 0; j < 2 * LATERALS + 1; j++)
				walls[j] = before[j];
		if (n > 0) {
			near = edge_px[phase][n - 1];	/* the row's near edge */
			for (j = -LATERALS; j <= LATERALS; j++)
				if (walls[LATERALS + j] && !before[LATERALS + j]) {
					/* the runs so far, nearer than this row, first */
					flushAll(n - 1);
					if (j == 0)
						frontFace(MIDDLE - near[0], MIDDLE + near[0], 0, n);
					else if (j > 0)
						frontFace(MIDDLE + near[j - 1], MIDDLE + near[j], j, n);
					else
						frontFace(MIDDLE - near[-j], MIDDLE - near[-j - 1], j, n);
				}
		}
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
		for (j = 0; j < 2 * LATERALS + 1; j++)
			before[j] = walls[j];
	}
	flushAll(n - 1);
	for (c = first; c <= last; c++)
		if (covered[IX(c)] != 0xFF)
			farPiece(c, (unsigned char)~covered[IX(c)]);
}

/* The pieces of sector column c painted on screen column `screen`,
 * farthest first. */
static void paint(int c, int screen)
{
	struct piece *p;
	int i, first = 1, rows, y0, y1;

	/* The common column, a wall along the axis and nothing else:
	 * copied, and its ceiling and floor filled when they changed. */
	p = &pieces[IX(c)][0];
	if (count[IX(c)] == 1 && p->kind == kPlane && p->mask == 0xFF) {
		data_select(p->slot);
		rows = p->bottom - p->top + 1;
		if (p->mirror)
			ms_vput_mirror(screen, VIEW_TOP + p->top, rows, p->words);
		else
			ms_vput(screen, VIEW_TOP + p->top, rows, p->words);
		if (p->top > 0 && p->top != shownTop[screen - VIEW_LEFT])
			ms_vfill(screen, VIEW_TOP, VIEW_TOP + p->top - 1, 0, 0);
		if (p->bottom < VIEW_ROWS - 1 && p->bottom != shownBottom[screen - VIEW_LEFT])
			ms_vfill(screen, VIEW_TOP + p->bottom + 1, BOTTOM, FLOOR);
		shownTop[screen - VIEW_LEFT] = p->top;
		shownBottom[screen - VIEW_LEFT] = p->bottom;
		return;
	}
	/* The farthest piece paints the column whole: its ceiling and
	 * floor over all eight pixels, its wall under its mask, the rest
	 * of the wall's pixels black.  A nearer piece is taller, so its
	 * ceiling and floor lie inside the farthest one's, painted
	 * already: it paints its wall rows alone, under its mask. */
	for (i = count[IX(c)] - 1; i >= 0; i--, first = 0) {
		p = &pieces[IX(c)][i];
		if (first && p->top > 0 && p->top != shownTop[screen - VIEW_LEFT])
			ms_vfill(screen, VIEW_TOP, VIEW_TOP + p->top - 1, 0, 0);
		rows = p->bottom - p->top + 1;
		switch (p->kind) {
		case kPlane:
			data_select(p->slot);
			if (p->mirror) {
				if (p->mask == 0xFF)
					ms_vput_mirror(screen, VIEW_TOP + p->top, rows, p->words);
				else
					ms_vput_mirror_masked(screen, VIEW_TOP + p->top, rows, p->words, p->mask);
			} else if (p->mask == 0xFF) {
				ms_vput(screen, VIEW_TOP + p->top, rows, p->words);
			} else {
				ms_vput_masked(screen, VIEW_TOP + p->top, rows, p->words, p->mask);
			}
			break;
		case kFront:
			y0 = VIEW_TOP + HALF - p->half;
			y1 = VIEW_TOP + HALF + p->half - 1;
			if (p->mask == 0xFF)
				tex_column_in(mips[p->mip], p->texCol, screen, y0, y1, VIEW_TOP, BOTTOM);
			else
				tex_column_masked(mips[p->mip], p->texCol, screen, y0, y1, VIEW_TOP, BOTTOM, p->mask);
			break;
		default:
			if (p->mask == 0xFF)
				ms_vfill(screen, VIEW_TOP + p->top, VIEW_TOP + p->bottom, FAR_PIXELS, FAR_INK);
			else
				ms_vfill_masked(screen, VIEW_TOP + p->top, VIEW_TOP + p->bottom, FAR_PIXELS, p->mask);
			break;
		}
		if (first && p->bottom < VIEW_ROWS - 1 && p->bottom != shownBottom[screen - VIEW_LEFT])
			ms_vfill(screen, VIEW_TOP + p->bottom + 1, BOTTOM, FLOOR);
	}
	/* Remembered only for a column of one piece: with more, the nearer
	 * ones' pixels lie among the farthest one's ceiling and floor rows,
	 * and the next pass must paint those over. */
	if (count[IX(c)] == 1) {
		shownTop[screen - VIEW_LEFT] = p->top;
		shownBottom[screen - VIEW_LEFT] = p->bottom;
	} else {
		shownTop[screen - VIEW_LEFT] = shownBottom[screen - VIEW_LEFT] = UNSHOWN;
	}
}

/* The memory of what the screen shows moved with the picture: `by`
 * columns to the right, or to the left, the columns coming in unknown. */
static void shownScrolled(int by)
{
	int c;

	if (by > 0) {
		for (c = VIEW_CELLS - 1; c >= by; c--) {
			shownTop[c] = shownTop[c - by];
			shownBottom[c] = shownBottom[c - by];
		}
		for (c = 0; c < by; c++)
			shownTop[c] = shownBottom[c] = UNSHOWN;
	} else {
		for (c = 0; c < VIEW_CELLS + by; c++) {
			shownTop[c] = shownTop[c - by];
			shownBottom[c] = shownBottom[c - by];
		}
		for (c = VIEW_CELLS + by; c < VIEW_CELLS; c++)
			shownTop[c] = shownBottom[c] = UNSHOWN;
	}
}

static void place(const struct view *v)
{
	cellX = v->cellX;
	cellY = v->cellY;
	phase = v->phase;
	facing(v->facing);
}

/* PROF, a build switch for measuring: 1 finds the pieces and paints
 * nothing, 2 paints the pieces found once and finds no more. */
#ifndef PROF
#define PROF 0
#endif

void view_render(const struct view *v)
{
	static int found;
	int c;

	if (!found)
		for (c = 0; c < VIEW_CELLS; c++)
			shownTop[c] = shownBottom[c] = UNSHOWN;
	place(v);
	if (PROF != 2 || !found)
		visibility(SECTOR_FIRST, SECTOR_FIRST + VIEW_CELLS - 1);
	found = 1;
	if (PROF == 1)
		return;
	for (c = SECTOR_FIRST; c < SECTOR_FIRST + VIEW_CELLS; c++)
		paint(c, VIEW_LEFT + c - SECTOR_FIRST);
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
 * `screen`.  The columns of one facing are found together. */
static void turnColumns(int s0, int s1, int screen)
{
	struct view v = turnView;
	int c;

	if (s0 < 0 && s1 >= 0) {		/* the ranges straddle the facings: each its own */
		turnColumns(s0, -1, screen);
		turnColumns(0, s1, screen + (0 - s0));
		return;
	}
	if (s0 < SECTOR_CELLS && s1 >= SECTOR_CELLS) {
		turnColumns(s0, SECTOR_CELLS - 1, screen);
		turnColumns(SECTOR_CELLS, s1, screen + (SECTOR_CELLS - s0));
		return;
	}
	if (s0 >= SECTOR_CELLS) {
		s0 -= SECTOR_CELLS;
		s1 -= SECTOR_CELLS;
		v.facing = turnFacing;
	} else if (s0 < 0) {
		s0 += SECTOR_CELLS;
		s1 += SECTOR_CELLS;
		v.facing = turnFacing;
	}
	place(&v);
	visibility(s0, s1);
	for (c = s0; c <= s1; c++)
		paint(c, screen + c - s0);
}

void view_turn_pass(int pass)
{
	if (turnDir > 0) {			/* to the right: the picture slides left */
		ms_hscroll(VIEW_LEFT, VIEW_LEFT + VIEW_CELLS - 1, VIEW_TOP, BOTTOM, -4);
		shownScrolled(-4);
		turnColumns(SECTOR_FIRST + 4 * pass + VIEW_CELLS - 4, SECTOR_FIRST + 4 * pass + VIEW_CELLS - 1,
			    VIEW_LEFT + VIEW_CELLS - 4);
	} else {
		ms_hscroll(VIEW_LEFT, VIEW_LEFT + VIEW_CELLS - 1, VIEW_TOP, BOTTOM, 4);
		shownScrolled(4);
		turnColumns(SECTOR_FIRST - 4 * pass, SECTOR_FIRST - 4 * pass + 3, VIEW_LEFT);
	}
}
