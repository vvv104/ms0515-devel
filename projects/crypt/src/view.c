/* view.c - the view from the party's eyes (view.h).
 *
 * The rays are cast the way Lode Vandevenne's tutorial casts them, in
 * 8.8 fixed point: a direction vector (cos, sin) and a camera plane
 * across it, tan 30 degrees long for a view 60 degrees wide; each
 * column's ray is the direction plus a share of the plane, found by
 * adding a constant share from column to column.  The ray walks the
 * grid a cell edge at a time, the distance to the next x edge and the
 * next y edge kept and the nearer taken, until a wall; the distance
 * walked along the ray's axis is the wall's perpendicular distance, no
 * fish-eye and no division.  1/|ray| for the edge-to-edge distances
 * and the wall's height for a distance come from tables.
 *
 * A wall is drawn flat, its shade by distance and by the side it shows,
 * or with the brick texture, the column by where the ray met the cell;
 * the ceiling is black and the floor a dark dither.  The walls stand
 * from the window's middle, as tall as the table says, cut by the
 * window's edges. */

#include <fx.h>
#include <ms0515.h>
#include <tex.h>
#include "art.h"
#include "map.h"
#include "view.h"

#define PLANE 148			/* tan 30 degrees in 8.8 */
#define PLANE_SHARE 23			/* 2/22 of the plane, in 8.8, a column's step */
#define FAR 32767
#define STEPS 48			/* cells a ray may walk */
#define MIDDLE (VIEW_TOP + VIEW_ROWS / 2)
#define BOTTOM (VIEW_TOP + VIEW_ROWS - 1)

static int recip[512];			/* 65536 / v for v in 8.8: the edge-to-edge distance */
static int heights[512];		/* a wall's rows by its distance, 16ths of a cell */
static int ready;
static int heightSeen[VIEW_CELLS];
static int topSeen[VIEW_CELLS], bottomSeen[VIEW_CELLS];	/* the wall's rows last time */

void view_init(void)
{
	int i;

	if (ready)
		return;
	ready = 1;
	for (i = 0; i < VIEW_CELLS; i++) {	/* as if a wall filled the window: all painted once */
		topSeen[i] = VIEW_TOP;
		bottomSeen[i] = BOTTOM;
	}
	recip[0] = recip[1] = FAR;
	for (i = 2; i < 512; i++)
		recip[i] = (int)(65536L / i);
	heights[0] = 2 * VIEW_ROWS;
	for (i = 1; i < 512; i++) {
		long h = (long)VIEW_ROWS * 16 / i;	/* a cell-tall wall fills the window a cell away */
		heights[i] = h > 2 * VIEW_ROWS ? 2 * VIEW_ROWS : (int)h;
	}
}

int view_height(int c)
{
	return heightSeen[c];
}

/* The ray of one column walked to its wall: the perpendicular distance
 * back in 8.8, the side it hit (0 an x edge, 1 a y edge) and where
 * along the cell (0..255) through the pointers. */
static int cast(const struct view *v, int rayX, int rayY, int *side, int *along)
{
	int mapX = v->x >> 8, mapY = v->y >> 8;
	int deltaX = recip[rayX < 0 ? -rayX : rayX];
	int deltaY = recip[rayY < 0 ? -rayY : rayY];
	int stepX, stepY, sideX, sideY, n, perp;

	if (rayX < 0) {
		stepX = -1;
		sideX = fx_mul(v->x & 255, deltaX);
	} else {
		stepX = 1;
		sideX = fx_mul(256 - (v->x & 255), deltaX);
	}
	if (rayY < 0) {
		stepY = -1;
		sideY = fx_mul(v->y & 255, deltaY);
	} else {
		stepY = 1;
		sideY = fx_mul(256 - (v->y & 255), deltaY);
	}
	for (n = 0; n < STEPS; n++) {
		if (sideX < sideY) {
			sideX = sideX > FAR - deltaX ? FAR : sideX + deltaX;
			mapX += stepX;
			*side = 0;
		} else {
			sideY = sideY > FAR - deltaY ? FAR : sideY + deltaY;
			mapY += stepY;
			*side = 1;
		}
		if (map_at(mapX, mapY))
			break;
	}
	perp = *side == 0 ? sideX - deltaX : sideY - deltaY;
	if (perp < 16)
		perp = 16;
	*along = (*side == 0 ? v->y + fx_mul(perp, rayY) : v->x + fx_mul(perp, rayX)) & 255;
	return perp;
}

/* A flat wall's look by its distance and side: the pattern and the ink. */
static void shade(int perp, int side, unsigned char *pattern, unsigned char *attribute)
{
	static const unsigned char patterns[5] = {0xFF, 0xFF, 0xDD, 0x55, 0x11};
	static const unsigned char attributes[5] = {
		MS_INK(2) | MS_BRIGHT, MS_INK(2), MS_INK(2), MS_INK(2), MS_INK(2)};
	int level = perp < 2 * CELL ? 0 : perp < 5 * CELL ? 1 : perp < 10 * CELL ? 2 : perp < 18 * CELL ? 3 : 4;

	if (side && level < 4)
		level++;
	*pattern = patterns[level];
	*attribute = attributes[level];
}

static void column(const struct view *v, int c, int perp, int side, int along)
{
	int index = perp >> 4;
	int h = heights[index > 511 ? 511 : index];
	int top = MIDDLE - h / 2, bottom = top + h - 1;
	unsigned char pattern, attribute;
	int screen = VIEW_LEFT + c;

	int ctop = top < VIEW_TOP ? VIEW_TOP : top;		/* the wall's rows in the window */
	int cbottom = bottom > BOTTOM ? BOTTOM : bottom;

	heightSeen[c] = h;
	/* The ceiling and the floor are painted only where the wall stood
	 * last time and stands no more: the rest is as it was. */
	if (topSeen[c] < ctop)
		ms_vfill(screen, topSeen[c], ctop - 1, 0, 0);
	if (bottomSeen[c] > cbottom)
		ms_vfill(screen, cbottom + 1, bottomSeen[c], 0xAA, MS_INK(4));
	topSeen[c] = ctop;
	bottomSeen[c] = cbottom;
	if (v->textured) {
		tex_column_in(&wall, along >> 4, screen, top, bottom, VIEW_TOP, BOTTOM);
	} else {
		shade(perp, side, &pattern, &attribute);
		ms_vfill(screen, ctop, cbottom, pattern, attribute);
	}
}

void view_render(const struct view *v)
{
	int dirX = fx_cos(v->angle), dirY = fx_sin(v->angle);
	int planeX = fx_mul(-dirY, PLANE), planeY = fx_mul(dirX, PLANE);
	int shareX = fx_mul(planeX, PLANE_SHARE), shareY = fx_mul(planeY, PLANE_SHARE);
	int rayX = dirX - planeX + shareX / 2, rayY = dirY - planeY + shareY / 2;
	int c, side, along, perp;

	for (c = 0; c < VIEW_CELLS; c++, rayX += shareX, rayY += shareY) {
		perp = cast(v, rayX, rayY, &side, &along);
		column(v, c, perp, side, along);
	}
}
