/ vbox.s - a box of words from memory onto the screen.
/
/ void ms_put(int c, int y, int cells, int rows, const void *words)
/
/ A picture kept in memory as the screen keeps it - a word a cell, the
/ attribute over the pixels, row after row, `cells` words a row - put
/ at cell column c, row y: a view's layer drawn once and copied a
/ pass.  Two instructions a word and a few a row.  Arguments above the
/ return address, after the registers saved: c at 012(sp), y at
/ 014(sp), the cells at 016(sp), the rows at 020(sp), the words at
/ 022(sp).  r0, r1 free, r2..r5 kept.

	.globl	_ms_put

STRIDE	= 80

	.text
_ms_put:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	020(sp), r3		/ the rows
	ble	9f
	mov	016(sp), r4		/ the cells
	ble	9f
	mov	014(sp), r0		/ r0 = y * 80 + c * 2 + the window
	asl	r0
	asl	r0
	asl	r0
	asl	r0
	mov	r0, r1
	asl	r0
	asl	r0
	add	r1, r0
	add	012(sp), r0
	add	012(sp), r0
	add	$0100000, r0
	mov	022(sp), r1		/ the words
	mov	$STRIDE, r5
	sub	r4, r5
	sub	r4, r5			/ r5 = what is left of a row after the cells
1:	mov	r4, r2
2:	mov	(r1)+, (r0)+
	sob	r2, 2b
	add	r5, r0
	sob	r3, 1b
9:	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc
