/* SPDX-License-Identifier: MIT */
/* board.h — ギルド伝言板。NPC のうち添字 i % 8 == 3 の人は「仲間」で、どのギルドにも 7〜8 人いる
 * （tools/gen_npc.py がギルドを添字から決める）。仲間は口癖・課金コース・引退の日を持ち、
 * 6 時間（ゲーム内）に 1 回、日付とイベントの状況に合った文を書き込む。引退の数日前から弱音が増え、
 * 引退の日に「○○さんがギルドを脱退しました」が流れる。どれも NPC 番号と日付のハッシュで決まるので保存しない。 */
#ifndef CORE_BOARD_H
#define CORE_BOARD_H
#include "../types.h"
#include "../config.h"

#define BOARD_IS_MATE(i)  (((i) & 7) == 3)
#define BOARD_SLOT_SECS   (GSEC_PER_DAY / 4)
#define BOARD_NEVER       0xFFFFu
#define BOARD_LOOKBACK    64          /* さかのぼるのは 16 日ぶん */
#define BOARD_MAX         8

#define BP_POST  0
#define BP_LEAVE 1

typedef struct {
    u16 npc;
    u8  kind;            /* BP_POST / BP_LEAVE */
    u8  tone;            /* 口癖 0..3（0 = なし） */
    u16 text;            /* 文言 id（BP_POST のとき） */
    u16 alt;             /* 1 つ前（古い方）と同じ文になったときの代わり */
    u16 param;           /* 文の % に入れる文言 id（無ければ 0） */
    u16 day;             /* 書き込んだ日 */
} BoardPost;

typedef struct {
    u8 skip;             /* 入力: 新しい順に何件飛ばすか */
    u8 want;             /* 入力: 何件ほしいか（BOARD_MAX まで） */
    u8 n;                /* 出力: 件数（BOARD_MAX まで） */
    BoardPost post[BOARD_MAX];
} BoardList;

u16  npc_hash(u16 a, u16 b);           /* ranking.c と同じハッシュ（同じバンクから直接呼ぶ） */
u16  mate_retire_day(u16 npc);         /* 仲間の引退の日（最後まで残るなら BOARD_NEVER）。同じバンクから直接呼ぶ */
u8   npc_tone(u16 npc);               /* 口癖 0..3（仲間でなくても決まる）。同じバンクから直接呼ぶ */
u16  mate_info(u16 npc);              /* 別のバンク向け: 下位 8 ビットが引退の日（MATE_STAYS = 最後まで残る）、上位が口癖 */
#define MATE_STAYS 0xFF
void board_list(BoardList *l);
#endif
