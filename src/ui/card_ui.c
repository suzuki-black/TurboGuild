/* SPDX-License-Identifier: MIT */
/* card_ui.c — カード一覧（絵のグリッド。下に選んだカードの名前と攻撃/防御）と、
 * 詳細（絵を 2 倍で大きく、右に数値）。★ はデッキに入っているカード。
 * 並べ方は [0] で 入手順 → レア度 → 攻撃 → コスト と替わる（g_order。デッキ・合成の一覧も同じ順）。 */
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
#define GRID_X  8           /* マスの左端は 8 ドット刻み（文字の桁とそろえる） */
#define CELL_W  40
#define CELL_H  52
#define INFO_Y  176

#define g_with_remove g_grid_remove
#define SORT_KINDS 4

/* 並べ替えの鍵（大きいほど前）。レア度は同じなら攻撃の高い順、コストは安い順 */
static u16 sort_key(u8 i)
{
    CardRec c;
    u32 atk;
    tbl_card(g_save.cards[i].id, &c);
    atk = card_atk_of(i);
    switch (g_card_sort) {
    case 1:  return (u16)(((u16)c.rarity << 13) | (atk >> 3 > 8191 ? 8191 : atk >> 3));
    case 2:  return (u16)(atk > 65535 ? 65535 : atk);
    default: return (u16)(((u16)(63 - c.cost) << 10) | (atk >> 6 > 1023 ? 1023 : atk >> 6));
    }
}

/* g_order を作り直す（並べ方か所持カードが変わったら）。挿入ソートで同じ鍵は入手順のまま */
FAR(void, card_sort, (void))
{
    u16 key[CARD_MAX];
    u8 n = (u8)g_save.card_count, i, j;
    for (i = 0; i < n; i++) g_order[i] = i;
    if (!g_card_sort) return;
    for (i = 0; i < n; i++) key[i] = sort_key(i);
    for (i = 1; i < n; i++) {
        u16 k = key[i];
        u8 o = g_order[i];
        for (j = i; j > 0 && key[j - 1] < k; j--) { key[j] = key[j - 1]; g_order[j] = g_order[j - 1]; }
        key[j] = k;
        g_order[j] = o;
    }
}

/* 案内の行の右に、いまの並べ方 */
void grid_sort_label(void)
{
    vdp_fill(176, ROW_LAST, 80, 16, COL_DGRAY);
    txt_id(23, ROW_LAST, (u16)(T_SORT0 + g_card_sort), COL_YELLOW, COL_DGRAY);
}

void grid_setup(u8 with_remove)
{
    card_sort();
    g_with_remove = with_remove;
    g_list.count = g_save.card_count + with_remove;
    g_list.rows = GRID_PAGE;
    if (g_list.cursor >= g_list.count) g_list.cursor = 0;
    g_list.top = (u16)(g_list.cursor / GRID_PAGE * GRID_PAGE);
}

s16 grid_card(void)
{
    s16 k = (s16)g_list.cursor - g_with_remove;
    return k < 0 ? -1 : (s16)g_order[k];
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
    g_txt_maxcol = 20;                            /* 名前が長くても攻撃の欄に食い込まない */
    txt_id((u8)(cx + 1), INFO_Y, c.name_text, COL_WHITE, COL_DBLUE);
    g_txt_maxcol = 0;
    cx = txt_id(21, INFO_Y, T_ATK, COL_LGRAY, COL_DBLUE);
    txt_num_r(cx, INFO_Y, card_atk_of((u16)i), 6, COL_YELLOW, COL_DBLUE);
}

static void cell(u16 n)
{
    u16 k = n - g_list.top;
    u8 x = (u8)(GRID_X + (k % GRID_COLS) * CELL_W), y = (u8)(GRID_Y + (k / GRID_COLS) * CELL_H);
    u8 on = (u8)(n == g_list.cursor), lv;
    s16 i = (s16)n - g_with_remove;
    if (i >= 0 && n < g_list.count) i = g_order[i];
    vdp_fill(x, y, CELL_W - 2, CELL_H - 2, on ? COL_YELLOW : COL_BLACK);
    vdp_fill((u16)x + 2, (u16)y + 2, CELL_W - 6, CELL_H - 6, on ? COL_BLUE : COL_DBLUE);
    if (n >= g_list.count) { vdp_fill(x, y, CELL_W - 2, CELL_H - 2, COL_BLACK); return; }
    if (i < 0) {                                  /* 「はずす」は裏面だけ（名前は下の行に出る） */
        gfx_part(ATL_CARDBACK_S, (u8)(x + 5), (u8)(y + 7));
        return;
    }
    gfx_thumb(g_save.cards[i].id, (u8)(x + 5), (u8)(y + 7));
    if (inv_in_deck((u16)i)) gfx_sprite(ATL_SPARK, (u8)(x + 26), (u8)(y + 3));
    lv = g_save.cards[i].lv;                      /* Lv は絵の左下に小さな数字 */
    vdp_fill((u16)x + 5, (u16)y + 32, lv >= 10 ? 16 : 9, 11, COL_BLACK);
    gfx_num((u8)(x + 6), (u8)(y + 32), lv);
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
    if (input_digit() == 0) {                    /* [0] 並べ方を替える */
        g_card_sort = (u8)((g_card_sort + 1) % SORT_KINDS);
        card_sort();
        g_list.cursor = 0;
        g_list.top = 0;
        psg_beep(0x0A0, 3);
        grid_draw();
        grid_sort_label();
        return 1;
    }
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
    grid_sort_label();
}

FAR(void, cardlist_update, (void))
{
    u8 p = input_pressed();
    grid_input(p);
    if ((p & BTN_OK) && g_list.count) { g_sel_card = (u16)grid_card(); screen_change(&scr_carddetail); }
    if (p & BTN_CANCEL) { g_list.cursor = 0; screen_change(g_viewonly ? &scr_title : &scr_home); }
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
    u8 d = g_viewonly ? NO_DIGIT : input_digit();      /* 思い出を見るときは合成させない */
    if (d == 1) { screen_change(&scr_fuse); return; }
    if (d == 2) { screen_change(&scr_evolve); return; }
    if (input_pressed() & (BTN_CANCEL | BTN_OK)) screen_change(&scr_cardlist);
}

SCREEN(scr_carddetail, 0, carddetail_update, carddetail_draw, 0)
