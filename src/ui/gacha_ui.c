/* gacha_ui.c — ガチャの選択 → 1 回 / 10 連 → 結果（仕様 §4.3）。
 * イベントガチャ（id 4）はイベントが始まる Phase2 まで一覧に出さない。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../core/gacha.h"
#include "../core/tables.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"

#define KINDS   5
#define MENU_Y  ROW(3)

static const u8 kinds[KINDS] = { 1, 2, 3, 5, 6 };
static const u16 cur_name[5] = { 0, T_GOLD, T_GEMS, T_TICKET, T_SRTICKET };

static void draw_tickets(void)
{
    u8 cx;
    ui_fill_rows(ROW(10), 1, COL_BLACK);
    cx = txt_id(1, ROW(10), T_TICKET, COL_GREEN, COL_BLACK);
    cx = txt_num(cx, ROW(10), g_save.gacha_tickets, COL_WHITE, COL_BLACK);
    cx = txt_id((u8)(cx + 2), ROW(10), T_SRTICKET, COL_LYELLOW, COL_BLACK);
    txt_num(cx, ROW(10), g_save.sr_tickets, COL_WHITE, COL_BLACK);
}

static void kind_row(u8 i)
{
    GachaRec g;
    u8 y = (u8)(MENU_Y + i * LINE_H * 1);
    u8 on = (u8)(i == g_cursor);
    u8 cx;
    tbl_gacha(kinds[i], &g);
    ui_fill_rows(y, 1, COL_BLACK);
    ui_cursor(0, y, on);
    txt_id(2, y, g.name_text, on ? COL_LYELLOW : COL_WHITE, COL_BLACK);
    if (g.currency == CUR_FREE) return;
    cx = txt_id(18, y, cur_name[g.currency], COL_GRAY, COL_BLACK);
    cx = txt_num(cx, y, g.price, COL_WHITE, COL_BLACK);
    if (g.price10) {
        cx = txt_draw(cx, y, "/", COL_GRAY, COL_BLACK);
        txt_num(cx, y, g.price10, COL_WHITE, COL_BLACK);
    }
}

FAR(void, gacha_draw, (void))
{
    u8 i;
    ui_clear();
    ui_title(T_GACHA_TITLE);
    ui_money(ROW(1));
    for (i = 0; i < KINDS; i++) kind_row(i);
    draw_tickets();
    ui_footer(T_HELP_SELECT);
}

FAR(void, gacha_init, (void))
{
    g_cursor = (u8)(g_sel_gacha < KINDS ? g_sel_gacha : 0);
}

static u8 ask_count(void)
{
    static const u16 counts[2] = { T_GACHA_1, T_GACHA_10 };
    GachaRec g;
    u8 cur = 0;
    tbl_gacha(kinds[g_cursor], &g);
    if (!g.price10) return 1;
    ui_fill_rows(ROW(8), 2, COL_BLACK);
    ui_menu_draw(20, ROW(8), counts, 2, cur);
    g_ui_state = UIS_WAITKEY;
    for (;;) {
        u8 p;
        ui_frame();
        p = input_pressed();
        if (ui_menu_input(&cur, 2, p)) ui_menu_draw(20, ROW(8), counts, 2, cur);
        if (p & BTN_CANCEL) { ui_fill_rows(ROW(8), 2, COL_BLACK); return 0; }
        if (p & BTN_OK) break;
    }
    ui_fill_rows(ROW(8), 2, COL_BLACK);
    return (u8)(cur ? GACHA_ONE_MAX : 1);
}

static void pull(void)
{
    static const u16 errs[5] = { 0, T_GACHA_NO_MONEY, T_GACHA_FREE_USED, T_GACHA_FULL, T_GACHA_NA };
    u8 n = ask_count();
    if (!n) return;
    g_gacha.gacha = kinds[g_cursor];
    g_gacha.count = n;
    gacha_pull(&g_gacha);
    if (g_gacha.status != G_OK) { ui_msg(errs[g_gacha.status]); return; }
    g_save_dirty = 1;
    g_sel_gacha = g_cursor;
    screen_change(&scr_gacharesult);
}

FAR(void, gacha_update, (void))
{
    u8 p = input_pressed();
    u8 old = g_cursor;
    if (ui_menu_input(&g_cursor, KINDS, p)) { kind_row(old); kind_row(g_cursor); }
    if (p & BTN_OK) pull();
    if (p & BTN_CANCEL) screen_change(&scr_home);
}

SCREEN(scr_gacha, gacha_init, gacha_update, gacha_draw, 0)

/* ---- 結果 ---- */

FAR(void, gacharesult_draw, (void))
{
    ui_clear();
    ui_title(T_GACHA_RESULT);
    ui_footer(T_HELP_ANY);
    ui_reveal();
    if (g_gacha.pity_hit) txt_id(1, ROW(11), T_GACHA_PITY, COL_PINK, COL_BLACK);
}

FAR(void, gacharesult_update, (void))
{
    if (input_pressed() & (BTN_OK | BTN_CANCEL)) screen_change(&scr_gacha);
}

SCREEN(scr_gacharesult, 0, gacharesult_update, gacharesult_draw, 0)
