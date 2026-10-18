/* font.h — 半角文字の描画（BIOS CGTABL の字形を SCREEN5 へ展開） */
#ifndef FONT_H
#define FONT_H
#include "../types.h"
void font_putc(u8 cx, u8 cy, u8 ch, u8 fg, u8 bg);
void font_puts(u8 cx, u8 cy, const char *s, u8 fg, u8 bg);
void font_puthex(u8 cx, u8 cy, u16 v, u8 digits, u8 fg, u8 bg);
void font_putdec(u8 cx, u8 cy, u32 v, u8 fg, u8 bg);
#endif
