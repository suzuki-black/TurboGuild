/* SPDX-License-Identifier: MIT */
/* credits_ui.c — スタッフロール（仕様 §3.1）とライセンスの画面（§11.5）。
 * スタッフロールは VDP の縦スクロール（R#23）で 1 ドットずつ流す。画面の下の外（212〜255 行）に
 * 次の行を描いておき、スクロールで見えてくる。行は 16 ドット刻みなので 256 行の中で折り返しても切れない。
 * サービス終了で一度だけ流し（g_save.credits_seen）、あとはタイトルの「スタッフロール」から何度でも。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../data/bgm_data.h"
void bgm_play(u8 song);
#include "../ram.h"
#include "../config.h"
#include "../core/tables.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define ROLL_END ((u16)(CREDIT_COUNT * 16 + 96))   /* 最後の行が画面の中ほどに来たら止める */

static void put_line(u16 k, u8 y)
{
    vdp_fill(0, y, SCREEN_W, 16, COL_BLACK);
    if (k < CREDIT_COUNT) {
        u16 id = (u16)(CREDIT_TEXT + k);
        u8 w = txt_id_width(id);
        txt_id((u8)((32 - w) / 2), y, id, k == 0 ? COL_YELLOW : COL_WHITE, COL_BLACK);
    }
}

FAR(void, credits_init, (void))
{
    bgm_play(BGM_HOME);
    g_cred_pos = 0;
}

FAR(void, credits_draw, (void))
{
    vdp_wreg(23, 0);
    vdp_fill(0, 0, SCREEN_W, 256, COL_BLACK);
    g_ui_state = UIS_IDLE;
}

static void finish(void)
{
    vdp_wreg(23, 0);
    if (!g_save.credits_seen) {
        g_save.credits_seen = 1;
        g_save_dirty = 1;
        ui_saving();
    }
    screen_change(&scr_title);
}

FAR(void, credits_update, (void))
{
    u8 p = input_pressed();
    if (p & BTN_CANCEL) { finish(); return; }        /* ESC で飛ばせる */
    if (g_cred_pos >= ROLL_END) {                    /* 流し終えたら押下を待つ */
        g_ui_state = UIS_WAITKEY;
        if (p & BTN_OK) finish();
        return;
    }
    if (g_frames & 1) return;                        /* 2 フレームに 1 ドット */
    if ((g_cred_pos & 15) == 0) put_line(g_cred_pos >> 4, (u8)(g_cred_pos + 208));
    g_cred_pos++;
    vdp_wreg(23, (u8)g_cred_pos);
}

SCREEN(scr_credits, credits_init, credits_update, credits_draw, 0)

/* ---- ライセンス（タイトルの [0]） ---- */

FAR(void, license_draw, (void))
{
    ui_clear();
    ui_header(T_LICENSE_TITLE);
    {                                             /* ROM の版と言語（セーブにも書く, DECISIONS #78） */
        u8 cx = txt_draw(2, 30, "TurboGuild v", COL_YELLOW, COL_BLACK);
        cx = txt_num(cx, 30, GAME_VER_MAJOR, COL_YELLOW, COL_BLACK);
        cx = txt_draw(cx, 30, ".", COL_YELLOW, COL_BLACK);
        cx = txt_num(cx, 30, GAME_VER_MINOR, COL_YELLOW, COL_BLACK);
        cx = txt_draw(cx, 30, ".", COL_YELLOW, COL_BLACK);
        cx = txt_num(cx, 30, GAME_VER_PATCH, COL_YELLOW, COL_BLACK);
        txt_draw((u8)(cx + 1), 30, GAME_LANG == LANG_EN_CODE ? "(EN)" : "(JA)", COL_LGRAY, COL_BLACK);
    }
    txt_draw(2, 54, "MIT License", COL_WHITE, COL_BLACK);
    txt_draw(1, 72, "Copyright (c) 2026 suzuki-black", COL_WHITE, COL_BLACK);
    txt_id(2, 100, T_LICENSE_SRC, COL_LGRAY, COL_BLACK);
    txt_draw(2, 118, "https://github.com/", COL_CYAN, COL_BLACK);
    txt_draw(4, 136, "suzuki-black/TurboGuild", COL_CYAN, COL_BLACK);
    txt_id(2, 162, T_LICENSE_NOTE, COL_LGRAY, COL_BLACK);
    ui_guide(T_HELP_BACK);
}

FAR(void, license_update, (void))
{
    if (input_pressed() & (BTN_OK | BTN_CANCEL)) screen_change(&scr_title);
}

SCREEN(scr_license, 0, license_update, license_draw, 0)
