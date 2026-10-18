# MEMORY_MAP — ASCII8 ページ割り当て表

`tools/link_rom.py` がこの表を読んで `bin/TurboGuild.rom` を組み立て、
`src/data/rompages.h`（`#define PAGE_<名前> n`）を生成する。**表を書き換えたら make し直す。**

- 1 ページ = 8KB。64 ページ（512KB）。絵を入れたので Phase 1 の 256KB から広げた。
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
| 1 | 6000h コード | CORE | code: src/core/player.c src/core/inventory.c src/core/deck.c src/core/battle.c src/core/quest.c src/core/gacha.c src/core/login.c src/core/wallet.c | ゲームロジック |
| 2 | 6000h コード | UI_START | code: src/ui/title.c src/ui/name.c src/ui/course.c src/ui/firstgacha.c src/ui/savedisk.c src/core/save.c src/core/time.c src/sys/rtc.c | タイトル・名前・お財布・最初の 10 連・セーブディスク・時計 |
| 3 | 6000h コード | UI_HOME | code: src/ui/home.c src/ui/shop.c src/ui/login_ui.c | マイページ・ショップ・ログインスタンプ |
| 4 | 6000h コード | UI_QUEST | code: src/ui/quest_ui.c | エリア一覧・探索 |
| 5 | 6000h コード | UI_GACHA | code: src/ui/gacha_ui.c | ガチャのバナー一覧・結果 |
| 6 | 6000h コード | UI_CARD | code: src/ui/card_ui.c src/ui/deck_ui.c | カード一覧・詳細・デッキ編成 |
| 7 | 6000h コード | UI_DEBUG | code: src/ui/p0menu.c src/ui/p0tests.c src/ui/p0save.c src/ui/p0bank.c src/sys/font.c | Phase0 の診断画面（タイトルで ESC） |
| 8 | 6000h コード | UI_COMMON | code: src/ui/common.c | 共通部品（上下の帯・状態欄・箱・保存） |
| 9 | 6000h コード | UI_BOSS | code: src/ui/boss.c | ボス戦 |
| 10 | 6000h コード | UI_ANIM | code: src/ui/gacha_anim.c | ガチャの開封演出とカードめくり |
| 11 | 8000h データA | CARDS | build/cards.bin | カード |
| 12 | 8000h データA | AREAS | build/areas.bin | エリア |
| 13 | 8000h データA | STAGES | build/stages.bin | 500 ステージ（tools/gen_stages.py） |
| 14 | 8000h データA | GACHA | build/gacha.bin | ガチャ |
| 15 | 8000h データA | LOGIN | build/login.bin | ログインボーナス |
| 16 | A000h データB | TEXT0 | build/text0.dat | 文字列の目次と本体 |
| 17 | A000h データB | TEXT1 | build/text1.dat | 文字列（溢れ） |
| 18 | A000h データB | TEXT2 | build/text2.dat | 文字列（溢れ） |
| 19 | A000h データB | TEXT3 | build/text3.dat | 文字列（溢れ） |
| 20 | VRAM へ | ATLAS0 | build/atlas0.bin | 画面部品のアトラス（起動時に VRAM ページ 1 へ） |
| 21 | VRAM へ | ATLAS1 | build/atlas1.bin | 画面部品のアトラス（起動時に VRAM ページ 1 へ） |
| 22 | VRAM へ | ATLAS2 | build/atlas2.bin | 画面部品のアトラス（起動時に VRAM ページ 1 へ） |
| 23 | VRAM へ | ATLAS3 | build/atlas3.bin | 画面部品のアトラス（起動時に VRAM ページ 1 へ） |
| 24 | VRAM へ | ATLAS4 | build/atlas4.bin | 画面部品のアトラス（起動時に VRAM ページ 1 へ） |
| 25 | 8000h データA | CARDART0 | build/cardart0.bin | カード絵 56x72（4 枚/ページ） |
| 26 | 8000h データA | CARDART1 | build/cardart1.bin | カード絵 56x72（4 枚/ページ） |
| 27 | 8000h データA | CARDART2 | build/cardart2.bin | カード絵 56x72（4 枚/ページ） |
| 28 | 8000h データA | CARDART3 | build/cardart3.bin | カード絵 56x72（4 枚/ページ） |
| 29 | 8000h データA | CARDART4 | build/cardart4.bin | カード絵 56x72（4 枚/ページ） |
| 30 | 8000h データA | CARDART5 | build/cardart5.bin | カード絵 56x72（4 枚/ページ） |
| 31 | 8000h データA | CARDART6 | build/cardart6.bin | カード絵 56x72（4 枚/ページ） |
| 32 | 8000h データA | CARDART7 | build/cardart7.bin | カード絵 56x72（4 枚/ページ） |
| 33 | 8000h データA | CARDART8 | build/cardart8.bin | カード絵 56x72（4 枚/ページ） |
| 34 | 8000h データA | CARDART9 | build/cardart9.bin | カード絵 56x72（4 枚/ページ） |
| 35 | 8000h データA | THUMB0 | build/thumb0.bin | カードの小さい絵 28x36（16 枚/ページ） |
| 36 | 8000h データA | THUMB1 | build/thumb1.bin | カードの小さい絵 28x36（16 枚/ページ） |
| 37 | 8000h データA | THUMB2 | build/thumb2.bin | カードの小さい絵 28x36（16 枚/ページ） |
| 38 | 8000h データA | COURSES | build/courses.bin | お財布のコース（軍資金） |
| 39 | 8000h データA | PACKS | build/packs.bin | 石のパック（課金） |
| 40-63 | - | - | - | 空き |
