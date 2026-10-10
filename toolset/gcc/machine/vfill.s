/ vfill.s - a column of cells filled word by word down the rows.
/
/ void ms_vfill(int c, int y0, int y1, unsigned char pixels, unsigned char attribute)
/
/ The one loop a view cast by rays runs most: three times a column, a
/ word a row.  In C it came to four instructions a row, memory-bound
/ as it is; here a row is a store through an index into the row below,
/ eight rows to a turn of the loop, and the rows left over one by one.
/ Rows are 80 bytes apart (MS_STRIDE).  Arguments on the stack above the
/ return address: c at 2(sp), y0 at 4(sp), y1 at 6(sp), the pixels at
/ 010(sp), the attribute at 012(sp).  r0, r1 free, r2, r3 kept.

	.globl	_ms_vfill

STRIDE	= 80

	.text
_ms_vfill:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	010(sp), r2		/ y0
	mov	012(sp), r3		/ y1
	sub	r2, r3			/ r3 = the rows less one
	bmi	9f
	inc	r3			/ r3 = the rows
	mov	r2, r0			/ r0 = y0 * 80 = y0 * 64 + y0 * 16
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
	clr	r1			/ the attribute over the pixels (bisb: no sign)
	bisb	016(sp), r1
	swab	r1
	bisb	014(sp), r1
	mov	r3, r2
	bic	$0177770, r2		/ r2 = the rows left over, 0..7 (octal: a leading 0)
	asr	r3
	asr	r3
	asr	r3			/ r3 = the turns of eight
	beq	2f
1:	mov	r1, (r0)
	mov	r1, STRIDE(r0)
	mov	r1, 2*STRIDE(r0)
	mov	r1, 3*STRIDE(r0)
	mov	r1, 4*STRIDE(r0)
	mov	r1, 5*STRIDE(r0)
	mov	r1, 6*STRIDE(r0)
	mov	r1, 7*STRIDE(r0)
	add	$8*STRIDE, r0
	sob	r3, 1b
2:	tst	r2
	beq	9f
3:	mov	r1, (r0)
	add	$STRIDE, r0
	sob	r2, 3b
9:	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc
