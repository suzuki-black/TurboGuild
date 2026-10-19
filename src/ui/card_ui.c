/* card_ui.c — カード一覧（絵のグリッド。下に選んだカードの名前と攻撃/防御）と、
 * 詳細（絵を 2 倍で大きく、右に数値）。★ はデッキに入っているカード。 */
#include "screen.h"
#include "widget.h"
#include "cardgrid.h"
#include "../far.h"
#include "../ram.h"
#include "../core/deck.h"
#include "../core/inventory.h"
#include "../core/tables.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/gfx.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define GRID_Y  18
#define CELL_W  42
#define CELL_H  52
#define INFO_Y  176

#define g_with_remove g_grid_remove

void grid_setup(u8 with_remove)
{
    g_with_remove = with_remove;
    g_list.count = g_save.card_count + with_remove;
    g_list.rows = GRID_PAGE;
    if (g_list.cursor >= g_list.count) g_list.cursor = 0;
    g_list.top = (u16)(g_list.cursor / GRID_PAGE * GRID_PAGE);
}

s16 grid_card(void)
{
    return (s16)g_list.cursor - g_with_remove;
}

static void info(void)
{
    CardRec c;
    s16 i = grid_card();
    u8 cx;
    vdp_fill(0, INFO_Y, SCREEN_W, 16, COL_DBLUE);
    if (i < 0) { txt_id(1, INFO_Y, T_DECK_CLEAR, COL_YELLOW, COL_DBLUE); return; }
    tbl_card(g_save.cards[i].id, &c);
    cx = ui_rarity(0, INFO_Y, c.rarity, COL_DBLUE);
    txt_id((u8)(cx + 1), INFO_Y, c.name_text, COL_WHITE, COL_DBLUE);
    cx = txt_id(21, INFO_Y, T_ATK, COL_LGRAY, COL_DBLUE);
    txt_num_r(cx, INFO_Y, card_atk_of((u16)i), 6, COL_YELLOW, COL_DBLUE);
}

static void cell(u16 n)
{
    u16 k = n - g_list.top;
    u8 x = (u8)((k % GRID_COLS) * CELL_W + 2), y = (u8)(GRID_Y + (k / GRID_COLS) * CELL_H);
    u8 on = (u8)(n == g_list.cursor);
    s16 i = (s16)n - g_with_remove;
    u8 cx;
    vdp_fill(x, y, CELL_W - 2, CELL_H - 2, on ? COL_YELLOW : COL_BLACK);
    vdp_fill((u16)x + 2, (u16)y + 2, CELL_W - 6, CELL_H - 6, on ? COL_BLUE : COL_DBLUE);
    if (n >= g_list.count) { vdp_fill(x, y, CELL_W - 2, CELL_H - 2, COL_BLACK); return; }
    if (i < 0) {
        gfx_part(ATL_CARDBACK_S, (u8)(x + 6), (u8)(y + 4));
        txt_id((u8)(x / 8), (u8)(y + 34), T_DECK_CLEAR, COL_WHITE, on ? COL_BLUE : COL_DBLUE);
        return;
    }
    gfx_thumb(g_save.cards[i].id, (u8)(x + 6), (u8)(y + 3));
    if (inv_in_deck((u16)i)) gfx_sprite(ATL_SPARK, (u8)(x + 28), (u8)(y + 2));
    cx = txt_id((u8)(x / 8), (u8)(y + 34), T_LV, COL_LGRAY, on ? COL_BLUE : COL_DBLUE);
    txt_num(cx, (u8)(y + 34), g_save.cards[i].lv, COL_WHITE, on ? COL_BLUE : COL_DBLUE);
}

void grid_draw(void)
{
    u16 n;
    for (n = g_list.top; n < g_list.top + GRID_PAGE; n++) cell(n);
    info();
}

u8 grid_input(u8 p)
{
    u16 old = g_list.cursor, top = g_list.top;
    if (!g_list.count) return 0;
    if ((p & BTN_LEFT) && g_list.cursor > 0) g_list.cursor--;
    if ((p & BTN_RIGHT) && g_list.cursor + 1 < g_list.count) g_list.cursor++;
    if ((p & BTN_UP) && g_list.cursor >= GRID_COLS) g_list.cursor -= GRID_COLS;
    if ((p & BTN_DOWN) && g_list.cursor + GRID_COLS < g_list.count) g_list.cursor += GRID_COLS;
    if (g_list.cursor == old) return 0;
    psg_beep(0x100, 2);
    g_list.top = (u16)(g_list.cursor / GRID_PAGE * GRID_PAGE);
    if (g_list.top != top) { grid_draw(); return 1; }
    cell(old);
    cell(g_list.cursor);
    info();
    return 1;
}

FAR(void, cardlist_init, (void))
{
    grid_setup(0);
}

FAR(void, cardlist_draw, (void))
{
    u8 cx;
    ui_clear();
    ui_header(T_CARD_TITLE);
    cx = txt_num(24, 0, g_save.card_count, COL_YELLOW, COL_DBLUE);
    cx = txt_draw(cx, 0, "/", COL_LGRAY, COL_DBLUE);
    txt_num(cx, 0, CARD_MAX, COL_LGRAY, COL_DBLUE);
    grid_draw();
    ui_guide(T_GUIDE_CARD);
}

FAR(void, cardlist_update, (void))
{
    u8 p = input_pressed();
    grid_input(p);
    if ((p & BTN_OK) && g_list.count) { g_sel_card = g_list.cursor; screen_change(&scr_carddetail); }
    if (p & BTN_CANCEL) { g_list.cursor = 0; screen_change(&scr_home); }
}

SCREEN(scr_cardlist, cardlist_init, cardlist_update, cardlist_draw, 0)

/* ---- 詳細 ---- */

static u8 stat(u8 y, u16 label, u32 v, u32 max, u8 col)
{
    u8 cx = txt_id(16, y, label, COL_LGRAY, COL_BLACK);
    txt_num_r(cx, y, v, 6, COL_WHITE, COL_BLACK);
    if (max) gfx_gauge(128, (u8)(y + 17), 120, (u16)(v > max ? max / 16 : v / 16), (u16)(max / 16), col);
    return cx;
}

FAR(void, carddetail_draw, (void))
{
    CardRec c;
    const OwnedCard *o = &g_save.cards[g_sel_card];
    u8 cx;
    tbl_card(o->id, &c);
    ui_clear();
    ui_header(T_CARD_DETAIL);
    gfx_card2x(o->id, 4, 20);
    cx = ui_rarity(16, 20, c.rarity, COL_BLACK);
    txt_id((u8)(cx + 1), 20, (u16)(T_ELEM_0 + c.element), elem_color(c.element), COL_BLACK);
    txt_id(16, 38, c.name_text, COL_WHITE, COL_BLACK);
    cx = txt_id(16, 60, T_LV, COL_LGRAY, COL_BLACK);
    cx = txt_num(cx, 60, o->lv, COL_YELLOW, COL_BLACK);
    cx = txt_draw(cx, 60, "/", COL_LGRAY, COL_BLACK);
    txt_num(cx, 60, c.max_lv, COL_LGRAY, COL_BLACK);
    stat(80, T_ATK, card_atk_of(g_sel_card), 9000, COL_RED);
    stat(104, T_DEF, card_def_of(g_sel_card), 9000, COL_BLUE);
    stat(128, T_COST, c.cost, 0, 0);
    cx = txt_id(16, 148, T_SKILL, COL_LGRAY, COL_BLACK);
    txt_id(16, 166, (u16)(T_SKILL_0 + c.skill), COL_YELLOW, COL_BLACK);
    if (inv_in_deck(g_sel_card)) gfx_sprite(ATL_SPARK, 104, 22);
    ui_guide(T_CARD_ACTIONS);
}

FAR(void, carddetail_update, (void))
{
    u8 d = input_digit();
    if (d == 1) { screen_change(&scr_fuse); return; }
    if (d == 2) { screen_change(&scr_evolve); return; }
    if (input_pressed() & (BTN_CANCEL | BTN_OK)) screen_change(&scr_cardlist);
}

SCREEN(scr_carddetail, 0, carddetail_update, carddetail_draw, 0)
