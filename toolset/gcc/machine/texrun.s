/ texrun.s - the loop of a texture column: texels down the rows.
/
/ void tex_run(unsigned char *p, const unsigned char *column,
/              unsigned acc, unsigned step, int rows)
/ void tex_run_masked(...)  the same, a texel of pattern 0 skipped
/
/ p is the screen word of the first row, column the texture column's
/ first texel - a word, the attribute over the pattern, as the screen
/ has it - acc the texel's row in 8.8 (its high byte the row, so SWAB
/ finds it), step what acc grows by a row, rows how many rows.  Rows
/ are 80 bytes apart.
/
/ A row costs nine instructions, one of them the store of the whole
/ word; in C it cost twenty.  When a texel covers four rows or more
/ (step at most 64) it is found once and stored into four rows, the
/ sum grown by four steps; two rows when it covers two (step at most
/ 128).  The rows left over go one by one.  So the texel of a run of
/ rows is the texel of the run's first row: a boundary between texels
/ may fall a row or three later than it would row by row, less than
/ the texel is tall.
/
/ Arguments at 2(sp)..012(sp); r2..r5 saved.

	.globl	_tex_run, _tex_run_masked

STRIDE	= 80

	.text
_tex_run:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	012(sp), r0		/ p
	mov	014(sp), r3		/ the column
	mov	016(sp), r1		/ acc
	mov	020(sp), r2		/ step
	mov	022(sp), r4		/ rows
	ble	9f
	cmp	r2, $64
	bhi	2f
/ four rows a texel: r4 / 4 turns, then the rest one by one
	mov	r4, -(sp)
	asr	r4
	asr	r4
	beq	1f
	asl	r2			/ the step for four rows
	asl	r2
4:	mov	r1, r5
	swab	r5
	bic	$0177400, r5
	asl	r5
	add	r3, r5
	mov	(r5), r5
	mov	r5, (r0)
	mov	r5, STRIDE(r0)
	mov	r5, 2*STRIDE(r0)
	mov	r5, 3*STRIDE(r0)
	add	r2, r1
	add	$4*STRIDE, r0
	sob	r4, 4b
	asr	r2			/ the step for one again
	asr	r2
1:	mov	(sp)+, r4
	bic	$0177774, r4		/ the rows left over
	beq	9f
	br	8f
2:	cmp	r2, $128
	bhi	8f
/ two rows a texel
	mov	r4, -(sp)
	asr	r4
	beq	3f
	asl	r2
5:	mov	r1, r5
	swab	r5
	bic	$0177400, r5
	asl	r5
	add	r3, r5
	mov	(r5), r5
	mov	r5, (r0)
	mov	r5, STRIDE(r0)
	add	r2, r1
	add	$2*STRIDE, r0
	sob	r4, 5b
	asr	r2
3:	mov	(sp)+, r4
	bic	$0177776, r4
	beq	9f
/ a row a texel
8:	mov	r1, r5
	swab	r5
	bic	$0177400, r5
	asl	r5
	add	r3, r5
	mov	(r5), (r0)
	add	r2, r1
	add	$STRIDE, r0
	sob	r4, 8b
9:	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

_tex_run_masked:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	012(sp), r0
	mov	014(sp), r3
	mov	016(sp), r1
	mov	020(sp), r2
	mov	022(sp), r4
	ble	9f
1:	mov	r1, r5
	swab	r5
	bic	$0177400, r5
	asl	r5
	add	r3, r5
	tstb	(r5)			/ the pattern: 0 is nothing to draw
	beq	2f
	mov	(r5), (r0)
2:	add	r2, r1
	add	$STRIDE, r0
	sob	r4, 1b
9:	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc
