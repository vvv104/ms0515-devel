/* TEX - textures and sprites drawn scaled (tex.h) put to the test: a
 * texture's columns stretched, squeezed and clipped at the top and the
 * bottom of the screen; a sprite scaled up over a box, its empty texels
 * leaving the box be, and one cut by the screen's right edge.  The
 * harness (../tests) draws the same by the same rule on the host and
 * compares the cells.  Then Q. */

#include <ms0515.h>
#include <tex.h>

/* 4 columns of 8 texels: column k's texel r is 8k + r + 1 as a pattern,
 * k + 1 as an attribute - all of them lit.  A texel is two bytes, the
 * pattern then the attribute. */
static unsigned char wallTexels[64];
static const struct tex wall = {4, 8, wallTexels};

/* 2 columns of 4: the first texel of each column empty. */
static const unsigned char spriteTexels[16] = {
	0, 9, 0x11, 1, 0x22, 2, 0x33, 3,
	0, 9, 0x44, 4, 0x55, 5, 0x66, 6,
};
static const struct tex sprite = {2, 4, spriteTexels};

int main(void)
{
	int i;

	for (i = 0; i < 32; i++) {
		wallTexels[2 * i] = (unsigned char)(i + 1);
		wallTexels[2 * i + 1] = (unsigned char)(i / 8 + 1);
	}
	ms_screen_begin(0);
	tex_column(&wall, 1, 3, 20, 39);	/* 8 texels over 20 rows */
	tex_column(&wall, 2, 4, 60, 64);	/* over 5 */
	tex_column(&wall, 3, 5, -10, 29);	/* 40 rows, the first ten above the screen */
	tex_column(&wall, 0, 6, 180, 219);	/* 40 rows, the last twenty below it */
	tex_column(&wall, 0, 7, 100, 399);	/* 300 rows: past the table */
	tex_column_in(&wall, 2, 8, 100, 139, 110, 129);	/* 40 rows, the window rows 110..129 */
	ms_fill(10, 100, 15, 111, 0x11, MS_INK(7));
	tex_sprite(&sprite, 10, 100, 6, 12);	/* three times its size, over the box */
	tex_sprite(&sprite, 38, 150, 4, 8);	/* two of its four cells past the edge */
	ms_keys_begin();
	while (ms_key() != MS_KEY_Q)
		;
	ms_keys_end();
	ms_screen_end();
	return 0;
}
