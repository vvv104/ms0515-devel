/ vput.s - a column of words from memory down the rows.
/
/ void ms_vput(int c, int y0, int rows, const void *words)
/ void ms_vput_mirror(int c, int y0, int rows, const void *words)
/ void ms_vput_folded(int c, int y, int rows, const void *words)
/
/ The loop a view of drawn-beforehand columns runs most: a word moved
/ from the column in memory to the row below the last, eight rows to a
/ turn, the rows left over one by one.  The mirror: the pixel byte
/ through the table ms_reverse (draw.c) on the way, the attribute as
/ it is - four instructions a row.  Rows are 80 bytes apart
/ (MS_STRIDE).  The folded: each word put twice, down from row y and
/ up from the row above it, four rows of each to a turn - two
/ instructions a row put.  Arguments above the return address: c at
/ 2(sp), y0 at 4(sp), the rows at 6(sp), the words at 010(sp).  r0, r1
/ free, r2, r3 kept.

	.globl	_ms_vput, _ms_vput_mirror, _ms_vput_folded
	.globl	_ms_reverse

STRIDE	= 80

	.text
_ms_vput_mirror:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	012(sp), r3		/ the rows
	ble	9f
	mov	010(sp), r0		/ r0 = y0 * 80 + c * 2 + the window
	asl	r0
	asl	r0
	asl	r0
	asl	r0
	mov	r0, r1
	asl	r0
	asl	r0
	add	r1, r0
	add	6(sp), r0
	add	6(sp), r0
	add	$0100000, r0
	mov	014(sp), r1		/ the words
	clr	r2
5:	bisb	(r1)+, r2		/ the pixels, as an index
	movb	_ms_reverse(r2), (r0)
	clr	r2
	movb	(r1)+, 1(r0)		/ the attribute
	add	$STRIDE, r0
	sob	r3, 5b
	br	9f

_ms_vput:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	012(sp), r3		/ the rows
	ble	9f
	mov	010(sp), r0		/ r0 = y0 * 80 = y0 * 64 + y0 * 16
	asl	r0
	asl	r0
	asl	r0
	asl	r0
	mov	r0, r1
	asl	r0
	asl	r0
	add	r1, r0
	add	6(sp), r0		/ + c * 2
	add	6(sp), r0
	add	$0100000, r0		/ the window
	mov	014(sp), r1		/ the words
	mov	r3, r2
	bic	$0177770, r2		/ r2 = the rows left over, 0..7
	asr	r3
	asr	r3
	asr	r3			/ r3 = the turns of eight
	beq	2f
1:	mov	(r1)+, (r0)
	mov	(r1)+, STRIDE(r0)
	mov	(r1)+, 2*STRIDE(r0)
	mov	(r1)+, 3*STRIDE(r0)
	mov	(r1)+, 4*STRIDE(r0)
	mov	(r1)+, 5*STRIDE(r0)
	mov	(r1)+, 6*STRIDE(r0)
	mov	(r1)+, 7*STRIDE(r0)
	add	$8*STRIDE, r0
	sob	r3, 1b
2:	tst	r2
	beq	9f
3:	mov	(r1)+, (r0)
	add	$STRIDE, r0
	sob	r2, 3b
9:	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

_ms_vput_folded:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	012(sp), r3		/ the rows
	ble	9f
	mov	010(sp), r0		/ r0 = y * 80 + c * 2 + the window
	asl	r0
	asl	r0
	asl	r0
	asl	r0
	mov	r0, r1
	asl	r0
	asl	r0
	add	r1, r0
	add	6(sp), r0
	add	6(sp), r0
	add	$0100000, r0
	mov	r0, r2
	sub	$STRIDE, r2		/ r2 = the row above, going up
	mov	014(sp), r1		/ the words
	mov	r3, -(sp)		/ kept for the rows left over
	asr	r3
	asr	r3			/ r3 = the turns of four
	beq	2f
1:	mov	(r1), (r0)
	mov	(r1)+, (r2)
	mov	(r1), STRIDE(r0)
	mov	(r1)+, -STRIDE(r2)
	mov	(r1), 2*STRIDE(r0)
	mov	(r1)+, -2*STRIDE(r2)
	mov	(r1), 3*STRIDE(r0)
	mov	(r1)+, -3*STRIDE(r2)
	add	$4*STRIDE, r0
	sub	$4*STRIDE, r2
	sob	r3, 1b
2:	mov	(sp)+, r3
	bic	$0177774, r3		/ the rows left over, 0..3
	beq	9f
3:	mov	(r1), (r0)
	mov	(r1)+, (r2)
	add	$STRIDE, r0
	sub	$STRIDE, r2
	sob	r3, 3b
9:	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc
