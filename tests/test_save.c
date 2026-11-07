/* SPDX-License-Identifier: MIT */
/* test_save.c — 封と検証、壊れたデータの検出、構造体サイズ（2KB = 4 セクタ以下）、古い版の読み込み */
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
    /* CRC が正しくても、枚数が配列を超えるセーブは読まない（カード一覧の並べ替えなどが RAM を壊す） */
    save_new(&s, 1);
    s.card_count = CARD_MAX;
    save_seal(&s);
    CHECK(save_valid(&s));
    s.card_count = CARD_MAX + 1;
    save_seal(&s);
    CHECK(!save_valid(&s));
    /* 封をすると言語と版が入る */
    save_new(&s, 1);
    save_seal(&s);
    CHECK(s.version == SAVE_VERSION && s.lang == GAME_LANG && s.game_ver[0] == GAME_VER_MAJOR &&
          s.game_ver[1] == GAME_VER_MINOR && s.game_ver[2] == GAME_VER_PATCH);
    /* 上位互換: v8（1342 B）のセーブは、その大きさで検査して読み、足した欄は 0 から */
    save_new(&s, 1);
    s.gold = 777;
    s.credits_seen = 1;                 /* v8 の最後の欄 */
    s.lang = 0x55;                      /* v9 で足した欄（v8 のディスクではゴミ） */
    s.magic = SAVE_MAGIC;
    s.version = 8;
    s.crc = save_crc(&s);
    CHECK(save_valid(&s));
    s.lang = 0x66;                      /* v8 の外の欄が変わっても v8 としては正しい */
    CHECK(save_valid(&s));
    save_upgrade(&s);
    CHECK(s.version == SAVE_VERSION && s.gold == 777 && s.credits_seen == 1 && s.lang == 0 && s.game_ver[0] == 0);
    /* v9（v0.1.0）も読め、v10 で足したライバルは「いない」から */
    save_new(&s, 1);
    s.lang = 2;
    s.rival[1] = 0x1234;                /* v10 で足した欄（v9 のディスクではゴミ） */
    s.chaser = 0x5678;
    s.magic = SAVE_MAGIC;
    s.version = 9;
    s.crc = save_crc(&s);
    CHECK(save_valid(&s));
    save_upgrade(&s);
    CHECK(s.version == SAVE_VERSION && s.lang == 2 && s.rival[1] == 0 && s.chaser == 0);
    /* v10 も読め、v11 で足した戦い方と控えのデッキは 0（通常・空き）から */
    save_new(&s, 1);
    s.rival[0] = 5;
    s.stance = 2; s.deck_slot[1][0] = 9;
    s.magic = SAVE_MAGIC;
    s.version = 10;
    s.crc = save_crc(&s);
    CHECK(save_valid(&s));
    save_upgrade(&s);
    CHECK(s.version == SAVE_VERSION && s.rival[0] == 5 && s.stance == 0 && s.deck_slot[1][0] == 0);
    /* v11 も読め、v12 で足した運営モードの記録は 0 から */
    save_new(&s, 1);
    s.stance = 2;
    s.ops_best = 123456; s.ops_flags = 3;
    s.magic = SAVE_MAGIC;
    s.version = 11;
    s.crc = save_crc(&s);
    CHECK(save_valid(&s));
    save_upgrade(&s);
    CHECK(s.version == SAVE_VERSION && s.stance == 2 && s.ops_best == 0 && s.ops_flags == 0);
    /* v12 も読め、v13 で足した思い出は 0（まだ）から */
    save_new(&s, 1);
    s.ops_best = 777;
    s.memo[MEMO_SSR] = 5;
    s.magic = SAVE_MAGIC;
    s.version = 12;
    s.crc = save_crc(&s);
    CHECK(save_valid(&s));
    save_upgrade(&s);
    CHECK(s.version == SAVE_VERSION && s.ops_best == 777 && s.memo[MEMO_SSR] == 0);
    /* v5（Phase2）も読める。v4 より前は読まない */
    save_new(&s, 1);
    s.magic = SAVE_MAGIC;
    s.version = 5;
    s.guild = 9;                        /* v7 で足した欄 */
    s.crc = save_crc(&s);
    CHECK(save_valid(&s));
    save_upgrade(&s);
    CHECK(s.version == SAVE_VERSION && s.guild == 0 && s.box_no == 0);
    s.version = 4;
    s.crc = save_crc(&s);
    CHECK(!save_valid(&s));
    s.version = SAVE_VERSION + 1;       /* 未来の版も読まない */
    s.crc = save_crc(&s);
    CHECK(!save_valid(&s));
    CHECK_DONE("save");
    return 0;
}
