/* SPDX-License-Identifier: MIT */
/* ram.h — RAM に置く変数はすべてここに宣言する（仕様 §11）。実体は ram.c。
 * 合計は make の最後に "RAM used" として表示される。 */
#ifndef RAM_H
#define RAM_H
#include "types.h"
#include "sys/vdp.h"
#include "sys/bios.h"
#include "core/save.h"
#include "ui/screen.h"
#include "ui/widget.h"
#include "core/gacha.h"
#include "core/quest.h"
#include "core/login.h"
#include "core/fusion.h"
#include "core/event.h"
#include "core/ranking.h"
#include "core/guildwar.h"

/* sys/ */
extern u8 g_cart_slot;
extern u8 g_bank_code, g_bank_dataA, g_bank_dataB;
extern VdpCmd g_vdpcmd;
extern u8 g_fontbuf[32];
extern u8 g_glyph[128];
extern const char *g_txt_rest;  /* txt_draw が描き終えた位置（折り返し用） */      /* 1 文字ぶんの 4bpp（16x16 なら 8 バイト × 16 行） */
extern u8 g_input_held, g_input_pressed;
extern u16 g_digit_held;
extern u8 g_digit_pressed;
extern u8 g_beep_frames;
extern DiskReq g_diskreq;
extern u8 g_sector[512];

/* セーブ */
extern SaveData g_save;
extern u8 g_save_slot;         /* 次に書くスロット 0=A 1=B */
extern u8 g_save_status;       /* 起動時ロードの結果 */
extern u8 g_time_status;       /* 時刻差分の結果 TIME_* */
extern u32 g_time_added;       /* 起動時に加算したゲーム秒 */

/* 画面 */
extern const Screen *g_screen_cur, *g_screen_next;
extern u8 g_dirty;
extern u16 g_boot_magic;      /* 起動完了の目印（自動テストが待つ） */
extern u8 g_test_keys;        /* 自動テストが書くボタン（次のフレームで 1 回だけ押したことになる） */
extern u8 g_test_digit;       /* 自動テストが書く数字キー（数字 + 1。0 = なし） */
extern u8 g_ui_state;         /* UIS_*: 自動テストが「いま押してよいか」を知るため */
extern u8 g_frames;           /* 時刻同期までのフレーム数 */
extern u8 g_save_dirty;       /* 保存していない変更がある */
extern u16 g_sel_area;        /* 画面をまたいで渡す選択（エリア・ステージ・カード・ガチャ） */
extern u16 g_sel_stage;
extern u16 g_sel_card;
extern u8 g_sel_gacha;
extern UiList g_list;         /* 一覧画面の状態（同時に使うのは 1 画面だけ） */
extern u8 g_deck_slot;
extern u8 g_replay;
extern u8 g_grid_remove;
extern u8 g_order[CARD_MAX];   /* カード一覧の表示順（表示位置 → 所持カードの添字, ui/card_ui.c の card_sort） */
extern u8 g_card_sort;
extern u8 g_txt_maxcol;        /* 文字をこの桁の手前で止める（0 = 画面の右端, sys/text.c） */         /* 並べ方: 0 入手順 1 レア度 2 攻撃 3 コスト */
extern u8 g_paid;
extern FuseReq g_fuse;         /* 合成の選択 */
extern EventResult g_evres;    /* イベントの開幕・終了のお知らせ */
extern GvgResult g_gvgres;     /* ギルド対抗戦の参戦・結果のお知らせ */
extern RankList g_rank;        /* ランキングの表示 */
extern u8 g_kinds[6], g_kind_n; /* いま出すガチャの種類 */             /* 給料日の知らせを出すか */      /* カードの一覧の先頭に「はずす」を置くか */           /* クリア済みエリアを周回するときの位置 */
extern u8 g_ticker, g_ticker_gap;   /* お知らせの流し位置 */
extern char g_ticker_buf[96];  /* 流しているお知らせの 1 本（Shift-JIS） */
extern u8 g_ticker_k;          /* 次に流す出来事の番号 */
extern u8 g_viewonly;          /* サービス終了後の「思い出を見る」（カード一覧だけ） */
extern u16 g_gift;             /* 年表の贈り物（知らせる石の数） */
extern u8  g_joined;
extern u16 g_cred_pos;          /* スタッフロールの位置（流したドット数） */            /* マイページ: ギルドに入ったばかり（知らせる） */
extern u8 g_from_title;        /* タイトルの「つづきから」でマイページへ来た（保存を後回しにする） */
typedef struct { u16 pos, loop; u8 wait, inst, hi, lo, on; } BgmCh;   /* BGM の 1 チャンネル（sys/bgmdrv.c） */
extern BgmCh g_bgm_ch[3];
extern u8 g_bgm_song, g_bgm_nch;
extern u16 g_bgm_base;
extern u16 g_bgm_ticks;
extern u16 g_bgm_clock, g_bgm_acc;   /* テンポの時計（システムタイマーの値と、たまった経過） */         /* BGM を進めた回数（1/60 秒単位。テンポの確認用） */          /* 鳴らしている曲の、ページの中での位置 */
extern volatile u8 g_isr_safe;  /* 1 = 割り込みからカーネルを呼んでよい（ディスクの間は 0） */
extern u8 g_timi_stub[17];      /* H.TIMI から飛ぶ RAM の入口（sys/bgmdrv.c） */
extern u8 g_nosave_warned;    /* 保存に失敗した（ディスク無しなど）。「セーブできません」は 1 回だけ出し、
                                 * メニューの「保存」で成功するまで自動保存はディスクに触らない */
extern GachaReq g_gacha;
extern QuestResult g_qr;
extern LoginResult g_login;
extern u8 g_cursor;
extern u8 g_menu_pos;         /* メニューへ戻ったときのカーソル位置 */
extern u8 g_ui_prev;         /* 画面ごとの前回値（再描画の判定用） */

/* crt0.s が置く静的 RAM の終端 */
extern const u16 ram_end;
#endif
