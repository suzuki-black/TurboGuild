/* SPDX-License-Identifier: MIT */
/* memo.h — 思い出アルバム（DECISIONS #89）。出来事ごとに「初めてそれが起きた日 + 1」だけを g_save.memo[] に残す
 * （0 = まだ。1 件 2 バイト）。出来事の種類は欄の番号で決まるので ID は書かない。
 * 仲間が初めてギルドを去った日は伝言板と同じ式で決まるので記録しない（ui/ending_ui.c が計算する）。
 * MEMO_MARK はどのバンクからでも使えるよう、関数ではなくマクロにする（g_save を見る所で使う）。 */
#ifndef CORE_MEMO_H
#define CORE_MEMO_H

#define MEMO_START    0    /* 冒険を始めた（最初の 10 連が済んだ） */
#define MEMO_SSR      1    /* 初めて SSR 以上を手に入れた */
#define MEMO_GUILD    2    /* ギルドに入った */
#define MEMO_PAY      3    /* 初めて課金した */
#define MEMO_LV10     4
#define MEMO_LV50     5
#define MEMO_EVOLVE   6    /* 初めて進化させた */
#define MEMO_STAGE100 7    /* ステージ 100 を越えた */
#define MEMO_RAID     8    /* 初めてレイドボスを倒した */
#define MEMO_GVGWIN   9    /* ギルド対抗戦で初めて勝った */
#define MEMO_RIVAL    10   /* 初めてライバルを抜いた */
#define MEMO_LOST     11   /* 初めて抜き返された */
#define MEMO_TOP100   12   /* イベントで初めて 100 位以内 */
#define MEMO_TOP10    13   /* イベントで初めて 10 位以内 */
#define MEMO_LASTDAY  14   /* サービス最終日にいた */
#define MEMO_STOLEN   15   /* 初めてカードを盗まれた（v14） */
#define MEMO_KINDS    16   /* 記録する種類 */
#define MEMO_SLOTS    16   /* セーブの欄 */

#define MEMO_MARK(k) do { if (!g_save.memo[k]) g_save.memo[k] = (u16)((u32)g_save.g_time / GSEC_PER_DAY + 1); } while (0)

#endif
