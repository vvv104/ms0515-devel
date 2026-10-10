/ hscroll.s - a box of the screen moved sideways by whole cells.
/
/ void ms_hscroll(int c0, int c1, int y0, int y1, int by)
/
/ A view that turns: the words of each row moved `by` cells to the
/ right, from the right end so that nothing is trodden on, or to the
/ left (by < 0) from the left end; the cells uncovered keep what they
/ had, what moves past the box is lost.  Two instructions a word, rows
/ 80 bytes apart.  Arguments above the return address, after the four
/ registers saved: c0 at 012(sp), c1 at 014(sp), y0 at 016(sp), y1 at
/ 020(sp), by at 022(sp).  r0, r1 free; r2..r5 kept.

	.globl	_ms_hscroll

STRIDE	= 80

	.data
by2:	.word	0			/ the move in bytes

	.text
_ms_hscroll:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	020(sp), r3		/ y1
	sub	016(sp), r3		/ - y0
	bmi	9f
	inc	r3			/ r3 = the rows
	mov	014(sp), r4		/ c1
	sub	012(sp), r4		/ - c0
	inc	r4			/ r4 = the width in cells
	mov	016(sp), r0		/ r0 = y0 * 80 = y0 * 64 + y0 * 16
	asl	r0
	asl	r0
	asl	r0
	asl	r0
	mov	r0, r1
	asl	r0
	asl	r0
	add	r1, r0
	add	$0100000, r0		/ the window: the row's first word
	mov	022(sp), r5		/ by
	beq	9f
	bmi	5f

	sub	r5, r4			/ to the right: the words moved, width - by
	ble	9f
	asl	r5
	mov	r5, by2
	add	014(sp), r0		/ r0 = the word after c1, the moves end there
	add	014(sp), r0
	add	$2, r0
1:	mov	r0, r2			/ the destination, from the right end
	mov	r0, r1
	sub	by2, r1			/ the source, by cells to the left of it
	mov	r4, r5
2:	mov	-(r1), -(r2)
	sob	r5, 2b
	add	$STRIDE, r0
	sob	r3, 1b
	br	9f

5:	add	r5, r4			/ to the left: width + by
	ble	9f
	neg	r5
	asl	r5
	mov	r5, by2
	add	012(sp), r0		/ r0 = the word of c0
	add	012(sp), r0
6:	mov	r0, r2			/ the destination, from the left end
	mov	r0, r1
	add	by2, r1			/ the source, by cells to the right of it
	mov	r4, r5
7:	mov	(r1)+, (r2)+
	sob	r5, 7b
	add	$STRIDE, r0
	sob	r3, 6b

9:	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc
