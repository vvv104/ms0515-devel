/* hud.h - the screen round the view: the title band, the party, the
 * buttons, the map and the compass.
 *
 * In cells (8 pixels) across and rows down, as the design has it:
 *
 *    0        10        20        30       39
 *    +--------------------------------------+ 0
 *    | the place, the day, the last message | 0-15    two lines of text
 *    +----------------------+---------------+ 16
 *    |                      | hero 1 | hero 2|
 *    |   the view, 22 cells | face   | face  | 16-75   faces of 32x32,
 *    |   by 120 rows        +--------+-------+ 76      names, bars
 *    |                      | hero 3 | hero 4|
 *    |                      |        |       | 76-135
 *    +------+---------------+--------+-------+ 136
 *    | turn | the map and   | attack | spell |
 *    | step | the compass   | wait   |       | 136-199
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
/* The compass's letter for the party's direction (256ths of a turn). */
void hud_compass(int angle);
/* The place's name and the day on the band's first line, a message on
 * its second. */
void hud_title(const char *place, int day);
void hud_message(const char *text);

#endif
