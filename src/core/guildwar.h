/* guildwar.h — ギルド対抗戦（GvG, DECISIONS #64）。年表の GUILDWAR 行から 7 日間。
 * 1 日 2 回の「戦の時間」（12:00〜13:00 と 21:00〜22:00, ゲーム内）に別のギルドと 1 戦ずつ、計 14 戦。
 * ギルドの点は所属 NPC の戦力から式で決まる（状態は保存しない）。自分は戦の時間中に
 * 相手ギルドの冒険者へ攻撃（AP 10）して点を足し、1 戦に 1 回「応援」できる（ギルドの点 +3%）。
 * 終わった戦はマイページに入ったときにまとめて結果を出す（何日空けても同じ）。 */
#ifndef CORE_GUILDWAR_H
#define CORE_GUILDWAR_H
#include "../types.h"
#include "event.h"

#define GVG_MATCHES    14
#define GVG_SLOT_SECS  3600UL
#define GVG_AP         10
#define GVG_TARGETS    3
#define GVG_WIN_GEMS   100       /* 1 勝ごとの石 */

typedef struct {
    u8  war;             /* 開催中の回（0 = なし） */
    u8  now;             /* 戦の時間中 */
    u8  m;               /* いまの（次の）戦の番号 0..13 */
    u8  foe;             /* 相手ギルド 0..15 */
    u16 left_min;        /* 戦の残り（分）。戦の時間外なら次の戦まで（分） */
    u32 us, them;        /* いまの点（NPC の分は経過に比例して伸びる） */
    u8  combo;           /* 味方の攻撃の続き具合（見せ方） */
    u8  cheered;
} GvgNow;

typedef struct {
    u8  joined;          /* このたび参加した */
    u8  n;               /* このたび結果を出した戦の数 */
    u8  win;             /* そのうち勝った数 */
    u8  ended;           /* 対抗戦が終わった（順位と報酬） */
    u8  rank;            /* ギルド順位 1..16 */
    u32 us, them;        /* 最後に出した戦の点 */
    RewardList gain;
} GvgResult;

typedef struct {
    u8  status;          /* 0 = OK, 1 = 戦の時間外, 2 = AP 不足 */
    u8  win;
    u16 npc;
    u32 pts;
    u8  combo;
} GvgAttack;

void gvg_update(GvgResult *r);           /* マイページに入るたび */
void gvg_now(GvgNow *n);
u16  gvg_target(u16 k);                  /* いまの戦の相手ギルドの k 番目（1..3）の冒険者（NPC） */
void gvg_attack(u16 k, GvgAttack *a);
u8   gvg_cheer(void);                    /* 応援。できたら 1 */
#endif
