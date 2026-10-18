# TurboGuild

MSX turboR 専用の「2011 年頃のソシャゲ文化再現 RPG」。完全オフラインで、
「オンラインっぽさ」（NPC・ランキング・運営年表）はすべてシミュレーションで再現する。

- 企画書: [docs/MSX_turboR_SocialRPG_Design.md](docs/MSX_turboR_SocialRPG_Design.md)
- 実装指示書: [IMPLEMENTATION_SPEC.md](IMPLEMENTATION_SPEC.md)（企画書と矛盾したらこちらを優先）
- 仮決めの記録: [docs/DECISIONS.md](docs/DECISIONS.md)
- ROM ページ割り当て: [docs/MEMORY_MAP.md](docs/MEMORY_MAP.md)

配布形態は **ASCII8 メガROM（フラッシュカートリッジ）＋ セーブ専用フロッピー**。

## 必要なもの

- SDCC 4.x（`sdcc`, `sdasz80`）、Python 3.11+、gcc、GNU make
- openMSX（動作確認は 21.0）と Panasonic FS-A1ST のシステムROM
  （`~/.openMSX/share/systemroms/` に置く。リポジトリには含めない）

## ビルドと実行

```bash
make
```

```bash
make run
```

`make` で `bin/TurboGuild.rom`（256KB, ASCII8）と `bin/save.dsk`（空の 2DD）ができる。
`make run` は FS-A1ST（FDD 付き）で ROM とセーブディスクを挿して起動する。
手で起動する場合は `-romtype ASCII8` を必ず付ける:

```bash
openmsx -machine Panasonic_FS-A1ST -carta bin/TurboGuild.rom -romtype ASCII8 -diska bin/save.dsk
```

## 自動確認

| コマンド | 確認すること |
|---|---|
| `make test` | `core/`（rng・time・save）の PC 単体テスト |
| `make check` | ヘッドレス起動 → 各試験画面のスクショ → 保存 → リセット → 再起動で RTC 差分 × 10 が `g_time` に加算される |
| `make check-nodisk` | ディスク無しで起動し、保存しようとすると `NO DISK` を出して続行できる |
| `make check-msx2` | MSX2（C-BIOS）では「turboR 専用」を出して止まる |

結果は `build/run*/check.txt` とスクリーンショット `build/run*/*.png`。
openMSX の後始末は自分の実行ディレクトリのパスで絞って行う（`pkill -f openmsx` は使わない）。

## Phase 0 の状況

| 項目 | 状態 | 確認方法 |
|---|---|---|
| `make` で ROM、`make run` で turboR + FDD が起動し SCREEN5 に「TurboGuild」 | 済 | `make check` の `menu.png` |
| H.STKE 方式で起動し、HIMEM と空き RAM を表示 | 済 | 起動画面。実測 HIMEM=DE67h、C000h〜HIMEM = 7,783 B |
| バンク切替テスト画面（全ページの見出しを 3 つの窓で照合） | 済 | `bank.png`: 31/31 OK × 3 窓 |
| `sys/` 各モジュールの最小実装と動作確認 | 済 | ROM 内の試験画面で代替（DECISIONS #4） |
| `build_data.py` → `link_rom.py` で cards.csv（10 枚）を ROM から読む | 済 | `cards.png` |
| `tests/` で `core/rng.c` と `core/time.c` が gcc で通る | 済 | `make test` |
| セーブ → リセット → ロードで RTC 差分が加算、ディスク無しでも遊べる | 済 | `make check` / `make check-nodisk` |
| turboR 以外では専用メッセージを出して止まる | 済 | `make check-msx2` |
| 実機（フラッシュカートリッジ＋FD）での確認 | 未 | 仕様では各フェーズ末の受け入れ時 |

## 構成

```
src/sys/   ハード依存（crt0・VDP・フォント・入力・PSG・RTC・ディスク・バンク）
src/core/  ゲームロジック（ハード非依存。PC でテストする）
src/ui/    画面（Screen 表）とセーブディスク管理（大半はコードバンク 1 に載る）
src/data/  生成ヘッダ（手で編集しない）
data/      元データ（UTF-8 の CSV）
tools/     Python ビルドツール
openmsx/   自動確認用 Tcl
```

## 踏んだ地雷（再発させない）

- **REDCLK はメインROMではなく SUB-ROM のエントリ（01F5h）。** メインROMの 01F5h を直接
  `call` すると RDSLT の内部に飛び込み、ページ3 がカートリッジに切り替わって暴走する。
  `ld ix,#0x01F5 / call EXTROM` で呼ぶ。他の SUB-ROM ルーチンも同様。
- **VDP コマンド（HMMV）が走っている間に CPU で VRAM へ書くと、後からコマンドに塗りつぶされる。**
  文字描画の前に S#2 の CE を待つ。
- **SCREEN5 の色 0 は既定で透明**で、周辺色（R#7）が見える。R#8 の TP を立てる。
- **セーブ構造体（882 B）をセクタ単位（512 B × 2）で直接読み書きすると、後ろの RAM 変数まで
  保存・復元される。** リセットしたのにメニュー位置が前回のまま、という形で出た。
  1 セクタずつ `g_sector` 経由でコピーする（`src/ui/savedisk.c`）。
- SDCC の `--codeseg BANK1` が作る領域名は `_BANK1`。リンカには `-Wl-b_BANK1=0x6010` と書く。
- openMSX の Tcl で `after` から呼ぶ手順に空の要素を混ぜると「invalid command name ""」で
  連鎖が無言で止まる。

## カートリッジへの書き込み（実機）

ASCII8 対応のフラッシュROMカートリッジに `bin/TurboGuild.rom` を **マッパ種別 ASCII8** を
指定して書き込む。セーブ用に物理フォーマット済みの 2DD フロッピーを A ドライブに入れる。
**このディスクはゲーム専用**（DOS でファイルを書き込むとセーブ領域を壊す）。
