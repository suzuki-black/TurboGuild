/* SPDX-License-Identifier: MIT */
/* wiki_ui.c — まとめ記事（DECISIONS #97, 追加案 60 の 60 番）。サービス終了後に、百科事典ふうの「TurboGuild（サービス終了）」の項目。
 * 概要と推移は決まった文、「あるプレイヤー」と「評価」は自分のセーブ（課金・所属・順位・残った仲間・運営モードの成績）から組む。
 * 思い出（アルバム）の 4 で開く。上下で段落を送る。バンク 102 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../core/board.h"
#include "../core/memo.h"
#include "../core/opsim.h"
#include "../core/save.h"
#include "../data/screens.h"
#include "../data/tables.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define WK_MAX  14
#define WK_END  196

typedef struct { u16 text; u32 num; u8 fg; } Para;

/* 記事の段落を並べる。段落の数 */
static u8 paras(Para *p)
{
    Roster r;
    u8 n = 0;
#define P(t, v, c) do { p[n].text = (t); p[n].num = (v); p[n].fg = (c); n++; } while (0)
    P(T_WK_HEAD, 0, COL_YELLOW);
    P(T_WK_P1, 0, COL_WHITE);
    P(T_WK_H_HIST, 0, COL_CYAN);
    P(T_WK_HIST1, 0, COL_WHITE);
    P(T_WK_HIST2, 0, COL_WHITE);
    P(T_WK_H_PL, 0, COL_CYAN);
    if (g_save.guild && g_save.guild <= GUILD_COUNT) P(T_WK_PL1, g_save.spent, COL_WHITE);
    else P(T_WK_PL1N, g_save.spent, COL_WHITE);
    if (g_save.memo[MEMO_TOP10]) P(T_WK_PL_TOP10, 0, COL_WHITE);
    else if (g_save.memo[MEMO_TOP100]) P(T_WK_PL_TOP100, 0, COL_WHITE);
    board_roster(&r);                                /* 終わった後にギルドにいる = 最後まで残った仲間 */
    P(r.n ? T_WK_PL_MATES : T_WK_PL_ALONE, r.n, COL_WHITE);
    P(T_WK_H_EVAL, 0, COL_CYAN);
    if (g_save.ops_flags & OPS_SAVED) P(T_WK_EV_SAVED, 0, COL_WHITE);
    else if (g_save.ops_flags & OPS_PLAYED) P(T_WK_EV_OPS, g_save.ops_best / 10000, COL_WHITE);
    else P(T_WK_EV0, 0, COL_WHITE);
    P(T_WK_LAST, 0, COL_LGRAY);
#undef P
    return n;
}

/* 段落を写して $ に名前、# にギルドの名前、% に数を入れる */
static void build(char *b, u8 cap, const Para *pa)
{
    char tmp[128];                                   /* 段落は 60 字まで（スタックが小さい） */
    const char *s = txt_ptr(pa->text);
    u8 n = 0, k, m = 0;
    while (s[m] && m < sizeof(tmp) - 1) { tmp[m] = s[m]; m++; }
    tmp[m] = 0;
    for (m = 0; tmp[m] && n < cap - 1; m++) {
        if (tmp[m] == '$') {
            for (k = 0; k < sizeof(g_save.name) && g_save.name[k] && n < cap - 1; k++) b[n++] = g_save.name[k];
        } else if (tmp[m] == '#') {
            const char *g = txt_ptr((u16)(GUILD_TEXT + g_save.guild - 1));
            while (*g && n < cap - 1) b[n++] = *g++;
        } else if (tmp[m] == '%') {
            char d[11];
            u32 v = pa->num;
            u8 c = 0;
            do { d[c++] = (char)('0' + v % 10); v /= 10; } while (v && c < 10);
            while (c && n < cap - 1) b[n++] = d[--c];
        } else b[n++] = tmp[m];
    }
    b[n] = 0;
}

FAR(void, wiki_draw, (void))
{
    Para p[WK_MAX];
    char b[160];
    u8 np = paras(p), k, y = 18;
    ui_clear();
    ui_header(T_WK_TITLE);
    if (g_cursor >= np) g_cursor = (u8)(np - 1);
    for (k = g_cursor; k < np && y + 16 <= WK_END; k++) {
        const char *s = b;
        build(b, sizeof(b), &p[k]);
        while (*s && y + 16 <= WK_END) {
            txt_draw(1, y, s, p[k].fg, COL_BLACK);
            s = g_txt_rest;
            y = (u8)(y + 16);
        }
        y = (u8)(y + 4);
    }
    ui_guide(T_WK_GUIDE);
}

FAR(void, wiki_update, (void))
{
    u8 p = input_pressed();
    if ((p & BTN_UP) && g_cursor) { g_cursor--; psg_beep(0x100, 2); g_dirty = 1; }
    if ((p & BTN_DOWN) && g_cursor < WK_MAX - 1) { g_cursor++; psg_beep(0x100, 2); g_dirty = 1; }
    if (p & (BTN_OK | BTN_CANCEL)) screen_change(&scr_album);
}

SCREEN(scr_wiki, 0, wiki_update, wiki_draw, 0)
