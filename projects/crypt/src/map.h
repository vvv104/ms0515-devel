/* map.h - the dungeon's cells.
 *
 * A level is a grid of 32 by 32 cells, a byte each: 0 is floor, any
 * other the kind of wall there.  Positions are in 8.8 fixed point, 256
 * to a cell, so a level is 8192 units across; a direction is an angle
 * in 256ths of a turn, 0 along +x (east), 64 along +y (south: y grows
 * down the map as it does down the screen). */

#ifndef CRYPT_MAP_H
#define CRYPT_MAP_H

#define MAP_SIDE 32
#define CELL 256			/* 8.8: one cell */

extern unsigned char map[MAP_SIDE * MAP_SIDE];
extern int map_startX, map_startY;	/* the cell the party begins on */

/* The cell at (x, y) in cells; a wall past the edge. */
int map_at(int x, int y);
/* Whether a point in 8.8 is on floor. */
int map_free(int x, int y);
/* The test level: a ring of walls, a few pillars and a corridor. */
void map_test(void);

#endif
