/* SPDX-License-Identifier: MIT */
/* keitai.h — ガラケーと 2011 年の空気（DECISIONS #95）。どれも日付と番号のハッシュで決まり、保存は請求とメンテのお詫びだけ。
 *   文体:      伝言板の仲間ごとの書き方（半角ｶﾅ・小さい「ぁぃぅぇぉ」）。表示するときに文を置き換える（文は増やさない）
 *   冒険中:    いまつないでいる人数（残っている人数 × 時刻の山。21 時が山で、夜中は少ない）
 *   足あと:    マイページに来た人（仲間・ライバル・知らない人。盗んだ人は盗む前に、去る仲間は最後の日に付ける）
 *   メンテ:    2 週に 1 回の定期メンテ（13〜15 時）と、イベント初日の緊急メンテ（10〜12 時）。ときどき 1 時間延長。
 *              その間はマイページに入れず、終わるまで待つ。延長と緊急のあとは詫び石 */
#ifndef CORE_KEITAI_H
#define CORE_KEITAI_H
#include "../types.h"

#define STYLE_NONE   0
#define STYLE_KANA   1           /* 半角ｶﾅ（英語版はすべて小文字） */
#define STYLE_SMALL  2           /* あいうえお → ぁぃぅぇぉ（英語版は you → u） */

void style_apply(char *s, u16 style);   /* s（RAM の文字列）をその場で置き換える。長くはならない */
u16  online_now(u16 active);            /* 残っている人数 active のうち、いま冒険中の人数 */

#define FOOT_MAX    8
#define FOOT_MATE   0            /* ギルドの仲間 */
#define FOOT_RIVAL  1
#define FOOT_THIEF  2            /* 盗んでいった人 */
#define FOOT_BYE    3            /* 去る仲間の最後の足あと */
#define FOOT_OTHER  4
typedef struct { u16 npc; u16 day; u8 hour; u8 kind; } FootRow;
typedef struct { u8 n; FootRow r[FOOT_MAX]; } FootList;
void foot_list(FootList *l);            /* 新しい順に、さかのぼって 16 日ぶんまで */

#define MAINT_NONE   0
#define MAINT_REG    1           /* 定期メンテナンス */
#define MAINT_EMERG  2           /* 緊急メンテナンス */
typedef struct {
    u8  kind;
    u8  extended;                /* 1 時間延長する */
    u16 day;
    u32 start, end, end0;        /* end0 = はじめの終了予定 */
} Maint;
u8   maint_now(Maint *m);               /* いまメンテ中なら 1（m にその回） */
u16  maint_comp(void);                  /* 終わったメンテのお詫びの石をまだ配っていなければ配る。配った個数（0 = なし） */
#endif
