/* config.h — マジックナンバーの集約先（仕様 §11） */
#ifndef CONFIG_H
#define CONFIG_H

/* 時間（仕様 §3） */
#define TIME_SCALE          10UL        /* 現実1日 = ゲーム内10日 */
#define GSEC_PER_DAY        86400UL
#define OFFLINE_MAX_REAL    (30UL * 86400UL)  /* 現実30日分まで加算 */
#define AP_RECOVER_REAL     300UL       /* 現実5分で AP+1 */

/* ゲーム */
#define AP_MAX              20
#define DECK_SIZE           5
#define CARD_MAX            200

/* RAM 予算（仕様 §2.3, §11） */
#define RAM_BASE            0xC000
#define RAM_BUDGET          6144        /* 静的 RAM の上限。DECISIONS #1 */

/* ROM レイアウト（仕様 §2.2, docs/MEMORY_MAP.md と一致させる） */
#define ROM_PAGES           32
#define PAGE_HDR_SIZE       16
#define WIN_CODE            0x6000
#define WIN_DATAA           0x8000
#define WIN_DATAB           0xA000

/* 自動テスト用 */
#define BOOT_MAGIC          0x4754      /* "TG" */

#endif
