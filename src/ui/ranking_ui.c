/* SPDX-License-Identifier: MIT */
/* ranking_ui.c — ランキング。[1] イベント [2] 総戦力 [0] 自分の位置、上下で 1 行、左右で 10 行送る。
 * 上位 3 人は金・銀・銅の印。自分の行は青。NPC の名前は「前半 + 後半」のハンドル名。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../core/ranking.h"
#include "../core/tables.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/gfx.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define LIST_Y 36
#define ROW_H  16          /* 文字の高さ。15 だと下の行に食い込む */
#define SHOW   9           /* 36 + 16 × 9 = 180（案内の行は 192 から） */

static const u8 medal[3] = { COL_YELLOW, COL_LGRAY, COL_ORANGE };

static void tabs(void)
{
    u8 i;
    for (i = 0; i < 2; i++) {
        u8 on = (u8)(i == g_rank.mode), x = (u8)(i * 128);
        ui_box(x, 16, 128, 20, on ? COL_BLUE : COL_DBLUE, on ? COL_YELLOW : COL_DGRAY);
        ui_key((u8)(i + 1), (u8)(x + 4), 20);
        txt_id((u8)(x / 8 + 3), 18, i ? T_RANK_POWER : T_RANK_EVENT, on ? COL_YELLOW : COL_WHITE, on ? COL_BLUE : COL_DBLUE);
    }
}

/* rows[k] を画面の pos 行目に描く */
static void row(u8 k, u8 pos)
{
    const RankRow *r = &g_rank.rows[k];
    u8 y = (u8)(LIST_Y + pos * ROW_H), me = (u8)(r->npc == RANK_PLAYER), cx;
    u8 bg = me ? COL_BLUE : (k & 1 ? COL_BLACK : COL_DGRAY);
    vdp_fill(0, y, SCREEN_W, ROW_H, bg);
    if (r->rank <= 3) vdp_fill(2, (u16)y + 2, 10, 11, medal[r->rank - 1]);
    txt_num_r(1, y, r->rank, 4, me ? COL_YELLOW : COL_WHITE, bg);
    if (me) {
        cx = txt_draw(7, y, g_save.name, COL_YELLOW, bg);
        txt_id((u8)(cx + 1), y, T_RANK_YOU, COL_CYAN, bg);
    } else {
        NpcRec n;
        tbl_npc(r->npc, &n);
        cx = txt_id(7, y, (u16)(NPC_PRE_TEXT + n.name_id / NPC_SUF_COUNT), COL_WHITE, bg);
        txt_id(cx, y, (u16)(NPC_SUF_TEXT + n.name_id % NPC_SUF_COUNT), COL_WHITE, bg);
    }
    txt_num_r(23, y, r->value, 9, me ? COL_YELLOW : COL_LGRAY, bg);
}

static void list(void)
{
    u8 k, s = 0;
    g_ui_state = UIS_BUSY;                 /* 描き直しの間（自動試験が途中で撮らないように） */
    ranking_list(&g_rank);
    g_rank.first = g_rank.rows[0].rank;
    vdp_fill(0, LIST_Y, SCREEN_W, ROW_H * SHOW, COL_BLACK);
    for (k = 0; k < g_rank.n; k++)                 /* 自分の行が 9 行目より下なら、そこが見えるようずらす */
        if (g_rank.rows[k].npc == RANK_PLAYER && k >= SHOW) s = (u8)(k - SHOW + 1);
    for (k = s; k < g_rank.n && k < s + SHOW; k++) row(k, (u8)(k - s));
    {                                      /* 自分の順位は見出しの右に */
        u8 cx;
        vdp_fill(128, 0, 128, 15, COL_DBLUE);
        cx = txt_num(20, 0, g_rank.player_rank, COL_YELLOW, COL_DBLUE);
        cx = txt_draw(cx, 0, "/", COL_LGRAY, COL_DBLUE);
        cx = txt_num(cx, 0, g_rank.total, COL_LGRAY, COL_DBLUE);
        txt_id(cx, 0, T_I, COL_LGRAY, COL_DBLUE);
    }
}

FAR(void, ranking_init, (void))
{
    g_rank.mode = g_save.event_id ? RANK_EVENT : RANK_POWER;
    g_rank.first = 0;
}

FAR(void, ranking_draw, (void))
{
    ui_clear();
    ui_header(T_RANK_TITLE);
    tabs();
    list();
    ui_guide(T_RANK_GUIDE);
}

FAR(void, ranking_update, (void))
{
    u8 p = input_pressed();
    u8 d = input_digit();
    u16 f = g_rank.first;
    if (d == 1 || d == 2) { g_rank.mode = (u8)(d - 1); g_rank.first = 0; psg_beep(0x100, 2); tabs(); list(); return; }
    if (d == 0) { g_rank.first = 0; list(); return; }
    if ((p & BTN_UP) && f > 1) f--;
    if (p & BTN_DOWN) f++;
    if (p & BTN_LEFT) f = f > RANK_ROWS ? (u16)(f - RANK_ROWS) : 1;
    if (p & BTN_RIGHT) f = (u16)(f + RANK_ROWS);
    if (f != g_rank.first) { g_rank.first = f; psg_beep(0x100, 1); list(); }
    if (p & (BTN_CANCEL | BTN_OK)) screen_change(g_save.event_id ? &scr_event : &scr_home);
}

SCREEN(scr_ranking, ranking_init, ranking_update, ranking_draw, 0)
