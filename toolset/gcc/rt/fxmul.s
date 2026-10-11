/ fxmul.s - fx_mul, the full 8.8 product (include/fx.h), in a file of
/ its own: a program that does not use it links none of it.  The
/ arguments on the stack above the return address, the result in r0,
/ r0 and r1 free, r2..r5 kept - GCC's convention, as fx.s.

	.globl	_fx_mul

	.text

/ int fx_mul(int a, int b): a * b / 256 - the full 32-bit product of the
/ magnitudes by shift-and-add, its middle sixteen bits taken, the sign
/ put back.
_fx_mul:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r5, -(sp)
	mov	010(sp), r2		/ a
	mov	012(sp), r3		/ b
	mov	r2, r5
	xor	r3, r5
	mov	r5, -(sp)		/ the product's sign: the two differ
	tst	r2
	bpl	1f
	neg	r2
1:	tst	r3
	bpl	2f
	neg	r3
2:	clr	r0			/ r0:r1 = r2 * r3, the multiplicand growing into r5:r2
	clr	r1
	clr	r5
3:	clc
	ror	r3
	bcc	4f
	add	r2, r1
	adc	r0
	add	r5, r0
4:	asl	r2
	rol	r5
	tst	r3
	bne	3b
	swab	r1			/ the middle sixteen: r0's low byte over r1's high
	bic	$0177400, r1
	swab	r0
	bic	$0377, r0
	bis	r1, r0
	tst	(sp)+
	bpl	5f
	neg	r0
5:	mov	(sp)+, r5
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc
