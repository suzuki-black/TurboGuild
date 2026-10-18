; bios_asm.s — BIOS 呼び出しの薄いラッパ。BIOS は IX/IY を壊すので必ず退避する。
; sdcccall(1): 第1引数 A(u8)/HL(ptr)、戻り値 A(u8)。

	.module	bios_asm

CHGMOD	= 0x005F
SNSMAT	= 0x0141
PHYDIO	= 0x0144
EXTROM	= 0x015F
SUB_REDCLK = 0x01F5		; ★SUB-ROM のエントリ。メインROMの 01F5h は RDSLT の内部なので直接呼ぶと暴走する

	.globl	_bios_chgmod
	.globl	_bios_snsmat
	.globl	_bios_redclk
	.globl	_bios_phydio

	.area	_CODE

; void bios_chgmod(u8 mode)
_bios_chgmod:
	push	ix
	push	iy
	call	CHGMOD
	pop	iy
	pop	ix
	ei
	ret

; u8 bios_snsmat(u8 row)
_bios_snsmat:
	push	ix
	push	iy
	call	SNSMAT
	pop	iy
	pop	ix
	ret

; u8 bios_redclk(u8 reg)   reg = ブロック<<4 | レジスタ。値は下位 4bit
_bios_redclk:
	push	ix
	push	iy
	ld	c, a
	ld	ix, #SUB_REDCLK
	call	EXTROM
	and	a, #0x0F
	pop	iy
	pop	ix
	ei			; ★EXTROM(CALSLT) は割り込み禁止のまま戻る。戻さないと JIFFY が止まりフレーム待ちで固まる
	ret

; u8 bios_phydio(DiskReq *r)  HL -> { u8 write; u8 drive; u8 count; u8 media;
;                                     u16 sector; u8 *buf; }
; 戻り値: 0=成功、それ以外は PHYDIO のエラーコード + 1
_bios_phydio:
	push	ix
	push	iy
	ld	a, (hl)			; write
	inc	hl
	push	af
	ld	a, (hl)			; drive
	inc	hl
	ld	b, (hl)			; count
	inc	hl
	ld	c, (hl)			; media
	inc	hl
	ld	e, (hl)
	inc	hl
	ld	d, (hl)			; sector
	inc	hl
	push	de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)			; buf
	ex	de, hl
	pop	de
	ld	(drive_tmp), a
	pop	af
	or	a, a
	ld	a, (drive_tmp)
	jr	z, 1$
	scf
	jr	2$
1$:	or	a, a			; CF=0 で読み込み
2$:	call	PHYDIO
	ei
	jr	c, 3$
	xor	a, a
	jr	4$
3$:	inc	a
4$:	pop	iy
	pop	ix
	ret

	.area	_DATA
drive_tmp:
	.ds	1

	.globl	_bios_getcpu
	.area	_CODE
GETCPU	= 0x0183
; u8 bios_getcpu(void)  0=Z80 1=R800 ROM 2=R800 DRAM
_bios_getcpu:
	push	ix
	push	iy
	call	GETCPU
	pop	iy
	pop	ix
	ret
