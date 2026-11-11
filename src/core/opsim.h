/* SPDX-License-Identifier: MIT */
/* opsim.h — 運営モード（裏面, DECISIONS #88）。270 日のサービスを運営の側から回す。
 * NPC 1000 人に「課金の型（無課金・微課金・重課金・石油王）」「我慢の限界」「課金に転ぶ境目」「活動度」を持たせ、
 * 型ごとの不満（stress）が「入ったときから」限界を超えた人から辞めていく。
 * 1000 人は最初から全員いるのではなく、事前登録の人から始まり、流入で入ってくる。流入は 3 本:
 *   口コミ（前の日の DAU に比例し、日がたつほど細る）・プラットフォームの送客（一押し枠・相互送客。会議で取る）・
 *   広告（会議で予算の上限と打ち切りの CVR を決める。打ち続けると CVR が下がり、基準を割るか予算が尽きたら打ち切る）。
 * 日ごとに DAU・新規・課金した人・引退・売上・広告費を出す。
 * 節目（会議）では選択肢を選ぶ。1 が史実（本編の年表で実際に起きたこと）。
 * 240 日目に「239 日目までの利益（売上 - 広告費）+ DAU」の点が基準に届けばサービス継続、届かなければ史実どおり終了の告知。
 *
 * 1000 人ぶんの状態（1 人 5 バイト）は ops_mem_* に置く。ROM では VRAM の画面の外、PC では配列。
 * 乱数は自前の xorshift なので、同じ選択なら ROM でも PC でも同じ数字になる。 */
#ifndef CORE_OPSIM_H
#define CORE_OPSIM_H
#include "../types.h"

#define OPS_DAYS      270
#define OPS_CLASSES   4
#define OPS_FREE      0
#define OPS_LIGHT     1
#define OPS_HEAVY     2
#define OPS_WHALE     3
#define OPS_MEETINGS  12
#define OPS_VERDICT   11         /* 最後の会議は選ばない（継続か終了かの判定） */
#define OPS_OPTIONS   3
#define OPS_REC       5          /* 1 人ぶんのバイト数 */
#define OPS_MEM_SIZE  ((u16)(1000 * OPS_REC))

#define OPS_CONTINUE  1
#define OPS_END       2

#define OPS_AD_SPENT  1          /* 予算を使い切った */
#define OPS_AD_CVR    2          /* CVR が基準を割った */

#define OPS_PLAYED    0x01       /* g_save.ops_flags */
#define OPS_SAVED     0x02

typedef struct {
    u16 day;                     /* 次に回す日（0..OPS_DAYS） */
    u8  next;                    /* 次の会議の番号 */
    u8  pick[OPS_MEETINGS];      /* 選んだもの（0 = 史実） */
    u16 stress[OPS_CLASSES];     /* 型ごとの不満。限界（tol × 16）を超えた人から辞める */
    u16 tempt;                   /* 無課金が課金に転ぶ誘惑（境目 conv × 16 を超えた人が微課金になる） */
    u8  fresh;                   /* 強いカードが出てからの日数（重課金・石油王の飽き） */
    u8  boost;                   /* 確率アップの残り日数 */
    u8  boost_x;                 /* その倍率（2 / 3） */
    u8  spike;                   /* 新しいカードの直後で課金が増える残り日数 */
    u8  glut;                    /* 石を配った後で課金が減る残り日数 */
    u8  event;                   /* 開催中のイベントの残り日数 */
    u8  verdict;                 /* 0 = まだ / OPS_CONTINUE / OPS_END */
    u8  push;                    /* プラットフォームの送客の残り日数 */
    u8  ad_on;                   /* 広告を打っている */
    u8  ad_stop;                 /* 最後の広告が止まったわけ: 0 = まだ / OPS_AD_SPENT / OPS_AD_CVR */
    u16 ad_cvr;                  /* 広告の CVR（0.01%） */
    u16 ad_cut;                  /* 打ち切る CVR（0.01%） */
    u32 ad_left;                 /* 広告の予算の残り（円） */
    u16 carry;                   /* 流入の端数（1/256 人） */
    u16 reg;                     /* 入った人（事前登録 + 流入、累計） */
    u16 rng;
    u16 dau;                     /* 以下は最後に回した日の数字 */
    u16 fresh_users;             /* その日に入った人 */
    u16 payers;                  /* その日に課金した人 */
    u16 retired;                 /* 辞めた人（累計） */
    u32 sales_day;               /* 円 */
    u32 sales_total;
    u32 ad_day;                  /* その日の広告費（円） */
    u32 ad_total;
    s32 profit_judge;            /* 判定に使う 239 日目までの利益 */
    u16 dau_judge;               /* 判定の日（239 日目）の DAU */
    u16 points;                  /* 判定の点（/100）。OPS_CONT_POINTS 以上で継続 */
    /* じっくり（DECISIONS #98）: 日々の判断。早回しでは 0 のまま（全通りの検査は早回しで回す） */
    u8  slow;                    /* じっくりで遊んでいる */
    u8  inc;                     /* 次の日々の判断の番号 */
    u8  outage;                  /* 今日は障害でログインが半分 */
    u8  cap;                     /* 月額の上限を入れた（微課金・重課金の 1 回の額が減る） */
    /* 企画会議（DECISIONS #99）: 15 日ごとに施策を 1 つ。早回しでもじっくりでも開く */
    u8  pnext;                   /* 次の企画会議の番号 */
    u8  gtimer;                  /* 期間つきのガチャの残り日数 */
    u8  gkind;                   /* そのガチャ（OPS_P_STEP / OPS_P_BOX） */
    u8  collab;                  /* コラボの残り日数 */
    u8  sq;                      /* 続いている施策（OPS_SQ_*） */
    u8  news;                    /* 下の帯で知らせること（OPS_NEWS_*。画面が出したら 0 に） */
    u8  womcut;                  /* 口コミが止まっている残り日数（確率を絞ったのがばれた） */
} OpsState;

/* 企画会議の施策。0 = 見送る（史実。いつも選択肢 1） */
#define OPS_P_SKIP     0
#define OPS_P_RAID     1
#define OPS_P_EASTWEST 2         /* 東西対抗戦（全ギルドの大規模戦） */
#define OPS_P_COLLAB   3         /* アニメとのコラボ */
#define OPS_P_RANK     4         /* ランキングイベント */
#define OPS_P_STEP     5         /* ステップアップガチャ */
#define OPS_P_BOX      6         /* 多段の BOX ガチャ */
#define OPS_P_COMP     7         /* コンプガチャ（200 日目に業界の自主規制） */
#define OPS_P_REVIVAL  8         /* 限定 SSR の復刻 */
#define OPS_P_PASS     9         /* 月パス（サブスク） */
#define OPS_P_FUKU     10        /* 福袋 */
#define OPS_P_PAYWALL  11        /* イベントの報酬を課金者だけに */
#define OPS_P_RIG      12        /* 確率をこっそり絞る */
#define OPS_P_NOPITY   13        /* 天井を外す */
#define OPS_P_STINGY   14        /* 詫び石を渋る */
#define OPS_P_COUNT    15
#define OPS_PLANS      16        /* 企画会議の数（7 日目から 15 日ごと） */
#define OPS_PLAN_OPTS  5         /* 見送る・イベント・ガチャ・課金・締め付け */
#define OPS_SQ_PASS    0x01
#define OPS_SQ_PAYWALL 0x02
#define OPS_SQ_RIG     0x04
#define OPS_SQ_STINGY  0x08
#define OPS_SQ_COMP    0x10
#define OPS_SQ_RIGOUT  0x20      /* 絞ったのがばれた（もう絞れない） */
#define OPS_SQ_COMPBAN 0x40      /* コンプガチャは規制された */
#define OPS_NEWS_RIG   1         /* 検証班にばれた */
#define OPS_NEWS_COMP  2         /* コンプガチャの規制 */
#define OPS_COMP_BAN_DAY 200

typedef struct { u16 day; u8 n; u16 text; } OpsMeeting;   /* text: 題 / 議題 / 史実のわけ / 選択肢 × n */

extern OpsState g_ops;

void ops_start(void);
u8   ops_meeting(OpsMeeting *m);     /* 今日が会議の日なら 1（m に中身）。選ぶまで ops_step を呼ばない */
void ops_decide(u8 pick);            /* 会議で選ぶ（0 = 史実）。判定の会議は 0 を渡すと判定する */
void ops_step(void);                 /* 1 日回す */
u16  ops_chat(void);
u8   ops_plan_offer(u8 slot);       /* いまの企画会議の slot 番目（0..OPS_PLAN_OPTS-1）の施策（OPS_P_*） */                 /* 今日の社内チャットの文（無ければ 0）。15 日ごと、模型の様子で選ぶ */
#define OPS_INCS      16             /* じっくりの日々の判断の数 */
#define OPS_INC_KINDS 8              /* 障害・CS 4 種・社長の思いつき 3 種（文は T_OPS_I0 から OPS_TEXTS ずつ） */

/* 1000 人の状態の置き場（ROM: sys/vdp.c, PC: tests/tables_pc.c） */
void ops_mem_read(u16 off, u8 n, u8 *buf);
void ops_mem_write(u16 off, u8 n, const u8 *buf);
#endif
