/* SPDX-License-Identifier: MIT */
/* thread_ui.c — 攻略スレの画面（core/thread.h, DECISIONS #104）。お知らせの 5 から。
 * 新しいレスが上。上下で 1 レスずつさかのぼる（さかのぼった数は g_cursor）。
 * ボーダーの点は「正解（data/border.h）× レスが言った %」。正解が 0 の順位（最終イベントの 500 位）は「誰でも入る」。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../config.h"
#include "../core/opsim.h"
#include "../core/save.h"
#include "../core/tables.h"
#include "../core/thread.h"
#include "../data/border.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define TH_Y    18
#define TH_END  ROW_LAST
#define TH_BACK 200              /* さかのぼれるレスの数 */

typedef struct { char b[112]; u8 n; } Bld;   /* 組み立てたレスの本文（静的 RAM が残り少ないのでスタックに置く） */

static void put(Bld *w, const char *s)
{
    while (*s && w->n < sizeof(w->b) - 1) w->b[w->n++] = *s++;
    w->b[w->n] = 0;
}

static void put_num(Bld *w, u32 v)
{
    char s[11];
    u8 n = 10;
    s[10] = 0;
    do { s[--n] = (char)('0' + v % 10); v /= 10; } while (v && n);
    put(w, s + n);
}

/* 点を「2.1万」「21k」の形に（JA は 1 万、EN は 1000 で割る） */
static void put_pts(Bld *w, u32 v)
{
    u32 unit = GAME_LANG == LANG_EN_CODE ? 1000UL : 10000UL;
    if (v < unit) { put_num(w, v); return; }
    put_num(w, v / unit);
    if (v < unit * 10) {
        char d[3];
        d[0] = '.'; d[1] = (char)('0' + v % unit * 10 / unit); d[2] = 0;
        if (d[1] != '0') put(w, d);
    }
    put(w, txt_ptr(T_TH_UNIT));
}

/* 文を写して % に点（card = 0）かカード名を入れる */
static void fill(Bld *w, u16 id, u32 v, u16 card)
{
    char tmp[80];
    const char *s = txt_ptr(id);
    u8 n = 0, k;
    while (s[n] && n < sizeof(tmp) - 1) { tmp[n] = s[n]; n++; }
    tmp[n] = 0;
    for (k = 0; tmp[k]; k++) {
        if (tmp[k] != '%') { char c[2]; c[0] = tmp[k]; c[1] = 0; put(w, c); continue; }
        if (card) { CardRec c; tbl_card(card, &c); put(w, txt_ptr(c.name_text)); }
        else put_pts(w, v);
    }
}

/* レスの本文（id は文。運営モードでは雑談を模型の様子の文に替えて渡す） */
static void compose(Bld *w, const ThPost *o, u16 id)
{
    u32 v = 0;
    if (o->kind == TH_BORDER) {
        v = th_border[o->ev - 1][o->tier];
        if (!v) id = T_TH_BDZ;                    /* 参加すれば入れる順位 */
        else v = v * o->pct / 100;
    }
    w->n = 0; w->b[0] = 0;
    if (o->ref) { put(w, ">>"); put_num(w, o->ref); put(w, " "); }
    fill(w, id, v, o->kind == TH_BORDER ? 0 : o->card);
}

/* 運営モードの答え合わせ: 「→デマ（正解 2.1万）」。無ければ 0 */
static u8 truth(Bld *w, const ThPost *o)
{
    w->n = 0; w->b[0] = 0;
    if (o->kind == TH_BORDER) {
        u32 v = th_border[o->ev - 1][o->tier];
        if (!v) return 0;
        fill(w, o->lie ? T_TH_LIE : T_TH_TRUE, v, 0);
        return 1;
    }
    if ((o->kind == TH_BOON || o->kind == TH_NEXT) && o->lie) { fill(w, T_TH_LIEC, 0, o->real); return 1; }
    return 0;
}

/* レスの頭の行: 123 名無しさん ID:aB3x */
static void head(u8 y, const ThPost *o, u8 bg)
{
    static const char al[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/";
    char id[8];
    u8 cx, k;
    u16 h = o->id;
    vdp_fill(0, y, SCREEN_W, 16, bg);
    cx = txt_num(1, y, o->no, COL_WHITE, bg);
    cx = txt_id((u8)(cx + 1), y, T_TH_NANASHI, COL_GREEN, bg);
    id[0] = 'I'; id[1] = 'D'; id[2] = ':';
    for (k = 0; k < 4; k++) { id[3 + k] = al[h & 63]; h = (u16)((h >> 6) | (h << 10)) ^ 0x5A5Au; }
    id[7] = 0;
    txt_draw((u8)(cx + 1), y, id, COL_LGRAY, bg);
}

/* 2 桁目から右端まで（30 桁）で折り返したときの行数（全角の字は行末の 1 桁に入らず次の行へ） */
static u8 rows(const char *t)
{
    const u8 *s = (const u8 *)t;
    u8 col = 0, n = 1;
    while (*s) {
        u8 wide = (u8)((*s >= 0x81 && *s <= 0x9F) || *s >= 0xE0);
        if (col + (wide ? 2 : 1) > 30) { n++; col = 0; }
        col = (u8)(col + (wide ? 2 : 1));
        s += wide ? 2 : 1;
    }
    return n;
}

/* 運営モード: 雑談の文を模型の様子で替える（グラフの人間語訳）。
 * その時々のこと（詫び石・確率アップ・コラボ・障害・ばれた・規制・判定）を先に、無ければ直近 7 日の引退と DAU の動きで */
static u16 stat7(u8 k)                            /* 7 日前と今日の DAU（k = 0）か引退の累計（k = 1）の差の元 */
{
    u8 b[4];
    VdpXfer x;
    u16 d = g_ops.day > 7 ? (u16)(g_ops.day - 8) : 0;
    x.addr = (u16)(VRAM2_STAT + d * 4); x.src = b; x.len = 4;
    vdp_page2_read(&x);
    return k ? (u16)(b[2] | (b[3] << 8)) : (u16)(b[0] | (b[1] << 8));
}

static u16 mood(const ThPost *o)
{
    u16 h = o->id, quit, dau0;
    if (o->kind != TH_CHAT) return o->text;
    if ((h & 3) == 0) {                            /* 4 本に 1 本は、その時々のこと */
        if (g_ops.verdict == OPS_END) return T_TH_OEND;
        if (g_ops.verdict == OPS_CONTINUE) return T_TH_OCONT;
        if (g_ops.sq & OPS_SQ_RIGOUT) return T_TH_ORIG;
        if (g_ops.outage) return T_TH_OOUT;
        if (g_ops.sq & OPS_SQ_COMPBAN) return T_TH_OCOMP;
        if (g_ops.glut) return T_TH_OGLUT;
        if (g_ops.boost) return T_TH_OBOOST;
        if (g_ops.collab) return T_TH_OCOLLAB;
    }
    quit = (u16)(g_ops.retired - stat7(1));
    dau0 = stat7(0);
    h = (u16)((h >> 2) & 3);
    if (quit >= 20) return (u16)(T_TH_OQ0 + h);                       /* 引退が続く */
    if (dau0 && g_ops.dau * 100UL < dau0 * 85UL) return (u16)(T_TH_OD0 + h);   /* 減っている */
    if (g_ops.dau * 100UL > dau0 * 110UL) return (u16)(T_TH_OU0 + h);          /* 増えている */
    return (u16)(T_TH_ON0 + h);
}

/* 新しいレス p から下へ、入るだけ描く（ops = 運営モード: 雑談を替え、答え合わせを足す） */
static void list(u16 p, u8 ops)
{
    ThPost o;
    Bld w, t;
    u8 y = TH_Y, bg, i = 0, tr;
    for (;; p--, i++) {
        const char *s;
        thread_post(p, &o);
        compose(&w, &o, ops ? mood(&o) : o.text);
        tr = (u8)(ops && truth(&t, &o));
        if (y + 16 * (1 + rows(w.b) + (tr ? rows(t.b) : 0)) > TH_END) break;   /* 入り切らないレスは描かない */
        bg = i & 1 ? COL_BLACK : COL_DGRAY;
        head(y, &o, bg);
        y = (u8)(y + 16);
        s = w.b;
        while (*s) {
            vdp_fill(0, y, SCREEN_W, 16, bg);
            txt_draw(2, y, s, o.ref ? COL_CYAN : COL_WHITE, bg);
            s = g_txt_rest;
            y = (u8)(y + 16);
        }
        for (s = t.b; tr && *s; y = (u8)(y + 16)) {
            vdp_fill(0, y, SCREEN_W, 16, bg);
            txt_draw(2, y, s, o.lie ? COL_ORANGE : COL_GREEN, bg);
            s = g_txt_rest;
        }
        if (!p || o.no == 1) break;               /* スレの頭より前は前のスレ */
    }
}

FAR(void, thread_draw, (void))
{
    u16 last = thread_last();
    u8 cx;
    ui_clear();
    ui_header(T_TH_TITLE);
    cx = txt_id(16, 0, T_TH_PART, COL_LGRAY, COL_DBLUE);
    txt_num(cx, 0, last / TH_PART + 1, COL_YELLOW, COL_DBLUE);
    if (g_cursor > last) g_cursor = (u8)last;
    list((u16)(last - g_cursor), 0);
    ui_guide(T_TH_GUIDE);
}

/* 運営モード（5）: その日の終わりまでのスレ。押されたら戻る（呼んだ側が描き直す） */
FAR(void, ops_thread, (void))
{
    u16 day = g_ops.day ? g_ops.day : 1;
    u8 cx;
    ui_clear();
    ui_header(T_TH_OHEAD);
    cx = txt_num(18, 0, day, COL_YELLOW, COL_DBLUE);
    txt_id(cx, 0, T_OPS_DAYS, COL_LGRAY, COL_DBLUE);
    list((u16)(day * (u16)(GSEC_PER_DAY / TH_SECS) - 1), 1);
    ui_guide(T_OPS_GUIDE_NEXT);
    psg_beep(0x80, 4);
    ui_wait_key();
    g_dirty = 1;
}

FAR(void, thread_update, (void))
{
    u8 p = input_pressed();
    if (p & (BTN_OK | BTN_CANCEL)) { psg_beep(0x200, 4); screen_change(&scr_news); return; }
    if ((p & BTN_DOWN) && g_cursor < TH_BACK && g_cursor < thread_last()) { g_cursor++; psg_beep(0x100, 2); g_dirty = 1; }
    if ((p & BTN_UP) && g_cursor) { g_cursor--; psg_beep(0x100, 2); g_dirty = 1; }
}

SCREEN(scr_thread, 0, thread_update, thread_draw, 0)
