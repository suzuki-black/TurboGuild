/* screen.h — 画面は関数ポインタ表 1 枚。main のループは update と draw を呼ぶだけ（仕様 §9） */
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

extern const Screen scr_p0menu, scr_banktest, scr_cardtest, scr_savetest, scr_inputtest;

/* 画面共通の小物 */
void ui_clear(void);
void ui_header(const char *title);
void ui_footer(const char *help);
void back_on_cancel(void);    /* ESC でメニューへ戻る（update 用） */
#endif
