/* SPDX-License-Identifier: MIT */
/* screen.h — 画面は関数ポインタ表 1 枚。main のループは update と draw を呼ぶだけ（仕様 §9）。
 * コードバンクの画面は SCREEN(...) の印で表をカーネルに生成させる（src/far.h）。 */
#ifndef UI_SCREEN_H
#define UI_SCREEN_H
#include "../types.h"
typedef struct {
    void (*init)(void);
    void (*update)(void);
    void (*draw)(void);      /* g_dirty が立っているときだけ main から呼ばれる */
    void (*exit)(void);
} Screen;

void screen_change(const Screen *next);
void screen_run(void);
void time_sync(void);        /* RTC を読んで g_time を進める */

/* Phase0 の診断画面用（8x8 の英数字。UI_DEBUG バンクの中だけで使う） */
void dbg_header(const char *title);
void dbg_footer(const char *help);
void back_on_cancel(void);
#endif
