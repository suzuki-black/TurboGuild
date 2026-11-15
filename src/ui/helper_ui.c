/* SPDX-License-Identifier: MIT */
/* helper_ui.c — 助っ人を選ぶ画面と「助けに来た」（DECISIONS #106）。探索の画面とイベントのトップの 7 から。
 * 借りるのは次の 1 戦だけ（g_help）。同じ人からは 1 日 1 回（g_save.help_bits）。去った人からは借りられない。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../core/board.h"
#include "../core/tables.h"
#include "../data/tables.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define HP_Y 40
#define HP_H 16

static u8 name_at(u8 cx, u8 y, u16 npc, u8 fg, u8 bg)
{
    NpcRec n;
    tbl_npc(npc, &n);
    cx = txt_id(cx, y, (u16)(NPC_PRE_TEXT + n.name_id / NPC_SUF_COUNT), fg, bg);
    return txt_id(cx, y, (u16)(NPC_SUF_TEXT + n.name_id % NPC_SUF_COUNT), fg, bg);
}

/* カード名と Lv（cx から） */
static void card_at(u8 cx, u8 y, const MateLead *m, u8 bg)
{
    CardRec c;
    tbl_card(m->card, &c);
    cx = txt_id(cx, y, c.name_text, rarity_color(c.rarity), bg);
    cx = txt_id((u8)(cx + 1), y, T_RS_LV, COL_LGRAY, bg);
    txt_num(cx, y, m->lv, COL_WHITE, bg);
}

static void row(const RosterRow *o, u8 i, u8 cur, u16 enemy)
{
    MateLead m;
    CardRec c;
    u8 y = (u8)(HP_Y + i * HP_H), bg = i == cur ? COL_DBLUE : (i & 1 ? COL_BLACK : COL_DGRAY);
    mate_lead(o, &m);
    vdp_fill(0, y, SCREEN_W, HP_H, bg);
    if (g_help == o->k + 1) txt_id(0, y, T_HP_MARK, COL_YELLOW, bg);
    name_at(2, y, o->npc, m.used ? COL_LGRAY : COL_WHITE, bg);
    if (!m.card) return;
    tbl_card(m.card, &c);
    ui_rarity(14, y, c.rarity, bg);
    txt_num_r(19, y, help_atk(o->npc, enemy), 7, m.used ? COL_LGRAY : COL_WHITE, bg);
    if (m.used) txt_id(27, y, T_HP_USED, COL_ORANGE, bg);
    if (i == cur) {
        vdp_fill(0, 172, SCREEN_W, 16, COL_BLACK);
        card_at(1, 172, &m, COL_BLACK);
    }
}

/* 選ぶ画面。終わったら呼んだ側が描き直す（g_dirty） */
FAR(void, ui_helper_pick, (u16 enemy))
{
    Roster r;
    MateLead m;
    u8 cur = 0, i, p;
    board_roster(&r);
    if (!r.n) { ui_msg(T_HP_NONE); g_dirty = 1; return; }
    ui_clear();
    ui_header(T_HP_TITLE);
    txt_id(1, 20, T_HP_HINT, COL_LGRAY, COL_BLACK);
    for (i = 0; i < r.n; i++) if (g_help == r.r[i].k + 1) cur = i;
    for (i = 0; i < r.n; i++) row(&r.r[i], i, cur, enemy);
    ui_guide(T_HP_GUIDE);
    psg_beep(0x80, 4);
    for (;;) {
        u8 old = cur;
        ui_frame();
        p = input_pressed();
        if (p & BTN_CANCEL) break;
        if (input_digit() == 0) { g_help = 0; psg_beep(0x200, 4); break; }
        if ((p & BTN_UP) && cur) cur--;
        if ((p & BTN_DOWN) && cur + 1 < r.n) cur++;
        if (cur != old) { psg_beep(0x100, 2); row(&r.r[old], old, cur, enemy); row(&r.r[cur], cur, cur, enemy); continue; }
        if (p & BTN_OK) {
            mate_lead(&r.r[cur], &m);
            if (m.used || !m.card) { ui_msg(T_HP_DONE); break; }
            g_help = (u8)(r.r[cur].k + 1);
            psg_beep(0x080, 6);
            break;
        }
    }
    g_dirty = 1;
}

/* 選んである助っ人の 1 行（「助っ人: 名前」）。いなければ何もしない */
FAR(void, ui_helper_line, (u16 y))
{
    u16 npc;
    u8 cx;
    if (!g_help || (g_help & HELP_JOINED)) return;
    npc = help_npc();
    if (npc == BOARD_NOREF) { g_help = 0; return; }
    cx = txt_id(1, (u8)y, T_HP_SET, COL_CYAN, COL_DBLUE);
    name_at(cx, (u8)y, npc, COL_WHITE, COL_DBLUE);
}

/* 直前の戦いで加勢していたら、2 行の箱で知らせる（y から 42 ドット） */
FAR(void, ui_helper_say, (u16 y))
{
    Roster r;
    MateLead m;
    u8 i, cx, k = (u8)(g_help & 7);
    if (!(g_help & HELP_JOINED)) return;
    g_help = 0;
    board_roster(&r);
    for (i = 0; i < r.n && r.r[i].k != k; i++) ;
    if (i == r.n) return;
    mate_lead(&r.r[i], &m);
    if (!m.card) return;
    ui_box(0, (u8)y, SCREEN_W, 42, COL_DBLUE, COL_CYAN);
    cx = name_at(1, (u8)(y + 4), r.r[i].npc, COL_WHITE, COL_DBLUE);
    txt_id(cx, (u8)(y + 4), T_HP_JOIN, COL_CYAN, COL_DBLUE);
    card_at(1, (u8)(y + 22), &m, COL_DBLUE);
    psg_beep(0x060, 8);
    ui_wait(40);
}
