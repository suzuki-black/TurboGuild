; kanji_asm.s — 漢字ROM（JIS 第1水準, I/O D8h/D9h）から 16x16 の字形 32 バイトを読む。
; 番地 = 字番号 × 32。D8h に字番号の下位 6bit、D9h に上位 6bit を書き、D9h から 32 回読む。
; 字形は 8x8 の 4 ブロック（左上・右上・左下・右下）の順に並んでいる。

	.module	kanji_asm

	.globl	_kanji_read

	.area	_CODE

; void kanji_read(u16 index, u8 *buf)   HL=字番号, DE=32 バイトの置き場
_kanji_read:
	ld	a, l
	and	a, #0x3F
	out	(0xD8), a
	add	hl, hl
	add	hl, hl			; H = 字番号 >> 6
	ld	a, h
	and	a, #0x3F
	out	(0xD9), a
	ex	de, hl
	ld	bc, #(32 * 256 + 0xD9)
	inir
	ret
