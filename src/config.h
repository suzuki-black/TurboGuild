/* SPDX-License-Identifier: MIT */
/* config.h — マジックナンバーの集約先（仕様 §11） */
#ifndef CONFIG_H
#define CONFIG_H

/* 時間（仕様 §3） */
#define TIME_SCALE          10UL        /* 現実1日 = ゲーム内10日 */
#define CLOCK_MIN_YEAR      2026        /* 起動時の RTC がこれより前なら、時計を合わせ直してもらう（電池切れ, DECISIONS #101）。RTC は 1980〜2079 年 */
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
#define LEADER_PCT          20          /* リーダー（デッキの 1 枚目）と同じ属性のカードの攻撃 +20%（DECISIONS #87） */
#define DECK_SLOTS          3           /* デッキは 3 つまで持てる */
/* 戦い方（DECISIONS #87）: 節約は AP 半分（切り上げ）・攻撃 60%、全力は AP 2 倍・攻撃 250%。報酬は変わらない */
#define STANCE_NORMAL       0
#define STANCE_SAVE         1
#define STANCE_ALL          2
#define STANCES             3
#define STANCE_SAVE_ATK     60
#define STANCE_ALL_ATK      250

/* 運営モード（core/opsim.c, DECISIONS #88）。tests/sim_ops.c で合わせる */
#define OPS_TEXTS           6        /* 会議 1 つの文: 題・議題・史実のわけ・選択肢 3 */
#define OPS_PCT_WHALE       5        /* 型の割合（/256, 累計）: 石油王 2% */
#define OPS_PCT_HEAVY       20       /* 重課金 6% */
#define OPS_PCT_LIGHT       64       /* 微課金 17%、残りが無課金 */
#define OPS_PCT_CONV        96       /* 無課金のうち、誘惑で微課金に転ぶことがある人（/256） */
#define OPS_TOL_BASE        40       /* 我慢の限界 = これ + 0..127 + 活動度 / 2 */
#define OPS_CONV_BASE       8        /* 転ぶ境目 = これ + 0..127 */
#define OPS_BORE_FREE       9        /* 毎日の飽き（不満 /16） */
#define OPS_BORE_LIGHT      7
#define OPS_BORE_HEAVY      6
#define OPS_BORE_WHALE      5
#define OPS_SPEND_LIGHT     5        /* 1 回の課金（百円） */
#define OPS_SPEND_HEAVY     30
#define OPS_SPEND_WHALE     100
#define OPS_WANT_LIGHT      10       /* ふだんの課金する気（/256） */
#define OPS_WANT_HEAVY      30
#define OPS_WANT_WHALE      60
#define OPS_ACT_BASE        64       /* ログインする確率 = これ + 活動度（+ イベント中）/256 */
#define OPS_ACT_EVENT       24
#define OPS_EVENT_RELIEF    80
#define OPS_RATEUP_RELIEF   60
#define OPS_BOOST_DAYS      7
#define OPS_INFL_FREE       250
#define OPS_INFL_LIGHT      200
#define OPS_INFL_HEAVY      60
#define OPS_RARE_FREE       600
#define OPS_RARE_LIGHT      450
#define OPS_RARE_HEAVY      120
#define OPS_RARE_WHALE      40
#define OPS_RARE_TEMPT      300
#define OPS_SPIKE_DAYS      10
#define OPS_GIFT_RELIEF     50       /* 石 100 個あたり */
#define OPS_GLUT_DAYS       2        /* 石 100 個あたり、課金が半分になる日数 */
#define OPS_LOW_FREE        4        /* 排出率 1%: 無課金の不満が毎日増える */
#define OPS_HIGH_FREE       3        /* 排出率 6%: 減る */
#define OPS_PITY_RELIEF     2
#define OPS_FRESH_DAYS      30       /* 強いカードが出ないと、これを過ぎてから重課金・石油王が飽きる */
#define OPS_TEMPT_DAILY     2
#define OPS_TEMPT_BOOST     20
#define OPS_TEMPT_PITY      2
#define OPS_END_SHOCK       800
#define OPS_END_DAILY       40
#define OPS_CONT_RELIEF     200
#define OPS_JUDGE_DAY       240
#define OPS_CONT_PROFIT     150000UL /* 継続の判定: 239 日目までの利益 15 万円で 0.01 点（1500 万円で 1 点） */
#define OPS_CONT_DAU        260      /* 239 日目の DAU 260 人で 1 点 */
#define OPS_CONT_POINTS     200      /* 合わせて 2.00 点で継続 */
#define OPS_PREREG          350      /* 事前登録（0 日目からいる人） */
#define OPS_WOM             160      /* 口コミ: 前の日の DAU × これ / 25600 人（古くなるほど細る） */
#define OPS_WOM_RATE        48      /* 排出率 6% で増え、1% で減る */
#define OPS_WOM_PITY        32       /* 天井があると増える */
#define OPS_PUSH_DAYS       7        /* プラットフォームの一押し枠・相互送客の日数 */
#define OPS_PUSH_NEW        8        /* その間に毎日入る人 */
#define OPS_AD_L1_MAN       300      /* 開始の宣伝の予算（万円）: 史実 */
#define OPS_AD_L2_MAN       1000     /*                          大きな予算 */
#define OPS_AD_N1_MAN       500      /* 95 日目の宣伝の予算: 史実 */
#define OPS_AD_N2_MAN       1500
#define OPS_AD_CUT1         100      /* 打ち切る CVR（0.01%）: 史実は 1.00% */
#define OPS_AD_CUT2         50       /* 大きな予算では 0.50% まで粘る */
#define OPS_AD_CVR_LAUNCH   200      /* 開始の宣伝の CVR（0.01%） */
#define OPS_AD_CVR_LATER    260      /* 95 日目の宣伝の CVR */
#define OPS_AD_DAILY        200000UL /* 1 日に使う広告費（円） */
#define OPS_AD_CPC          200      /* 1 クリックの単価（円） */
#define OPS_AD_FATIGUE      97       /* 打つたびに CVR がこの % になる */
#define OPS_HIST_SALES      17779500UL  /* 史実の選択での売上の合計（tests/test_game.c が確かめる） */
#define OPS_I_MAINT_EXT     120      /* じっくり（DECISIONS #98）: 延長を重ねたメンテの不満 */
#define OPS_I_MAINT_LONG    50       /* 長めに告知して早く開けたメンテの不満 */
#define OPS_I_CS            30       /* CS の返し方 1 回ぶんの不満 */
#define OPS_I_REFUND        30000UL  /* 返金する額（円） */
#define OPS_I_RUSH          60       /* 社長の思いつきを急いで入れた出来の悪さ */
#define OPS_I_AD_MAN        300      /* 社長の言う追加の広告（万円） */
#define OPS_CHAT_LOW_DAU    150      /* 社内チャットが弱音になる DAU */
#define OPS_CHAT_GOOD_SALES 300000UL /* 浮かれる 1 日の売上（円） */
#define OPS_P_RAID_MAN      100      /* 企画会議（DECISIONS #99）: レイドの開発費（万円） */
#define OPS_P_EW_MAN        200      /* 東西対抗戦 */
#define OPS_P_COLLAB_MAN    300     /* コラボの版権料 */
#define OPS_P_RANK_MAN      50
#define OPS_COLLAB_DAYS     10
#define OPS_COLLAB_NEW      25       /* コラボの間に毎日入る人 */
#define OPS_P_COLLAB_LEAVE  80      /* コラボが終わると無課金の不満が跳ねる */
#define OPS_P_PRESSURE      60       /* ランキング・ガチャ・復刻・天井を外すの不満 */
#define OPS_P_GACHA_DAYS    10
#define OPS_P_PASS_TEMPT    3
#define OPS_P_PASS_PCT      70       /* 微課金・重課金のうち月パスを買う割合 */
#define OPS_P_PASS_PRICE    20       /* 月パスの 1 か月（百円。石が多く付く上位のパスを含めた平均） */
#define OPS_P_RIG_FIND      63       /* 確率を絞ると、毎日 1/64 で見つかる */
#define OPS_P_SCANDAL       400      /* 見つかった・規制されたときの不満 */
#define OPS_HIST_PROFIT     14579500UL  /* 同じく利益（売上 - 広告費）。決算の上司のひとことはこれと比べる */

/* 宿屋（core/player.c player_sleep, DECISIONS #91）: 泊まると翌朝まで進み、AP が全回復する */
#define INN_GOLD            100      /* 宿代（ゴールド） */
#define INN_WAKE_SECS       (6UL * 3600UL)   /* 朝 6 時に起きる */

/* 盗み・盗まれ（core/steal.c, DECISIONS #90） */
#define STEAL_AP            20       /* 盗みに行くたびに払う AP（AP をすべて盗みに使っても SSR は全期間で 1〜2 枚ほど） */
#define STEAL_SLOT_SECS     (GSEC_PER_DAY / 4)   /* 留守の間はゲーム内 6 時間ごとに盗まれるかを決める */
#define STEAL_SLOTS_MAX     280      /* 数えるのは直近 70 日ぶんまで */
#define STEAL_PER_CHECK     3        /* 1 回戻るたびに盗まれるのはここまで */
#define STEAL_START_DAY     7        /* 最初の 1 週間は盗まれない */
#define STEAL_END_DAY       270      /* サービス終了の日 */
#define STEAL_P_BASE        6        /* 6 時間ごとに盗まれる確率（/256）: ギルドの仲間が 8 人いるとき */
#define STEAL_P_PER_GONE    3        /* 仲間が 1 人去るごとに足す */
#define STEAL_P_NOGUILD     30       /* ギルドに入っていない */
#define STEAL_PCT_EVEN      50       /* 総戦力が同じ相手から盗める確率（%）。相手が強いほど下がる */
#define STEAL_PCT_MIN       10
#define STEAL_PCT_MAX       90
#define STEAL_PCT_BACK      20       /* 取り返すときに足す */
#define MAINT_GEMS          100      /* 延長したメンテと緊急メンテのあとのお詫びの石 */
#define KEEP_DAYS           30       /* 去った仲間の形見分けを受け取れるのは、去った日からこの日数まで（古いセーブで一度に届かないように） */

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
#define GAME_VER_MINOR      3
#define GAME_VER_PATCH      1
#define LANG_JA_CODE        0
#define LANG_EN_CODE        1
#ifdef LANG_EN
#define GAME_LANG           LANG_EN_CODE
#else
#define GAME_LANG           LANG_JA_CODE
#endif

#endif
