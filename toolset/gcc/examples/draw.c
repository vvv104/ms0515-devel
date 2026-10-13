/* DRAW - the drawing of ms0515.h put to the test: a column, a box, an
 * image and two lines of text - ASCII and Cyrillic in the ROM's font -
 * a column of words from memory, whole and under a mask, a masked
 * fill, two boxes scrolled sideways, on a screen the harness
 * (../tests) reads cell by cell, then Q. */

#include <ms0515.h>

static const unsigned char pixels[6] = {0x81, 0x18, 0xFF, 0x00, 0x3C, 0xC3};
static const unsigned char attributes[6] = {1, 2, 3, 4, 5, 6};
static const struct ms_image image = {2, 3, pixels, attributes};
static const struct ms_image bare = {2, 3, pixels, 0};
/* Four words of a column: the pattern, then the attribute - on an even
 * address, as words must lie. */
static const unsigned char column[8] __attribute__((aligned(2))) = {0x0F, 0x12, 0xF0, 0x34, 0x55, 0x56, 0xAA, 0x78};

/* Twelve words: row r's pattern r + 1, attribute 0x10 + r. */
static unsigned char twelve[24] __attribute__((aligned(2)));
static unsigned char *const high = (unsigned char *)0060000;	/* RT-11's free memory, bank 3 */

int main(void)
{
	int i;

	for (i = 0; i < 12; i++) {
		twelve[2 * i] = (unsigned char)(i + 1);
		twelve[2 * i + 1] = (unsigned char)(0x10 + i);
	}
	ms_screen_begin(MS_INK(7));
	ms_vfill(5, 10, 20, 0xFF, MS_INK(2));
	ms_fill(10, 30, 12, 32, 0xAA, MS_PAPER(1) | MS_BRIGHT);
	ms_blit(&image, 20, 50);
	ms_blit(&bare, 24, 50);				/* the attributes left as they are */
	ms_text(0, 100, "Hi!", MS_INK(6));
	ms_text(10, 100, "\304\301 \001", MS_INK(5));	/* KOI-8 "Da", then a code with no glyph */
	ms_vput(30, 10, 4, column);			/* rows 10..13 */
	ms_fill(31, 10, 31, 13, 0xFF, MS_INK(7));
	ms_vput_masked(31, 10, 4, column, 0x0F);	/* the right half from the column */
	ms_vfill_masked(32, 10, 13, 0x33, 0xF0);	/* the left half set, 0x30 on the clear screen */
	ms_vput(35, 10, 12, twelve);			/* a run of eight and four over */
	for (i = 0; i < 24; i++)			/* the same from the free memory above the program */
		high[i] = twelve[i];
	ms_vput(36, 10, 12, high);
	ms_vput_mirror(33, 10, 4, column);		/* the pixels turned round */
	ms_fill(34, 10, 34, 13, 0xFF, MS_INK(7));
	ms_vput_mirror_masked(34, 10, 4, column, 0xF0);	/* the left half of the mirror */
	ms_vput_folded(37, 30, 6, twelve);		/* rows 30..35, and 29..24 turned over */
	for (i = 0; i < 6; i++) {			/* two boxes of six cells, a cell a number */
		ms_fill(20 + i, 70, 20 + i, 72, (unsigned char)(i + 1), (unsigned char)(i + 1));
		ms_fill(20 + i, 74, 20 + i, 76, (unsigned char)(i + 1), (unsigned char)(i + 1));
	}
	ms_hscroll(20, 25, 70, 72, 2);			/* 1 2 1 2 3 4 */
	ms_hscroll(20, 25, 74, 76, -3);			/* 4 5 6 4 5 6 */
	ms_put(30, 80, 3, 4, twelve);			/* the twelve words as a box of 3 by 4 */
	ms_keys_begin();
	while (ms_key() != MS_KEY_Q)
		;
	ms_keys_end();
	ms_screen_end();
	return 0;
}
