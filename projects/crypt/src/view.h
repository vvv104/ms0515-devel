/* view.h - the view from the party's eyes: the dungeon cast by rays.
 *
 * The window is 22 cells (176 pixels) by 120 rows at the screen's top
 * left under the title band, as Eye of the Beholder had it.  One ray a
 * cell column: it walks the grid from the party's position in the
 * direction the column looks, and the wall it meets stands as tall on
 * the screen as its distance allows, from a table.  The party's
 * position and direction are fractional (map.h), so a step or a turn
 * can be shown in parts, frame by frame. */

#ifndef CRYPT_VIEW_H
#define CRYPT_VIEW_H

#define VIEW_CELLS 22
#define VIEW_ROWS 120
#define VIEW_TOP 16			/* the first screen row of the window */
#define VIEW_LEFT 0			/* its first cell column */

struct view {
	int x, y;			/* the eyes, 8.8 */
	int angle;			/* where they look, 256ths of a turn */
	int textured;			/* the walls with their texture, or flat */
};

/* The tables made (the first time). */
void view_init(void);
/* The window drawn for the view. */
void view_render(const struct view *v);
/* The wall column c of the last render stood this tall (rows), 0 for
 * none seen: for the sprites to be drawn against. */
int view_height(int c);

#endif
