/* common.c — 画面の共通部品のうち、引数がレジスタで済むもの（UI_COMMON バンク）。
 * 上下の帯・状態欄（ゲージ）・中央の箱（メッセージ／はい・いいえ）・保存。 */
#include "widget.h"
#include "savedisk.h"
#include "../far.h"
#include "../ram.h"
#include "../config.h"
#include "../core/player.h"
#include "../core/wallet.h"
#include "../data/text_ids.h"
#include "../sys/disk.h"
#include "../sys/gfx.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define BOX_X 8
#define BOX_W 240

FAR(void, ui_header, (u16 id))
{
    vdp_fill(0, 0, SCREEN_W, 16, COL_DBLUE);
    vdp_fill(0, 0, 4, 16, COL_YELLOW);
    vdp_fill(0, 15, SCREEN_W, 1, COL_BLUE);
    txt_id(1, 0, id, COL_WHITE, COL_DBLUE);
}

FAR(void, ui_guide, (u16 id))
{
    vdp_fill(0, ROW_LAST, SCREEN_W, SCREEN_H - ROW_LAST, COL_DGRAY);
    txt_id(1, ROW_LAST, id, COL_LGRAY, COL_DGRAY);
}

/* 名前 Lv / AP ゲージ / EXP ゲージ / 所持金 / 軍資金。マイページとショップで使う（x=64..255 の 72 行） */
FAR(void, ui_status_block, (u8 y))
{
    u8 cx;
    vdp_fill(64, y, SCREEN_W - 64, 72, COL_BLACK);
    cx = txt_draw(9, y, g_save.name, COL_WHITE, COL_BLACK);
    cx = txt_id((u8)(cx + 1), y, T_LV, COL_YELLOW, COL_BLACK);
    txt_num(cx, y, g_save.level, COL_YELLOW, COL_BLACK);
    gfx_sprite(ATL_ITEM_AP, 70, (u8)(y + 19));
    gfx_gauge(88, (u8)(y + 23), 96, g_save.ap, AP_MAX, COL_GREEN);
    cx = txt_num_r(24, (u8)(y + 16), g_save.ap, 3, COL_WHITE, COL_BLACK);
    cx = txt_draw(cx, (u8)(y + 16), "/", COL_LGRAY, COL_BLACK);
    txt_num(cx, (u8)(y + 16), AP_MAX, COL_LGRAY, COL_BLACK);
    gfx_sprite(ATL_ITEM_EXP, 70, (u8)(y + 31));
    gfx_gauge(88, (u8)(y + 35), 96, g_save.exp, EXP_NEED(g_save.level), COL_CYAN);
    gfx_sprite(ATL_ITEM_GOLD, 70, (u8)(y + 42));
    cx = txt_num(11, (u8)(y + 40), g_save.gold, COL_YELLOW, COL_BLACK);
    gfx_sprite(ATL_ITEM_GEMS, (u8)(cx * 8 + 8), (u8)(y + 42));
    txt_num((u8)(cx + 3), (u8)(y + 40), g_save.gems, COL_CYAN, COL_BLACK);
    cx = txt_id(9, (u8)(y + 56), T_FUNDS, COL_ORANGE, COL_BLACK);
    if (wallet_infinite()) {
        txt_id((u8)(cx + 1), (u8)(y + 56), T_INFINITE, COL_YELLOW, COL_BLACK);
    } else {
        cx = txt_num((u8)(cx + 1), (u8)(y + 56), g_save.funds, COL_WHITE, COL_BLACK);
        txt_id(cx, (u8)(y + 56), T_YEN, COL_LGRAY, COL_BLACK);
    }
}

static void box(u8 lines)
{
    ui_box(BOX_X, 72, BOX_W, (u8)(16 * lines + 12), COL_DBLUE, COL_CYAN);
}

FAR(void, ui_msg, (u16 id))
{
    box(2);
    txt_id(2, 78, id, COL_WHITE, COL_DBLUE);
    ui_key(0, 234, 96);
    psg_beep(0x90, 4);
    ui_wait_key();
    g_dirty = 1;          /* 下の画面を描き直してもらう */
}

FAR(u8, ui_confirm, (u16 id))
{
    static const u16 yn[2] = { T_YES, T_NO };
    u8 cur = 0;
    box(3);
    txt_id(2, 78, id, COL_YELLOW, COL_DBLUE);
    ui_box(56, 96, 56, 20, cur == 0 ? COL_BLUE : COL_DBLUE, COL_WHITE);
    g_ui_state = UIS_WAITKEY;
    for (;;) {
        u8 p;
        u8 i;
        for (i = 0; i < 2; i++) {
            u8 x = (u8)(56 + i * 88);
            ui_box(x, 96, 56, 20, i == cur ? COL_BLUE : COL_DBLUE, i == cur ? COL_YELLOW : COL_LGRAY);
            txt_id((u8)(x / 8 + 2), 98, yn[i], COL_WHITE, i == cur ? COL_BLUE : COL_DBLUE);
        }
        do {
            ui_frame();
            p = input_pressed();
        } while (!p);
        if (p & (BTN_LEFT | BTN_RIGHT | BTN_UP | BTN_DOWN)) { cur ^= 1; psg_beep(0x100, 2); }
        if (p & BTN_CANCEL) { cur = 1; break; }
        if (p & BTN_OK) break;
    }
    g_ui_state = UIS_BUSY;
    g_dirty = 1;
    return (u8)(cur == 0);
}

FAR(void, ui_saving, (void))
{
    u8 e;
    g_ui_state = UIS_BUSY;          /* 書き込み中（自動試験がここでリセットしないように） */
    box(1);
    txt_id(12, 78, T_SAVING, COL_WHITE, COL_DBLUE);
    e = savedisk_save();
    if (e == DISK_OK) {
        g_save_dirty = 0;
    } else if (!g_nosave_warned) {
        g_nosave_warned = 1;
        ui_msg(T_SAVE_FAIL);
    }
    g_dirty = 1;
}
