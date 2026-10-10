/* tex.c - textures and sprites drawn scaled (include/tex.h).
 *
 * A column of texels stretched over a run of screen rows: the texel's
 * row is the high byte of a sum that grows by the step - rows * 256 /
 * height in 8.8 - once a screen row.  The sum fits sixteen bits since
 * a texture has at most 255 rows.  The steps for the heights up to 255
 * come from a table made for the first texture's rows (a wall's
 * columns all share their texture's rows); a texture of other rows,
 * or a taller column, nearer than the screen is high, costs a
 * division - the table is 256 of them, too dear to remake as the
 * textures alternate.  Where the run begins above its window, the sum
 * starts at the step times the rows skipped.  The rows themselves are
 * texrun.s: a word a row, nine instructions. */

#include <ms0515.h>
#include <tex.h>

void tex_run(unsigned char *p, const unsigned char *column, unsigned acc, unsigned step, int rows);
void tex_run_masked(unsigned char *p, const unsigned char *column, unsigned acc, unsigned step, int rows);

static int stepRows;			/* the rows the table is for; 0: none */
static unsigned steps[256];

unsigned tex_step(int rows, int height)
{
	int h;

	if (height <= 0)
		return 0;
	if (stepRows == 0) {
		stepRows = rows;
		for (h = 1; h < 256; h++)
			steps[h] = (unsigned)(((unsigned long)rows << 8) / (unsigned)h);
	}
	if (height > 255 || rows != stepRows)
		return (unsigned)(((unsigned long)rows << 8) / (unsigned)height);
	return steps[height];
}

/* The run clipped to rows top..bottom: y0 and y1 moved in, the sum
 * started where the first row kept is; 0 when nothing is kept. */
static int clip(int *y0, int *y1, int top, int bottom, unsigned step, unsigned *acc)
{
	if (*y1 > bottom)
		*y1 = bottom;
	if (*y0 < top) {
		*acc = (unsigned)((unsigned long)step * (unsigned)(top - *y0));
		*y0 = top;
	} else {
		*acc = 0;
	}
	return *y0 <= *y1;
}

/* The column's first texel: cols * rows words in, rows a column. */
static const unsigned char *texels(const struct tex *t, int col)
{
	return t->texels + ((col * t->rows) << 1);
}

void tex_column_in(const struct tex *t, int col, int c, int y0, int y1, int top, int bottom)
{
	unsigned step = tex_step(t->rows, y1 - y0 + 1);
	unsigned acc;

	if (clip(&y0, &y1, top, bottom, step, &acc))
		tex_run(ms_row(y0) + (c << 1), texels(t, col), acc, step, y1 - y0 + 1);
}

void tex_column(const struct tex *t, int col, int c, int y0, int y1)
{
	tex_column_in(t, col, c, y0, y1, 0, MS_ROWS - 1);
}

/* A row at a time in C: a wall's edge takes a column or two a pass. */
void tex_column_masked(const struct tex *t, int col, int c, int y0, int y1, int top, int bottom, unsigned char mask)
{
	unsigned step = tex_step(t->rows, y1 - y0 + 1);
	const unsigned char *column, *texel;
	unsigned char *p;
	unsigned acc;
	int n;

	if (!clip(&y0, &y1, top, bottom, step, &acc))
		return;
	column = texels(t, col);
	p = ms_row(y0) + (c << 1);
	for (n = y1 - y0 + 1; n > 0; n--, p += MS_STRIDE, acc += step) {
		texel = column + ((acc >> 8) << 1);
		p[0] = (unsigned char)((p[0] & ~mask) | (texel[0] & mask));
		p[1] = texel[1];
	}
}

void tex_sprite_column(const struct tex *t, int col, int c, int y0, int y1)
{
	unsigned step = tex_step(t->rows, y1 - y0 + 1);
	unsigned acc;

	if (clip(&y0, &y1, 0, MS_ROWS - 1, step, &acc))
		tex_run_masked(ms_row(y0) + (c << 1), texels(t, col), acc, step, y1 - y0 + 1);
}

void tex_sprite(const struct tex *t, int c, int y, int cells, int height)
{
	unsigned step, acc = 0;
	int k;

	if (cells <= 0 || height <= 0)
		return;
	step = ((unsigned)t->cols << 8) / (unsigned)cells;	/* once a sprite: no table */
	for (k = 0; k < cells; k++, c++, acc += step)
		if (c >= 0 && c < MS_STRIDE / 2)
			tex_sprite_column(t, (int)(acc >> 8), c, y, y + height - 1);
}
