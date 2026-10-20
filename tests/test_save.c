/* SPDX-License-Identifier: MIT */
/* test_save.c — 封と検証、壊れたデータの検出、構造体サイズ（2KB = 4 セクタ以下） */
#include "check.h"
#include "core/save.h"

static SaveData s;  /* g_save は tables_pc.c */

int main(void)
{
    CHECK(sizeof(SaveData) <= 2048);
    save_new(&s, 123456);
    CHECK(!save_valid(&s));            /* 封をする前は無効 */
    save_seal(&s);
    CHECK(save_valid(&s));
    CHECK(s.crc == save_crc(&s));      /* crc 欄を含めても同じ値になる */
    s.gold = 1;
    CHECK(!save_valid(&s));            /* 1 バイトでも変われば検出 */
    save_seal(&s);
    CHECK(save_valid(&s));
    s.cards[199].lv ^= 0x80;
    CHECK(!save_valid(&s));
    CHECK_DONE("save");
    return 0;
}
