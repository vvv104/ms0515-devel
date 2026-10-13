/* hud.h - the screen round the view: the title band, the party, the
 * buttons, the map and the compass.
 *
 * In cells (8 pixels) across and rows down, as the design has it:
 *
 *    0        10        20        30       39
 *    +--------------------------------------+ 0
 *    | the place, the day                   | 0-15    a line of text
 *    +----------------------+---------------+ 16
 *    |                      | hero 1 | hero 2|  boxes of 8 cells, 23..30
 *    |   the view, 22 cells |        |       |  and 32..39, a cell of blue
 *    |   by 120 rows        +--------+-------+  between: the name, the
 *    |                      | hero 3 | hero 4|  face of 32x32, the hit
 *    |                      |        |       |  points left, the mana right
 *    +------+---------------+--------+-------+ 136
 *    | turn | the map,      | attack | spell |
 *    | step | 11 cells      |      wait      | 136-199
 *    +------+---------------+----------------+
 */

#ifndef CRYPT_HUD_H
#define CRYPT_HUD_H

struct hero {
	const char *name;
	const struct ms_image *face;
	int hp, hpMax, mana, manaMax;
};

/* The band, the frames and the buttons drawn once. */
void hud_frame(void);
/* A hero's panel: slot 0..3, the face, the name, the bars. */
void hud_hero(int slot, const struct hero *h);
/* The map round the party: the cells of the level, a byte each,
 * 12 across by 9 down, six rows tall, the party's cell in the middle as an arrow
 * pointing the way it faces (angle in 256ths of a turn, 0 east); the
 * map slides under the arrow as the party moves. */
void hud_map(int cellX, int cellY, int angle);
/* The place's name and the day in the middle of the band. */
void hud_title(const char *place, int day);

#endif
