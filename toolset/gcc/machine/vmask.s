/ vmask.s - columns painted under a mask of pixels: where two surfaces
/ share a cell.
/
/ void ms_vput_masked(int c, int y0, int rows, const void *words, unsigned char mask)
/ void ms_vput_mirror_masked(int c, int y0, int rows, const void *words, unsigned char mask)
/ void ms_vfill_masked(int c, int y0, int y1, unsigned char pixels, unsigned char mask)
/
/ A row: the pixels under the mask cleared in the screen's byte and
/ the new ones, masked, OR-ed in - two byte instructions on the screen
/ word itself - then the attribute, taken whole by the puts and left by
/ the fill.  The mirror turns the new pixels round through ms_reverse
/ (draw.c) first.  Rows are 80 bytes apart.  Arguments above the return
/ address, after the registers saved: for the puts c at 012(sp), y0 at
/ 014(sp), the rows at 016(sp), the words at 020(sp), the mask at
/ 022(sp); for the fill c at 010(sp), y0 at 012(sp), y1 at 014(sp), the
/ pixels at 016(sp), the mask at 020(sp).  A byte argument lies in the
/ low byte of its word.  r0, r1 free, the rest kept.

	.globl	_ms_vput_masked, _ms_vput_mirror_masked, _ms_vfill_masked
	.globl	_ms_reverse

STRIDE	= 80

	.text

/ r0 = the screen word of row y0, column c: y0 * 80 + c * 2 + the window;
/ y0 in r0, c in r1 on entry.
address:
	asl	r0
	asl	r0
	asl	r0
	asl	r0
	mov	r0, -(sp)
	asl	r0
	asl	r0
	add	(sp)+, r0
	add	r1, r0
	add	r1, r0
	add	$0100000, r0
	rts	pc

_ms_vput_masked:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	016(sp), r4		/ the rows
	ble	9f
	mov	014(sp), r0		/ y0
	mov	012(sp), r1		/ c
	jsr	pc, address
	mov	020(sp), r1		/ the words
	clr	r2
	bisb	022(sp), r2		/ r2 = the mask
	mov	r2, r3
	com	r3			/ r3 = its complement
1:	movb	(r1)+, r5		/ the new pixels
	bicb	r3, r5			/ under the mask
	bicb	r2, (r0)		/ the screen's cleared there
	bisb	r5, (r0)		/ and given them
	movb	(r1)+, 1(r0)		/ the attribute, whole
	add	$STRIDE, r0
	sob	r4, 1b
9:	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

_ms_vput_mirror_masked:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	016(sp), r4		/ the rows
	ble	9f
	mov	014(sp), r0		/ y0
	mov	012(sp), r1		/ c
	jsr	pc, address
	mov	020(sp), r1		/ the words
	clr	r2
	bisb	022(sp), r2		/ r2 = the mask
	mov	r2, r3
	com	r3
1:	clr	r5
	bisb	(r1)+, r5		/ the new pixels, as an index
	movb	_ms_reverse(r5), r5	/ turned round
	bicb	r3, r5
	bicb	r2, (r0)
	bisb	r5, (r0)
	movb	(r1)+, 1(r0)
	add	$STRIDE, r0
	sob	r4, 1b
9:	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

_ms_vfill_masked:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	014(sp), r4		/ y1
	sub	012(sp), r4		/ - y0
	bmi	9f
	inc	r4			/ r4 = the rows
	mov	012(sp), r0		/ y0
	mov	010(sp), r1		/ c
	jsr	pc, address
	clr	r2
	bisb	020(sp), r2		/ r2 = the mask
	clr	r3
	bisb	016(sp), r3		/ the pixels
	mov	r2, r1
	com	r1
	bic	r1, r3			/ r3 = the pixels under the mask
1:	bicb	r2, (r0)		/ the pixels under the mask cleared
	bisb	r3, (r0)		/ and set from the pattern
	add	$STRIDE, r0
	sob	r4, 1b
9:	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc
