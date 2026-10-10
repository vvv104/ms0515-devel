/* hud.c - the screen round the view (hud.h), drawn with ms0515.h:
 * boxes by ms_fill, pictures by ms_blit, text in the ROM's font. */

#include <ms0515.h>
#include <string.h>
#include "hud.h"

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

void hud_compass(int angle)
{
	static const char letters[4] = {'E', 'S', 'W', 'N'};	/* 0 along +x, then clockwise */
	char text[2];

	text[0] = letters[((angle + 32) >> 6) & 3];
	text[1] = 0;
	ms_text(19, 186, text, MS_INK(6) | MS_BRIGHT);
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
