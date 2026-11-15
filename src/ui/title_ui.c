/* SPDX-License-Identifier: MIT */
/* title_ui.c — 称号の一覧（思い出アルバムの 2）と、マイページの見出しの称号（DECISIONS #107）。
 * 持っている称号は思い出・課金・Lv から毎回決める（core/title.h）。ここで選んだものを g_save.title_sel に残す。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../core/save.h"
#include "../core/title.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define TL_Y    40
#define TL_H    16
#define TL_ROWS (TITLE_COUNT / 2)
#define TL_HINT 172

/* 見出しの上に称号を cx から（括弧は付けない。宿敵と日付の場所を残すため, 称号は 10 桁まで）。次の桁 */
FAR(u8, ui_title_tag, (u16 cx, u16 y))
{
    return txt_id((u8)cx, (u8)y, (u16)(T_TT_0 + title_now()), COL_YELLOW, COL_DBLUE);
}

/* 称号 i の欄（左右 2 列 × 8 行） */
static void cell(u8 i, u16 got)
{
    u8 x = (u8)(i < TL_ROWS ? 0 : 16), y = (u8)(TL_Y + (i % TL_ROWS) * TL_H), on = (u8)(i == g_cursor), has = (u8)((got >> i) & 1);
    u8 bg = on ? COL_BLUE : ((i % TL_ROWS) & 1 ? COL_BLACK : COL_DGRAY);
    vdp_fill((u16)x * 8, y, 128, TL_H, bg);
    if (has && i == title_now()) txt_id(x, y, T_HP_MARK, COL_YELLOW, bg);
    txt_id((u8)(x + 1), y, has ? (u16)(T_TT_0 + i) : T_TT_LOCK, has ? COL_WHITE : COL_LGRAY, bg);
    if (on) {
        vdp_fill(0, TL_HINT, SCREEN_W, 16, COL_BLACK);
        txt_id(1, TL_HINT, (u16)(T_TT_H0 + i), has ? COL_CYAN : COL_LGRAY, COL_BLACK);
    }
}

FAR(void, titles_init, (void)) { g_cursor = title_now(); }

FAR(void, titles_draw, (void))
{
    u16 got = title_earned();
    u8 i, n = 0, cx;
    for (i = 0; i < TITLE_COUNT; i++) n = (u8)(n + ((got >> i) & 1));
    ui_clear();
    ui_header(T_TT_TITLE);
    cx = txt_num(24, 0, n, COL_YELLOW, COL_DBLUE);
    cx = txt_draw(cx, 0, "/", COL_LGRAY, COL_DBLUE);
    txt_num(cx, 0, TITLE_COUNT, COL_LGRAY, COL_DBLUE);
    cx = txt_id(1, 20, g_save.title_sel ? T_TT_NOW : T_TT_AUTO, COL_LGRAY, COL_BLACK);
    {
        u8 x = txt_id(cx, 20, T_TT_L, COL_YELLOW, COL_BLACK);
        x = txt_id(x, 20, (u16)(T_TT_0 + title_now()), COL_WHITE, COL_BLACK);
        txt_id(x, 20, T_TT_R, COL_YELLOW, COL_BLACK);
    }
    for (i = 0; i < TITLE_COUNT; i++) cell(i, got);
    ui_guide(T_TT_GUIDE);
}

FAR(void, titles_update, (void))
{
    u8 p = input_pressed(), d = input_digit(), old = g_cursor;
    u16 got;
    if (p & BTN_CANCEL) { screen_change(&scr_album); return; }
    if (d == 0) { g_save.title_sel = 0; g_save_dirty = 1; psg_beep(0x200, 4); g_dirty = 1; return; }   /* おまかせ */
    if ((p & BTN_UP) && g_cursor % TL_ROWS) g_cursor--;
    if ((p & BTN_DOWN) && g_cursor % TL_ROWS < TL_ROWS - 1) g_cursor++;
    if ((p & BTN_LEFT) && g_cursor >= TL_ROWS) g_cursor -= TL_ROWS;
    if ((p & BTN_RIGHT) && g_cursor < TL_ROWS) g_cursor += TL_ROWS;
    got = title_earned();
    if (g_cursor != old) { psg_beep(0x100, 2); cell(old, got); cell(g_cursor, got); return; }
    if (p & BTN_OK) {
        if (!((got >> g_cursor) & 1)) { psg_beep(0x300, 6); return; }   /* まだ持っていない */
        g_save.title_sel = (u8)(g_cursor + 1);
        g_save_dirty = 1;
        psg_beep(0x080, 6);
        g_dirty = 1;
    }
}

SCREEN(scr_titles, titles_init, titles_update, titles_draw, 0)
