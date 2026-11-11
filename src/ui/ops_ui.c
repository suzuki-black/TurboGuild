/* SPDX-License-Identifier: MIT */
/* ops_ui.c — 運営モード（裏面, core/opsim.h, DECISIONS #88）。サービス終了後のタイトルから入る。
 * 上に今日の数字、まん中に 270 日のグラフ（DAU 緑・課金率 黄・引退 赤・新規 水色・売上 暗い赤の棒）。
 * 節目の日にはグラフの上に会議の箱を出して選ばせ、閉じたらグラフを描き直す。
 * グラフの点は日ごとに VRAM のページ 2（VRAM2_HIST）へ残すので、何度でも描き直せる。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../data/bgm_data.h"
void bgm_play(u8 song);
#include "../ram.h"
#include "../core/opsim.h"
#include "../core/save.h"
#include "../core/tables.h"
#include "../data/tables.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define GX     8                  /* グラフの左（240 ドットに 270 日） */
#define GW     240
#define GB     178                /* グラフの底 */
#define GH     120                /* 高さ（点は 0..GH。8 ビット目は会議の印） */
#define MEET   0x80
#define BOX_Y  52
#define BOX_H  132
#define HIST   5                  /* 1 日ぶんの点のバイト数 */

#define ST_RUN  0
#define ST_MEET 1
#define ST_DONE 2
#define ST_MODE 3                 /* 早回しかじっくりかを選ぶ（DECISIONS #98） */

static u8 st;
static u8 newrec;                 /* 決算で新記録だった */
static u8 ad_seen;                /* 知らせた広告の止まり方（下の帯に出す） */
static OpsMeeting mt;
static u8 last[4];                /* 前の日の点（線をつなぐ） */
static u16 pay_avg;               /* 課金率（0.1%）をならしたもの。日ごとでは上下しすぎて読めない */

static u8 day_x(u16 d) { return (u8)(GX + (u16)(d * 8 / 9)); }

static void vline(u8 x, u8 a, u8 b, u8 col)
{
    VdpCmd c;
    u8 t;
    if (a > b) { t = a; a = b; b = t; }
    c.sx = 0; c.sy = 0;
    c.dx = x; c.dy = (u16)(GB - b);
    c.nx = 1; c.ny = (u16)(b - a + 1);
    c.clr = col;
    c.arg = 0;
    c.cmd = 0x80;                 /* LMMV: 1 ドット幅 */
    vdp_cmd(&c);
}

static u8 scale(u32 v, u32 max)
{
    if (v >= max) return GH;
    return (u8)(v * GH / max);
}

static s32 profit(void) { return (s32)(g_ops.sales_total - g_ops.ad_total); }

/* 1 日ぶんの点: DAU（1000 人で上まで）・課金率（ならして 20%）・引退（1000 人）・新規（1 日 30 人）・売上の棒（1 日 40 万円） */
static void points(u8 *p)
{
    p[0] = scale(g_ops.dau, 1000);
    pay_avg = (u16)((pay_avg * 3 + (g_ops.dau ? (u32)g_ops.payers * 1000 / g_ops.dau : 0)) / 4);
    p[1] = scale(pay_avg, 200);
    p[2] = scale(g_ops.retired, 1000);
    p[3] = scale(g_ops.fresh_users, 30);
    p[4] = scale(g_ops.sales_day, 400000UL);
}

static void plot(u16 d, const u8 *p, u8 *prev)
{
    u8 x = day_x(d), k;
    static const u8 col[4] = { COL_GREEN, COL_YELLOW, COL_RED, COL_CYAN };
    if (p[0] & MEET) vline(x, 0, GH, COL_DGRAY);
    if (p[4]) vline(x, 0, p[4], COL_DRED);
    for (k = 4; k--; ) {
        u8 v = (u8)(p[k] & ~MEET);
        vline(x, d ? prev[k] : v, v, col[k]);
        prev[k] = v;
    }
}

static void graph(void)
{
    u8 buf[12 * HIST], prev[4];
    u16 d = 0;
    VdpXfer x;
    ui_box(GX - 2, GB - GH - 4, GW + 4, GH + 8, COL_BLACK, COL_DGRAY);
    while (d < g_ops.day) {
        u8 n = (u8)(g_ops.day - d > 12 ? 12 : g_ops.day - d), k;
        x.addr = (u16)(VRAM2_HIST + d * HIST); x.src = buf; x.len = (u8)(n * HIST);
        vdp_page2_read(&x);
        for (k = 0; k < n; k++, d++) plot(d, buf + k * HIST, prev);
    }
    for (d = 0; d < 4; d++) last[d] = prev[d];
}

/* 1234万円 / -320万円 */
static u8 money(u8 cx, u8 y, s32 v, u8 fg, u8 bg)
{
    if (v < 0) { cx = txt_draw(cx, y, "-", fg, bg); v = -v; }
    cx = txt_num(cx, y, (u32)v / 10000, fg, bg);
    return txt_id(cx, y, T_OPS_MAN, COL_LGRAY, bg);
}

/* 上の帯の右に日付、その下 2 行に今日の数字（色はグラフの線と同じ）。
 * 毎日描くので消してから描かない（ちらつく）。数字は幅を決めて上書きし、長さの変わる利益の後ろだけ消す */
static void kpi(void)
{
    u8 cx;
    u16 pct = g_ops.dau ? (u16)((u32)g_ops.payers * 100 / g_ops.dau) : 0;
    cx = txt_id(20, 0, T_OPS_DAYP, COL_LGRAY, COL_DBLUE);
    cx = txt_num_r((u8)(cx + 1), 0, g_ops.day, 3, COL_WHITE, COL_DBLUE);
    txt_id(cx, 0, T_OPS_DAYS, COL_LGRAY, COL_DBLUE);
    cx = txt_id(1, 18, T_OPS_DAU, COL_GREEN, COL_BLACK);
    cx = txt_num_r((u8)(cx + 1), 18, g_ops.dau, 4, COL_WHITE, COL_BLACK);
    cx = txt_id((u8)(cx + 1), 18, T_OPS_NEW, COL_CYAN, COL_BLACK);
    cx = txt_num_r((u8)(cx + 1), 18, g_ops.fresh_users, 3, COL_WHITE, COL_BLACK);
    cx = txt_id((u8)(cx + 1), 18, T_OPS_RET, COL_RED, COL_BLACK);
    txt_num_r((u8)(cx + 1), 18, g_ops.retired, 4, COL_WHITE, COL_BLACK);
    cx = txt_id(1, 34, T_OPS_PAY, COL_YELLOW, COL_BLACK);
    cx = txt_num_r((u8)(cx + 1), 34, pct, 2, COL_WHITE, COL_BLACK);
    cx = txt_draw(cx, 34, "%", COL_WHITE, COL_BLACK);
    cx = txt_id((u8)(cx + 1), 34, T_OPS_PROFIT, COL_ORANGE, COL_BLACK);
    cx = money((u8)(cx + 1), 34, profit(), COL_WHITE, COL_BLACK);
    if (cx < 32) vdp_fill((u16)(cx * 8), 34, (u16)(SCREEN_W - cx * 8), 16, COL_BLACK);
    if (g_ops.ad_on && cx < 26) txt_id(26, 34, T_OPS_ADON, COL_CYAN, COL_BLACK);
}

/* 箱の中の行（2 桁目から 28 桁。選択肢は 4 桁目から 25 桁） */
static u8 row_y(u8 row) { return (u8)(BOX_Y + 6 + row * 18); }

static u8 line(u8 row, u16 id, u8 fg)
{
    return txt_id(2, row_y(row), id, fg, COL_DBLUE);
}

static void points_text(u8 cx, u8 y, u16 v, u8 fg)
{
    cx = txt_num(cx, y, v / 100, fg, COL_DBLUE);
    cx = txt_draw(cx, y, ".", fg, COL_DBLUE);
    if (v % 100 < 10) cx = txt_draw(cx, y, "0", fg, COL_DBLUE);
    cx = txt_num(cx, y, v % 100, fg, COL_DBLUE);
    txt_id(cx, y, T_OPS_PT, fg, COL_DBLUE);
}

/* 本編を終える前は史実を伏せる（DECISIONS #98）: 史実のわけの行を「？？？」に、選択肢 1 の「（史実）」を外す */
static u8 hidden(void) { return (u8)(!g_save.credits_seen && mt.text < T_OPS_I0); }

static void option(u8 y, u16 id, u8 first)
{
    char b[48];
    const char *s = txt_ptr(id);
    u8 n = 0, cut = 0;
    while (s[n] && n < sizeof(b) - 1) { b[n] = s[n]; n++; }
    b[n] = 0;
    if (first && hidden()) {
        for (n = 0; b[n]; n++) {
            if ((u8)b[n] == 0x81 && (u8)b[n + 1] == 0x69) cut = n;   /* 「（」 */
            if (b[n] == ' ' && b[n + 1] == '(') cut = n;
            if ((((u8)b[n] >= 0x81 && (u8)b[n] <= 0x9F) || (u8)b[n] >= 0xE0) && b[n + 1]) n++;   /* 2 バイトの字（途中で切れていたら進めない） */
        }
        if (cut) b[cut] = 0;
    }
    txt_draw(4, y, b, COL_WHITE, COL_DBLUE);
}

static void npc_at(u8 cx, u8 y, u16 npc)
{
    NpcRec r;
    tbl_npc(npc, &r);
    cx = txt_id(cx, y, (u16)(NPC_PRE_TEXT + r.name_id / NPC_SUF_COUNT), COL_CYAN, COL_DBLUE);
    txt_id(cx, y, (u16)(NPC_SUF_TEXT + r.name_id % NPC_SUF_COUNT), COL_CYAN, COL_DBLUE);
}

/* 企画会議（DECISIONS #99）: 見送る・イベント・ガチャ・課金・締め付けの 5 枚から 1 つ */
static void plan_box(void)
{
    u8 k, cx, y;
    ui_box(GX, BOX_Y, GW, BOX_H, COL_DBLUE, COL_CYAN);
    cx = line(0, T_OPS_PLAN, COL_CYAN);
    cx = txt_num((u8)(cx + 1), row_y(0), mt.day, COL_CYAN, COL_DBLUE);
    txt_id(cx, row_y(0), T_OPS_DAYS, COL_CYAN, COL_DBLUE);
    line(1, T_OPS_PLAN_B, COL_WHITE);
    for (k = 0; k < OPS_PLAN_OPTS; k++) {
        u8 a = ops_plan_offer(k);
        y = row_y((u8)(2 + k));
        ui_key((u8)(k + 1), 16, (u8)(y + 2));
        txt_id(4, y, (u16)(T_OPS_PA0 + a), k == 4 ? COL_ORANGE : COL_WHITE, COL_DBLUE);   /* 締め付けは橙 */
    }
    ui_guide(T_OPS_GUIDE_PLAN);
}

static void meeting_box(void)
{
    u8 k, cx, y, inc = (u8)(mt.text >= T_OPS_I0 && mt.text < T_OPS_I0 + OPS_INC_KINDS * OPS_TEXTS);
    if (mt.text == T_OPS_PLAN) { plan_box(); return; }
    ui_box(GX, BOX_Y, GW, BOX_H, COL_DBLUE, COL_YELLOW);
    cx = line(0, inc ? T_OPS_INC : T_OPS_MEETING, COL_YELLOW);
    cx = txt_num((u8)(cx + 1), row_y(0), mt.day, COL_YELLOW, COL_DBLUE);
    txt_id(cx, row_y(0), T_OPS_DAYS, COL_YELLOW, COL_DBLUE);
    line(1, mt.text, COL_WHITE);
    line(2, (u16)(mt.text + 1), COL_WHITE);
    if (inc && mt.text >= T_OPS_I0 + OPS_TEXTS && mt.text < T_OPS_I0 + 5 * OPS_TEXTS) {   /* CS のメール: 差出人は 1000 人の誰か */
        cx = line(3, T_OPS_FROM, COL_CYAN);
        npc_at((u8)(cx + 1), row_y(3), (u16)(((u32)mt.day * 389 + 77) % NPC_COUNT));
    } else line(3, hidden() ? T_OPS_HIDDEN : (u16)(mt.text + 2), COL_CYAN);
    if (mt.n) {
        for (k = 0; k < mt.n; k++) {
            y = row_y((u8)(4 + k));
            ui_key((u8)(k + 1), 16, (u8)(y + 2));
            option(y, (u16)(mt.text + 3 + k), (u8)(k == 0));
        }
        ui_guide(T_OPS_GUIDE_PICK);
        return;
    }
    /* 判定（ops_decide はもう済ませてある）: 利益 1432万円 DAU 240 → 1.87 点（基準 2.00 点） */
    y = row_y(4);
    cx = txt_id(2, y, T_OPS_PROFIT, COL_ORANGE, COL_DBLUE);
    cx = money((u8)(cx + 1), y, g_ops.profit_judge, COL_WHITE, COL_DBLUE);
    cx = txt_id((u8)(cx + 2), y, T_OPS_DAU, COL_GREEN, COL_DBLUE);
    txt_num((u8)(cx + 1), y, g_ops.dau_judge, COL_WHITE, COL_DBLUE);
    y = row_y(5);
    cx = txt_id(2, y, T_OPS_JUDGE, COL_LGRAY, COL_DBLUE);
    points_text((u8)(cx + 1), y, g_ops.points, COL_WHITE);
    cx = txt_id(17, y, T_OPS_NEED, COL_LGRAY, COL_DBLUE);
    points_text((u8)(cx + 1), y, OPS_CONT_POINTS, COL_LGRAY);
    line(6, g_ops.verdict == OPS_CONTINUE ? T_OPS_CONT : T_OPS_ENDV, g_ops.verdict == OPS_CONTINUE ? COL_GREEN : COL_RED);
    ui_guide(T_OPS_GUIDE_NEXT);
}

static void result_box(void)
{
    u8 cx, saved = (u8)(g_ops.verdict == OPS_CONTINUE), more = (u8)(profit() >= (s32)OPS_HIST_PROFIT);
    u16 boss = saved ? (more ? T_OPS_BOSS0 : T_OPS_BOSS1) : (more ? T_OPS_BOSS2 : T_OPS_BOSS3);
    ui_box(GX, BOX_Y, GW, BOX_H, COL_DBLUE, COL_YELLOW);
    line(0, T_OPS_RESULT, COL_YELLOW);
    cx = line(1, T_OPS_SALES, COL_ORANGE);
    money((u8)(cx + 1), row_y(1), (s32)g_ops.sales_total, COL_WHITE, COL_DBLUE);
    cx = line(2, T_OPS_AD, COL_CYAN);
    money((u8)(cx + 1), row_y(2), (s32)g_ops.ad_total, COL_WHITE, COL_DBLUE);
    cx = line(3, T_OPS_PROFIT, COL_YELLOW);
    money((u8)(cx + 1), row_y(3), profit(), COL_YELLOW, COL_DBLUE);
    cx = line(4, T_OPS_R_STAY, COL_GREEN);
    cx = txt_num((u8)(cx + 1), row_y(4), (u32)(g_ops.reg - g_ops.retired), COL_WHITE, COL_DBLUE);
    txt_id(cx, row_y(4), T_OPS_NIN, COL_LGRAY, COL_DBLUE);
    cx = line(5, T_OPS_R_BEST, COL_LGRAY);
    cx = money((u8)(cx + 1), row_y(5), (s32)g_save.ops_best, COL_WHITE, COL_DBLUE);
    if (newrec) txt_id((u8)(cx + 1), row_y(5), T_OPS_R_NEW, COL_YELLOW, COL_DBLUE);
    line(6, boss, COL_WHITE);
    ui_guide(T_OPS_GUIDE_NEXT);
}

/* 270 日を回し終えた: 最高の利益と「続けられた」をセーブに残す */
static void finish(void)
{
    s32 v = profit();
    newrec = (u8)(v > 0 && (u32)v > g_save.ops_best);
    if (g_save.flags & SF_STARTED) {                 /* 本編を始めていないセーブには書かない（タイトルから入ったとき） */
        if (newrec) g_save.ops_best = (u32)v;
        g_save.ops_flags |= (u8)(OPS_PLAYED | (g_ops.verdict == OPS_CONTINUE ? OPS_SAVED : 0));
        g_save_dirty = 1;
    }
    st = ST_DONE;
    result_box();
}

/* 下の帯: 走っている間は ESC の案内。広告が止まったら、そのわけを次の会議まで出す */
static void guide_run(void)
{
    ad_seen = g_ops.ad_stop;
    ui_guide(ad_seen == OPS_AD_CVR ? T_OPS_AD_CVR : ad_seen == OPS_AD_SPENT ? T_OPS_AD_SPENT : T_OPS_GUIDE_RUN);
}

/* 早回しかじっくりか。本編の途中なら、その間も本編の時間が進むと知らせる */
static void mode_box(void)
{
    ui_box(GX, BOX_Y, GW, BOX_H, COL_DBLUE, COL_YELLOW);
    line(0, T_OPS_MODE, COL_YELLOW);
    ui_key(1, 16, (u8)(row_y(1) + 2));
    txt_id(4, row_y(1), T_OPS_FAST, COL_WHITE, COL_DBLUE);
    line(2, T_OPS_FAST2, COL_LGRAY);
    ui_key(2, 16, (u8)(row_y(3) + 2));
    txt_id(4, row_y(3), T_OPS_SLOW, COL_WHITE, COL_DBLUE);
    line(4, T_OPS_SLOW2, COL_LGRAY);
    if ((g_save.flags & SF_STARTED) && !g_save.credits_seen) line(6, T_OPS_RTC, COL_ORANGE);
    ui_guide(T_OPS_GUIDE_MODE);
}

FAR(void, ops_init, (void))
{
    ops_start();
    st = ST_MODE;
    ad_seen = 0;
    pay_avg = 0;
    bgm_play(BGM_EVENT);
}

FAR(void, ops_draw, (void))
{
    ui_clear();
    ui_header(T_OPS_TITLE);
    kpi();
    graph();
    if (st == ST_MEET) meeting_box();
    else if (st == ST_DONE) result_box();
    else if (st == ST_MODE) mode_box();
    else guide_run();
}

FAR(void, ops_update, (void))
{
    u8 p = input_pressed(), d = input_digit(), pt[HIST];
    VdpXfer x;
    if (st == ST_MODE) {
        if (d == 1 || d == 2) { psg_beep(0x080, 4); g_ops.slow = (u8)(d == 2); st = ST_RUN; g_dirty = 1; }
        else if (p & BTN_CANCEL) screen_change(&scr_title);
        return;
    }
    if (st == ST_MEET) {
        if (mt.n ? (d >= 1 && d <= mt.n) : (p & BTN_OK) != 0) {
            psg_beep(0x080, 4);
            if (mt.n) ops_decide((u8)(d - 1));
            g_ops.ad_stop = 0;                 /* 会議を開いたら広告の知らせは消す */
            st = ST_RUN;
            g_dirty = 1;
        }
        return;
    }
    if (st == ST_DONE) {
        if (p & (BTN_OK | BTN_CANCEL)) screen_change(&scr_title);
        return;
    }
    if (p & BTN_CANCEL) {
        if (ui_confirm(T_OPS_QUIT)) { screen_change(&scr_title); return; }
        g_dirty = 1;
        return;
    }
    if (ops_meeting(&mt)) {
        if (!mt.n) ops_decide(0);          /* 判定の会議は選ばない（先に決めて、箱で見せる） */
        st = ST_MEET;
        psg_beep(0x100, 6);
        meeting_box();
        return;
    }
    {
        u8 meet = (u8)(g_ops.next && g_ops.day == mt.day);   /* 会議の日の点に印 */
        ops_step();
        points(pt);
        if (meet) pt[0] |= MEET;
        x.addr = (u16)(VRAM2_HIST + (g_ops.day - 1) * HIST); x.src = pt; x.len = HIST;
        vdp_page2_write(&x);
        plot((u16)(g_ops.day - 1), pt, last);
        kpi();
        if (g_ops.news) {                                /* 確率操作がばれた・コンプガチャの規制 */
            ui_guide((u16)(T_OPS_NEWS1 + g_ops.news - 1));
            psg_beep(0x300, 12);
            g_ops.news = 0;
        } else if (g_ops.ad_stop != ad_seen || g_ops.day % 15 == 0) guide_run();
        else if (!ad_seen) {                             /* 社内チャット（15 日ごと。次の区切りまで下の帯に出す） */
            u16 c = ops_chat();
            if (c) ui_guide(c);
        }
    }
    if (g_ops.day >= OPS_DAYS) finish();
}

SCREEN(scr_ops, ops_init, ops_update, ops_draw, 0)
