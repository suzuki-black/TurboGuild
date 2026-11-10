/* SPDX-License-Identifier: MIT */
/* news_ui.c — お知らせ（運営年表）と「みんなの動き」（NPC の出来事）、ギルド伝言板、流れるお知らせの文を作る。
 * 文は文言の部品と数字・名前を RAM の 1 本の文字列に組み立てる（マイページのマーキーが流す）。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../core/board.h"
#include "../core/ranking.h"
#include "../core/save.h"
#include "../core/tables.h"
#include "../core/timeline.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/gfx.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define BUF_MAX (sizeof(g_ticker_buf) - 1)

static u8 len;

static void add_str(const char *s)
{
    while (*s && len < BUF_MAX) g_ticker_buf[len++] = *s++;
    g_ticker_buf[len] = 0;
}

static void add_text(u16 id) { add_str(txt_ptr(id)); }

static void add_num(u32 v)
{
    char d[11];
    u8 n = 0;
    do { d[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 10);
    while (n && len < BUF_MAX) g_ticker_buf[len++] = d[--n];
    g_ticker_buf[len] = 0;
}

static void add_npc(u16 npc)
{
    NpcRec r;
    tbl_npc(npc, &r);
    add_text((u16)(NPC_PRE_TEXT + r.name_id / NPC_SUF_COUNT));
    add_text((u16)(NPC_SUF_TEXT + r.name_id % NPC_SUF_COUNT));
}

/* 出来事 1 件を文にする */
static void feed_text(const FeedLine *f)
{
    CardRec c;
    add_npc(f->npc);
    switch (f->kind) {
    case FEED_SSR:    tbl_card(f->param, &c); add_text(T_FD_GA); add_text(c.name_text); add_text(T_FD_SSR); break;
    case FEED_BOSS:   add_text(T_FD_BOSS); break;
    case FEED_LEVEL:  add_text(T_FD_LEVEL); add_num(f->param); add_text(T_FD_LEVEL2); break;
    case FEED_RANKUP: add_text(T_FD_RANKUP); add_num(f->param); add_text(T_FD_RANKUP2); break;
    case FEED_BYE:    add_text((u16)(T_FD_BYE0 + f->param)); break;
    default:          add_text(T_FD_LOGIN); break;
    }
}

/* 文言を写す。% には param の文言、# には ref の人の名前、$ には自分の名前を入れる
 * （Shift-JIS の 2 バイト目は 40h 以上なので、%・#・$ と取り違えない）。
 * txt_ptr の窓は次の add_text で替わるので、先に手元へ写してから組み立てる */
static void add_text_param(u16 id, u16 param, u16 ref)
{
    char tmp[64];
    const char *s = txt_ptr(id);
    u8 n = 0;
    while (s[n] && n < sizeof(tmp) - 1) { tmp[n] = s[n]; n++; }
    tmp[n] = 0;
    for (n = 0; tmp[n]; n++) {
        if (tmp[n] == '%') { if (param) add_text(param); continue; }
        if (tmp[n] == '#') { if (ref < NPC_COUNT) add_npc(ref); continue; }
        if (tmp[n] == '$') {                                       /* 名前は終わりの 0 が無いこともある */
            u8 k;
            for (k = 0; k < sizeof(g_save.name) && g_save.name[k] && len < BUF_MAX; k++) g_ticker_buf[len++] = g_save.name[k];
            g_ticker_buf[len] = 0;
            continue;
        }
        if (len < BUF_MAX) g_ticker_buf[len++] = tmp[n];
    }
    g_ticker_buf[len] = 0;
}

/* 伝言板の 1 件を文にする */
static void post_text(const BoardPost *p)
{
    if (p->kind == BP_SYS) { add_text_param(T_BOARD_YOUGM, 0, BOARD_NOREF); return; }   /* 自分がギルドマスターに */
    if (p->gm) add_text(T_BOARD_GM);
    add_npc(p->npc);
    if (p->kind == BP_LEAVE) { add_text(T_BOARD_LEFT); return; }
    add_text(T_BOARD_SAY);
    add_text_param(p->text, p->param, p->ref);
    if (p->tone) add_text((u16)(T_BOARD_TONE1 + p->tone - 1));
}

/* ライバル（chase = 0）か、イベントで抜いた元ライバル（chase = 1）との差と、最終日の見込み。いなければ 0 */
static u8 rival_text(u8 chase)
{
    RivalNow r;
    r.mode = RANK_MODE();
    r.npc = chase ? (r.mode == RANK_EVENT ? CHASER() : RANK_PLAYER) : RIVAL_OF(r.mode);
    if (r.npc == RANK_PLAYER) return 0;
    rival_now(&r);
    if (r.retired) return 0;
    if (!chase && r.value >= r.player) {           /* 上にいる相手 */
        add_text(T_RIVAL);
        add_str(" ");
        add_npc(r.npc);
        add_text(T_RV_BEHIND);
        add_num(r.value - r.player);
        if (r.pace) add_text(r.pfinal > r.final ? T_RV_PACE_WIN : T_RV_PACE_NO);
    } else if (chase && r.player >= r.value) {     /* 追ってくる相手 */
        add_npc(r.npc);
        add_text(T_RV_CHASE);
        add_num(r.player - r.value);
        add_text(T_RV_CHASE2);
        if (r.pace) add_text(r.final > r.pfinal ? T_RV_PACE_LOSE : T_RV_PACE_SAFE);
    } else {
        return 0;                                  /* 抜いた・抜かれたはマイページが知らせる */
    }
    return 1;
}

/* マーキーの次の 1 本: 年表の最新のお知らせ・ギルド伝言板・ライバル・みんなの動きを順に */
FAR(void, ticker_next, (void))
{
    FeedLine f;
    BoardList l;
    u16 tx;
    len = 0;
    g_ticker_buf[0] = 0;
    l.n = 0;
    if ((g_ticker_k & 3) == 2) {
        l.skip = (u8)((g_ticker_k >> 2) & 3);         /* 新しい 4 件を順に */
        l.want = 1;
        board_list(&l);
    }
    if ((g_ticker_k & 3) == 0 && timeline_news(0, &tx)) {
        add_text(tx);
    } else if (l.n) {
        add_text(T_BOARD_HEAD);
        post_text(&l.post[0]);
    } else if ((g_ticker_k & 3) == 3 && rival_text((u8)((g_ticker_k >> 2) & 1))) {
        /* ライバル（4 本に 1 本。イベント中は追ってくる人と交互） */
    } else {
        feed_line((u16)(g_ticker_k >> 1), &f);
        feed_text(&f);
    }
    g_ticker_k++;
}

/* g_ticker_buf を 32 桁で折り返して描く。次の y を返す（limit を超えたら止める） */
static u8 wrapped(u8 y, u8 fg, u8 bg, u8 limit)
{
    const char *s = g_ticker_buf;
    while (*s && y + 16 <= limit) {
        vdp_fill(0, y, SCREEN_W, 16, bg);
        txt_draw(0, y, s, fg, bg);
        s = g_txt_rest;
        y = (u8)(y + 16);
    }
    return y;
}

FAR(void, news_draw, (void))
{
    FeedLine f;
    u16 tx;
    u8 k, y, cx;
    ui_clear();
    ui_header(T_NEWS_TITLE);
    cx = txt_id(20, 0, T_PLAYERS, COL_LGRAY, COL_DBLUE);
    cx = txt_num(cx, 0, npc_active_count() + 1, COL_YELLOW, COL_DBLUE);
    txt_id(cx, 0, T_PERSON, COL_LGRAY, COL_DBLUE);
    for (k = 0, y = 18; k < 3 && timeline_news(k, &tx); k++) {
        len = 0;
        add_text(tx);
        y = wrapped(y, k == 0 ? COL_YELLOW : COL_WHITE, k & 1 ? COL_BLACK : COL_DGRAY, 98);
    }
    vdp_fill(0, 100, SCREEN_W, 16, COL_DBLUE);
    gfx_sprite(ATL_ICON_NEWS, 2, 96);
    txt_id(4, 100, T_FEED_TITLE, COL_WHITE, COL_DBLUE);
    for (k = 0, y = 118; k < 4; k++) {
        len = 0;
        feed_line(k, &f);
        feed_text(&f);
        y = wrapped(y, COL_WHITE, k & 1 ? COL_BLACK : COL_DGRAY, (u8)(y + 16));   /* 流れは 1 件 1 行 */
    }
    ui_guide(T_NEWS_GUIDE);
}

FAR(void, news_update, (void))
{
    u8 p = input_pressed();
    if (p & BTN_OK) { psg_beep(0x80, 4); screen_change(&scr_board); return; }
    if (input_digit() == 1) { psg_beep(0x80, 4); screen_change(&scr_album); return; }   /* 思い出アルバム */
    if (p & BTN_CANCEL) screen_change(&scr_home);
}

SCREEN(scr_news, 0, news_update, news_draw, 0)

/* ---- ギルド伝言板（新しい順。上下で 1 件ずつ前後へ） ---- */

#define BOARD_Y   18
#define BOARD_END 196

static u8 board_shown;          /* いま見えている件数（下へ送れるか） */

/* g_ticker_buf を 32 桁で折り返したときの行数 */
static u8 lines_of_buf(void)
{
    const u8 *s = (const u8 *)g_ticker_buf;
    u16 w = 0;
    while (*s) {
        u8 wide = (u8)((*s >= 0x81 && *s <= 0x9F) || *s >= 0xE0);
        if (wide && (w & 31) == 31) w++;          /* 2 桁の字は行末に入らず次の行へ */
        w += wide ? 2 : 1;
        s += wide ? 2 : 1;
    }
    return (u8)((w + 31) >> 5);
}

FAR(void, board_init, (void)) { g_cursor = 0; }

FAR(void, board_draw, (void))
{
    BoardList l;
    u8 k, y;
    ui_clear();
    ui_header(T_BOARD_TITLE);
    if (g_save.guild) txt_id(16, 0, (u16)(GUILD_TEXT + g_save.guild - 1), COL_YELLOW, COL_DBLUE);
    l.skip = g_cursor;
    l.want = BOARD_MAX;
    board_list(&l);
    board_shown = 0;
    if (!l.n) txt_id(2, 40, T_BOARD_EMPTY, COL_LGRAY, COL_BLACK);
    for (k = 0, y = BOARD_Y; k < l.n && y + 16 <= BOARD_END; k++) {
        const BoardPost *p = &l.post[k];
        len = 0;
        add_num((u32)p->day + 1);
        add_text(T_BOARD_DAY);
        add_str(" ");
        post_text(p);
        if (y + lines_of_buf() * 16 > BOARD_END) break;     /* 入りきらない件は次の画面で */
        y = wrapped(y, p->kind == BP_LEAVE ? COL_LGRAY : p->kind == BP_SYS ? COL_YELLOW : COL_WHITE, k & 1 ? COL_BLACK : COL_DGRAY, BOARD_END);
        board_shown++;
    }
    ui_guide(T_BOARD_GUIDE);
}

FAR(void, board_update, (void))
{
    u8 p = input_pressed();
    if ((p & BTN_UP) && g_cursor) { g_cursor--; psg_beep(0x100, 2); g_dirty = 1; }
    if ((p & BTN_DOWN) && board_shown > 1) { g_cursor++; psg_beep(0x100, 2); g_dirty = 1; }
    if (p & (BTN_OK | BTN_CANCEL)) screen_change(&scr_news);
}

SCREEN(scr_board, board_init, board_update, board_draw, 0)
