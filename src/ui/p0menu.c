/* p0menu.c — Phase0 の起動画面。タイトルと起動診断（HIMEM・空き RAM・CPU）と試験メニュー */
#include "screen.h"
#include "../ram.h"
#include "../config.h"
#include "../sys/font.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/vdp.h"

#define ITEMS 4
static const char * const items[ITEMS] = {
    "BANK SWITCH TEST", "CARD TABLE", "SAVE / RTC", "INPUT / SOUND",
};
static const Screen * const targets[ITEMS] = {
    &scr_banktest, &scr_cardtest, &scr_savetest, &scr_inputtest,
};
static const char * const cpu_names[3] = { "Z80", "R800 ROM", "R800 DRAM" };

static void draw_diag(void)
{
    u16 himem = SYS_HIMEM;
    u8 cpu = bios_getcpu();
    font_puts(1, 4, "HIMEM", 7, 1);      font_puthex(12, 4, himem, 4, 15, 1);
    font_puts(1, 5, "RAM C000-HIMEM", 7, 1); font_putdec(17, 5, (u32)(himem - RAM_BASE), 15, 1);
    font_puts(1, 6, "STATIC USED", 7, 1);    font_putdec(17, 6, (u32)(ram_end - RAM_BASE), 15, 1);
    font_puts(1, 7, "FREE (STACK)", 7, 1);   font_putdec(17, 7, (u32)(himem - ram_end), 15, 1);
    font_puts(1, 8, "MSXVER", 7, 1);         font_putdec(12, 8, BIOS_MSXVER, 15, 1);
    font_puts(1, 9, "CPU", 7, 1);            font_puts(12, 9, cpu < 3 ? cpu_names[cpu] : "?", 15, 1);
    font_puts(1, 10, "CART SLOT", 7, 1);     font_puthex(12, 10, g_cart_slot, 2, 15, 1);
}

static void draw_items(void)
{
    u8 i;
    for (i = 0; i < ITEMS; i++) {
        u8 sel = (u8)(i == g_cursor);
        font_puts(2, (u8)(13 + i), sel ? ">" : " ", 11, 1);
        font_puts(4, (u8)(13 + i), items[i], sel ? 11 : 15, 1);
    }
}

static void init(void) { g_cursor = g_menu_pos; }

static void update(void)
{
    u8 p = input_pressed();
    if (p & BTN_UP)   { g_cursor = (u8)((g_cursor + ITEMS - 1) % ITEMS); psg_beep(0x100, 2); draw_items(); }
    if (p & BTN_DOWN) { g_cursor = (u8)((g_cursor + 1) % ITEMS); psg_beep(0x100, 2); draw_items(); }
    if (p & BTN_OK)   { psg_beep(0x80, 4); g_menu_pos = g_cursor; screen_change(targets[g_cursor]); }
}

static void draw(void)
{
    ui_clear();
    ui_header("TurboGuild  Phase0");
    vdp_fill(64, 16, 128, 8, 9);
    font_puts(11, 2, "TurboGuild", 15, 9);
    draw_diag();
    draw_items();
    ui_footer("UP/DOWN:SELECT  SPACE:OK");
}

const Screen scr_p0menu = { init, update, draw, 0 };
