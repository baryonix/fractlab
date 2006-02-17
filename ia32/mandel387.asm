	global	mandelbrot_fp

mandelbrot_fp:
	fild	word [four]
	fld	qword [esp+4]
	fld	qword [esp+12]
	mov	ebx, [esp+20]
	fld	st1
	fld	st1
	mov	ecx, 0

.0:
	fld	st1
	fmul	st2
	fld	st1
	fmul	st2

	fld	st3
	fmul	st3
	fimul	word [two]
	fadd	st5
	fstp	st3

	fld	st1
	fsub	st1
	fadd	st6
	fstp	st4

	fld	st1
	fadd	st1
	;ficomp	word [four]
	;fstsw	ax
	;sahf
	fcomip	st7
	jnc	.1

	;fincstp
	;fincstp
	ffreep	st0
	ffreep	st0
	inc	ecx
	cmp	ebx, ecx
	jg	.0

.1:
	mov	eax, ecx
	fninit
	ret

two:
	dw	2

four:
	dw	4
