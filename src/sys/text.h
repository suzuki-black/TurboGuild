/* text.h — Shift-JIS 文字列の描画（全角は漢字ROM の 16x16、半角は BIOS 字形を縦 2 倍の 8x16）
 * 座標: cx は 8 ドット単位の桁（0..31）、y はドット（0..211）。戻り値は次の桁。 */
#ifndef TEXT_H
#define TEXT_H
#include "../types.h"
u8   txt_draw(u8 cx, u8 y, const char *s, u8 fg, u8 bg);   /* 右端で止まったら g_txt_rest が続きを指す */
u8   txt_id(u8 cx, u8 y, u16 id, u8 fg, u8 bg);       /* text.dat の文字列 */
u8   txt_num(u8 cx, u8 y, u32 v, u8 fg, u8 bg);
u8   txt_num_r(u8 cx, u8 y, u32 v, u8 width, u8 fg, u8 bg);  /* 右寄せ（空きは bg で埋める） */
u8   txt_width(const char *s);                         /* 桁数 */
u8   txt_id_width(u16 id);
const char *txt_ptr(u16 id);      /* 窓 A000h を文字列のページに切り替えて指す。次の txt_* まで有効 */
#endif
