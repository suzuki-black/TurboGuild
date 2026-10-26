# MEMORY_MAP — ASCII8 ページ割り当て表

`tools/link_rom.py` がこの表を読んで `bin/TurboGuild.rom` を組み立て、
`src/data/rompages.h`（`#define PAGE_<名前> n`）を生成する。**表を書き換えたら make し直す。**

- 1 ページ = 8KB。128 ページ（1MB）。カード 120 枚の絵を入れるため 512KB から広げた（Phase 1 は 256KB）。
  カード絵はページ 39〜76 で 152 枚まで、小さい絵はページ 82〜91 で 160 枚まで（上のレア度のカードを足すときに広げた）。
- ページ 0 以外の先頭 16 バイトは**ページ見出し**: `[ページ番号, 'T', 'G', 'P', 種別, FFh×11]`。
  中身はオフセット 16（窓の先頭 + 10h）から置く。バンク切替テストはこの見出しを読む。
- 種別: 0=空き 1=コード 2=データA 3=データB
- ソース `kernel` は SDCC のリンク結果のうち 0x4000-0x5FFF（バンクに載せないソース全部）。
- ソース `hot: <ファイル…>` は RAM で実行するコード。SDCC の領域 `_HOT` に入り RAM の CC00h にリンクされる（asm は `.area _HOT` と自分で書く）。
  ROM にはこのページに置き、起動時に `main` が最初に CC00h へ写す（DECISIONS #72）。静的 RAM（C000h〜）は CC00h を越えてはいけない。
  スタックは DA00h〜HIMEM（DE67h）。
- ソース `code: <ファイル…>` はコードバンク。そのソースは SDCC の領域 `_BANKn` に入り、0xn6010 にリンクされる
  （24bit 番地。下位 16bit が窓 6000h の中の位置）。Makefile はこの表からソース→バンクを決める。
- 別バンクの関数は `FAR(...)` で定義し、カーネルに生成される中継を通して呼ぶ（`src/far.h`, DECISIONS #14）。
  直接呼ぶとリンクは通るが実行時に別物へ飛ぶので、`tools/check_banks.py` がビルド時に止める。
- `-` は空き（FFh 埋め）。データのページは ROM の中身をそのまま置く（+10h から）。

| page | 窓 | 名前 | ソース | 説明 |
|---|---|---|---|---|
| 0 | 4000h 固定 | KERNEL | kernel | 起動・sys/・文字描画・絵の描画・表の読み出し・画面の共通部品・中継 |
| 1 | 6000h コード | CORE | code: src/core/player.c src/core/inventory.c src/core/deck.c src/core/battle.c src/core/quest.c src/core/gacha.c src/core/login.c | ゲームロジック |
| 2 | 6000h コード | UI_START | code: src/ui/title.c src/ui/credits_ui.c src/ui/name.c src/ui/course.c src/ui/firstgacha.c src/ui/savedisk.c src/core/save.c src/core/time.c src/sys/rtc.c | タイトル・名前・お財布・最初の 10 連・セーブディスク・時計 |
| 3 | 6000h コード | UI_HOME | code: src/ui/home.c src/ui/shop.c src/ui/login_ui.c | マイページ・ショップ・ログインスタンプ |
| 4 | 6000h コード | UI_QUEST | code: src/ui/quest_ui.c | エリア一覧・探索 |
| 5 | 6000h コード | UI_GACHA | code: src/ui/gacha_ui.c | ガチャのバナー一覧・結果 |
| 6 | 6000h コード | UI_CARD | code: src/ui/card_ui.c src/ui/deck_ui.c | カード一覧・詳細・デッキ編成 |
| 7 | 6000h コード | UI_DEBUG | code: src/ui/p0menu.c src/ui/p0tests.c src/ui/p0save.c src/ui/p0bank.c src/sys/font.c | Phase0 の診断画面（タイトルで ESC） |
| 8 | 6000h コード | UI_COMMON | code: src/ui/common.c src/ui/gfx_card.c | 共通部品（上下の帯・状態欄・箱・保存） |
| 9 | 6000h コード | UI_BOSS | code: src/ui/boss.c | ボス戦・イベントのボス戦 |
| 10 | 6000h コード | UI_ANIM | code: src/ui/gacha_anim.c | ガチャの開封演出とカードめくり |
| 11 | 6000h コード | CORE2 | code: src/core/fusion.c src/core/event.c src/core/timeline.c | 合成・進化・イベント・ランキング・運営年表 |
| 12 | 6000h コード | UI_FUSION | code: src/ui/fusion_ui.c | 強化合成・進化合成 |
| 13 | 6000h コード | UI_EVENT | code: src/ui/event_ui.c src/ui/ranking_ui.c src/ui/news_ui.c | イベント・ランキング・イベント結果・お知らせ |
| 14 | 6000h コード | CORE3 | code: src/core/ranking.c src/core/wallet.c | NPC のいま・ランキング・みんなの動き、軍資金 |
| 15 | 6000h コード | BGM | code: src/ui/bgm.c | FM 音源の BGM ドライバと曲（tools/mml.py） |
| 16 | 8000h データA | CARDS | build/cards.bin | カード |
| 17 | 8000h データA | AREAS | build/areas.bin | エリア |
| 18 | 8000h データA | STAGES | build/stages.bin | 500 ステージ（tools/gen_stages.py） |
| 19 | 8000h データA | GACHA | build/gacha.bin | ガチャ |
| 20 | 8000h データA | LOGIN | build/login.bin | ログインボーナス |
| 21 | 8000h データA | COURSES | build/courses.bin | お財布のコース（軍資金） |
| 22 | 8000h データA | PACKS | build/packs.bin | 石のパック（課金） |
| 23 | 8000h データA | EVENTS | build/events.bin | イベント |
| 24 | 8000h データA | EVSTAGES | build/evstages.bin | イベントの面 |
| 25 | 8000h データA | EVREWARDS | build/evrewards.bin | イベント報酬（ポイント・順位） |
| 26 | 8000h データA | NPC0 | build/npc0.bin | NPC 1000 人（前半, tools/gen_npc.py） |
| 27 | 8000h データA | NPC1 | build/npc1.bin | NPC（後半） |
| 28 | 8000h データA | NPCPOW | build/npcpow.bin | 総戦力順の NPC 添字 |
| 29 | 8000h データA | TIMELINE | build/timeline.bin | 運営年表 |
| 30 | A000h データB | TEXT0 | build/text0.dat | 文字列の目次と本体 |
| 31 | A000h データB | TEXT1 | build/text1.dat | 文字列（溢れ） |
| 32 | A000h データB | TEXT2 | build/text2.dat | 文字列（溢れ） |
| 33 | A000h データB | TEXT3 | build/text3.dat | 文字列（溢れ） |
| 34 | VRAM へ | ATLAS0 | build/atlas0.bin | 画面部品のアトラス（起動時に VRAM ページ 1 へ） |
| 35 | VRAM へ | ATLAS1 | build/atlas1.bin | 画面部品のアトラス（起動時に VRAM ページ 1 へ） |
| 36 | VRAM へ | ATLAS2 | build/atlas2.bin | 画面部品のアトラス（起動時に VRAM ページ 1 へ） |
| 37 | VRAM へ | ATLAS3 | build/atlas3.bin | 画面部品のアトラス（起動時に VRAM ページ 1 へ） |
| 38 | VRAM へ | ATLAS4 | build/atlas4.bin | 画面部品のアトラス（起動時に VRAM ページ 1 へ） |
| 39 | 8000h データA | CARDART0 | build/cardart0.bin | カード絵 56x72（4 枚/ページ） |
| 40 | 8000h データA | CARDART1 | build/cardart1.bin | カード絵 56x72（4 枚/ページ） |
| 41 | 8000h データA | CARDART2 | build/cardart2.bin | カード絵 56x72（4 枚/ページ） |
| 42 | 8000h データA | CARDART3 | build/cardart3.bin | カード絵 56x72（4 枚/ページ） |
| 43 | 8000h データA | CARDART4 | build/cardart4.bin | カード絵 56x72（4 枚/ページ） |
| 44 | 8000h データA | CARDART5 | build/cardart5.bin | カード絵 56x72（4 枚/ページ） |
| 45 | 8000h データA | CARDART6 | build/cardart6.bin | カード絵 56x72（4 枚/ページ） |
| 46 | 8000h データA | CARDART7 | build/cardart7.bin | カード絵 56x72（4 枚/ページ） |
| 47 | 8000h データA | CARDART8 | build/cardart8.bin | カード絵 56x72（4 枚/ページ） |
| 48 | 8000h データA | CARDART9 | build/cardart9.bin | カード絵 56x72（4 枚/ページ） |
| 49 | 8000h データA | CARDART10 | build/cardart10.bin | カード絵 56x72（4 枚/ページ） |
| 50 | 8000h データA | CARDART11 | build/cardart11.bin | カード絵 56x72（4 枚/ページ） |
| 51 | 8000h データA | CARDART12 | build/cardart12.bin | カード絵 56x72（4 枚/ページ） |
| 52 | 8000h データA | CARDART13 | build/cardart13.bin | カード絵 56x72（4 枚/ページ） |
| 53 | 8000h データA | CARDART14 | build/cardart14.bin | カード絵 56x72（4 枚/ページ） |
| 54 | 8000h データA | CARDART15 | build/cardart15.bin | カード絵 56x72（4 枚/ページ） |
| 55 | 8000h データA | CARDART16 | build/cardart16.bin | カード絵 56x72（4 枚/ページ） |
| 56 | 8000h データA | CARDART17 | build/cardart17.bin | カード絵 56x72（4 枚/ページ） |
| 57 | 8000h データA | CARDART18 | build/cardart18.bin | カード絵 56x72（4 枚/ページ） |
| 58 | 8000h データA | CARDART19 | build/cardart19.bin | カード絵 56x72（4 枚/ページ） |
| 59 | 8000h データA | CARDART20 | build/cardart20.bin | カード絵 56x72（4 枚/ページ） |
| 60 | 8000h データA | CARDART21 | build/cardart21.bin | カード絵 56x72（4 枚/ページ） |
| 61 | 8000h データA | CARDART22 | build/cardart22.bin | カード絵 56x72（4 枚/ページ） |
| 62 | 8000h データA | CARDART23 | build/cardart23.bin | カード絵 56x72（4 枚/ページ） |
| 63 | 8000h データA | CARDART24 | build/cardart24.bin | カード絵 56x72（4 枚/ページ） |
| 64 | 8000h データA | CARDART25 | build/cardart25.bin | カード絵 56x72（4 枚/ページ） |
| 65 | 8000h データA | CARDART26 | build/cardart26.bin | カード絵 56x72（4 枚/ページ） |
| 66 | 8000h データA | CARDART27 | build/cardart27.bin | カード絵 56x72（4 枚/ページ） |
| 67 | 8000h データA | CARDART28 | build/cardart28.bin | カード絵 56x72（4 枚/ページ） |
| 68 | 8000h データA | CARDART29 | build/cardart29.bin | カード絵 56x72（4 枚/ページ） |
| 69 | 8000h データA | CARDART30 | build/cardart30.bin | カード絵 56x72（4 枚/ページ） |
| 70 | 8000h データA | CARDART31 | build/cardart31.bin | カード絵 56x72（4 枚/ページ） |
| 71 | 8000h データA | CARDART32 | build/cardart32.bin | カード絵 56x72（4 枚/ページ） |
| 72 | 8000h データA | CARDART33 | build/cardart33.bin | カード絵 56x72（4 枚/ページ） |
| 73 | 8000h データA | CARDART34 | build/cardart34.bin | カード絵 56x72（4 枚/ページ） |
| 74 | 8000h データA | CARDART35 | build/cardart35.bin | カード絵 56x72（4 枚/ページ） |
| 75 | 8000h データA | CARDART36 | build/cardart36.bin | カード絵 56x72（4 枚/ページ） |
| 76 | 8000h データA | CARDART37 | build/cardart37.bin | カード絵 56x72（4 枚/ページ） |
| 77 | 6000h コード | CORE4 | code: src/core/evfight.c | レイド・バトル（イベントの型） |
| 78 | 6000h コード | CORE5 | code: src/core/guildwar.c | ギルド対抗戦 |
| 79 | 6000h コード | UI_GUILD | code: src/ui/guildwar_ui.c | ギルド対抗戦の画面 |
| 80 | 8000h データA | BGMDATA | build/bgm.bin | BGM の曲データ（割り込みから窓 A に載せて読む, tools/mml.py） |
| 81 | RAM CC00h へ | HOTCODE | hot: src/sys/bgmdrv.c src/sys/text.c src/sys/vdp_asm.s src/sys/kanji_asm.s | 起動時に RAM CC00h〜D9FFh（最大 3,584 B）へ写して実行するコード。割り込みの BGM（4000h にどの ROM が出ていても動く）と、文字・VDP（R800 はカートリッジ ROM のコードが RAM の約 4 倍遅い） |
| 82 | 8000h データA | THUMB0 | build/thumb0.bin | カードの小さい絵 28x36（16 枚/ページ） |
| 83 | 8000h データA | THUMB1 | build/thumb1.bin | カードの小さい絵 28x36（16 枚/ページ） |
| 84 | 8000h データA | THUMB2 | build/thumb2.bin | カードの小さい絵 28x36（16 枚/ページ） |
| 85 | 8000h データA | THUMB3 | build/thumb3.bin | カードの小さい絵 28x36（16 枚/ページ） |
| 86 | 8000h データA | THUMB4 | build/thumb4.bin | カードの小さい絵 28x36（16 枚/ページ） |
| 87 | 8000h データA | THUMB5 | build/thumb5.bin | カードの小さい絵 28x36（16 枚/ページ） |
| 88 | 8000h データA | THUMB6 | build/thumb6.bin | カードの小さい絵 28x36（16 枚/ページ） |
| 89 | 8000h データA | THUMB7 | build/thumb7.bin | カードの小さい絵 28x36（16 枚/ページ） |
| 90 | 8000h データA | THUMB8 | build/thumb8.bin | カードの小さい絵 28x36（16 枚/ページ） |
| 91 | 8000h データA | THUMB9 | build/thumb9.bin | カードの小さい絵 28x36（16 枚/ページ） |
| 92-127 | - | - | - | 空き |
