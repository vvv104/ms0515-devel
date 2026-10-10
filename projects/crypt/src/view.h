/* view.h - the view from the party's eyes: the dungeon on a cylinder.
 *
 * The window is 22 cells (176 pixels) by 120 rows at the screen's top
 * left under the title band, as Eye of the Beholder had it.  The
 * picture is a cylinder round the party: a pixel column looks one
 * way for all its rows, a quarter of an angle unit (256 to the turn)
 * from the next, so a sector of 32 cell columns is a quarter turn and
 * the view is the middle 22 of it.  Walls stand 60 / r rows above and
 * below the horizon at distance r cells.
 *
 * The party is on a cell facing one of the four ways, or part way
 * along a step: phase k of PLANE_PHASES past its cell's centre.  The
 * walls beside it never change their angle in a step, only the
 * texture runs along them, so they come drawn from the host
 * (art/planes.py, CRYPT.DAT) for every phase, and the view copies
 * their columns; the walls ahead are drawn by texture columns.  A turn
 * is the sector sliding past: the picture moved four columns a pass
 * and the columns coming in drawn - eight passes a quarter turn. */

#ifndef CRYPT_VIEW_H
#define CRYPT_VIEW_H

#define VIEW_CELLS 22
#define VIEW_ROWS 120
#define VIEW_TOP 16			/* the first screen row of the window */
#define VIEW_LEFT 0			/* its first cell column */
#define SECTOR_CELLS 32			/* a quarter turn */
#define SECTOR_FIRST 5			/* the view's first column in the sector */
#define TURN_PASSES 8			/* a quarter turn, four columns each */

struct view {
	int cellX, cellY;		/* the party's cell */
	int facing;			/* 0 east, 1 south, 2 west, 3 north */
	int phase;			/* 0..PLANE_PHASES-1 past the cell's centre, the way it faces */
};

/* The window drawn for the view: standing, or part way along a step. */
void view_render(const struct view *v);
/* A turn from the view's facing, dir 1 to the right or -1 to the left,
 * begun; then pass 1..TURN_PASSES each slides the picture on, the last
 * leaving the new facing's view. */
void view_turn_begin(const struct view *v, int dir);
void view_turn_pass(int pass);

#endif
