# DECISIONS — 仮決め・仕様からの変更の記録（1 行 / 件）

1. **RAM 予算を 10KB → 静的 6KB に変更**。実測（openMSX FS-A1ST, FDD 1 台）で HIMEM=DE67h、C000h〜HIMEM は 7,783 B しかない。静的 RAM 6,144 B ＋ スタック約 1.6KB とし、`make` の表示も `RAM used: x / 6144`。実機で HIMEM がさらに低い場合は再調整。
2. CPU は仕様どおり `CHGCPU` に `81h`（R800 ROM モード + LED）。S1990 の直書きはしない。
3. 半角字形は ROM に持たず、本体 BIOS の `CGTABL`（0004h）を読む。半角カナもそのまま出せ、ROM を消費しない。
4. 仕様 §10 の `tools/demo_*.c` は作らず、ROM 内の Phase0 試験画面（BANK / CARD / SAVE・RTC / INPUT・SOUND）で代える。1 本の ROM で全部確認できるため。
5. `cards.csv` には名前を文字列のまま書き、`text_id` は `build_data.py` が振る。元データ（CSV・data/text）は UTF-8、`text.dat` は Shift-JIS(cp932) に変換して入れる。
6. セーブディスクのメディア ID は `F9h`（2DD 720KB）固定。turbo R の内蔵 FDD は 2DD のみのため、2HD 判定は実装しない。
7. Phase0 ではセーブディスク初期化の確認画面を省き、識別セクタが無いディスクには初回保存時に識別を書く。確認画面は Phase1 の UI で付ける。
8. ROM のページ 0 以外の先頭 16 バイトを「ページ見出し」`[page,'T','G','P',種別]` とし、中身は +10h から置く。バンク切替テストと取り違え検出のため。
9. INIT が 2 回呼ばれる（ASCII8 の初期バンクで 8000h にも "AB" が見える）ので、H.STKE が既に自分を指していれば黙って RET する。
10. ジョイスティックは BIOS `GTSTCK` でなく PSG R#14 を直接読む（キーボードのカーソルと混ざらないように）。
11. `AP_MAX = 20`（仮）。
12. 自動テストの機種は openMSX 同梱の `Panasonic_FS-A1ST`（FDD 付き実ファーム、システムROM が必要）と、非 turboR 確認用の `C-BIOS_MSX2_JP`。
13. **コードバンク 1（ページ 1）を窓 6000h に常駐**させ、カーネル 8KB に入らない UI とセーブ管理を置く（Makefile の `BANK1_SRC`、SDCC 領域 `_BANK1` を 0x6010 にリンク）。Phase0 の試験画面だけでカーネルが 8KB を超えたため。複数コードバンクの `bank_call` は Phase1 で作る。
