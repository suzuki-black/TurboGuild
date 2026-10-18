/* p0save.c — Phase0 のセーブ試験。起動時ロードの結果と RTC 差分の加算量を見せ、OK で保存する。
 * 受け入れ: 保存 → リセット → 起動で、止まっていた現実時間 × TIME_SCALE が g_time に足される。 */
#include "screen.h"
#include "../far.h"
#include "../data/screens.h"
#include "../sys/vdp.h"
#include "savedisk.h"
#include "../ram.h"
#include "../core/time.h"
#include "../sys/disk.h"
#include "../sys/font.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/rtc.h"

/* 上書きで前の文字が残らないよう同じ長さに揃える */
static const char * const sd_names[5] = { "LOADED       ", "NEW (EMPTY)  ", "NOT SAVE DISK", "NO DISK      ", "DISK ERROR   " };
static const char * const ts_names[3] = { "OK         ", "BACKWARD   ", "CLAMPED 30D" };
static const char * const de_names[6] = { "OK           ", "WRITE PROTECT", "NO DISK      ", "CRC ERROR    ", "ERROR        ", "NO FDD       " };

static void put2(u8 cx, u8 cy, u8 v)
{
    font_putc(cx, cy, (u8)('0' + v / 10), 15, 1);
    font_putc((u8)(cx + 1), cy, (u8)('0' + v % 10), 15, 1);
}

static void draw_clock(void)
{
    DateTime dt;
    rtc_read(&dt);
    font_putdec(8, 3, dt.year, 15, 1);
    put2(13, 3, dt.mon);  put2(16, 3, dt.day);
    put2(19, 3, dt.hour); put2(22, 3, dt.min); put2(25, 3, dt.sec);
    g_ui_prev = dt.sec;
}

static void draw_state(void)
{
    font_puts(1, 5, "LOAD", 7, 1);       font_puts(12, 5, sd_names[g_save_status], 15, 1);
    font_puts(1, 6, "TIME", 7, 1);       font_puts(12, 6, ts_names[g_time_status], 15, 1);
    font_puts(1, 7, "ADDED GSEC", 7, 1); font_putdec(12, 7, g_time_added, 15, 1);
    font_puts(1, 8, "G_TIME", 7, 1);     font_putdec(12, 8, g_save.g_time, 15, 1);
    font_puts(1, 9, "GAME DAY", 7, 1);   font_putdec(12, 9, time_gday(g_save.g_time), 15, 1);
    font_puts(1, 10, "SAVE COUNT", 7, 1); font_putdec(12, 10, g_save.save_count, 15, 1);
    font_puts(1, 11, "NEXT SLOT", 7, 1); font_puts(12, 11, g_save_slot ? "B" : "A", 15, 1);
    font_puts(1, 12, "SAVE SIZE", 7, 1); font_putdec(12, 12, sizeof(SaveData), 15, 1);
}

FAR(void, p0save_update, (void))
{
    u8 p = input_pressed();
    if (rtc_sec() != g_ui_prev) draw_clock();
    if (p & BTN_OK) {
        u8 e;
        font_puts(1, 15, "SAVING...             ", 11, 1);
        e = savedisk_save();
        font_puts(1, 15, "SAVE:                 ", 7, 1);
        font_puts(7, 15, de_names[e], e ? 8 : 3, 1);
        psg_beep(e ? 0x300 : 0x60, 6);
        draw_state();
    }
    if (p & BTN_CANCEL) screen_change(&scr_p0menu);
}

FAR(void, p0save_draw, (void))
{
    vdp_fill(0, 0, SCREEN_W, SCREEN_H, 1);
    dbg_header("SAVE / RTC");
    font_puts(1, 3, "RTC", 7, 1);
    draw_clock();
    draw_state();
    dbg_footer("SPACE:SAVE  ESC:BACK");
}

SCREEN(scr_savetest, 0, p0save_update, p0save_draw, 0)
