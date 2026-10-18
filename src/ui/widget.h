/* widget.h — 画面の共通部品（カーネル常駐）。1 行 = 16 ドット、y はドット、cx は 8 ドット桁 */
#ifndef WIDGET_H
#define WIDGET_H
#include "../types.h"
#include "../config.h"

#define ROW(n)    ((n) * LINE_H)            /* n 行目の y（0..12） */
#define ROW_LAST  ROW(12)

typedef struct {
    u16 count;      /* 項目数 */
    u16 cursor;
    u16 top;        /* 表示の先頭 */
    u8  rows;       /* 1 画面の行数 */
} UiList;

#define UL_NONE   0
#define UL_MOVED  1     /* カーソルだけ動いた（行を 2 つ描き直せばよい） */
#define UL_PAGE   2     /* 表示範囲が変わった（全部描き直す） */

void ui_clear(void);
void ui_box(u8 x, u8 y, u8 w, u8 h, u8 bg, u8 border);   /* 枠つきの箱 */
void ui_key(u8 digit, u8 x, u8 y);    /* 携帯の数字キーの印 */
u8   ui_menu_input(u8 *cursor, u8 n, u8 pressed);   /* 動いたら 1 */
u8   ui_list_input(UiList *l, u8 pressed);
void ui_frame(void);                  /* 1 フレーム待って入力と音を進める（状態は変えない） */
u8   ui_wait_key(void);               /* 何か押されるまで待つ。押されたボタンを返す */
void ui_wait(u8 frames);
void ui_msg(u16 id);                  /* 下に 1 行出して押下を待つ */
u8   ui_confirm(u16 id);              /* はい=1 / いいえ=0（中央の箱） */
void ui_header(u16 id);               /* 上の帯（ページの題） */
void ui_guide(u16 id);                /* 下の帯（キーの案内） */
void ui_status_block(u8 y);           /* 名前・Lv・AP/EXP ゲージ・所持金・軍資金（x=68 から右, 72 行） */
u8   ui_rarity(u8 cx, u8 y, u8 rarity, u8 bg);       /* [SSR] を色付きで。次の桁を返す */
void ui_saving(void);                 /* 「通信中…」を出して保存。失敗は 1 回だけ知らせる */
u8   rarity_color(u8 rarity);
u8   elem_color(u8 element);
#endif
