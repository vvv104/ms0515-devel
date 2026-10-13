/* hud.c - the screen round the view (hud.h), drawn with ms0515.h:
 * boxes by ms_fill, pictures by ms_blit, text in the ROM's font. */

#include <ms0515.h>
#include <string.h>
#include "hud.h"
#include "map.h"

#define BAND MS_INK(7) | MS_PAPER(1)		/* white on blue */
#define FRAME 0x00, MS_PAPER(1)			/* a blue box */
#define BUTTON 0x00, MS_PAPER(1) | MS_BRIGHT
#define PANEL_TOP(slot) (16 + ((slot) >> 1) * 60)
#define PANEL_LEFT(slot) (22 + ((slot) & 1) * 9)

static void button(int c, int y, const char *label)
{
	ms_fill(c, y, c + 3, y + 27, BUTTON);
	ms_text(c + 1, y + 10, label, MS_INK(6) | MS_PAPER(1) | MS_BRIGHT);
}

void hud_frame(void)
{
	ms_fill(0, 0, 39, 15, 0x00, MS_PAPER(1));			/* the band */
	ms_fill(22, 16, 39, 135, FRAME);				/* behind the heroes */
	ms_fill(0, 136, 39, 199, FRAME);				/* the bottom */
	button(1, 140, "<");					/* turn left, step, turn right */
	button(5, 140, "^");
	button(9, 140, ">");
	button(1, 170, "[");					/* step left, back, step right */
	button(5, 170, "v");
	button(9, 170, "]");
	ms_fill(14, 140, 25, 197, 0x00, MS_PAPER(0));		/* the map's ground */
	button(28, 140, "HIT");					/* the actions */
	button(33, 140, "MAG");
	button(28, 170, "WAIT");
}

static void bar(int c, int y, int value, int max, unsigned char ink)
{
	int cells = max > 0 ? (value * 8 + max - 1) / max : 0;	/* of 8 cells */

	if (cells > 8)
		cells = 8;
	ms_fill(c, y, c + 7, y + 2, 0x00, MS_PAPER(0));
	if (cells > 0)
		ms_fill(c, y, c + cells - 1, y + 2, 0xFF, ink);
}

void hud_hero(int slot, const struct hero *h)
{
	int c = PANEL_LEFT(slot), y = PANEL_TOP(slot);

	ms_text(c, y + 2, h->name, MS_INK(7) | MS_PAPER(1) | MS_BRIGHT);
	ms_blit(h->face, c, y + 12);
	bar(c, y + 46, h->hp, h->hpMax, MS_INK(2) | MS_BRIGHT);
	bar(c, y + 51, h->mana, h->manaMax, MS_INK(1) | MS_BRIGHT);
}

#define MAP_CELL 14			/* the map's ground: cells 14..25, rows 140..197 */
#define MAP_ROW 142
#define MAP_ACROSS 12			/* cells of the level shown, a byte each */
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

/* The band's first line: the place, to 30 letters, and the day.  Put
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
	ms_fill(0, 0, 39, 7, 0x00, MS_PAPER(1));
	ms_text(1, 0, line, BAND);
}

void hud_message(const char *text)
{
	ms_fill(0, 8, 39, 15, 0x00, MS_PAPER(1));
	ms_text(1, 8, text, BAND);
}
