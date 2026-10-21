; SPDX-License-Identifier: MIT
; vdp_asm.s — VDP I/O の唯一の入口（SDCC sdcccall(1): 第1引数 A/HL, 第2引数 L/DE）
; 1 操作を 1 つの di/ei 区間に閉じる。BIOS の割り込みが S#0 を読むので、
; アドレス設定の途中に割り込みが刺さるとラッチがずれる。

	.module	vdp_asm

VDP_DATA = 0x98
VDP_ADDR = 0x99
VDP_IND  = 0x9B

	.globl	_vdp_wreg
	.globl	_vdp_blit
	.globl	_vdp_cmd
	.globl	_vdp_wait_cmd
	.globl	_vdp_palette
	.globl	_vdp_put_rect
	.globl	_vdp_load
	.globl	_vdp_pal1

	.area	_HOT		; RAM で実行（MEMORY_MAP の hot: 行）

; void vdp_wreg(u8 reg, u8 val)   A=reg L=val
_vdp_wreg:
	ld	c, a
	ld	a, l
	di
	out	(VDP_ADDR), a
	ld	a, c
	or	a, #0x80
	out	(VDP_ADDR), a
	ei
	ret

; 書き込みアドレス設定（di 済みで呼ぶ）。HL = VRAM アドレス (0x0000-0xFFFF)
setw_raw:
	ld	a, h
	rlca
	rlca
	and	a, #0x03
	out	(VDP_ADDR), a
	ld	a, #0x8E		; R#14
	out	(VDP_ADDR), a
	ld	a, l
	out	(VDP_ADDR), a
	ld	a, h
	and	a, #0x3F
	or	a, #0x40
	out	(VDP_ADDR), a
	ret

; void vdp_blit(const VdpXfer *x)   HL -> { u16 addr; const u8 *src; u8 len; }
_vdp_blit:
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	inc	hl
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	inc	hl
	ld	a, (hl)			; len
	push	bc
	ld	b, a			; ★len は B に。setw_raw は A を壊す
	ex	de, hl
	di
	call	wait_cmd_raw		; ★塗りつぶしコマンドが走っている間に書くと後から上書きされる
	call	setw_raw
	pop	hl
	ld	c, #VDP_DATA
	otir
	ei
	ret

; void vdp_wait_cmd(void)  S#2 の CE が落ちるまで待つ
_vdp_wait_cmd:
	di
	call	wait_cmd_raw
	ei
	ret

; di 済みで呼ぶ。終わったら R#15 を 0 に戻す（BIOS の割り込みは S#0 を読む前提）
wait_cmd_raw:
	ld	a, #2
	out	(VDP_ADDR), a
	ld	a, #0x8F
	out	(VDP_ADDR), a
1$:	in	a, (VDP_ADDR)
	rrca
	jr	c, 1$
	xor	a, a
	out	(VDP_ADDR), a
	ld	a, #0x8F
	out	(VDP_ADDR), a
	ret

; void vdp_cmd(const VdpCmd *c)   HL -> R#32..R#46 の 15 バイト
; 実行中の VDP が SX/DX を書き換えるので、差分でなく毎回全部書く。
_vdp_cmd:
	push	hl
	call	_vdp_wait_cmd
	pop	hl
	di
	ld	a, #32
	out	(VDP_ADDR), a
	ld	a, #0x91		; R#17 = 32, 自動インクリメント
	out	(VDP_ADDR), a
	ld	bc, #(15 * 256 + VDP_IND)
	otir
	ei
	ret

; void vdp_palette(const u8 *pal)   HL -> 16 色 × 2 バイト (0RRR0BBB, 00000GGG)
_vdp_palette:
	di
	xor	a, a
	out	(VDP_ADDR), a
	ld	a, #0x90		; R#16 = 0
	out	(VDP_ADDR), a
	ld	bc, #(32 * 256 + 0x9A)
	otir
	ei
	ret

; void vdp_put_rect(const VdpRect *r)   HL -> { u16 addr; const u8 *buf; u8 w; u8 h; }
; SCREEN5 の矩形（w バイト × h 行）を CPU で書く。1 行 128 バイトなので行ごとにアドレスを設定し直す。
; 1 回の呼び出し = 1 つの di 区間。
_vdp_put_rect:
	ld	e, (hl)
	inc	hl
	ld	d, (hl)			; DE = VRAM アドレス
	inc	hl
	ld	a, (hl)
	inc	hl
	ld	b, (hl)
	inc	hl
	push	bc			; B = buf 上位
	ld	c, a			; C = buf 下位
	ld	a, (hl)			; w
	ld	(r_w), a
	inc	hl
	ld	a, (hl)			; h
	pop	hl
	ld	l, c			; HL = buf
	di
	push	af
	call	wait_cmd_raw
	pop	af
1$:	push	af
	ex	de, hl
	call	setw_raw
	ld	a, #128
	add	a, l
	ld	l, a
	jr	nc, 2$
	inc	h
2$:	ex	de, hl
	ld	a, (r_w)
	ld	b, a
	ld	c, #VDP_DATA
	otir
	pop	af
	dec	a
	jr	nz, 1$
	ei
	ret

	.area	_DATA
r_w:	.ds	1

	.area	_HOT		; RAM で実行（MEMORY_MAP の hot: 行）

; void vdp_load(const VdpXfer16 *x)   HL -> { u16 addr; const u8 *src; u16 len; }
; 連続した len バイトを VRAM へ（アトラスの読み込み用）。256 バイトずつ OTIR。
_vdp_load:
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	inc	hl
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	inc	hl
	push	bc			; src
	ld	c, (hl)
	inc	hl
	ld	b, (hl)			; BC = len
	ex	de, hl
	di
	call	wait_cmd_raw
	call	setw_raw
	pop	hl			; HL = src
	ld	a, c			; 端数
	ld	d, b			; 256 バイトの回数
	ld	c, #VDP_DATA
	or	a, a
	jr	z, 2$
	ld	b, a
	otir
2$:	ld	a, d
	or	a, a
	jr	z, 3$
1$:	ld	b, #0
	otir
	dec	d
	jr	nz, 1$
3$:	ei
	ret

; void vdp_pal1(u8 index, const u8 *pal)   A=番号, DE -> 2 バイト（演出でパレットを 1 色だけ変える）
_vdp_pal1:
	di
	out	(VDP_ADDR), a
	ld	a, #0x90		; R#16 = 番号
	out	(VDP_ADDR), a
	ex	de, hl
	ld	bc, #(2 * 256 + 0x9A)
	otir
	ei
	ret
