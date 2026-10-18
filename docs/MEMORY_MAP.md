# MEMORY_MAP — ASCII8 ページ割り当て表

`tools/link_rom.py` がこの表を読んで `bin/TurboGuild.rom` を組み立て、
`src/data/rompages.h`（`#define PAGE_<名前> n`）を生成する。**表を書き換えたら make し直す。**

- 1 ページ = 8KB。Phase 0/1 は 32 ページ（256KB）。
- ページ 0 以外の先頭 16 バイトは**ページ見出し**: `[ページ番号, 'T', 'G', 'P', 種別, FFh×11]`。
  中身はオフセット 16（窓の先頭 + 10h）から置く。バンク切替テストはこの見出しを読む。
- 種別: 0=空き 1=コード 2=データA 3=データB
- ソース `kernel` は SDCC のリンク結果のうち 0x4000-0x5FFF（バンクに載せないソース全部）。
- ソース `code: <ファイル…>` はコードバンク。そのソースは SDCC の領域 `_BANKn` に入り、0xn6010 にリンクされる
  （24bit 番地。下位 16bit が窓 6000h の中の位置）。Makefile はこの表からソース→バンクを決める。
- 別バンクの関数は `FAR(...)` で定義し、カーネルに生成される中継を通して呼ぶ（`src/far.h`, DECISIONS #14）。
  直接呼ぶとリンクは通るが実行時に別物へ飛ぶので、`tools/check_banks.py` がビルド時に止める。
- `-` は空き（FFh 埋め）。データのページは ROM の中身をそのまま置く（+10h から）。

| page | 窓 | 名前 | ソース | 説明 |
|---|---|---|---|---|
| 0 | 4000h 固定 | KERNEL | kernel | 起動・sys/・文字描画・表の読み出し・画面の共通部品・中継 |
| 1 | 6000h コード | CORE | code: src/core/player.c src/core/inventory.c src/core/deck.c src/core/battle.c src/core/quest.c src/core/gacha.c src/core/login.c | ゲームロジック |
| 2 | 6000h コード | UI_START | code: src/ui/title.c src/ui/name.c src/ui/firstgacha.c src/ui/savedisk.c src/core/save.c | タイトル・名前・最初の 10 連・セーブディスク |
| 3 | 6000h コード | UI_HOME | code: src/ui/home.c src/ui/shop.c | ホーム・ショップ・ログインボーナス |
| 4 | 6000h コード | UI_QUEST | code: src/ui/quest_ui.c | エリア・ステージ選択・戦闘・結果 |
| 5 | 6000h コード | UI_GACHA | code: src/ui/gacha_ui.c | ガチャ |
| 6 | 6000h コード | UI_CARD | code: src/ui/card_ui.c src/ui/deck_ui.c | カード一覧・詳細・デッキ編成 |
| 7 | 6000h コード | UI_DEBUG | code: src/ui/p0menu.c src/ui/p0tests.c src/ui/p0save.c src/ui/p0bank.c src/sys/font.c | Phase0 の診断画面（タイトルで ESC） |
| 8 | 6000h コード | UI_COMMON | code: src/ui/common.c | 共通部品（状態表示・確認・ガチャ演出・保存） |
| 9 | 8000h データA | CARDS | build/cards.bin | カード |
| 10 | 8000h データA | AREAS | build/areas.bin | エリア |
| 11 | 8000h データA | STAGES | build/stages.bin | 500 ステージ（tools/gen_stages.py） |
| 12 | 8000h データA | GACHA | build/gacha.bin | ガチャ |
| 13 | 8000h データA | LOGIN | build/login.bin | ログインボーナス |
| 14 | A000h データB | TEXT0 | build/text0.dat | 文字列の目次と本体 |
| 15 | A000h データB | TEXT1 | build/text1.dat | 文字列（溢れ） |
| 16 | A000h データB | TEXT2 | build/text2.dat | 文字列（溢れ） |
| 17 | A000h データB | TEXT3 | build/text3.dat | 文字列（溢れ） |
| 18-31 | - | - | - | 空き |
