/* guildwar_ui.c — ギルド対抗戦の画面。上に「味方 VS 相手」と点のゲージ、残り時間とコンボ、
 * 下に相手ギルドの冒険者 3 人（[1]〜[3] で攻撃, AP 10）と [4] 応援。戦の時間外は次の戦までの時間。
 * マイページから呼ぶ結果のお知らせ（参戦・戦の結果・最終順位と報酬）もここに置く。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../core/guildwar.h"
#include "../core/ranking.h"
#include "../core/tables.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/gfx.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define SCORE_Y  56
#define ROWS_Y   112

static u8 npc_name(u8 cx, u8 y, u16 npc, u8 fg, u8 bg)
{
    NpcRec n;
    tbl_npc(npc, &n);
    cx = txt_id(cx, y, (u16)(NPC_PRE_TEXT + n.name_id / NPC_SUF_COUNT), fg, bg);
    return txt_id(cx, y, (u16)(NPC_SUF_TEXT + n.name_id % NPC_SUF_COUNT), fg, bg);
}

/* 「第 1 回 3 勝 1 敗」 */
static void record(u8 y)
{
    u8 cx = txt_id(0, y, T_GVG_DAI, COL_LGRAY, COL_BLACK);
    cx = txt_num(cx, y, g_save.gvg_war, COL_WHITE, COL_BLACK);
    cx = txt_id(cx, y, T_GVG_NO, COL_LGRAY, COL_BLACK);
    cx = txt_num((u8)(cx + 2), y, g_save.gvg_win, COL_YELLOW, COL_BLACK);
    cx = txt_id(cx, y, T_GVG_WIN, COL_LGRAY, COL_BLACK);
    cx = txt_num((u8)(cx + 1), y, g_save.gvg_lose, COL_CYAN, COL_BLACK);
    txt_id(cx, y, T_GVG_LOSE, COL_LGRAY, COL_BLACK);
}

/* 点とゲージ（いまの戦の時間中だけ） */
static void scores(const GvgNow *n)
{
    u8 cx;
    u32 tot = n->us + n->them + 1;
    vdp_fill(0, SCORE_Y, SCREEN_W, 52, COL_BLACK);
    cx = txt_id(0, SCORE_Y, T_GVG_US, COL_YELLOW, COL_BLACK);
    txt_num((u8)(cx + 1), SCORE_Y, n->us, COL_WHITE, COL_BLACK);
    cx = txt_id(16, SCORE_Y, T_GVG_THEM, COL_CYAN, COL_BLACK);
    txt_num((u8)(cx + 1), SCORE_Y, n->them, COL_WHITE, COL_BLACK);
    gfx_gauge(4, SCORE_Y + 18, 248, (u16)(n->us * 256 / tot), 256, COL_ORANGE);
    cx = txt_id(0, SCORE_Y + 26, T_GVG_LEFT, COL_LGRAY, COL_BLACK);
    cx = txt_num((u8)(cx + 1), SCORE_Y + 26, n->left_min, COL_WHITE, COL_BLACK);
    cx = txt_id(cx, SCORE_Y + 26, T_GVG_MIN, COL_LGRAY, COL_BLACK);
    cx = txt_id((u8)(cx + 2), SCORE_Y + 26, T_GVG_COMBO, COL_ORANGE, COL_BLACK);
    txt_num((u8)(cx + 1), SCORE_Y + 26, n->combo, COL_YELLOW, COL_BLACK);
}

static void target_row(u8 k)
{
    NpcNow nn;
    u16 npc = gvg_target(k);
    u8 y = (u8)(ROWS_Y + (k - 1) * 20), bg = (u8)(k & 1 ? COL_DGRAY : COL_BLACK), cx;
    vdp_fill(0, y, SCREEN_W, 18, bg);
    ui_key(k, 2, (u8)(y + 2));
    npc_now(npc, &nn);
    npc_name(2, y, npc, COL_WHITE, bg);
    cx = txt_id(18, y, T_POWER, COL_LGRAY, bg);
    txt_num((u8)(cx + 1), y, nn.power, COL_WHITE, bg);
}

FAR(void, gvg_draw, (void))
{
    GvgNow n;
    u8 cx, k;
    ui_clear();
    ui_header(T_GVG_TITLE);
    gvg_now(&n);
    if (!n.war) { txt_id(2, 80, T_EV_CLOSED, COL_LGRAY, COL_BLACK); ui_guide(T_HELP_BACK); return; }
    record(18);
    cx = txt_id(0, 36, (u16)(GUILD_TEXT + g_save.guild - 1), COL_YELLOW, COL_BLACK);
    cx = txt_id((u8)(cx + 1), 36, T_GVG_VS, COL_RED, COL_BLACK);
    txt_id((u8)(cx + 1), 36, (u16)(GUILD_TEXT + n.foe), COL_CYAN, COL_BLACK);
    if (!n.now) {
        cx = txt_id(0, SCORE_Y, T_GVG_NEXT, COL_LGRAY, COL_BLACK);
        cx = txt_num((u8)(cx + 1), SCORE_Y, n.left_min, COL_WHITE, COL_BLACK);
        txt_id(cx, SCORE_Y, T_GVG_MIN, COL_LGRAY, COL_BLACK);
        txt_id(0, SCORE_Y + 20, T_GVG_TIMES, COL_LGRAY, COL_BLACK);
        ui_guide(T_HELP_BACK);
        return;
    }
    scores(&n);
    for (k = 1; k <= GVG_TARGETS; k++) target_row(k);
    ui_box(0, ROWS_Y + 62, 120, 18, n.cheered ? COL_DGRAY : COL_DRED, COL_ORANGE);
    ui_key(4, 3, ROWS_Y + 65);
    txt_id(3, ROWS_Y + 62, n.cheered ? T_GVG_CHEERED : T_GVG_CHEER, n.cheered ? COL_LGRAY : COL_YELLOW,
           n.cheered ? COL_DGRAY : COL_DRED);
    ui_guide(T_GVG_GUIDE);
}

static void attack(u8 k)
{
    GvgAttack a;
    GvgNow n;
    u8 cx;
    gvg_attack(k, &a);
    if (a.status == 1) { ui_msg(T_GVG_NOTNOW); return; }
    if (a.status == 2) { ui_msg(T_AP_SHORT); return; }
    g_save_dirty = 1;
    ui_box(16, 84, 224, 44, a.win ? COL_DRED : COL_DBLUE, COL_YELLOW);
    cx = npc_name(3, 88, a.npc, COL_WHITE, a.win ? COL_DRED : COL_DBLUE);
    txt_id((u8)(cx + 1), 88, a.win ? T_WIN : T_LOSE, COL_YELLOW, a.win ? COL_DRED : COL_DBLUE);
    cx = txt_id(3, 106, T_EV_PT, COL_LGRAY, a.win ? COL_DRED : COL_DBLUE);
    cx = txt_draw((u8)(cx + 1), 106, "+", COL_LGRAY, a.win ? COL_DRED : COL_DBLUE);
    txt_num(cx, 106, a.pts, COL_YELLOW, a.win ? COL_DRED : COL_DBLUE);
    psg_beep(a.win ? 0x050 : 0x200, 12);
    ui_wait(40);
    gvg_now(&n);
    scores(&n);
    g_dirty = 1;
}

FAR(void, gvg_update_scr, (void))
{
    u8 p = input_pressed();
    u8 d = input_digit();
    if (p & BTN_CANCEL) { screen_change(&scr_home); return; }
    if (d >= 1 && d <= GVG_TARGETS) { attack(d); return; }
    if (d == 4) {
        if (gvg_cheer()) { g_save_dirty = 1; psg_beep(0x060, 10); ui_msg(T_GVG_CHEER_OK); }
        g_dirty = 1;
        return;
    }
    if (g_frames == 0) {                             /* 1 秒ごとに点を伸ばして見せる */
        GvgNow n;
        gvg_now(&n);
        if (n.now) scores(&n);
    }
}

SCREEN(scr_guildwar, 0, gvg_update_scr, gvg_draw, 0)

/* ---- マイページで出す結果のお知らせ（g_gvgres） ---- */

FAR(void, gvg_result_show, (void))
{
    u8 cx, y = 60, i;
    if (!(g_gvgres.joined || g_gvgres.n || g_gvgres.ended)) return;
    ui_box(8, 40, 240, 120, COL_DBLUE, COL_YELLOW);
    txt_id(2, 44, g_gvgres.joined && !g_gvgres.n ? T_GVG_JOINED : T_GVG_RESULT, COL_YELLOW, COL_DBLUE);
    if (g_gvgres.n) {
        cx = txt_num(2, y, g_gvgres.n, COL_WHITE, COL_DBLUE);
        cx = txt_id(cx, y, T_GVG_MATCH, COL_LGRAY, COL_DBLUE);
        cx = txt_num((u8)(cx + 1), y, g_gvgres.win, COL_YELLOW, COL_DBLUE);
        txt_id(cx, y, T_GVG_WIN, COL_LGRAY, COL_DBLUE);
        y = (u8)(y + 18);
        cx = txt_num(2, y, g_gvgres.us, COL_YELLOW, COL_DBLUE);
        cx = txt_id((u8)(cx + 1), y, T_GVG_VS, COL_RED, COL_DBLUE);
        txt_num((u8)(cx + 1), y, g_gvgres.them, COL_CYAN, COL_DBLUE);
        y = (u8)(y + 18);
    }
    if (g_gvgres.ended) {
        cx = txt_id(2, y, T_GVG_RANK, COL_LGRAY, COL_DBLUE);
        cx = txt_num((u8)(cx + 1), y, g_gvgres.rank, COL_YELLOW, COL_DBLUE);
        txt_id(cx, y, T_I, COL_LGRAY, COL_DBLUE);
        y = (u8)(y + 18);
    }
    for (i = 0; i < g_gvgres.gain.n && y < 150; i++, y = (u8)(y + 16)) {
        if (g_gvgres.gain.item[i] == ITEM_CARD) {
            CardRec c;
            tbl_card(g_gvgres.gain.amount[i], &c);
            txt_id(2, y, c.name_text, rarity_color(c.rarity), COL_DBLUE);
        } else {
            cx = txt_id(2, y, (u16)(T_ITEM_0 + g_gvgres.gain.item[i]), COL_WHITE, COL_DBLUE);
            cx = txt_draw(cx, y, "x", COL_LGRAY, COL_DBLUE);
            txt_num(cx, y, g_gvgres.gain.amount[i], COL_YELLOW, COL_DBLUE);
        }
    }
    psg_beep(0x050, 15);
    g_gvgres.joined = g_gvgres.n = g_gvgres.ended = 0;
    ui_wait_key();
    g_dirty = 1;
}
