/* SPDX-License-Identifier: MIT */
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
u8   ui_weak(u8 cx, u8 y, u8 enemy, u8 bg);         /* [6] 弱点 水（6 キーで ui_fit_deck）。次の桁を返す */
void ui_fit_deck(u16 stage);          /* その面の敵に合わせておまかせ編成して知らせる */
void ui_helper_pick(u16 enemy);       /* 助っ人を選ぶ画面（ui/helper_ui.c, DECISIONS #106）。終わると g_dirty */
void ui_helper_line(u16 y);           /* 選んである助っ人の 1 行（濃い青の上に） */
u8   ui_title_tag(u16 cx, u16 y);     /* つけている称号を見出しの上に（ui/title_ui.c, DECISIONS #107）。次の桁 */
void ui_diary(u16 npc);              /* 仲間の日記（ui/diary_ui.c, DECISIONS #110）。閉じるまでここで待つ */
void deco_cards(u16 wall, u16 *ids); /* 待ち受け wall の 4 枚（ui/deco_ui.c, DECISIONS #113） */
u8   deco_song(void);                  /* タイトルで流す着うた */
void ui_payday(void);                /* 給料日の知らせとご利用料金・親バレ（ui/keitai_ui.c） */
void ui_helper_say(u16 y);            /* 直前の戦いで加勢していたら知らせる（y から 42 ドットの箱） */
u8   ui_stance(u8 cx, u8 y, u8 ap, u8 bg);       /* [0] 全力 AP10（ap = 0 なら AP は出さない）。次の桁を返す */
void ui_stance_next(void);            /* 戦い方を 通常 → 節約 → 全力 → 通常 と替える */
u8   rarity_color(u8 rarity);
u8   elem_color(u8 element);
#endif
