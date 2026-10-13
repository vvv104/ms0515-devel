/* hud.c - the screen round the view (hud.h), drawn with ms0515.h:
 * boxes by ms_fill, pictures by ms_blit, text in the ROM's font. */

#include <ms0515.h>
#include <string.h>
#include "hud.h"
#include "map.h"

#define BAND MS_INK(7) | MS_PAPER(1)		/* white on blue */
#define FRAME 0x00, MS_PAPER(1)			/* the blue between the boxes */
#define LINE MS_INK(7) | MS_BRIGHT		/* a box's line, bright white */
#define LABEL MS_INK(6) | MS_PAPER(1) | MS_BRIGHT	/* yellow on bright blue */
#define PANEL_TOP(slot) (16 + ((slot) >> 1) * 60)
#define PANEL_LEFT(slot) (23 + ((slot) & 1) * 9)
#define PANEL_CELLS 8

/* A box of cells c..c + w - 1, rows y..y + h - 1, a line of a pixel
 * round it on the ground of `paper`: across its first and last rows,
 * down the outer pixel of its first and last cells. */
static void box(int c, int y, int w, int h, unsigned char paper)
{
	ms_fill(c, y, c + w - 1, y + h - 1, 0x00, paper);
	ms_fill(c, y, c + w - 1, y, 0xFF, LINE | paper);
	ms_fill(c, y + h - 1, c + w - 1, y + h - 1, 0xFF, LINE | paper);
	ms_fill(c, y + 1, c, y + h - 2, 0x80, LINE | paper);
	ms_fill(c + w - 1, y + 1, c + w - 1, y + h - 2, 0x01, LINE | paper);
}

/* A button w cells wide, its label in the middle: w less the label's
 * length is even. */
static void button(int c, int y, int w, const char *label)
{
	int n = 0;

	while (label[n])
		n++;
	box(c, y, w, 28, MS_PAPER(1) | MS_BRIGHT);
	ms_text(c + (w - n) / 2, y + 10, label, LABEL);
}

void hud_frame(void)
{
	ms_fill(0, 0, 39, 15, 0x00, MS_PAPER(1));			/* the band */
	ms_fill(22, 16, 39, 135, FRAME);				/* behind the heroes */
	ms_fill(0, 136, 39, 199, FRAME);				/* the bottom */
	button(1, 140, 3, "<");					/* turn left, step, turn right */
	button(5, 140, 3, "^");
	button(9, 140, 3, ">");
	button(1, 170, 3, "[");					/* step left, back, step right */
	button(5, 170, 3, "v");
	button(9, 170, 3, "]");
	ms_fill(14, 140, 24, 197, 0x00, MS_PAPER(0));		/* the map's ground */
	button(27, 140, 5, "HIT");					/* the actions */
	button(34, 140, 5, "MAG");
	button(27, 170, 12, "WAIT");
}

/* A bar standing in cell c from row y, 32 rows tall, filled from the
 * bottom as value is of max, bright `ink` so far and dim above; none
 * when max is 0. */
static void bar(int c, int y, int value, int max, unsigned char ink)
{
	int rows = max > 0 ? (value * 32 + max - 1) / max : 0;

	if (max <= 0)
		return;
	if (rows > 32)
		rows = 32;
	if (rows < 32)
		ms_fill(c, y, c, y + 31 - rows, 0x3C, ink);
	if (rows > 0)
		ms_fill(c, y + 32 - rows, c, y + 31, 0x3C, ink | MS_BRIGHT);
}

/* A hero's panel: a box of eight cells, the name in the middle at the
 * top, the face under it, the hit points left of the face and the mana
 * right. */
void hud_hero(int slot, const struct hero *h)
{
	int c = PANEL_LEFT(slot), y = PANEL_TOP(slot), n = 0;

	while (h->name[n])
		n++;
	box(c, y + 1, PANEL_CELLS, 58, MS_PAPER(0));
	ms_text(c + (PANEL_CELLS - n) / 2, y + 7, h->name, MS_INK(7) | MS_PAPER(0) | MS_BRIGHT);
	ms_blit(h->face, c + 2, y + 19);
	bar(c + 1, y + 19, h->hp, h->hpMax, MS_INK(2));
	bar(c + 6, y + 19, h->mana, h->manaMax, MS_INK(1));
}

#define MAP_CELL 14			/* the map's ground: cells 14..24, rows 140..197 */
#define MAP_ROW 142
#define MAP_ACROSS 11			/* cells of the level shown, a byte each, the party's the middle */
#define MAP_DOWN 9
#define MAP_TALL 6			/* rows a cell: as tall as a byte is wide */

/* The arrow of the party's way, a byte a row: east, south, west, north
 * - one arrow turned, its head three steps of two pixels, its shaft two
 * wide. */
static const unsigned char arrows[4][MAP_TALL] = {
	{0x30, 0x3C, 0xFF, 0xFF, 0x3C, 0x30},
	{0x18, 0x18, 0x18, 0x7E, 0x3C, 0x18},
	{0x0C, 0x3C, 0xFF, 0xFF, 0x3C, 0x0C},
	{0x18, 0x3C, 0x7E, 0x18, 0x18, 0x18},
};

/* The map is drawn whole at every step, straight off the level: a
 * cell is a byte, so each has its own ink - white the walls, yellow the
 * party's arrow alone. */
void hud_map(int cellX, int cellY, int angle)
{
	const unsigned char *arrow = arrows[((angle + 32) >> 6) & 3];
	const char *cell;
	unsigned char *p;
	int r, q, i, mx, my;

	for (r = 0; r < MAP_DOWN; r++) {
		my = cellY - MAP_DOWN / 2 + r;
		for (i = 0; i < MAP_TALL; i++) {
			p = ms_row(MAP_ROW + r * MAP_TALL + i) + (MAP_CELL << 1);
			mx = cellX - MAP_ACROSS / 2;
			cell = my >= 0 && my < MAP_SIDE ? map_rows[my] + mx : 0;
			for (q = 0; q < MAP_ACROSS; q++, p += 2, mx++) {
				p[0] = cell && mx >= 0 && mx < MAP_SIDE && MAP_WALL(cell[q]) ? 0xFF : 0x00;
				p[1] = MS_INK(7);
			}
			if (r == MAP_DOWN / 2) {	/* the party's cell: the arrow, a row of it */
				p -= 2 * (MAP_ACROSS - MAP_ACROSS / 2);
				p[0] = arrow[i];
				p[1] = MS_INK(6) | MS_BRIGHT;
			}
		}
	}
}

/* The band's line: the place, to 30 letters, and the day.  Put
 * together by hand: strlen and memcpy would bring the whole of the
 * runtime's string.c into a program short of room. */
void hud_title(const char *place, int day)
{
	static const char dayWord[] = "  DAY ";
	char line[41];
	int n = 0, i;

	while (place[n] && n < 30) {
		line[n] = place[n];
		n++;
	}
	for (i = 0; dayWord[i]; i++)
		line[n++] = dayWord[i];
	line[n++] = (char)('0' + day / 10);
	line[n++] = (char)('0' + day % 10);
	line[n] = 0;
	ms_fill(0, 0, 39, 15, 0x00, MS_PAPER(1));
	ms_text(1, 4, line, BAND);
}
