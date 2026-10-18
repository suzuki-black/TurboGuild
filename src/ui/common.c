/* common.c — 画面の共通部品のうち、呼び出しが少なく引数がレジスタで済むもの（UI_COMMON バンク）。
 * カーネル 8KB を空けるため widget.c から分けた。呼ぶ側は widget.h だけを見ればよい。 */
#include "widget.h"
#include "savedisk.h"
#include "../far.h"
#include "../ram.h"
#include "../config.h"
#include "../core/tables.h"
#include "../data/text_ids.h"
#include "../sys/disk.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

FAR(void, ui_status, (u8 y))
{
    u8 cx;
    ui_fill_rows(y, 1, COL_DGREEN);
    cx = txt_draw(0, y, g_save.name, COL_WHITE, COL_DGREEN);
    cx = txt_id((u8)(cx + 1), y, T_LV, COL_LYELLOW, COL_DGREEN);
    cx = txt_num(cx, y, g_save.level, COL_WHITE, COL_DGREEN);
    cx = txt_id((u8)(cx + 1), y, T_AP, COL_LYELLOW, COL_DGREEN);
    cx = txt_num_r(cx, y, g_save.ap, 3, COL_WHITE, COL_DGREEN);
    cx = txt_draw(cx, y, "/", COL_GRAY, COL_DGREEN);
    txt_num(cx, y, AP_MAX, COL_GRAY, COL_DGREEN);
}

FAR(void, ui_money, (u8 y))
{
    u8 cx;
    ui_fill_rows(y, 1, COL_BLACK);
    cx = txt_id(0, y, T_GOLD, COL_LYELLOW, COL_BLACK);
    cx = txt_num((u8)(cx + 1), y, g_save.gold, COL_WHITE, COL_BLACK);
    cx = txt_id((u8)(cx + 1), y, T_GEMS, COL_PINK, COL_BLACK);
    cx = txt_num(cx, y, g_save.gems, COL_WHITE, COL_BLACK);
    cx = txt_id((u8)(cx + 1), y, T_POTION, COL_GREEN, COL_BLACK);
    txt_num(cx, y, g_save.potions, COL_WHITE, COL_BLACK);
}

FAR(void, ui_msg, (u16 id))
{
    ui_fill_rows(ROW(11), 1, COL_DRED);
    txt_id(1, ROW(11), id, COL_WHITE, COL_DRED);
    psg_beep(0x90, 4);
    ui_wait_key();
    ui_fill_rows(ROW(11), 1, COL_BLACK);
}

FAR(u8, ui_confirm, (u16 id))
{
    static const u16 yn[2] = { T_YES, T_NO };
    u8 cur = 0;
    ui_fill_rows(ROW(8), 3, COL_BLACK);
    txt_id(1, ROW(8), id, COL_LYELLOW, COL_BLACK);
    ui_menu_draw(4, ROW(9), yn, 2, cur);
    g_ui_state = UIS_WAITKEY;
    for (;;) {
        u8 p;
        ui_frame();
        p = input_pressed();
        if (ui_menu_input(&cur, 2, p)) ui_menu_draw(4, ROW(9), yn, 2, cur);
        if (p & BTN_CANCEL) cur = 1;
        if (p & (BTN_OK | BTN_CANCEL)) break;
    }
    ui_fill_rows(ROW(8), 3, COL_BLACK);
    return (u8)(cur == 0);
}

/* 1 枚ずつ表示。レアほど高い音、SSR は行を光らせてから出す（当時の「確定演出」のつもり） */
FAR(void, ui_reveal, (void))
{
    const u16 *ids = g_gacha.ids;
    u8 n = g_gacha.got, y0 = ROW(1);
    static const u16 tone[4] = { 0x200, 0x140, 0x0C0, 0x060 };
    CardRec c;
    u8 i;
    for (i = 0; i < n; i++) {
        u8 y = (u8)(y0 + i * LINE_H);
        tbl_card(ids[i], &c);
        if (c.rarity == RARITY_SSR) {
            u8 k;
            for (k = 0; k < 3; k++) {
                ui_fill_rows(y, 1, COL_PINK);
                psg_beep(0x050, 4);
                ui_wait(5);
                ui_fill_rows(y, 1, COL_BLACK);
                ui_wait(4);
            }
        }
        ui_card_row(y, ids[i], 1, 0, c.rarity >= RARITY_SR ? COL_DRED : COL_BLACK);
        psg_beep(tone[c.rarity], c.rarity >= RARITY_SR ? 10 : 3);
        ui_wait(c.rarity >= RARITY_SR ? 20 : 6);
    }
}

FAR(void, ui_saving, (void))
{
    u8 e;
    ui_fill_rows(ROW(11), 1, COL_BLUE);
    txt_id(1, ROW(11), T_SAVING, COL_WHITE, COL_BLUE);
    e = savedisk_save();
    ui_fill_rows(ROW(11), 1, COL_BLACK);
    if (e == DISK_OK) {
        g_save_dirty = 0;
    } else if (!g_nosave_warned) {
        g_nosave_warned = 1;
        ui_msg(T_SAVE_FAIL);
    }
}
