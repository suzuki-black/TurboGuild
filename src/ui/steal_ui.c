/* SPDX-License-Identifier: MIT */
/* steal_ui.c — 盗みに行く画面（core/steal.h, DECISIONS #90）。マイページの [0] から。
 * 上に相手（取り返せる人が先、続いて格下・同格・格上）、下の箱に選んでいる相手の持っているカードと総戦力。
 * 1〜6 か決定で AP を払って盗みに行く。相手の一覧は覚えずに、描くときと押したときに作り直す（日付と回数で決まる）。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../config.h"
#include "../core/player.h"
#include "../core/save.h"
#include "../core/steal.h"
#include "../core/tables.h"
#include "../data/screens.h"
#include "../data/tables.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define ROW_Y  20
#define ROW_H  18
#define BOX_Y  130

static u8 rows;                  /* いま並んでいる相手の数 */

static u8 npc_name(u8 cx, u8 y, u16 npc, u8 fg, u8 bg)
{
    NpcRec n;
    tbl_npc(npc, &n);
    cx = txt_id(cx, y, (u16)(NPC_PRE_TEXT + n.name_id / NPC_SUF_COUNT), fg, bg);
    return txt_id(cx, y, (u16)(NPC_SUF_TEXT + n.name_id % NPC_SUF_COUNT), fg, bg);
}

static u8 card_name(u8 cx, u8 y, u16 card, u8 bg)
{
    CardRec c;
    tbl_card(card, &c);
    cx = ui_rarity(cx, y, c.rarity, bg);
    return txt_id((u8)(cx + 1), y, c.name_text, COL_WHITE, bg);
}

static void row(const StealList *l, u8 k)
{
    const StealRow *r = &l->r[k];
    u8 y = (u8)(ROW_Y + k * ROW_H), on = (u8)(k == g_cursor), bg = on ? COL_DBLUE : COL_BLACK, cx;
    CardRec c;
    vdp_fill(0, y, SCREEN_W, ROW_H - 1, bg);
    ui_key((u8)(k + 1), 4, (u8)(y + 2));
    npc_name(3, (u8)(y + 1), r->npc, r->back ? COL_ORANGE : COL_CYAN, bg);
    if (r->back) txt_id(18, (u8)(y + 1), T_STEAL_BACK, COL_ORANGE, bg);
    else { tbl_card(r->card, &c); ui_rarity(18, (u8)(y + 1), c.rarity, bg); }
    cx = txt_num(27, (u8)(y + 1), r->pct, on ? COL_YELLOW : COL_WHITE, bg);
    txt_draw(cx, (u8)(y + 1), "%", COL_LGRAY, bg);
}

static void detail(const StealList *l)
{
    const StealRow *r = &l->r[g_cursor];
    u8 cx;
    ui_box(8, BOX_Y, 240, 62, COL_DBLUE, COL_DGRAY);
    cx = card_name(2, (u8)(BOX_Y + 4), r->card, COL_DBLUE);
    if (r->back) {
        cx = txt_draw((u8)(cx + 1), (u8)(BOX_Y + 4), "Lv", COL_LGRAY, COL_DBLUE);
        txt_num(cx, (u8)(BOX_Y + 4), r->lv, COL_WHITE, COL_DBLUE);
    }
    cx = txt_id(2, (u8)(BOX_Y + 22), T_STEAL_THEM, COL_LGRAY, COL_DBLUE);
    txt_num((u8)(cx + 1), (u8)(BOX_Y + 22), r->power, r->power > l->me ? COL_ORANGE : COL_WHITE, COL_DBLUE);
    cx = txt_id(2, (u8)(BOX_Y + 40), T_STEAL_ME, COL_LGRAY, COL_DBLUE);
    cx = txt_num((u8)(cx + 1), (u8)(BOX_Y + 40), l->me, COL_WHITE, COL_DBLUE);
    cx = txt_draw((u8)(cx + 2), (u8)(BOX_Y + 40), "AP", COL_LGRAY, COL_DBLUE);
    txt_num((u8)(cx + 1), (u8)(BOX_Y + 40), STEAL_AP, COL_YELLOW, COL_DBLUE);
}

static void header(void)
{
    u8 cx;
    player_ap_update();
    ui_header(T_STEAL_TITLE);
    vdp_fill(176, 0, 80, 16, COL_DBLUE);
    cx = txt_draw(22, 0, "AP", COL_LGRAY, COL_DBLUE);
    cx = txt_num((u8)(cx + 1), 0, g_save.ap, COL_YELLOW, COL_DBLUE);
    cx = txt_draw(cx, 0, "/", COL_LGRAY, COL_DBLUE);
    txt_num(cx, 0, AP_MAX, COL_LGRAY, COL_DBLUE);
}

FAR(void, steal_init, (void))
{
    g_cursor = 0;
}

FAR(void, steal_draw, (void))
{
    StealList l;
    u8 k;
    steal_list(&l);
    rows = l.n;
    if (g_cursor >= rows) g_cursor = 0;
    ui_clear();
    header();
    if (!rows) txt_id(2, 40, T_STEAL_NONE, COL_LGRAY, COL_BLACK);
    for (k = 0; k < rows; k++) row(&l, k);
    if (rows) detail(&l);
    ui_guide(T_STEAL_GUIDE);
}

/* 盗みに行った結果を下の箱に出して押下を待つ */
static void go(u8 k)
{
    StealList l;
    u8 st, cx;
    steal_list(&l);
    if (k >= l.n) return;
    g_cursor = k;
    st = steal_try(&l, k);
    ui_box(8, BOX_Y, 240, 62, COL_DBLUE, st == STEAL_OK ? COL_YELLOW : COL_RED);
    if (st == STEAL_OK) {
        if (l.r[k].back) cx = txt_id(2, (u8)(BOX_Y + 4), T_STEAL_GOTBACK, COL_YELLOW, COL_DBLUE);
        else {
            cx = npc_name(2, (u8)(BOX_Y + 4), l.r[k].npc, COL_CYAN, COL_DBLUE);
            txt_id(cx, (u8)(BOX_Y + 4), T_STEAL_FROM, COL_YELLOW, COL_DBLUE);
        }
        card_name(2, (u8)(BOX_Y + 24), l.r[k].card, COL_DBLUE);
        psg_beep(0x060, 12);
    } else {
        txt_id(2, (u8)(BOX_Y + 4), st == STEAL_NO_AP ? T_STEAL_NOAP : st == STEAL_FULL ? T_STEAL_FULL : T_STEAL_FAIL,
               COL_WHITE, COL_DBLUE);
        psg_beep(0x400, 10);
    }
    if (st == STEAL_OK || st == STEAL_FAIL) {        /* 相手のひとこと: 取り返された泥棒の捨て台詞・盗まれた人・見つけた人 */
        u16 t = st == STEAL_FAIL ? T_TH_CAUGHT0 : l.r[k].back ? T_TH_LOSE0 : T_TH_VICTIM0;
        cx = npc_name(2, (u8)(BOX_Y + 42), l.r[k].npc, COL_LGRAY, COL_DBLUE);
        cx = txt_id(cx, (u8)(BOX_Y + 42), T_BOARD_SAY, COL_LGRAY, COL_DBLUE);
        txt_id(cx, (u8)(BOX_Y + 42), (u16)(t + (l.r[k].npc + g_save.steal_seq) % 3), COL_WHITE, COL_DBLUE);
    }
    if (st == STEAL_OK || st == STEAL_FAIL) g_save_dirty = 1;
    ui_wait_key();
    g_dirty = 1;
}

FAR(void, steal_update, (void))
{
    u8 p = input_pressed();
    u8 d = input_digit();
    if (rows && ui_menu_input(&g_cursor, rows, p)) { g_dirty = 1; return; }
    if (d >= 1 && d <= rows) { go((u8)(d - 1)); return; }
    if ((p & BTN_OK) && rows) { go(g_cursor); return; }
    if (p & BTN_CANCEL) {
        if (g_save_dirty && !g_nosave_warned) ui_saving();
        screen_change(&scr_home);
    }
}

SCREEN(scr_steal, steal_init, steal_update, steal_draw, 0)
