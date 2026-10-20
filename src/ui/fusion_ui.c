/* SPDX-License-Identifier: MIT */
/* fusion_ui.c — 強化合成（素材をグリッドで選ぶ, 1 で「N を全部」）と進化合成（同じカードをもう 1 枚）。
 * 合成は素材が吸い込まれて光り、経験値ゲージが伸びてレベルが上がる。進化は 2 枚が重なって白く光り、
 * 進化後の絵を 2 倍で見せる（当時の Flash の強化演出のつもり）。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../core/fusion.h"
#include "../core/inventory.h"
#include "../core/tables.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/gfx.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define COLS    6
#define ROWS    2
#define PAGE    (COLS * ROWS)
#define GRID_Y  66
#define CELL_W  42
#define CELL_H  48
#define INFO_Y  164

/* ---- 共通: 素材の一覧（所持カード全部。ベースとデッキは選べない） ---- */

static u8 usable(u16 i)
{
    return (u8)(i != g_fuse.base && !inv_in_deck(i));
}

static void cell(u16 i)
{
    u16 k = i - g_list.top;
    u8 x = (u8)((k % COLS) * CELL_W + 2), y = (u8)(GRID_Y + (k / COLS) * CELL_H);
    u8 on = (u8)(i == g_list.cursor), sel;
    vdp_fill(x, y, CELL_W - 2, CELL_H - 2, COL_BLACK);
    if (i >= g_list.count) return;
    sel = (u8)(SEL_GET(g_fuse.sel, i) != 0);
    vdp_fill(x, y, CELL_W - 2, CELL_H - 2, on ? COL_YELLOW : (sel ? COL_ORANGE : COL_DGRAY));
    vdp_fill((u16)x + 2, (u16)y + 2, CELL_W - 6, CELL_H - 6, sel ? COL_DRED : COL_DBLUE);
    gfx_thumb(g_save.cards[i].id, (u8)(x + 6), (u8)(y + 3));
    if (!usable(i)) vdp_fill((u16)x + 6, (u16)y + 30, 28, 9, COL_DGRAY);
    if (sel) gfx_sprite(ATL_STAMP, (u8)(x + 8), (u8)(y + 10));
}

static void grid(void)
{
    u16 i;
    for (i = g_list.top; i < g_list.top + PAGE; i++) cell(i);
}

static u8 grid_move(u8 p)
{
    u16 old = g_list.cursor, top = g_list.top;
    if ((p & BTN_LEFT) && g_list.cursor > 0) g_list.cursor--;
    if ((p & BTN_RIGHT) && g_list.cursor + 1 < g_list.count) g_list.cursor++;
    if ((p & BTN_UP) && g_list.cursor >= COLS) g_list.cursor -= COLS;
    if ((p & BTN_DOWN) && g_list.cursor + COLS < g_list.count) g_list.cursor += COLS;
    if (g_list.cursor == old) return 0;
    psg_beep(0x100, 2);
    g_list.top = (u16)(g_list.cursor / PAGE * PAGE);
    if (g_list.top != top) grid();
    else { cell(old); cell(g_list.cursor); }
    return 1;
}

static void base_panel(void)
{
    CardRec c;
    const OwnedCard *o = &g_save.cards[g_fuse.base];
    u8 cx;
    tbl_card(o->id, &c);
    vdp_fill(0, 16, SCREEN_W, 48, COL_BLACK);
    gfx_thumb(o->id, 4, 20);
    cx = ui_rarity(5, 18, c.rarity, COL_BLACK);
    txt_id((u8)(cx + 1), 18, c.name_text, COL_WHITE, COL_BLACK);
    cx = txt_id(5, 36, T_LV, COL_LGRAY, COL_BLACK);
    cx = txt_num(cx, 36, o->lv, COL_YELLOW, COL_BLACK);
    cx = txt_draw(cx, 36, "/", COL_LGRAY, COL_BLACK);
    txt_num(cx, 36, c.max_lv, COL_LGRAY, COL_BLACK);
    gfx_gauge(128, 41, 120, o->exp, fusion_need(g_fuse.base), COL_CYAN);
}

static void info(void)
{
    u8 cx;
    fusion_preview(&g_fuse);
    vdp_fill(0, INFO_Y, SCREEN_W, 28, COL_DBLUE);
    cx = txt_id(0, INFO_Y, T_FUSE_COUNT, COL_LGRAY, COL_DBLUE);
    cx = txt_num(cx, INFO_Y, g_fuse.count, COL_WHITE, COL_DBLUE);
    cx = txt_id((u8)(cx + 1), INFO_Y, T_FUSE_GAIN, COL_LGRAY, COL_DBLUE);
    cx = txt_draw(cx, INFO_Y, "+", COL_LGRAY, COL_DBLUE);
    cx = txt_num(cx, INFO_Y, g_fuse.gain, COL_CYAN, COL_DBLUE);
    cx = txt_id((u8)(cx + 1), INFO_Y, T_GOLD, COL_YELLOW, COL_DBLUE);
    cx = txt_draw(cx, INFO_Y, "-", COL_LGRAY, COL_DBLUE);
    txt_num(cx, INFO_Y, g_fuse.gold, COL_WHITE, COL_DBLUE);
    if (g_fuse.count) {
        cx = txt_id(0, (u8)(INFO_Y + 12), T_LV, COL_LGRAY, COL_DBLUE);
        cx = txt_num(cx, (u8)(INFO_Y + 12), g_fuse.lv_before, COL_WHITE, COL_DBLUE);
        cx = txt_draw(cx, (u8)(INFO_Y + 12), "->", COL_LGRAY, COL_DBLUE);
        txt_num(cx, (u8)(INFO_Y + 12), g_fuse.lv_after, COL_YELLOW, COL_DBLUE);
    }
}

static void clear_sel(void)
{
    u8 i;
    for (i = 0; i < SEL_BYTES; i++) g_fuse.sel[i] = 0;
}

/* ================================================================ 強化合成 */

FAR(void, fuse_init, (void))
{
    g_fuse.base = g_sel_card;
    clear_sel();
    g_list.count = g_save.card_count;
    g_list.cursor = 0;
    g_list.top = 0;
}

FAR(void, fuse_draw, (void))
{
    ui_clear();
    ui_header(T_FUSE_TITLE);
    base_panel();
    grid();
    info();
    ui_guide(T_FUSE_GUIDE);
}

/* 選んだ素材（最大 6 枚）がベースへ吸い込まれ、光ってレベルが上がる */
static void fuse_anim(u8 lv_before)
{
    CardRec c;
    u16 i;
    u8 n = 0, k;
    u8 xs[6];
    vdp_fill(0, 16, SCREEN_W, 176, COL_BLACK);
    gfx_card(g_save.cards[g_fuse.base].id, 100, 100);
    for (i = 0; i < g_save.card_count && n < 6; i++) {
        if (!SEL_GET(g_fuse.sel, i)) continue;
        xs[n] = (u8)(8 + n * 40);
        gfx_thumb(g_save.cards[i].id, xs[n], 24);
        n++;
    }
    ui_wait(20);
    for (k = 1; k <= 4; k++) {                      /* 素材がベースへ寄っていく */
        u8 j;
        vdp_fill(0, 16, SCREEN_W, 84, COL_BLACK);
        for (j = 0; j < n; j++) {
            u8 x = (u8)(xs[j] + ((114 - xs[j]) * k) / 5);
            gfx_part(ATL_CARDBACK_S, x, (u8)(24 + k * 14));
        }
        psg_beep((u16)(0x200 - k * 0x30), 3);
        ui_wait(5);
    }
    vdp_fill(0, 16, SCREEN_W, 84, COL_BLACK);
    gfx_pal(COL_BLACK, 7, 7, 7);
    psg_beep(0x040, 12);
    ui_wait(4);
    gfx_pal_reset();
    fusion_apply(&g_fuse);
    g_save_dirty = 1;
    tbl_card(g_save.cards[g_fuse.base].id, &c);
    gfx_card(g_save.cards[g_fuse.base].id, 100, 100);
    txt_id(10, 40, T_FUSE_DONE, COL_YELLOW, COL_BLACK);
    k = txt_id(9, 60, T_LV, COL_LGRAY, COL_BLACK);
    k = txt_num(k, 60, lv_before, COL_WHITE, COL_BLACK);
    k = txt_draw(k, 60, "->", COL_LGRAY, COL_BLACK);
    txt_num(k, 60, g_save.cards[g_fuse.base].lv, COL_YELLOW, COL_BLACK);
    for (k = 0; k <= 8; k++) {                      /* 経験値ゲージが伸びる */
        gfx_gauge(64, 82, 128, (u16)(g_save.cards[g_fuse.base].exp * k / 8), fusion_need(g_fuse.base), COL_CYAN);
        ui_wait(2);
    }
    if (g_save.cards[g_fuse.base].lv > lv_before) {
        gfx_sprite(ATL_SPARK, 96, 98);
        gfx_sprite(ATL_SPARK, 150, 160);
        psg_beep(0x050, 15);
    }
    ui_wait_key();
    g_sel_card = g_fuse.base;
    screen_change(&scr_carddetail);
}

static void try_fuse(void)
{
    static const u16 errs[6] = { 0, T_NO_GOLD, T_FUSE_NONE, T_FUSE_BAD, T_FUSE_MAX, 0 };
    fusion_preview(&g_fuse);
    if (g_fuse.status != F_OK) { ui_msg(errs[g_fuse.status]); return; }
    if (!ui_confirm(T_FUSE_CONFIRM)) return;
    fuse_anim(g_fuse.lv_before);
}

FAR(void, fuse_update, (void))
{
    u8 p = input_pressed();
    u8 d = input_digit();
    grid_move(p);
    if (p & BTN_OK) {
        u16 i = g_list.cursor;
        if (usable(i)) {
            if (SEL_GET(g_fuse.sel, i)) SEL_CLR(g_fuse.sel, i); else SEL_SET(g_fuse.sel, i);
            psg_beep(0x0C0, 3);
            cell(i);
            info();
        }
    }
    if (d == 1) { fusion_select_all_n(&g_fuse); psg_beep(0x080, 6); grid(); info(); }
    if (d == 2) try_fuse();
    if (p & BTN_CANCEL) screen_change(&scr_carddetail);
}

SCREEN(scr_fuse, fuse_init, fuse_update, fuse_draw, 0)

/* ================================================================ 進化合成 */

FAR(void, evo_init, (void))
{
    g_fuse.base = g_sel_card;
    clear_sel();
    g_list.count = g_save.card_count;
    g_list.cursor = 0;
    g_list.top = 0;
    while (g_list.cursor < g_list.count && !evolve_partner_ok(g_fuse.base, g_list.cursor)) g_list.cursor++;
}

FAR(void, evo_draw, (void))
{
    ui_clear();
    ui_header(T_EVO_TITLE);
    base_panel();
    if (g_list.cursor >= g_list.count) {
        txt_id(2, 100, T_EVO_NONE, COL_LGRAY, COL_BLACK);
    } else {
        g_list.top = (u16)(g_list.cursor / PAGE * PAGE);
        grid();
    }
    ui_guide(T_GUIDE_LIST);
}

static void evo_anim(u16 partner)
{
    u16 id = g_save.cards[g_fuse.base].id;
    u8 k;
    vdp_fill(0, 16, SCREEN_W, 176, COL_BLACK);
    for (k = 0; k <= 6; k++) {                      /* 2 枚が真ん中へ寄る */
        vdp_fill(0, 40, SCREEN_W, 76, COL_BLACK);
        gfx_card(id, (u8)(10 + k * 10), 40);
        gfx_card(id, (u8)(190 - k * 10), 40);
        psg_beep((u16)(0x180 - k * 0x20), 3);
        ui_wait(4);
    }
    for (k = 0; k < 3; k++) {
        gfx_pal(COL_BLACK, 7, 7, 7);
        ui_wait(3);
        gfx_pal_reset();
        ui_wait(3);
    }
    evolve(g_fuse.base, partner);
    if (partner < g_fuse.base) g_fuse.base--;
    g_save_dirty = 1;
    vdp_fill(0, 16, SCREEN_W, 176, COL_BLACK);
    gfx_card2x(g_save.cards[g_fuse.base].id, 72, 20);
    txt_id(12, 172, T_EVO_DONE, COL_YELLOW, COL_BLACK);
    psg_beep(0x030, 20);
    ui_wait_key();
    g_sel_card = g_fuse.base;
    screen_change(&scr_carddetail);
}

FAR(void, evo_update, (void))
{
    u8 p = input_pressed();
    if (g_list.cursor < g_list.count) grid_move(p);
    if ((p & BTN_OK) && g_list.cursor < g_list.count) {
        if (!evolve_partner_ok(g_fuse.base, g_list.cursor)) { psg_beep(0x300, 4); return; }
        if (g_save.gold < 100) { ui_msg(T_NO_GOLD); return; }
        if (!ui_confirm(T_EVO_CONFIRM)) return;
        evo_anim(g_list.cursor);
        return;
    }
    if (p & BTN_CANCEL) screen_change(&scr_carddetail);
}

SCREEN(scr_evolve, evo_init, evo_update, evo_draw, 0)
