/* SPDX-License-Identifier: MIT */
/* input.h — キー/ジョイスティックを論理ボタンにまとめる */
#ifndef INPUT_H
#define INPUT_H
#include "../types.h"
#define BTN_UP     0x01
#define BTN_DOWN   0x02
#define BTN_LEFT   0x04
#define BTN_RIGHT  0x08
#define BTN_OK     0x10   /* SPACE / RETURN / トリガA */
#define BTN_CANCEL 0x20   /* ESC / トリガB */
void input_update(void);          /* 1 フレーム 1 回 */
u8   input_held(void);
u8   input_pressed(void);         /* 押した瞬間だけ立つ */
u8   input_digit(void);           /* 数字キーを押した瞬間なら 0..9、それ以外は 0xFF（携帯の数字キー操作） */
#define NO_DIGIT 0xFF
#endif
