/* home.c — ホーム。上に名前・Lv・AP と所持金、中央にメニュー、下にお知らせ（仕様 §9）。
 * 入るたびにログインボーナスを確かめ、保存していない変更があれば「通信中…」で保存する。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../core/login.h"
#include "../core/player.h"
#include "../core/time.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"

#define ITEMS  6
#define MENU_Y ROW(4)

static const u16 items[ITEMS] = {
    T_HOME_QUEST, T_HOME_GACHA, T_HOME_CARD, T_HOME_SHOP, T_HOME_SAVE, T_HOME_TITLE,
};

static void draw_info(void)
{
    u8 cx;
    ui_fill_rows(ROW(10), 1, COL_BLACK);
    cx = txt_num(1, ROW(10), time_gday(g_save.g_time) + 1, COL_WHITE, COL_BLACK);
    cx = txt_id(cx, ROW(10), T_DAY, COL_GRAY, COL_BLACK);
    cx = txt_id((u8)(cx + 2), ROW(10), T_CARDS, COL_GRAY, COL_BLACK);
    cx = txt_num((u8)(cx + 1), ROW(10), g_save.card_count, COL_WHITE, COL_BLACK);
    cx = txt_draw(cx, ROW(10), "/", COL_GRAY, COL_BLACK);
    txt_num(cx, ROW(10), CARD_MAX, COL_GRAY, COL_BLACK);
}

static void show_login_bonus(void)
{
    u8 cx;
    ui_fill_rows(ROW(4), 5, COL_BLACK);
    ui_fill_rows(ROW(4), 1, COL_DRED);
    txt_id(1, ROW(4), T_LOGIN_TITLE, COL_LYELLOW, COL_DRED);
    cx = txt_num(3, ROW(6), g_login.day, COL_WHITE, COL_BLACK);
    txt_id(cx, ROW(6), T_LOGIN_DAY, COL_WHITE, COL_BLACK);
    cx = txt_id(3, ROW(7), (u16)(T_ITEM_0 + g_login.item), COL_LYELLOW, COL_BLACK);
    cx = txt_draw((u8)(cx + 1), ROW(7), "x", COL_GRAY, COL_BLACK);
    txt_num(cx, ROW(7), g_login.amount, COL_WHITE, COL_BLACK);
    psg_beep(0x060, 12);
    ui_wait_key();
}

FAR(void, home_init, (void))
{
    g_cursor = g_menu_pos;
    player_ap_update();
    login_check(&g_login);
    if (g_login.given) g_save_dirty = 1;
    g_ui_prev = g_save.ap;
}

FAR(void, home_draw, (void))
{
    ui_clear();
    ui_status(ROW(0));
    ui_money(ROW(1));
    ui_fill_rows(ROW(2), 1, COL_DRED);
    txt_id(1, ROW(2), T_BANNER_NONE, COL_WHITE, COL_DRED);
    if (g_login.given) {
        show_login_bonus();
        g_login.given = 0;
        ui_money(ROW(1));
        ui_fill_rows(ROW(4), 5, COL_BLACK);
    }
    ui_menu_draw(9, MENU_Y, items, ITEMS, g_cursor);
    draw_info();
    ui_footer(T_HELP_SELECT);
    if (g_save_dirty) ui_saving();     /* 「通信中…」は ROW(11) を使うので、お知らせはその後 */
    txt_id(0, ROW(11), T_NEWS_0, COL_GRAY, COL_BLACK);
}

static void open(u8 i)
{
    g_menu_pos = i;
    switch (i) {
    case 0: screen_change(&scr_area); break;
    case 1: screen_change(&scr_gacha); break;
    case 2: screen_change(&scr_cardlist); break;
    case 3: screen_change(&scr_shop); break;
    case 4:
        g_save_dirty = 1;
        ui_saving();
        if (!g_save_dirty) ui_msg(T_SAVED);
        txt_id(0, ROW(11), T_NEWS_0, COL_GRAY, COL_BLACK);
        break;
    default: screen_change(&scr_title); break;
    }
}

FAR(void, home_update, (void))
{
    u8 p = input_pressed();
    if (ui_menu_input(&g_cursor, ITEMS, p)) ui_menu_draw(9, MENU_Y, items, ITEMS, g_cursor);
    if (p & BTN_OK) {
        psg_beep(0x80, 4);
        open(g_cursor);
    }
    if (g_frames == 0) {           /* 1 秒に 1 回: AP の回復を反映 */
        player_ap_update();
        if (g_save.ap != g_ui_prev) {
            g_ui_prev = g_save.ap;
            ui_status(ROW(0));
        }
    }
}

SCREEN(scr_home, home_init, home_update, home_draw, 0)
