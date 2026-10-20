/* SPDX-License-Identifier: MIT */
/* login.h — ログインボーナス（7 日周期, 仕様 §4.5）。ゲーム内日付が変わったら 1 回 */
#ifndef CORE_LOGIN_H
#define CORE_LOGIN_H
#include "../types.h"
typedef struct { u8 given; u8 day; u8 item; u16 amount; } LoginResult;
void login_check(LoginResult *r);
#endif
