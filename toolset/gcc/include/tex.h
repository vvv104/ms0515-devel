/* tex.h - textures and sprites drawn scaled: what a 3D view is painted
 * with (machine/tex.c).
 *
 * In a view cast by the grid, a wall never turns as a picture would:
 * the ray that hit it says which column of its texture is seen, and
 * the distance how tall the column stands on the screen.  So a texture
 * is drawn a column at a time, each stretched or squeezed to its
 * height - a texel every 256/height rows in 8.8, the texel's row the
 * high byte of a sum, no division in the row - and a sprite is the
 * same in both directions, with its empty texels left unwritten.
 *
 * A texture lies column by column, so that a column is a run of texels,
 * and a texel is a screen word: the pattern of eight pixels, then the
 * ink and paper over it - two bytes, texels[cols][rows][2], so that a
 * row is one word moved.  Rows up to 255. */

#ifndef TEX_H
#define TEX_H

struct tex {
	unsigned char cols, rows;
	const unsigned char *texels;		/* cols * rows words, column-major */
};

/* Column `col` of the texture on screen column c, stretched over rows
 * y0..y1 (both in): y0 may lie above the screen and y1 below it - the
 * part off the screen is skipped, the stretch kept - as a wall nearer
 * than the screen is tall does. */
void tex_column(const struct tex *t, int col, int c, int y0, int y1);

/* The same cut to rows top..bottom instead of the screen's: a wall in a
 * window that is not the whole screen. */
void tex_column_in(const struct tex *t, int col, int c, int y0, int y1, int top, int bottom);

/* The same into the pixels under `mask` only (bit 7 the leftmost), the
 * attribute taken whole: a wall's edge that falls inside a cell. */
void tex_column_masked(const struct tex *t, int col, int c, int y0, int y1, int top, int bottom, unsigned char mask);

/* The same with the empty texels - a pattern of 0 - left unwritten: a
 * sprite's column, for a program that draws its sprites a column at a
 * time against the depth of the wall there. */
void tex_sprite_column(const struct tex *t, int col, int c, int y0, int y1);

/* The texture as a sprite: `cells` wide and `height` rows high on the
 * screen, its top left at cell column c and row y, its empty texels
 * left unwritten; what lies off the screen is skipped. */
void tex_sprite(const struct tex *t, int c, int y, int cells, int height);

/* The texel step in 8.8 for `rows` texels over `height` rows: a table
 * for the heights up to 255, made for the first texture's rows; a
 * division for other rows, or past 255. */
unsigned tex_step(int rows, int height);

#endif
