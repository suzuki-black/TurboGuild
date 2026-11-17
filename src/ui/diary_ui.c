/* SPDX-License-Identifier: MIT */
/* diary_ui.c — 仲間の日記（DECISIONS #110）。メンバー一覧のプロフィールからスペースで開く。
 * 新しい日が上。1 ページ 5 日ぶん、上下でめくる。画面を替えずにここで待つ（静的 RAM を使わない）。 */
#include "widget.h"
#include "../config.h"
#include "../far.h"
#include "../ram.h"
#include "../core/board.h"
#include "../core/save.h"
#include "../core/tables.h"
#include "../core/time.h"
#include "../data/tables.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define DY_Y    24
#define DY_H    32          /* 1 日ぶん = 2 行 */
#define DY_PAGE 5
#define DY_COL  6           /* 本文の桁（左は日付） */
#define DY_W    26          /* 本文の幅（桁） */

static u8 name_at(u8 cx, u8 y, u16 npc, u8 fg, u8 bg)
{
    NpcRec n;
    tbl_npc(npc, &n);
    cx = txt_id(cx, y, (u16)(NPC_PRE_TEXT + n.name_id / NPC_SUF_COUNT), fg, bg);
    return txt_id(cx, y, (u16)(NPC_SUF_TEXT + n.name_id % NPC_SUF_COUNT), fg, bg);
}

/* 日付（0 日目 = 2011 年 7 月 1 日）を「12/24」の形で */
static void date_at(u8 cx, u8 y, u16 d, u8 bg)
{
    static const u8 mlen[12] = { 31, 31, 30, 31, 30, 31, 31, 28, 31, 30, 31, 30 };
    u8 m = 0;
    d %= 365;
    while (m < 11 && d >= mlen[m]) d -= mlen[m++];
    cx = txt_num(cx, y, (u8)((m + 6) % 12 + 1), COL_CYAN, bg);
    cx = txt_draw(cx, y, "/", COL_CYAN, bg);
    txt_num(cx, y, d + 1, COL_CYAN, bg);
}

/* SJIS の 1 文字のバイト数（全角の先頭なら 2） */
static u8 clen(u8 c) { return (u8)(((c >= 0x81 && c <= 0x9F) || c >= 0xE0) ? 2 : 1); }

/* 本文（% に param の文言を入れ、DY_W 桁で 2 行まで折り返す）。英語は語の切れ目で、日本語は 2 行の長さをそろえて切る */
static void body(u8 y, const DiaryRow *o, u8 bg)
{
    char b[96], line[48];
    const char *s = txt_ptr(o->text);
    u8 n = 0, k, w = 0, i, cut, sp, cols;
    while (*s && n < sizeof(b) - 1) {
        if (*s == '%' && o->param) {
            const char *p;
            char rest[64];
            u8 r = 0;
            s++;
            while (*s && r < sizeof(rest) - 1) rest[r++] = *s++;   /* 文言を開くと窓が替わるので、続きを先に写す */
            rest[r] = 0;
            p = txt_ptr(o->param);
            while (*p && n < sizeof(b) - 1) b[n++] = *p++;
            for (k = 0; rest[k] && n < sizeof(b) - 1; k++) b[n++] = rest[k];
            break;
        }
        b[n++] = *s++;
    }
    b[n] = 0;
    for (i = 0; b[i]; i += clen((u8)b[i])) w = (u8)(w + (clen((u8)b[i]) == 2 ? 2 : 1));
    i = 0;
    for (k = 0; k < 2 && b[i]; k++) {
        /* この行に入れるバイト数を決める */
        u8 lim = DY_W;
        if (k == 0 && w > DY_W && GAME_LANG != LANG_EN_CODE) lim = (u8)((w + 1) / 2 + 1);   /* 日本語は半分ずつ */
        cut = i; sp = 0; cols = 0;
        while (b[cut]) {
            u8 l = clen((u8)b[cut]), cw = (u8)(l == 2 ? 2 : 1);
            if (cols + cw > lim) break;
            if (b[cut] == ' ') sp = cut;
            cols = (u8)(cols + cw);
            cut = (u8)(cut + l);
        }
        if (b[cut] && sp > i && GAME_LANG == LANG_EN_CODE) cut = sp;       /* 語の途中で切らない */
        while ((u8)b[cut] == 0x81 && ((u8)b[cut + 1] == 0x41 || (u8)b[cut + 1] == 0x42 || (u8)b[cut + 1] == 0x63 || (u8)b[cut + 1] == 0x6A || (u8)b[cut + 1] == 0x49))
            cut = (u8)(cut + 2);                                        /* 、。…）！ は行の頭に置かない */
        for (n = 0; i < cut && n < sizeof(line) - 1; ) line[n++] = b[i++];
        line[n] = 0;
        txt_draw(DY_COL, (u8)(y + k * 16), line, COL_WHITE, bg);
        while (b[i] == ' ') i++;
    }
}

/* page ページ目（0 = いちばん新しい）を描く。次のページがあれば 1 */
static u8 draw_page(u16 npc, u8 page)
{
    DiaryRow o;
    u16 d = (u16)time_gday(g_save.g_time), stop = d > DIARY_BACK ? (u16)(d - DIARY_BACK) : 0;
    u16 skip = (u16)page * DY_PAGE;
    u8 n = 0, y, bg;
    vdp_fill(0, DY_Y, SCREEN_W, DY_H * DY_PAGE, COL_BLACK);
    o.npc = npc;
    for (;;) {
        o.day = d;
        if (diary_at(&o)) {
            if (skip) skip--;
            else {
                if (n == DY_PAGE) return 1;
                y = (u8)(DY_Y + n * DY_H);
                bg = (n & 1) ? COL_BLACK : COL_DGRAY;
                vdp_fill(0, y, SCREEN_W, DY_H, bg);
                date_at(0, y, d, bg);
                body(y, &o, bg);
                n++;
            }
        }
        if (d == stop) break;
        d--;
    }
    if (!n && !page) txt_id(2, DY_Y + 8, T_DY_NONE, COL_LGRAY, COL_BLACK);
    return 0;
}

FAR(void, ui_diary, (u16 npc))
{
    u8 page = 0, more, p;
    ui_clear();
    ui_header(T_DY_TITLE);
    vdp_fill(8, 0, SCREEN_W - 8, 15, COL_DBLUE);
    txt_id(name_at(1, 0, npc, COL_WHITE, COL_DBLUE), 0, T_DY_TITLE, COL_WHITE, COL_DBLUE);
    ui_guide(T_DY_GUIDE);
    more = draw_page(npc, 0);
    psg_beep(0x80, 4);
    for (;;) {
        g_ui_state = UIS_WAITKEY;
        do { ui_frame(); p = input_pressed(); } while (!(p & (BTN_UP | BTN_DOWN | BTN_OK | BTN_CANCEL)));
        g_ui_state = UIS_BUSY;
        if (p & BTN_OK) break;
        if (p & BTN_CANCEL) break;
        if ((p & BTN_DOWN) && more) { page++; psg_beep(0x100, 2); more = draw_page(npc, page); }
        else if ((p & BTN_UP) && page) { page--; psg_beep(0x100, 2); more = draw_page(npc, page); }
    }
    g_dirty = 1;
}
