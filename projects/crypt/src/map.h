/* map.h - the dungeon's cells.
 *
 * A level is a grid of 32 by 32 cells, a row a string of a character
 * each: '.' floor, 'S' the floor the party begins on, anything else
 * a wall - the text it is drawn as, read as it is, since the room is
 * short.  Positions are in 8.8 fixed point, 256 to a cell, so a level
 * is 8192 units across; a direction is an angle in 256ths of a turn,
 * 0 along +x (east), 64 along +y (south: y grows down the map as it
 * does down the screen). */

#ifndef CRYPT_MAP_H
#define CRYPT_MAP_H

#define MAP_SIDE 32
#define CELL 256			/* 8.8: one cell */

extern const char *const map_rows[MAP_SIDE];	/* the level, a row a string */
extern int map_startX, map_startY;	/* the cell the party begins on */

#define MAP_WALL(c) ((c) != '.' && (c) != 'S')	/* whether a cell's character is a wall */

/* Whether the cell at (x, y) in cells is a wall; one past the edge. */
int map_at(int x, int y);
/* Whether a point in 8.8 is on floor. */
int map_free(int x, int y);
/* The test level: a ring of walls, a few pillars and a corridor. */
void map_test(void);

#endif
