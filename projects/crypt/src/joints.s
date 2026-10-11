/ joints.s - what moves on a column of the view (view.c): the joints
/ of the bricks and the walls' corner lines as runs of rows, and the
/ column put up with them drawn in and taken out again.
/
/ void plane_joints(int mask, int mirror, int corners, int lateral, int c, int z0, int z1, int through)
/ void front_joints(int mask, int corner, int half, int n, int j, int c, int through)
/ void column_show(unsigned char *col, int screen)
/
/ The runs go to view_runs, three bytes each - the first row, the rows,
/ the pixel - view_runN of them, which the caller clears before the
/ column's pieces; view_edgeRows and view_edgeN are the floor's edges
/ across (view.c floorEdges), rows made black whole.  The tables are
/ geometry.c's.  Arguments above the return address after the four
/ registers saved, from 012(sp); r0, r1 free, the rest kept.
/
/ plane_joints, for a wall along the axis `lateral` planes out, on the
/ right when `mirror`, between depths z0 and z1 (40ths of a cell past
/ the party), its pixels `mask` of column c, its corners' pixels
/ `corners`: as view.c had it in C.  Along the wall a brick is BRICK
/ 40ths, the second and fourth rows of bricks laid half a brick on; a
/ joint is a line between two mortar rows in the pixel whose centre's
/ depth is the last not past the joint's; none within DETAIL pixels
/ of another or of the face's ends, whose lines are the corners';
/ none where the wall is shorter than 2 * DETAIL rows above the
/ horizon, nor in a column that sees more than two bricks.  The lines
/ stop at `through`, the last row wall throughout.
/
/ front_joints, for a wall across the axis `half` rows above the
/ horizon, n rows ahead and j cells to the right: a corner line down
/ the pixels `corner`, and a joint down the column's first pixel where
/ the brick along the face (half a cell) changes from the column
/ before, between the wall's top line and its foot line.
/
/ column_show: the runs turned on in the column (words: the pixel bit
/ in the low byte, xor), the edges' rows made black with their bytes
/ kept, the column put up by ms_vput, then all of it undone - nothing
/ else sets those pixels, so the turning back is exact.

	.globl	_plane_joints, _front_joints, _column_show
	.globl	_view_runs, _view_runN, _view_edgeRows, _view_edgeN, _view_phase8
	.globl	_wall_z, _wall_half, _col_zmin, _col_zmax, _col_tan
	.globl	_fx_div8, _fx_mul8, _ms_vput

HALF	= 60
ROWS	= 120
TOP	= 16			/ the window's first screen row
BRICK	= 20
HALFBRICK = 10			/ half a brick: the joints are so far apart (a slash would begin a comment)
DETAIL	= 4
MAXRUNS	= 32
EDGES	= 4			/ CROSS_DEPTH

	.data
_view_runN:	.word	0
_view_edgeN:	.word	0
_view_phase8:	.word	0
_view_runs:	.=.+MAXRUNS*3
_view_edgeRows:	.=.+EDGES
edgeWas:	.=.+EDGES
	.even
/ the line routine's words
lhalf:		.word	0
lfirst:		.word	0
lstep:		.word	0
lthrough:	.word	0
lfoot:		.word	0		/ -1 for a wall across: its foot's row is a line
ledges:		.=.+12		/ five edges: the top, three mortar rows, the foot
lbit:		.byte	0
	.even
/ plane_joints' words
jmask:		.word	0
jmirror:	.word	0
jc:		.word	0
jtc:		.word	0
jz0:		.word	0
jz1:		.word	0
jzmin:		.word	0
jzmax:		.word	0
jL40:		.word	0
jlastX:		.word	0
jxEnd:		.word	0
jrowZ:		.word	0
jrowH:		.word	0
jz:		.word	0
jparity:	.word	0
ji:		.word	0
jzs:		.=.+16
/ front_joints' words
fn:		.word	0
fj:		.word	0
lat40:		.word	20, 60, 100	/ the planes' laterals, 40ths of a cell
bits:		.byte	0200, 0100, 040, 020, 010, 4, 2, 1
	.even

	.text

/ The runs of a line down pixel lbit in the brick rows lfirst, lfirst
/ + lstep, ... of a wall lhalf rows above the horizon, each between
/ two of its lines (the top, the three mortar rows, the foot) and
/ clipped at lthrough.  r0..r5 used.
line:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	$HALF, r0
	sub	*$lhalf, r0
	mov	r0, *$ledges		/ 60 - half
	mov	*$lhalf, r1
	asr	r1
	cmp	*$lhalf, $2*DETAIL	/ a wall without mortar rows: its brick rows are its halves
	bge	7f
	clr	r1
7:	mov	$HALF, r0
	sub	r1, r0
	mov	r0, ledges+2		/ 60 - half / 2
	mov	$HALF, ledges+4
	mov	$HALF, r0
	add	r1, r0
	mov	r0, ledges+6		/ 60 + half / 2
	mov	$HALF, r0
	add	*$lhalf, r0
	add	*$lfoot, r0
	mov	r0, ledges+8		/ 60 + half, less one for a wall across (8 is decimal here: the fifth word)
	mov	*$lfirst, r2
1:	cmp	r2, $4
	bge	9f
	mov	r2, r3
	asl	r3
	mov	ledges(r3), r0		/ from = edge + 1, or 0
	bge	2f
	mov	$-1, r0
2:	inc	r0
	mov	ledges+2(r3), r1	/ to = the next edge, or through + 1
	cmp	r1, *$lthrough
	ble	3f
	mov	*$lthrough, r1
	inc	r1
3:	sub	r0, r1			/ the rows
	ble	4f
	cmp	*$_view_runN, $MAXRUNS
	bge	9f
	mov	*$_view_runN, r4
	mov	r4, r5
	asl	r4
	add	r5, r4			/ runN * 3
	add	$_view_runs, r4
	movb	r0, (r4)+
	movb	r1, (r4)+
	movb	*$lbit, (r4)
	inc	*$_view_runN
4:	add	*$lstep, r2
	br	1b
9:	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

/ r0 = depth z on the plane -> r0 = the sector pixel column of column
/ jc whose centre's depth is the last not past it, the pixels' depths
/ in jzs, the nearest first.  r1, r2 used.
pixelAt:
	clr	r1			/ i
	mov	$jzs+2, r2
1:	cmp	r1, $7
	bge	2f
	cmp	(r2)+, r0		/ zs[i + 1] <= z: on
	bgt	2f
	inc	r1
	br	1b
2:	tst	*$jmirror
	beq	3f
	neg	r1
	add	$7, r1
3:	mov	*$jc, r0
	asl	r0
	asl	r0
	asl	r0
	add	r1, r0
	rts	pc

_plane_joints:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	012(sp), *$jmask
	mov	014(sp), *$jmirror
	mov	022(sp), *$jc
	mov	024(sp), *$jz0
	mov	026(sp), *$jz1
	mov	030(sp), *$lthrough
	clr	*$lfoot
	mov	*$jc, r0			/ tc: the column in the tables, the left half's
	tst	*$jmirror
	beq	1f
	neg	r0
	add	$31, r0
1:	mov	r0, *$jtc
	mov	020(sp), r1		/ the lateral's rows of the tables
	mov	r1, r0
	asl	r0
	mov	lat40(r0), *$jL40
	swab	r1			/ lateral * 256: the row of wall_z, words
	mov	r1, r0
	add	$_wall_z, r0
	mov	r0, *$jrowZ
	mov	r1, r0
	asr	r0			/ lateral * 128: the row of wall_half, bytes
	add	$_wall_half, r0
	mov	r0, *$jrowH
	mov	r1, r0
	asr	r0
	asr	r0
	asr	r0			/ lateral * 32: the row of col_zmin, words
	add	*$jtc, r0
	add	*$jtc, r0
	mov	_col_zmin(r0), *$jzmin
	mov	_col_zmax(r0), *$jzmax
	mov	016(sp), r3		/ the corners: a line the wall's height
	beq	20f
	clr	r4			/ k
10:	movb	bits(r4), r0
	bitb	r0, r3
	beq	12f
	mov	r4, r1			/ the pixel in the tables: 7 - k mirrored
	tst	*$jmirror
	beq	11f
	neg	r1
	add	$7, r1
11:	mov	*$jtc, r0
	asl	r0
	asl	r0
	asl	r0
	add	r1, r0
	add	*$jrowH, r0
	clr	r2
	bisb	(r0), r2		/ half: a line when the wall is DETAIL rows tall
	cmp	r2, $DETAIL
	blt	12f
	mov	r2, *$lhalf
	movb	bits(r4), *$lbit
	clr	*$lfirst
	mov	$1, *$lstep
	jsr	pc, line
12:	inc	r4
	cmp	r4, $8
	blt	10b
20:	mov	*$jzmax, r0		/ a column that sees more than two bricks: no joints
	bge	28f			/ (a branch reaches 254 bytes and the assembler says
	jmp	90f			/ nothing past that: so jmp from here on)
28:	sub	*$jzmin, r0
	cmp	r0, $2*BRICK
	ble	29f
	jmp	90f
29:	mov	*$jtc, r0			/ the pixels' depths, the nearest first
	asl	r0
	asl	r0
	asl	r0
	asl	r0
	add	*$jrowZ, r0
	mov	$jzs, r1
	mov	$8, r2
21:	mov	(r0)+, (r1)+
	sob	r2, 21b
	mov	$-10, *$jlastX		/ the face's ends, where they fall in this column
	mov	$-10, *$jxEnd
	mov	*$jz0, r0
	cmp	r0, *$jzmin
	blt	22f
	jsr	pc, pixelAt
	mov	r0, *$jlastX
22:	mov	*$jz1, r0
	cmp	r0, *$jzmax
	bgt	23f
	jsr	pc, pixelAt
	mov	r0, *$jxEnd
23:	mov	*$jz1, r0
	cmp	r0, *$jzmax
	ble	24f
	mov	*$jzmax, *$jz1
24:	mov	*$jzmin, r0		/ nearer than the lateral is past the sector's edge
	cmp	r0, *$jL40
	bgt	25f
	mov	*$jL40, r0
	inc	r0
	mov	r0, *$jzmin
25:	mov	*$jzmin, r0		/ the first joint at the column's near depth or past it:
	cmp	r0, *$jz0			/ so many half-bricks on from the face's beginning
	ble	26f
	sub	*$jz0, r0
	add	$HALFBRICK-1, r0
	mov	$HALFBRICK, -(sp)
	mov	r0, -(sp)
	jsr	pc, _fx_div8
	cmp	(sp)+, (sp)+
	mov	r0, r1			/ z = z0 + i * 10
	asl	r1
	asl	r1
	asl	r1
	mov	r0, r2
	asl	r2
	add	r2, r1
	add	*$jz0, r1
	mov	r1, *$jz
	bic	$-2, r0
	mov	r0, *$jparity
	br	30f
26:	mov	*$jz0, *$jz
	clr	*$jparity
30:	clr	*$ji
31:	mov	*$jz, r0
	cmp	r0, *$jz1
	ble	32f
	jmp	90f
32:	mov	*$ji, r1			/ the pixel whose centre is not past the joint
	cmp	r1, $7
	bge	33f
	asl	r1
	cmp	jzs+2(r1), r0
	bgt	33f
	inc	*$ji
	br	32b
33:	mov	*$ji, r3			/ k, the pixel on the screen
	tst	*$jmirror
	beq	34f
	neg	r3
	add	$7, r3
34:	mov	*$jc, r2			/ x
	asl	r2
	asl	r2
	asl	r2
	add	r3, r2
	mov	r2, r0			/ none within DETAIL of the last, or of the face's ends
	sub	*$jlastX, r0
	cmp	r0, $DETAIL
	bge	35f
	cmp	r0, $-DETAIL
	bgt	40f
35:	mov	r2, r0
	sub	*$jxEnd, r0
	cmp	r0, $DETAIL
	bge	36f
	cmp	r0, $-DETAIL
	bgt	40f
36:	mov	r2, *$jlastX
	movb	bits(r3), r0
	bitb	r0, *$jmask
	beq	40f
	mov	*$jtc, r1			/ half = the pixel's, in the tables
	asl	r1
	asl	r1
	asl	r1
	add	*$ji, r1
	add	*$jrowH, r1
	clr	r4
	bisb	(r1), r4
	cmp	r4, $DETAIL		/ a wall shorter than DETAIL rows: a fill
	blt	40f
	mov	r4, *$lhalf
	movb	bits(r3), *$lbit
	mov	*$jparity, *$lfirst
	mov	$2, *$lstep
	cmp	r4, $2*DETAIL		/ without mortar rows: a joint a brick, the wall's height
	bge	37f
	tst	*$jparity
	bne	40f
	mov	$1, *$lstep
37:	jsr	pc, line
40:	add	$HALFBRICK, *$jz
	inc	*$jparity
	bic	$-2, *$jparity
	jmp	31b			/ too far back for a branch
90:	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

/ r0 = sector column c -> r0 = where along the face it looks, 256ths
/ of a cell from the face's left end: the column's tangent by the
/ face's depth, less the cell's offset.  r1, r2 used.
along:
	asl	r0
	mov	_col_tan(r0), r1
	mov	r1, r2			/ the sign
	bpl	1f
	neg	r1
1:	cmp	r1, $255
	ble	2f
	mov	$255, r1
2:	mov	*$fn, r0			/ d8 = n * 256 - 128 - the phase's eighths
	swab	r0
	sub	$128, r0
	sub	*$_view_phase8, r0
	mov	r0, -(sp)
	mov	r1, -(sp)
	jsr	pc, _fx_mul8
	cmp	(sp)+, (sp)+
	tst	r2
	bpl	3f
	neg	r0
3:	mov	*$fj, r1			/ less j * 256 - 128, j signed
	asl	r1
	asl	r1
	asl	r1
	asl	r1
	asl	r1
	asl	r1
	asl	r1
	asl	r1
	sub	$128, r1
	sub	r1, r0
	rts	pc

/ r0, r1 -> the bricks they fall in, r0 / 128 and r1 / 128, compared:
/ the Z flag set when the same.
sameBrick:
	asr	r0
	asr	r0
	asr	r0
	asr	r0
	asr	r0
	asr	r0
	asr	r0
	asr	r1
	asr	r1
	asr	r1
	asr	r1
	asr	r1
	asr	r1
	asr	r1
	cmp	r0, r1
	rts	pc

_front_joints:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	026(sp), *$lthrough
	mov	$-1, *$lfoot
	mov	016(sp), r2		/ half: a wall shorter than DETAIL rows is a fill
	cmp	r2, $DETAIL
	blt	90f
	mov	r2, *$lhalf
	mov	020(sp), *$fn
	mov	022(sp), *$fj
	mov	014(sp), r0		/ the corners
	beq	1f
	movb	r0, *$lbit
	clr	*$lfirst
	mov	$1, *$lstep
	jsr	pc, line
1:	bit	$0200, 012(sp)		/ the joint: down the column's first pixel, when the wall has it
	beq	90f
	mov	024(sp), r0		/ and there is a column before
	beq	90f
	jsr	pc, along
	mov	r0, r3			/ u8 here
	mov	024(sp), r0
	dec	r0
	jsr	pc, along
	mov	r0, r4			/ u8 in the column before
	mov	r3, r0
	mov	r4, r1
	jsr	pc, sameBrick		/ the first and third brick rows
	beq	2f
	movb	$0200, *$lbit
	clr	*$lfirst
	mov	$2, *$lstep
	cmp	*$lhalf, $2*DETAIL	/ without mortar rows: a joint a brick, the wall's height
	bge	11f
	mov	$1, *$lstep
11:	jsr	pc, line
2:	cmp	*$lhalf, $2*DETAIL	/ the second and fourth rows only between mortar rows
	blt	90f
	mov	r3, r0			/ the second and fourth, laid half a brick on
	add	$64, r0
	mov	r4, r1
	add	$64, r1
	jsr	pc, sameBrick
	beq	90f
	movb	$0200, *$lbit
	mov	$1, *$lfirst
	mov	$2, *$lstep
	jsr	pc, line
90:	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

/ The runs turned in the column at r5: a word's low byte xor-ed down
/ the rows.  r0..r4 used.
flips:
	mov	*$_view_runN, r4
	beq	9f
	mov	$_view_runs, r3
1:	clr	r0
	bisb	(r3)+, r0		/ the first row
	asl	r0
	add	r5, r0			/ its word
	clr	r1
	bisb	(r3)+, r1		/ the rows
	clr	r2
	bisb	(r3)+, r2		/ the pixel
2:	xor	r2, (r0)
	add	$2, r0
	sob	r1, 2b
	sob	r4, 1b
9:	rts	pc

_column_show:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	012(sp), r5		/ the column
	jsr	pc, flips
	mov	*$_view_edgeN, r4		/ the floor's edges across: rows black whole, their bytes kept
	beq	2f
	mov	$_view_edgeRows, r3
	mov	$edgeWas, r2
1:	clr	r0
	bisb	(r3)+, r0
	asl	r0
	add	r5, r0
	movb	(r0), (r2)+
	movb	$-1, (r0)
	sob	r4, 1b
2:	mov	r5, -(sp)		/ ms_vput(screen, TOP, ROWS, col)
	mov	$ROWS, -(sp)
	mov	$TOP, -(sp)
	mov	022(sp), -(sp)		/ the screen column, three words up by now
	jsr	pc, _ms_vput
	add	$8, sp
	mov	*$_view_edgeN, r4		/ and all of it undone
	beq	4f
	mov	$_view_edgeRows, r3
	mov	$edgeWas, r2
3:	clr	r0
	bisb	(r3)+, r0
	asl	r0
	add	r5, r0
	movb	(r2)+, (r0)
	sob	r4, 3b
4:	jsr	pc, flips
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc
