# MEMORY_MAP — ASCII8 ページ割り当て表

`tools/link_rom.py` がこの表を読んで `bin/TurboGuild.rom` を組み立て、
`src/data/rompages.h`（`#define PAGE_<名前> n`）を生成する。**表を書き換えたら make し直す。**

- 1 ページ = 8KB。Phase 0/1 は 32 ページ（256KB）。
- ページ 0 以外の先頭 16 バイトは**ページ見出し**: `[ページ番号, 'T', 'G', 'P', 種別, FFh×11]`。
  中身はオフセット 16（窓の先頭 + 10h）から置く。バンク切替テストはこの見出しを読む。
- 種別: 0=空き 1=コード 2=データA 3=データB
- ソース `kernel` は SDCC のリンク結果のうち 0x4000-0x5FFF、`bank1` は 0x6010-0x7FFF（領域 BANK1）。
  `-` は空き（FFh 埋め）。
- ★コードバンク 1 は起動時に窓 6000h へ出したまま**常駐**させる。窓 6000h を切り替えてよいのは
  カーネル上のコード（バンク切替テストと、Phase1 で作る `bank_call`）だけ。

| page | 窓 | 名前 | ソース | 説明 |
|---|---|---|---|---|
| 0 | 4000h 固定 | KERNEL | kernel | 起動・sys/・main.c・Phase0 画面 |
| 1 | 6000h コード | CODE1 | bank1 | 常駐コードバンク（UI・セーブ管理）。SDCC の領域 BANK1 を 0x6010 にリンク |
| 2 | 8000h データA | CARDS | build/cards.bin | カードテーブル |
| 3 | A000h データB | TEXT | build/text.dat | 文字列（Shift-JIS） |
| 4-31 | - | - | - | 空き |
