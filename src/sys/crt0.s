; SPDX-License-Identifier: MIT
; crt0.s — ASCII8 メガROM のヘッダと起動（H.STKE 方式, 仕様 §2.3）
; INIT では H.STKE にフックを仕掛けて即 RET。ディスクROM 初期化後に
; BASIC 起動直前の H.STKE から boot_main へ入り、二度と戻らない。

	.module	crt0

MSXVER	= 0x002D
CHGCPU	= 0x0180
RSLREG	= 0x0138
ENASLT	= 0x0024
INITXT	= 0x006C
CHPUT	= 0x00A2
EXPTBL	= 0xFCC1
SLTTBL	= 0xFCC5
HIMEM	= 0xFC4A
H_STKE	= 0xFEDA

BANK_CODE  = 0x6800		; 0x6000-0x7FFF
BANK_DATAA = 0x7000		; 0x8000-0x9FFF
BANK_DATAB = 0x7800		; 0xA000-0xBFFF

	.globl	_main
	.globl	_g_cart_slot
	.globl	_g_bank_code
	.globl	_g_bank_dataA
	.globl	_g_bank_dataB
	.globl	s__INITIALIZER
	.globl	l__INITIALIZER
	.globl	s__INITIALIZED
	.globl	s__DATA
	.globl	l__DATA
	.globl	s__BSS
	.globl	l__BSS
	.globl	s__HEAP
	.globl	_ram_end

	.area	_HEADER (ABS)
	.org	0x4000
	.db	0x41, 0x42		; "AB"
	.dw	init
	.dw	0, 0, 0
	.db	0, 0, 0, 0, 0, 0

	.area	_CODE
	.area	_HOME
	.area	_INITIALIZER
	.area	_GSINIT
	.area	_GSFINAL
	.area	_DATA
	.area	_INITIALIZED
	.area	_BSEG
	.area	_BSS
	.area	_HEAP

	.area	_CODE

; BIOS は 0x8000 側の "AB" も探す。ASCII8 の初期バンクが 0 だと page2 に
; seg0 の鏡が見えて INIT が 2 回呼ばれるので、自分のフックなら黙って戻る。
init:
	ld	a, (MSXVER)
	cp	a, #3
	jr	c, not_turbor
	ld	a, (H_STKE)
	cp	a, #0xC9
	jr	z, hook_free
	ld	hl, (H_STKE + 2)
	ld	de, #boot_main
	or	a, a
	sbc	hl, de
	ret	z
	ld	hl, #msg_busy
	jp	puts_bios		; 他のフックを壊さず BASIC へ戻す
hook_free:
	call	slotid_page1
	ld	(H_STKE + 1), a
	ld	hl, #boot_main
	ld	(H_STKE + 2), hl
	ld	a, #0xC9
	ld	(H_STKE + 4), a
	ld	a, #0xF7		; RST 30h = CALLF。最後に書いてフックを有効化
	ld	(H_STKE), a
	ret

not_turbor:
	call	INITXT
	ld	hl, #msg_notr
	call	puts_bios
	di
1$:	halt
	jr	1$

puts_bios:
	ld	a, (hl)
	or	a, a
	ret	z
	call	CHPUT
	inc	hl
	jr	puts_bios

msg_notr:
	.ascii	"TurboGuild: MSX turboR only."
	.db	13, 10
	.db	0xC0, 0xB0, 0xCE, 0xDE, 0x52, 0x20	; ﾀｰﾎﾞR (MSX と SJIS の半角カナは同じ符号)
	.db	0xBE, 0xDD, 0xD6, 0xB3, 0xC3, 0xDE, 0xBD	; ｾﾝﾖｳﾃﾞｽ
	.db	13, 10, 0
msg_busy:
	.ascii	"TurboGuild: H.STKE busy. Remove other cartridges."
	.db	13, 10, 0

; ページ1 のスロット ID (E000SSPP)。RSLREG と EXPTBL/SLTTBL から。
slotid_page1:
	call	RSLREG
	rrca
	rrca
	and	a, #0x03
	ld	c, a
	ld	b, #0
	ld	hl, #EXPTBL
	add	hl, bc
	ld	a, (hl)
	and	a, #0x80
	or	a, c
	ld	c, a
	ld	de, #(SLTTBL - EXPTBL)
	add	hl, de
	ld	a, (hl)
	and	a, #0x0C
	or	a, c
	ret

; H.STKE から CALLF で来る。page1 はカートリッジ、page2 は RAM。
boot_main:
	di
	ld	a, #0xC9
	ld	(H_STKE), a		; 二度と呼ばれないように外す
	ld	sp, (HIMEM)

	call	slotid_page1
	push	af
	ld	h, #0x80
	call	ENASLT			; page2 (0x8000-0xBFFF) もカートリッジへ
	di
	; ★スロット ID はスタックに逃がす。この下で BSS ごとゼロ化されるため
	; BSS/DATA をゼロ化してから初期値つき変数を写す
	ld	hl, #s__DATA
	ld	bc, #l__DATA
	call	zero_fill
	ld	hl, #s__BSS
	ld	bc, #l__BSS
	call	zero_fill
	ld	bc, #l__INITIALIZER
	ld	a, b
	or	a, c
	jr	z, 2$
	ld	hl, #s__INITIALIZER
	ld	de, #s__INITIALIZED
	ldir
2$:
	pop	af
	ld	(_g_cart_slot), a

	; ASCII8 の窓を既定位置へ。窓 0x4000 は自分が乗っているので触らない
	ld	a, #1
	ld	(BANK_CODE), a
	ld	(_g_bank_code), a
	ld	a, #2
	ld	(BANK_DATAA), a
	ld	(_g_bank_dataA), a
	ld	a, #3
	ld	(BANK_DATAB), a
	ld	(_g_bank_dataB), a

	ld	a, #0x81		; R800 ROM モード + LED（仕様 §2）。S1990 直書きは実ファームで暴走する
	call	CHGCPU
	call	gsinit
	ei
	call	_main
3$:	jr	3$

; 静的 RAM の終端（_HEAP の先頭）。空き RAM の表示に使う
_ram_end:
	.dw	s__HEAP

zero_fill:
	ld	a, b
	or	a, c
	ret	z
	ld	(hl), #0
	dec	bc
	ld	a, b
	or	a, c
	ret	z
	ld	d, h
	ld	e, l
	inc	de
	ldir
	ret

	.area	_GSINIT
gsinit:
	.area	_GSFINAL
	ret
