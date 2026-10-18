/* screen.c — 画面遷移と、どの画面でも使う見出し・帯の描画 */
#include "screen.h"
#include "../ram.h"
#include "../sys/vdp.h"
#include "../sys/font.h"
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
        if (next_scr) apply_change();
        cur->update();
        if (g_dirty) {
            g_dirty = 0;
            cur->draw();
        }
    }
}

void ui_clear(void)
{
    vdp_fill(0, 0, SCREEN_W, SCREEN_H, 1);
}

void ui_header(const char *title)
{
    vdp_fill(0, 0, SCREEN_W, 8, 4);
    font_puts(0, 0, title, 15, 4);
}

void ui_footer(const char *help)
{
    vdp_fill(0, 200, SCREEN_W, 12, 4);
    font_puts(0, 25, help, 15, 4);
}
