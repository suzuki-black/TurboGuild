/* SPDX-License-Identifier: MIT */
/* trade_ui.c — トレード掲示板（DECISIONS #115）。ギルド伝言板の 2 から。
 * 左右か 1・2 で「売ります」（NPC の出品を石で買う）と「買います」（求められたカードを手放す）を替える。
 * 中身は core/trade.c が毎回決める。画面の状態は g_cursor（= タブ × TRADE_ROWS + 行）だけ。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../config.h"
#include "../core/inventory.h"
#include "../core/save.h"
#include "../core/tables.h"
#include "../core/time.h"
#include "../core/trade.h"
#include "../data/cards.h"
#include "../data/screens.h"
#include "../data/tables.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define TR_Y  32
#define TR_H  32

static const u8 rar_col[RARITIES] = { COL_LGRAY, COL_WHITE, COL_CYAN, COL_YELLOW, COL_ORANGE, COL_RED };

static u8 name_at(u8 cx, u8 y, u16 npc, u8 fg, u8 bg)
{
    NpcRec n;
    tbl_npc(npc, &n);
    cx = txt_id(cx, y, (u16)(NPC_PRE_TEXT + n.name_id / NPC_SUF_COUNT), fg, bg);
    return txt_id(cx, y, (u16)(NPC_SUF_TEXT + n.name_id % NPC_SUF_COUNT), fg, bg);
}

/* 手放せる 1 枚（デッキに入っていない、いちばん Lv の低いもの）。無ければ INV_FULL */
static u16 spare_of(u16 card)
{
    u16 i, best = INV_FULL;
    for (i = 0; i < g_save.card_count && i < CARD_MAX; i++)
        if (g_save.cards[i].id == card && !inv_in_deck(i) && (best == INV_FULL || g_save.cards[i].lv < g_save.cards[best].lv)) best = i;
    return best;
}

static void gems(void)
{
    u8 cx;
    vdp_fill(184, 0, SCREEN_W - 184, 15, COL_DBLUE);           /* 題（最長 21 桁）の右に、持っている石 */
    cx = txt_id(23, 0, T_GEMS, COL_LGRAY, COL_DBLUE);
    txt_num(cx, 0, g_save.gems, COL_WHITE, COL_DBLUE);
}

static void row(u8 k)
{
    TradeRow o;
    CardRec c;
    u8 tab = (u8)(g_cursor / TRADE_ROWS), y = (u8)(TR_Y + k * TR_H), on = (u8)(g_cursor % TRADE_ROWS == k), cx;
    u8 bg = on ? COL_BLUE : (k & 1 ? COL_BLACK : COL_DGRAY);
    vdp_fill(0, y, SCREEN_W, TR_H, bg);
    if (!trade_row(TRADE_TK(tab, k), &o)) return;
    tbl_card(o.card, &c);
    cx = txt_id(1, y, (u16)(T_RAR_0 + o.rarity), rar_col[o.rarity], bg);
    txt_id((u8)(cx + 1), y, c.name_text, COL_WHITE, bg);
    cx = txt_id(22, y, T_GEMS, COL_LGRAY, bg);
    txt_num_r(cx, y, o.price, (u8)(31 - cx), o.cheap ? COL_ORANGE : COL_YELLOW, bg);
    cx = name_at(3, (u8)(y + 15), o.npc, COL_LGRAY, bg);
    txt_id(cx, (u8)(y + 15), tab == TRADE_SELL ? T_TR_BY : T_TR_WANTS, COL_LGRAY, bg);
    if (tab == TRADE_SELL && o.cheap) txt_id(25, (u8)(y + 15), T_TR_CHEAP, COL_ORANGE, bg);
    if (tab == TRADE_WANT && spare_of(o.card) != INV_FULL) txt_id(25, (u8)(y + 15), T_TR_HAVE, COL_CYAN, bg);
}

static void rows(void)
{
    u8 k;
    vdp_fill(0, 16, SCREEN_W, 16, COL_BLACK);
    if (g_cursor < TRADE_ROWS) txt_id(1, 16, time_gday(g_save.g_time) < TRADE_AUDIT_DAY ? T_TR_WARN : T_TR_AUDIT, COL_ORANGE, COL_BLACK);
    else txt_id(1, 16, T_TR_WANTHEAD, COL_LGRAY, COL_BLACK);
    for (k = 0; k < TRADE_ROWS; k++) row(k);
}

FAR(void, trade_init, (void)) { g_cursor = 0; }

FAR(void, trade_draw, (void))
{
    TradeRow o;
    ui_clear();
    ui_header(g_cursor < TRADE_ROWS ? T_TR_TITLE0 : T_TR_TITLE1);
    gems();
    ui_guide(T_TR_GUIDE);
    if (!trade_row(0, &o)) { txt_id(2, 40, T_TR_CLOSED, COL_LGRAY, COL_BLACK); return; }   /* サービス終了後 */
    rows();
}

static void deal(void)
{
    TradeRow o;
    u8 tab = (u8)(g_cursor / TRADE_ROWS);
    u16 i;
    if (!trade_row(TRADE_TK(tab, g_cursor % TRADE_ROWS), &o)) return;
    if (tab == TRADE_SELL) {
        if (g_save.gems < o.price) { ui_msg(T_GACHA_NO_MONEY); return; }
        if (g_save.card_count >= CARD_MAX) { ui_msg(T_TR_FULL); return; }
        if (!ui_confirm(T_TR_BUYQ)) return;
        g_save.gems -= o.price;
        trade_next();
        g_save_dirty = 1;
        if (o.scam) { psg_beep(0x300, 12); ui_msg(T_TR_SCAM); return; }   /* 払ったのに届かない */
        inv_add(o.card);
        psg_beep(0x080, 8);
        ui_msg(T_TR_GOT);
    } else {
        i = spare_of(o.card);
        if (i == INV_FULL) { ui_msg(T_TR_NOHAVE); return; }
        if (!ui_confirm(T_TR_SELLQ)) return;
        inv_remove(i);
        g_save.gems += o.price;
        trade_next();
        g_save_dirty = 1;
        psg_beep(0x080, 8);
        ui_msg(T_TR_SOLD);
    }
}

FAR(void, trade_update, (void))
{
    u8 p = input_pressed(), d = input_digit(), old = g_cursor, k = (u8)(g_cursor % TRADE_ROWS), tab = (u8)(g_cursor / TRADE_ROWS);
    TradeRow o;
    g_ui_state = UIS_WAITKEY;
    if (p & BTN_CANCEL) { psg_beep(0x200, 4); screen_change(&scr_board); return; }
    if (!trade_row(0, &o)) return;
    if ((p & BTN_LEFT) || d == 1) tab = 0;
    if ((p & BTN_RIGHT) || d == 2) tab = 1;
    if ((p & BTN_UP) && k) k--;
    if ((p & BTN_DOWN) && k < TRADE_ROWS - 1) k++;
    g_cursor = (u8)(tab * TRADE_ROWS + k);
    if (g_cursor / TRADE_ROWS != old / TRADE_ROWS) { psg_beep(0x100, 2); g_dirty = 1; return; }   /* タブを替えた */
    if (g_cursor != old) { psg_beep(0x100, 2); row((u8)(old % TRADE_ROWS)); row(k); return; }
    if (p & BTN_OK) { deal(); g_dirty = 1; }
}

SCREEN(scr_trade, trade_init, trade_update, trade_draw, 0)
