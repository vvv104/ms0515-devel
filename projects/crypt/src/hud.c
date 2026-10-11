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
#define MAP_ROW 140
#define MAP_ACROSS 24			/* cells of the level shown, 4 pixels each */
#define MAP_DOWN 14

/* The arrow of the party's way, 4 by 4 pixels, a nibble a row: east,
 * south, west, north. */
static const unsigned char arrows[4][4] = {
	{0x8, 0xC, 0xE, 0xC}, {0x6, 0x6, 0xF, 0x6}, {0x1, 0x3, 0x7, 0x3}, {0x6, 0xF, 0x6, 0x6},
};

/* A row of the map's cells as bytes, two cells a byte, a nibble each:
 * the level's row my from column mx on, nothing past the level's edge.
 * Straight off the level, since the map is drawn whole at every step. */
static void mapRow(int mx, int my, unsigned char *bytes)
{
	const char *cell = my >= 0 && my < MAP_SIDE ? map_rows[my] + mx : 0;
	int q;

	for (q = 0; q < MAP_ACROSS / 2; q++, cell += 2, mx += 2) {
		unsigned char b = 0;

		if (cell) {
			if (mx >= 0 && mx < MAP_SIDE && MAP_WALL(cell[0]))
				b = 0xF0;
			if (mx + 1 >= 0 && mx + 1 < MAP_SIDE && MAP_WALL(cell[1]))
				b |= 0x0F;
		}
		bytes[q] = b;
	}
}

void hud_map(int cellX, int cellY, int angle)
{
	const unsigned char *arrow = arrows[((angle + 32) >> 6) & 3];
	unsigned char bytes[MAP_ACROSS / 2];
	unsigned char *p;
	int r, q, i;

	for (r = 0; r < MAP_DOWN; r++) {
		mapRow(cellX - MAP_ACROSS / 2, cellY - MAP_DOWN / 2 + r, bytes);
		for (i = 0; i < 4; i++) {
			if (r == MAP_DOWN / 2)		/* the party's cell: the arrow, a row of it */
				bytes[MAP_ACROSS / 4] = (unsigned char)((bytes[MAP_ACROSS / 4] & 0x0F) | (arrow[i] << 4));
			p = ms_row(MAP_ROW + r * 4 + i) + (MAP_CELL << 1);
			for (q = 0; q < MAP_ACROSS / 2; q++, p += 2) {
				p[0] = bytes[q];
				p[1] = (unsigned char)(r == MAP_DOWN / 2 && q == MAP_ACROSS / 4 ? MS_INK(6) | MS_BRIGHT : MS_INK(7));
			}
		}
	}
}

void hud_title(const char *place, int day)
{
	char line[41];
	int n = (int)strlen(place);

	if (n > 30)
		n = 30;
	memcpy(line, place, (unsigned)n);
	memcpy(line + n, "  DAY ", 6);
	line[n + 6] = (char)('0' + day / 10);
	line[n + 7] = (char)('0' + day % 10);
	line[n + 8] = 0;
	ms_fill(0, 0, 39, 7, 0x00, MS_PAPER(1));
	ms_text(1, 0, line, BAND);
}

void hud_message(const char *text)
{
	ms_fill(0, 8, 39, 15, 0x00, MS_PAPER(1));
	ms_text(1, 8, text, BAND);
}
