/ rows.s - rows of a column of the view (view.c): the ceiling and the
/ floor laid down, rows made wall, a pixel turned down them.
/
/ void column_base(unsigned char *q)
/ void wall_rows(unsigned char *q, int stride, int row, int rows, int wall)
/ void column_flip(unsigned char *q, int rows, int bit)
/
/ column_base lays the 120 rows of the column at q, a word a row: the
/ ceiling's 60 rows black, then the floor's green with the pixels of
/ the floor's lines from view_lines[row].  q at 2(sp); r0, r1 used.
/
/ The `rows` rows from the word at q, `stride` bytes apart, the first
/ being window row `row`: pixels `wall` of each are wall, besides what
/ the row had.  As view.c's wallRow has it: a row that was wall keeps
/ its wall pixels and its mortar's, a mixed row its wall pixels; the
/ pixels not wall are the ink - black above the horizon, where the
/ attribute is the wall's, and green below it under the attribute for
/ a mixed row, unless the row is wall throughout - and the wall's
/ pixels are the paper but for the mortar's, from view_mortar[row],
/ where the row is wall throughout.  In C the compiler made fifty
/ instructions of a row; here it is fifteen.
/
/ Arguments above the return address, after the registers saved: q at
/ 012(sp), the stride at 014(sp), the row at 016(sp), the rows at
/ 020(sp), the wall at 022(sp).  r0, r1 free, the rest kept.
/
/ column_flip turns the pixels `bit` in the pixel bytes of `rows` rows
/ from q, two bytes apart: a joint drawn down a column, and taken out
/ again by the same call, since nothing else sets those pixels.  The
/ word is turned, the bit in its low byte, four rows to a turn of the
/ loop and the rest one by one; q at 2(sp), the rows at 4(sp), the
/ bit at 6(sp); r0, r1 used, r2 kept.

	.globl	_column_base, _wall_rows, _column_flip
	.globl	_view_mortar, _view_lines

ROWS	= 120
HALF	= 60
WALL	= 020			/ MS_PAPER(2) | MS_INK(0)
MIXED	= 024			/ MS_PAPER(2) | MS_INK(4)
FLOOR	= 040			/ MS_PAPER(4) | MS_INK(0)

	.text

_column_base:
	mov	2(sp), r0		/ q
	mov	$15, r1			/ the ceiling: black, four rows a turn (HALF / 4; a slash would begin a comment)
1:	clr	(r0)+
	clr	(r0)+
	clr	(r0)+
	clr	(r0)+
	sob	r1, 1b
	mov	$_view_lines+HALF, r1	/ the floor: its lines' pixels, green
	mov	$ROWS-HALF, -(sp)
2:	movb	(r1)+, (r0)+
	movb	$FLOOR, (r0)+
	dec	(sp)
	bne	2b
	tst	(sp)+
	rts	pc

_wall_rows:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	020(sp), r3		/ the rows
	ble	9f
	mov	012(sp), r1		/ q
	mov	014(sp), r2		/ the stride
	mov	016(sp), r4
	add	$_view_mortar, r4	/ r4 = &view_mortar[row]
1:	movb	022(sp), r0		/ wall = the pixels given
	cmpb	1(r1), $WALL
	bne	2f
	movb	(r1), r5		/ a wall row: the wall's pixels are the paper's
	comb	r5
	bisb	r5, r0
	bisb	(r4), r0		/ and the mortar's
	br	4f
2:	cmpb	1(r1), $MIXED
	bne	4f
	movb	(r1), r5		/ a mixed row: its wall pixels the paper's, no mortar
	comb	r5
	bisb	r5, r0
4:	cmp	r4, $_view_mortar+HALF
	blt	5f			/ above the horizon: the wall's attribute whatever the pixels
	cmpb	r0, $-1
	beq	5f			/ wall throughout
	comb	r0			/ mixed: the rest green, the ink
	movb	r0, (r1)
	movb	$MIXED, 1(r1)
	br	6f
5:	movb	(r4), r5		/ the mortar's pixels that are wall
	mov	r0, -(sp)
	comb	(sp)
	bicb	(sp)+, r5
	comb	r0			/ the rest the ink, black
	bisb	r5, r0
	movb	r0, (r1)
	movb	$WALL, 1(r1)
6:	add	r2, r1
	inc	r4
	sob	r3, 1b
9:	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

_column_flip:
	mov	r2, -(sp)
	mov	6(sp), r1		/ the rows
	ble	9f
	mov	4(sp), r0		/ q
	clr	r2
	bisb	010(sp), r2		/ r2 = the bit, in the low byte
	br	3f
1:	xor	r2, (r0)		/ four rows a turn
	xor	r2, 2(r0)
	xor	r2, 4(r0)
	xor	r2, 6(r0)
	add	$8, r0
3:	sub	$4, r1
	bge	1b
	add	$4, r1			/ the rows left over, 0..3
	beq	9f
2:	xor	r2, (r0)
	add	$2, r0
	sob	r1, 2b
9:	mov	(sp)+, r2
	rts	pc
