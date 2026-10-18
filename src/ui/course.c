/* course.c — 新規開始時にお財布（軍資金のコース）を選ぶ。1〜4 キーでも選べる。
 * 無敵モードは軍資金が減らない（ソシャゲで無双した気分だけ味わう, DECISIONS #33）。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../core/tables.h"
#include "../core/wallet.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/gfx.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define Y0 52

static void row(u8 i)
{
    CourseRec c;
    u8 on = (u8)(i == g_cursor), y = (u8)(Y0 + i * 30), bg = on ? COL_BLUE : COL_DBLUE;
    tbl_course((u8)(i + 1), &c);
    ui_box(8, y, 240, 28, bg, on ? COL_YELLOW : COL_DGRAY);
    ui_key((u8)(i + 1), 14, (u8)(y + 8));
    gfx_sprite(c.infinite ? ATL_SPARK : ATL_ITEM_GOLD, 32, (u8)(y + 7));
    txt_id(6, (u8)(y + 6), c.name_text, on ? COL_YELLOW : COL_WHITE, bg);
}

FAR(void, course_draw, (void))
{
    u8 i;
    ui_clear();
    ui_header(T_COURSE_TITLE);
    txt_id(1, 26, T_COURSE_NOTE, COL_LGRAY, COL_BLACK);
    for (i = 0; i < COURSE_COUNT; i++) row(i);
    ui_guide(T_GUIDE_LIST);
}

FAR(void, course_update, (void))
{
    u8 p = input_pressed();
    u8 d = input_digit();
    u8 old = g_cursor;
    if (ui_menu_input(&g_cursor, COURSE_COUNT, p)) { row(old); row(g_cursor); }
    if (d >= 1 && d <= COURSE_COUNT) { g_cursor = (u8)(d - 1); p = BTN_OK; }
    if (p & BTN_OK) {
        psg_beep(0x060, 8);
        wallet_start((u8)(g_cursor + 1));
        screen_change(&scr_firstgacha);
    }
    if (p & BTN_CANCEL) screen_change(&scr_name);
}

SCREEN(scr_course, 0, course_update, course_draw, 0)
