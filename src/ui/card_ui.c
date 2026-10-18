/* card_ui.c — カード一覧（先頭は「デッキ編成」への入口）と詳細。
 * 一覧は所持順。★ はデッキに入っているカード。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../core/deck.h"
#include "../core/inventory.h"
#include "../core/tables.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define LIST_Y ROW(1)
#define LIST_N 10

static void row(u16 i)
{
    u8 y = (u8)(LIST_Y + (i - g_list.top) * LINE_H);
    u8 on = (u8)(i == g_list.cursor);
    u8 bg = on ? COL_BLUE : COL_BLACK;
    if (i == 0) {
        ui_fill_rows(y, 1, bg);
        txt_id(3, y, T_DECK_TITLE, COL_LYELLOW, bg);
        return;
    }
    ui_card_row(y, g_save.cards[i - 1].id, g_save.cards[i - 1].lv, inv_in_deck(i - 1), bg);
}

static void rows(void)
{
    u16 i;
    ui_fill_rows(LIST_Y, LIST_N, COL_BLACK);
    for (i = g_list.top; i < g_list.count && i < g_list.top + LIST_N; i++) row(i);
}

FAR(void, cardlist_init, (void))
{
    g_list.count = g_save.card_count + 1;
    g_list.rows = LIST_N;
    if (g_list.cursor >= g_list.count) g_list.cursor = 0;
    if (g_list.top > g_list.cursor) g_list.top = g_list.cursor;
}

FAR(void, cardlist_draw, (void))
{
    u8 cx;
    ui_clear();
    ui_fill_rows(0, 1, COL_BLUE);
    cx = txt_id(1, 0, T_CARD_TITLE, COL_WHITE, COL_BLUE);
    cx = txt_num((u8)(cx + 2), 0, g_save.card_count, COL_WHITE, COL_BLUE);
    cx = txt_draw(cx, 0, "/", COL_GRAY, COL_BLUE);
    txt_num(cx, 0, CARD_MAX, COL_GRAY, COL_BLUE);
    rows();
    ui_footer(T_HELP_SELECT);
}

FAR(void, cardlist_update, (void))
{
    u8 p = input_pressed();
    u16 old = g_list.cursor;
    u8 r = ui_list_input(&g_list, p);
    if (r == UL_PAGE) rows();
    if (r == UL_MOVED) { row(old); row(g_list.cursor); }
    if (p & BTN_OK) {
        if (g_list.cursor == 0) screen_change(&scr_deck);
        else { g_sel_card = g_list.cursor - 1; screen_change(&scr_carddetail); }
    }
    if (p & BTN_CANCEL) { g_list.cursor = 0; g_list.top = 0; screen_change(&scr_home); }
}

SCREEN(scr_cardlist, cardlist_init, cardlist_update, cardlist_draw, 0)

/* ---- 詳細: 枠の色 = レアリティ、中の色 = 属性（Phase1 の絵は枠 4 種だけ） ---- */

static void frame(const CardRec *c)
{
    vdp_fill(16, ROW(2), 80, 112, rarity_color(c->rarity));
    vdp_fill(22, ROW(2) + 6, 68, 100, COL_BLACK);
    vdp_fill(26, ROW(2) + 10, 60, 92, elem_color(c->element));
    txt_id(5, ROW(5), (u16)(T_ELEM_0 + c->element), COL_BLACK, elem_color(c->element));
}

static u8 field(u8 y, u16 label, u32 v)
{
    u8 cx = txt_id(14, y, label, COL_GRAY, COL_BLACK);
    return txt_num((u8)(cx + 1), y, v, COL_WHITE, COL_BLACK);
}

FAR(void, carddetail_draw, (void))
{
    CardRec c;
    const OwnedCard *o = &g_save.cards[g_sel_card];
    u8 cx;
    tbl_card(o->id, &c);
    ui_clear();
    ui_title(T_CARD_DETAIL);
    frame(&c);
    cx = ui_rarity(14, ROW(2), c.rarity, COL_BLACK);
    txt_id((u8)(cx + 1), ROW(2), c.name_text, COL_WHITE, COL_BLACK);
    cx = field(ROW(4), T_LV, o->lv);
    cx = txt_draw(cx, ROW(4), "/", COL_GRAY, COL_BLACK);
    txt_num(cx, ROW(4), c.max_lv, COL_GRAY, COL_BLACK);
    field(ROW(5), T_ATK, card_atk_of(g_sel_card));
    field(ROW(6), T_DEF, card_def_of(g_sel_card));
    field(ROW(7), T_COST, c.cost);
    cx = txt_id(14, ROW(8), T_SKILL, COL_GRAY, COL_BLACK);
    txt_id((u8)(cx + 1), ROW(8), (u16)(T_SKILL_0 + c.skill), COL_LYELLOW, COL_BLACK);
    if (inv_in_deck(g_sel_card)) txt_id(14, ROW(10), T_IN_DECK, COL_LYELLOW, COL_BLACK);
    ui_footer(T_HELP_BACK);
}

FAR(void, carddetail_update, (void))
{
    if (input_pressed() & (BTN_CANCEL | BTN_OK)) screen_change(&scr_cardlist);
}

SCREEN(scr_carddetail, 0, carddetail_update, carddetail_draw, 0)
