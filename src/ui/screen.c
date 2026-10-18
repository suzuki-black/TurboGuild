/* screen.c — 画面遷移とメインループ。1 秒ごとに RTC からゲーム時間を進める */
#include "screen.h"
#include "../ram.h"
#include "../config.h"
#include "../sys/vdp.h"
#include "../sys/input.h"
#include "../sys/psg.h"

#define cur      g_screen_cur
#define next_scr g_screen_next

void screen_change(const Screen *next) { next_scr = next; }

static void apply_change(void)
{
    if (cur && cur->exit) cur->exit();
    cur = next_scr;
    next_scr = 0;
    g_cursor = 0;
    if (cur->init) cur->init();
    g_dirty = 1;
}

void screen_run(void)
{
    for (;;) {
        vdp_wait_vsync();
        input_update();
        psg_tick();
        if (++g_frames >= 60) {
            g_frames = 0;
            time_sync();
        }
        if (next_scr) apply_change();
        g_ui_state = UIS_IDLE;
        cur->update();
        if (g_dirty && !next_scr) {
            g_dirty = 0;
            g_ui_state = UIS_BUSY;
            cur->draw();
        }
    }
}
