/* SPDX-License-Identifier: MIT */
/* config.h — マジックナンバーの集約先（仕様 §11） */
#ifndef CONFIG_H
#define CONFIG_H

/* 時間（仕様 §3） */
#define TIME_SCALE          10UL        /* 現実1日 = ゲーム内10日 */
#define GSEC_PER_DAY        86400UL
#define OFFLINE_MAX_REAL    (30UL * 86400UL)  /* 現実30日分まで加算 */
#define AP_RECOVER_REAL     300UL       /* 現実5分で AP+1 */

/* ゲーム（数値はすべて仮。変えたら DECISIONS に残す） */
#define AP_MAX              100         /* 企画書「最大100」 */
#define AP_RECOVER_GSEC     (AP_RECOVER_REAL * TIME_SCALE)
#define DECK_SIZE           5
#define CARD_MAX            200
#define LEVEL_MAX           150         /* EXP_NEED が u16 に収まる上限 */
#define START_GOLD          3000UL
#define START_GEMS          3000UL
#define PLAYER_HP_BASE      200         /* DECISIONS #18 */
#define PLAYER_HP_PER_LV    20
#define DECK_COST_BASE      30          /* デッキコスト上限 = BASE + PER_LV * Lv */
#define DECK_COST_PER_LV    2
#define EXP_NEED_BASE       50          /* 次の Lv まで = BASE + K * Lv^2（DECISIONS #20） */
#define EXP_NEED_K          2
#define FIRST_GACHA_COUNT   10
#define POTION_PRICE_GEMS   100
#define BATTLE_LOG_MAX      5           /* 戦闘ログは最大 5 回に圧縮（仕様 §6） */
#define SKILL_PCT           10
#define ELEM_GOOD_PCT       120
#define ELEM_BAD_PCT        80

/* 画面 */
#define TEXT_COLS           32
#define LINE_H              16

/* RAM 予算（仕様 §2.3, §11） */
#define RAM_BASE            0xC000
#define RAM_BUDGET          6144        /* 静的 RAM の上限。DECISIONS #1 */

/* ROM レイアウト（仕様 §2.2, docs/MEMORY_MAP.md と一致させる） */
#define ROM_PAGES           32
#define PAGE_HDR_SIZE       16
#define WIN_CODE            0x6000
#define WIN_DATAA           0x8000
#define WIN_DATAB           0xA000

/* 色（パレット番号。tools/art/palette.py と一致させる） */
#define COL_CLEAR   0
#define COL_BLACK   1
#define COL_DGRAY   2
#define COL_LGRAY   3
#define COL_WHITE   4
#define COL_SKIN    5
#define COL_DRED    6
#define COL_RED     7
#define COL_ORANGE  8
#define COL_YELLOW  9
#define COL_DGREEN  10
#define COL_GREEN   11
#define COL_DBLUE   12
#define COL_BLUE    13
#define COL_CYAN    14
#define COL_PURPLE  15
/* Phase1 までの名前（新パレットの近い色へ） */
#define COL_LBLUE   COL_CYAN
#define COL_PINK    COL_PURPLE
#define COL_LYELLOW COL_YELLOW
#define COL_MAGENTA COL_PURPLE
#define COL_GRAY    COL_LGRAY

/* 自動テスト用 */
#define BOOT_MAGIC          0x4754      /* "TG" */
#define UIS_BUSY            0           /* 描画・演出中 */
#define UIS_IDLE            1           /* 画面の update 待ち */
#define UIS_WAITKEY         2           /* メッセージ・確認・結果で押下待ち */

/* ゲームの版（セーブに書く。リリースのたびに上げる）と言語（既定の make で LANG_EN、make LANG=ja で日本語） */
#define GAME_VER_MAJOR      0
#define GAME_VER_MINOR      9
#define GAME_VER_PATCH      0
#define LANG_JA_CODE        0
#define LANG_EN_CODE        1
#ifdef LANG_EN
#define GAME_LANG           LANG_EN_CODE
#else
#define GAME_LANG           LANG_JA_CODE
#endif

#endif
