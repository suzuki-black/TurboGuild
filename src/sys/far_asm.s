; far_asm.s — コードバンク間呼び出しのトランポリン（DECISIONS #14）
; 窓 6000h を呼び先のバンクに切り替えて呼び、戻ったら呼び出し元のバンクに戻す。
; スタブ（tools/gen_far.py が生成）は 6 バイト:
;     call far_tramp / .dw 関数 / .db バンク
; call の戻り番地がそのまま {関数, バンク} を指すので、それを読んで本当の戻り番地は 1 段上。
; 引数はレジスタだけ（A, L, HL, DE）。スタック引数があると位置がずれるので gen_far.py が禁止する。
; 戻り値 A / DE / HLDE を壊さないよう、戻り道では BC と退避用 RAM だけを使う。

	.module	far_asm

BANK_CODE = 0x6800

	.globl	far_tramp
	.globl	_g_bank_code

	.area	_CODE

far_tramp:
	ex	(sp), hl		; HL = {関数, バンク} の位置、スタック = 引数の HL
	ld	(t_a), a
	ld	a, (hl)
	ld	(t_fn), a
	inc	hl
	ld	a, (hl)
	ld	(t_fn + 1), a
	inc	hl
	ld	c, (hl)			; 呼び先のバンク
	pop	hl			; 引数の HL を戻す
	ld	a, (_g_bank_code)
	push	af			; 呼び出し元のバンク（入れ子になってもスタックで戻せる）
	ld	a, c
	ld	(_g_bank_code), a
	ld	(BANK_CODE), a
	ld	a, (t_a)
	call	jump_fn
	pop	bc			; B = 呼び出し元のバンク
	ld	(t_a), a
	ld	a, b
	ld	(_g_bank_code), a
	ld	(BANK_CODE), a
	ld	a, (t_a)
	ret

jump_fn:
	push	hl
	ld	hl, (t_fn)
	ex	(sp), hl
	ret

	.area	_DATA
t_a:	.ds	1
t_fn:	.ds	2
