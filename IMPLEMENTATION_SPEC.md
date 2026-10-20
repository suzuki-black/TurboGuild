# TurboGuild 実装指示書（AIコーディング用）

対象: MSX turboR 専用「2011年頃のソシャゲ文化再現RPG」
元文書: `MSX_turboR_SocialRPG_Design.md`（企画書）
本書の位置づけ: 企画書を **コードを書く側の Claude** が迷わず実装できる粒度に分解したもの。
企画書と矛盾する場合は本書を優先し、本書に無いことは企画書に従う。
本書で「（仮）」と書いた数値は実装者が勝手に変えてよいが、変えたら `docs/DECISIONS.md` に1行残すこと。

---

## 0. 最初に守ること（憲法）

1. **データ駆動**。ステージ・カード・イベント・NPCを個別コードで書かない。すべてテーブル＋汎用ロジック。
2. **ビルドはPCで、実行はMSXで**。テーブルはCSV/JSONで書き、Pythonでバイナリに変換してから実機に渡す。MSX側でテキストをパースしない。
3. **オフライン単独動作**。通信コードは一切書かない。「オンラインっぽさ」は全部シミュレーション。
4. **1機能＝1ソースファイル**。1ファイル500行を超えたら分割。
5. **動くものを小さく積む**。各フェーズの受け入れ条件を満たすまで次フェーズに手を出さない。
6. **固有名詞は自作**。既存ソシャゲのキャラ名・カード名・イベント名・用語（「怪盗」「ロワイヤル」等）を一切使わない。

---

## 1. ワークスペース

```
TurboGuild/
├── README.md
├── IMPLEMENTATION_SPEC.md     ← 本書
├── docs/
│   ├── DECISIONS.md           ← 仮決め変更の記録（1行/件）
│   └── MEMORY_MAP.md          ← セグメント割り当て表
├── src/                       ← C ソース（SDCC）
│   ├── main.c                 ← 起動・メインループ・画面遷移
│   ├── sys/                   ← ハード依存（下記 §2）
│   ├── core/                  ← ゲームロジック（ハード非依存、PCで単体テスト可）
│   ├── ui/                    ← 画面描画・入力
│   └── data/                  ← 生成されたヘッダ（手で編集禁止）
├── data/                      ← 元データ（人間が編集）
│   ├── cards.csv
│   ├── areas.csv
│   ├── stages_rule.csv
│   ├── events.csv
│   ├── gacha.csv
│   ├── login_bonus.csv
│   ├── timeline.csv           ← 運営シミュレーション年表
│   ├── npc_names.csv
│   └── text/*.txt             ← 文章（Shift-JIS）
├── tools/                     ← Python ビルドツール
│   ├── build_data.py          ← data/* → build/*.bin + src/data/*.h
│   ├── gen_stages.py          ← 500ステージ自動生成
│   ├── gen_npc.py             ← NPC1000人初期状態生成
│   ├── link_rom.py            ← MEMORY_MAP.md 通りに .bin を並べて TurboGuild.rom を生成
│   ├── mkblank_dsk.py         ← openMSX 用セーブディスクイメージ（.dsk）生成
│   └── sim_economy.py         ← インフレ/ガチャ期待値シミュレーター（PC上）
├── tests/                     ← core/ のPC単体テスト（gcc）
├── build/                     ← 中間生成物
├── bin/                       ← 出力（TurboGuild.rom, save.dsk）
├── openmsx/                   ← 自動テスト用 TCL スクリプト
└── Makefile                   ← make / make test / make run（openMSX 起動）
```

---

## 2. 技術方針（決定事項）

| 項目 | 決定 | 理由 |
|---|---|---|
| 実行形態 | **ASCII8 メガROM（8Mbit = 1MB、8KB×128ページ）＋ セーブのみ FDD** | フラッシュROMカートリッジで実機に焼ける。FD からの読み込み待ちがゼロ。開発中のテストは openMSX（`-romtype ASCII8`）で完結し、実機は検証のみ |
| 言語 | **C（SDCC）＋ 必要箇所のみ Z80 アセンブラ** | 500ステージ・NPC1000人のロジックをBASICで書くと速度と保守性が破綻する |
| CPU | 起動時に BIOS `CHGCPU`（0180h）で R800 ROM モードへ。Z80 互換コードのみ書く | R800 専用命令は使わない（エミュ・実機差回避）。turboR 以外では起動時に「turboR 専用です」と出して停止 |
| 画面 | **SCREEN 5（256×212, 16色）** + 自前フォント描画（8×8 半角、8×12 または漢字ROM経由の全角） | ソシャゲ風のカード絵・バナーを出すため。テキストモードでは「それっぽさ」が出ない |
| 文字 | Shift-JIS。漢字ROM（JIS第1水準）を `BIOS KNJRD` 相当で読んで VRAM に転送 | turboR は漢字ROM標準搭載 |
| 音 | PSG 3ch。BGM は簡易 MML → バイナリ列。FM（MSX-MUSIC）は turboR 標準なので Phase3 で対応 | |
| メモリ | ASCII8 バンク切替。レイアウトは §2.2。RAM は本体の 4000h〜BFFFh を使わず、**C000h〜FFFFh の約 12KB のみ**（BIOS ワークエリアを除く） | メモリマッパは使わない（ROM 起動時はマッパ RAM の管理を自前でやる必要があり、面倒の割に得が無い） |
| セーブ | **メインROM BIOS `PHYDIO`（0144h）による論理セクタ読み書き**。ファイルシステムは使わない。専用「セーブディスク」1枚運用 | `PHYDIO` はディスクROMの `DSKIO` を呼ぶ BIOS エントリで、FDC を直接叩くわけではない（直叩きは禁止）。ROM から DOS の BDOS は使わない。詳細 §5.1 |
| 起動方式 | **H.STKE フック方式**。ROM ヘッダの `INIT` では H.STKE（FEDAh）に自分へのジャンプを書いて即 `RET`。全スロット初期化後（ディスクROM 初期化済み、BASIC 起動直前）にフックから本体へ入る | カートリッジの `INIT` は内蔵ディスクROM（スロット3-2）より**先に**呼ばれるため、`INIT` で居座るとディスクシステム未初期化で `PHYDIO` が動かない。詳細 §2.3 |
| 時計 | turboR 内蔵 RTC（`BIOS REDCLK`）。保存した最終時刻との差分でゲーム内時間を進める | |
| 乱数 | **xorshift32**、種はセーブデータに保存。「NPCシミュレーション用」と「ガチャ用」で系列を分ける | ガチャの再現性をデバッグできるようにする |
| ツールチェーン | SDCC 4.x、Python 3.11+、openMSX（CI/自動テスト用、`-romtype ASCII8 -ext fdd` 相当）。Fusion-C は使わない（ライセンスとサイズ）。BIOS 呼び出しは自前の薄いラッパ `src/sys/` に閉じ込める | |
| 実機検証 | フラッシュROMカートリッジ（ASCII8 対応のもの）に `TurboGuild.rom` を書き込み、ブランク 3.5" FD（2DD/2HD どちらでも可、要物理フォーマット済み）をセーブ用に挿す | 実機確認は各フェーズ末の受け入れ時のみ。日常はエミュレータ |

### 2.2 ROM レイアウト（ASCII8、8KB ページ）

| Z80 アドレス | 用途 | 切替 |
|---|---|---|
| 4000h〜5FFFh | **カーネル**（ページ0 固定）: 起動、バンク切替ルーチン、割り込み、`sys/` 全部、`main.c` | 固定 |
| 6000h〜7FFFh | **コードバンク**: 画面ごと／`core/` モジュールごとに1ページ | `bank_code(n)` |
| 8000h〜9FFFh | **データバンク A**: テーブル（cards/stages/npc…） | `bank_dataA(n)` |
| A000h〜BFFFh | **データバンク B**: テキスト、画像、BGM | `bank_dataB(n)` |
| C000h〜HIMEM | RAM（セーブ構造体、VRAM 転送バッファ、スタック）。**上限は固定アドレスでなく `HIMEM`（FC4Ah）を読む** | — |
| HIMEM〜FFFFh | ディスクROM ワークエリア + BIOS ワークエリア。触らない | — |

### 2.3 起動シーケンス（H.STKE 方式）

```
電源ON
 → BIOS がスロット順に ROM ヘッダ "AB" を探して INIT を呼ぶ
 → [カートリッジ INIT]  H.STKE に JP boot_main を書く / RET   ←ここでは何もしない
 → [内蔵ディスクROM INIT]  FDD 初期化、HIMEM を下げてワーク確保
 → BASIC 起動直前に H.STKE が呼ばれる
 → [boot_main]  スタックを HIMEM 直下へ / ENASLT で 8000h〜BFFFh をカートリッジへ
               / CHGCPU で R800 / SCREEN5 初期化 / タイトルへ。二度と戻らない
```

- H.STKE 書き込み時は、既に別のフックが入っていたら（`FEDAh` が `C9h` でない）上書きせず、エラー表示して BASIC に戻す。
- RAM のサイズは `HIMEM - C000h` で動的に求め、§11 の `RAM used` チェックは **10KB（仮）** を上限とする。ディスクROM がワークを取った後の値で設計すること。
- openMSX でテストするときは **必ず FDD 付き機種設定**で起動する。FDD 無し設定ではディスクROM が初期化されず、フックの流れが実機と変わる。
- この方式は「ディスク対応 ROM ゲーム」の定番で、実機でもエミュでも同じ動作をする。

- 起動時に `ENASLT` で 8000h〜BFFFh をカートリッジスロットに向ける（ROM ヘッダは 4000h の `AB`）。
- バンク切替は ASCII8 のレジスタ 6000h/6800h/7000h/7800h へ書き込み。**現在のバンク番号を RAM に必ず鏡写し**し、割り込みハンドラは触らない（割り込み中にバンクを変えない）。
- `core/` の関数をコードバンクから呼ぶときは `bank_call(bank, fn, arg)` トランポリン経由。SDCC の `__banked` は使わず自前で書く（挙動が読める方を優先）。
- ページ割り当て表は `docs/MEMORY_MAP.md` に置き、`tools/link_rom.py` がそれを読んで 1MB イメージを組み立てる。空きページは FFh で埋める。
- 1MB フルに使う必要はない。Phase1 は 256KB（32ページ）で組み、必要に応じて広げる。

### 2.1 `src/sys/` の責務（ハード依存はここだけ）
- `vdp.c` — SCREEN5 初期化、矩形コピー、パレット、ページ切替
- `font.c` — 半角/全角描画、Shift-JIS デコード、漢字ROM読み
- `input.c` — カーソル・SPACE/RETURN/ESC・テンキー。ジョイスティック対応
- `rtc.c` — 現在時刻取得（秒精度）、`YYYYMMDDhhmmss` → 通算秒
- `disk.c` — `PHYDIO` 経由のセクタ read/write、ディスク未挿入・書込禁止・CRC エラーを戻り値で返す（リトライ 3 回）
- `bank.c` — ASCII8 バンク切替、`bank_call` トランポリン
- `psg.c` — BGM/SE ドライバ

`core/` から `sys/` を直接呼ばず、`main.c`/`ui/` 経由にする。これで `core/` は gcc で単体テストできる。

---

## 3. 時間システム（運営シミュレーションの土台）

| 定数 | 値 | 備考 |
|---|---|---|
| `TIME_SCALE` | **10** | 現実1日＝ゲーム内10日（企画書） |
| ゲーム内1日 | 86400 ゲーム秒 ＝ 現実 8640 秒（2.4時間） | |
| AP 回復 | **現実5分で1**（企画書）＝ ゲーム内50分 | 企画書の「5分」は現実時間と解釈。変更可（仮） |
| オフライン上限 | 現実 30日分まで加算、それ以上は切り捨て | RTC電池切れ対策 |
| 時刻逆行 | 差分が負なら 0 扱いにし、警告画面を1回出す | |

ゲーム内の絶対時刻 `g_time`（32bit ゲーム秒、サービス開始＝0）を**唯一の時間軸**とし、イベント開催・NPC行動・運営年表すべてこれで判定する。現実時刻は起動時に差分を取るためだけに使う。

### 3.1 運営年表 `data/timeline.csv`
```
day,kind,param,text
0,SERVICE_START,,サービス開始！
3,EVENT,1,第1回イベント開催
17,EVENT,2,
30,GACHA_RATEUP,5,SSR確率2倍キャンペーン
45,INFLATION,1,新SSR実装（攻撃力上限+50%）
...
180,PLAYER_DROP,30,アクティブ数が30%減少
240,ANNOUNCE_END,,サービス終了のお知らせ
270,SERVICE_END,,
```
- `kind` の処理は `core/timeline.c` の switch 1個で済ませる。新しい kind を増やすときだけコードを触る。
- `INFLATION` はカードの攻撃力乗数 `g_inflation_mul`（1.0→1.5→2.0…）を上げる。**カードデータ自体は書き換えない**。
- `PLAYER_DROP` は NPC のアクティブ率を下げ、ランキングの人数・チャット頻度が減る。
- `SERVICE_END` 以後はタイトルに「サービスは終了しました」を出し、閲覧モード（カード一覧のみ）に入る。周回したい場合は「思い出をリセット」で `g_time=0` の新セーブ。

---

## 4. データテーブル仕様

全テーブルは `tools/build_data.py` で **固定長レコードのバイナリ** にし、先頭に `レコード数(u16)` を置く。C 側は `struct` でそのまま読む。文字列はレコード内に持たず、`text_id(u16)` で `text.dat` を引く。

### 4.1 カード `data/cards.csv`
| 列 | 型 | 内容 |
|---|---|---|
| id | u16 | 1〜 |
| name_text | u16 | 名前（text.dat 参照） |
| rarity | u8 | 0=N 1=R 2=SR 3=SSR |
| element | u8 | 0=炎 1=水 2=雷 3=光 4=闇 |
| atk | u16 | 基礎攻撃 |
| def | u16 | 基礎防御 |
| cost | u8 | デッキコスト |
| skill | u8 | 0=なし 1=攻撃+10% 2=防御+10% 3=AP消費-1 4=ドロップ率+10%（4種で固定、増やさない） |
| max_lv | u8 | N20 R30 SR40 SSR50 |
| evo_to | u16 | 進化先 id（0=なし） |
| event_id | u16 | 0=恒常、それ以外はイベント限定 |
| img | u8 | 画像番号（Phase1 はレアリティ別の枠絵4種のみ） |

基礎ステータス目安（仮）: N atk 100〜300 / R 300〜800 / SR 800〜2000 / SSR 2000〜5000。
Lv補正: `atk × (100 + lv×5) / 100`。合成で Lv が上がる（§7）。
**初期収録 120枚**（N50 R40 SR20 SSR10）。Phase1 は 40枚で始めてよい。

### 4.2 エリア・ステージ
`data/areas.csv`: `id, name_text, element, bg_img, boss_card_id`（50行、手書き）。
ステージは手書きしない。`tools/gen_stages.py` が `stages_rule.csv` の式から 500 レコード生成する。

```
stage_no = (area-1)*10 + s        (1..500)
ap_cost  = 3 + area/5              (3..13)
enemy_hp = 200 * stage_no^1.3      (仮)
enemy_def= 10  * stage_no
exp      = 5   * stage_no
gold     = 20  * stage_no
drop_rate= 10% (s<10) / 100% ボス(s==10)
drop_tbl = area に応じた N/R テーブル id
```
`s==10` はエリアボス。撃破で次エリア解放。

### 4.3 ガチャ `data/gacha.csv`
| gacha_id | 名称 | 通貨 | 価格 | N | R | SR | SSR | 天井 | 対象プール |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 無料 | — | 1日1回 | 80 | 18 | 2 | 0 | — | 恒常 |
| 2 | ゴールド | gold | 1000 | 70 | 25 | 5 | 0 | — | 恒常 |
| 3 | プレミア | 石 | 300（10連 2700） | 74 | 20 | 5 | 1 | 100連（仮） | 恒常+開催中イベント |
| 4 | イベント | 石 | 300 | 74 | 20 | 5 | 1 | 100連 | イベント限定2倍 |

- 抽選は `rand(10000)` に対する累積閾値表。小数は使わない。
- 10連は「SR以上1枚確定」（仮）。
- プール: `event_id==0` または `event_id==開催中` のカードから **レアリティ内で等確率**。

### 4.4 イベント `data/events.csv`
```
id, name_text, story_text, boss_card_id, bonus_card_id, bonus_mul(5), evo_bonus_mul(20),
reward_tbl_id, duration_days(14), kind
```
`kind` は **テンプレート番号**。Phase2 では1種類だけ実装する:
- `kind=0 周回ボス型`: イベント専用ステージ3つ。クリアでポイント。ポイントはランキングと報酬ラインに使う。

イベント専用ロジックを `events.c` に書かず、`quest.c` が `kind` を見て分岐する分岐点は **1箇所** に留める。

### 4.5 ログインボーナス `data/login_bonus.csv`
`day(1..7), item_kind, amount`。企画書通り。判定は **ゲーム内日付** が変わったとき（現実2.4時間ごと）。7日で一周し、連続ログインが途切れてもリセットしない（当時風の緩さ）。

### 4.6 NPC `tools/gen_npc.py` → `npc.dat`
1000 件、1件 **16 bytes**（＝16KB、1セグメントに収まる）:
```
u16 name_id      (姓テーブル×名テーブルの組み合わせ番号)
u8  guild_id     (0..15)
u8  level
u16 power        (総戦力 /10 で格納)
u32 last_login   (ゲーム秒)
u32 event_pt
u8  activity     (0..255: 行動頻度)
u8  flags        (bit0: 退会済 bit1: ランカー)
```
NPC の実名リストは **姓20×名50=1000通り** を生成し、実在作品・実在人物を含まないこと。

---

## 5. セーブデータ（`core/save.h`）

```
struct SaveData {              // 固定長、合計 ≤ 8KB
  u32 magic;  u16 version; u16 crc;
  u32 real_time_last;          // RTC 通算秒
  u32 g_time;                  // ゲーム秒
  u32 rng_sim, rng_gacha;
  // プレイヤー
  char name[12]; u8 level; u16 exp; u32 gold; u32 gems;
  u8 ap; u8 potions; u8 gacha_tickets; u8 sr_tickets;
  u16 stage_cleared;           // 最大クリアステージ
  u16 pity_count;              // プレミア天井カウンタ
  u8 login_day; u32 last_login_gday; u32 last_free_gacha_gday;
  // 所持カード（最大 200 枚）
  u16 card_count;
  struct { u16 id; u8 lv; u8 flags; } cards[200];
  u8 deck[5];                  // cards[] のインデックス
  // イベント
  u16 event_id; u32 event_pt; u8 event_rewards_claimed;
  // 運営年表
  u16 timeline_cursor;
  // NPC は毎回 npc.dat から読み、g_time で決定論的に進めるので保存しない
};
```
- NPC 状態は保存しない。`npc.dat` の初期値 ＋ `rng_sim`（種固定）＋ `g_time` から **毎回同じ結果を再計算** する。これで容量ゼロ、かつ「久しぶりに起動したらあいつがランク上がってた」が再現できる。
- 構造体は **2KB 以下**（4 セクタ）に収める。RAM 約 10KB のうちセーブ用はこの 1 面分だけ。ロード時の A/B 比較はセクタ単位で読みながら CRC と `g_time` だけ先に見て、採用する方だけ展開する。`cards[200]` は 4 bytes × 200 = 800 bytes なので収まる。

### 5.1 セーブディスクのセクタ配置（ファイルシステム不使用）

| 論理セクタ | 内容 |
|---|---|
| 0 | 触らない（ブートセクタ。物理フォーマット済みディスクをそのまま使う） |
| 1〜15 | 触らない（FAT/ルートディレクトリ領域。DOS から見ると空ディスクのまま） |
| 16 | **ディスク識別**: `"TGSAVE01"` + 作成日時。無ければ「セーブディスクを初期化しますか？」を出して書く |
| 17〜20 | セーブスロット A（4 セクタ = 2KB） |
| 21〜24 | セーブスロット B（同上、A の 1 世代前） |
| 25〜 | 未使用（将来の拡張用：スクリーンショット、イベント履歴など） |

- 書き込みは **A と B を交互** に。ロード時は両方読んで CRC が通る方のうち `g_time` が大きい方を採用。
- DOS 的には FAT が未使用のままなので、ユーザーが間違って DOS でファイルをコピーすると壊れる。識別セクタに警告文字列を入れ、README に「このディスクはゲーム専用」と書く。Phase4 で FAT12 エントリを自前で書く対応を検討（優先度低）。
- FDD 未接続・ディスク未挿入時は「セーブできません（ディスクを入れてください）」で続行可能。**セーブ無しでも遊べる**こと。
- 2DD/2HD の判定は `PHYDIO` のメディア ID で行い、セクタ番号の扱いは同じ（先頭 33 セクタしか使わないので差が出ない）。

---

## 6. 戦闘（`core/battle.c`）

企画書通り最小限。
```
deck_atk = Σ(card.atk×lv補正×属性補正×特効×inflation) over deck[5]
deck_def = Σ(card.def×...)
damage_to_enemy = max(1, deck_atk - enemy_def)
turns = ceil(enemy_hp / damage_to_enemy)
player_damage = max(0, enemy_atk - deck_def) × turns
result = (player_damage < player_hp) ? WIN : LOSE
```
- 属性補正: 炎>雷>水>炎 の三すくみ ×1.2 / ×0.8、光⇔闇 相互 ×1.2。
- 演出: 「こうげき！」「○○ダメージ」のログを `turns` 回（最大5回に圧縮）表示するだけ。アニメ不要。
- 1戦闘の計算は 1 フレーム以内で終えること。

---

## 7. 合成・進化（`core/fusion.c`）

- 素材カードを選ぶ → 消費 → ベースカードに経験値。
- 得られる経験値: N 10 / R 50 / SR 300 / SSR 2000（仮）。同レアリティ素材は +50%。
- 必要経験値: `lv × 20 × (rarity+1)`。
- 進化: `evo_to` を持つカードを **同一 id 2枚** で合成すると `evo_to` に変化（Lv 1 に戻る）。
- ゴールド消費: `100 × 素材枚数`。
- 一括素材選択（「Nカードを全部」ボタン）を必ず付ける。当時の大量消費文化の再現。

---

## 8. NPC シミュレーション & 疑似オンライン（`core/npc.c`, `core/feed.c`）

起動時および画面遷移ごとに `npc_advance(from_gtime, to_gtime)`:
```
for 各 NPC:
  if 退会: continue
  経過日数 d = (to - from) / 86400
  行動回数 n = d × activity / 32            (activity 255 で 8回/日)
  for n 回:
    r = rng_sim.next()
    5%: level++ , power += ...
    1%: SSR 獲得 → feed に push
    30%: event_pt += ...
    …
  last_login = to - rng(0..1日)
  if 年表 PLAYER_DROP 発動中 and rng < drop_rate: flags |= 退会
```
- **全 NPC を毎回回さない**。`to - from` が長いときは `n` を直接計算して結果だけ加算する（ループではなく式）。1000人分で 1秒以内が目標。
- フィード（「○○がSSRを獲得」）は直近 **32件** をリングバッファで保持。画面下部に 1 行ずつ流す。
- ランキングは `power` または `event_pt` でソート。1000件のソートはやらず、**プレイヤーより上の人数をカウント**して順位にする（O(n)）。表示は上位10 + 自分前後5。
- ギルドは 16 個。名前のみ。プレイヤーは所属するだけ（ギルド機能は Phase3 以降の拡張枠）。

---

## 9. 画面一覧と遷移（`ui/`）

```
TITLE ─┬─ NEW GAME → NAME → FIRST_GACHA(10連, リセマラ可) → HOME
       └─ CONTINUE → HOME

HOME ─┬─ QUEST  → AREA_SELECT → STAGE_SELECT → BATTLE → RESULT → (HOME)
      ├─ EVENT  → EVENT_TOP → EVENT_STAGE → BATTLE → RESULT
      ├─ GACHA  → GACHA_SELECT → GACHA_RESULT
      ├─ CARD   → CARD_LIST ─┬─ CARD_DETAIL
      │                       ├─ DECK_EDIT
      │                       └─ FUSION
      ├─ RANKING
      ├─ GUILD (名簿のみ)
      ├─ NEWS (フィード一覧 + 運営お知らせ)
      ├─ SHOP (回復薬を石で)
      └─ SAVE / QUIT
```
- 画面は `struct Screen { init, update, draw, exit }` の関数ポインタ表。`main.c` のループは `current->update(); current->draw();` だけ。
- HOME は常に上部に `名前 Lv AP/MAX ゴールド 石`、下部にフィード 1 行、中央にメニュー＋**今日のバナー**（開催中イベント名）。
- 全画面 ESC で戻る。決定は SPACE/RETURN。マウス不要。
- レイアウトは 8px グリッド。文字は半角 32 桁×全角 16 桁を基準。

---

## 10. フェーズ計画と受け入れ条件

### Phase 0: 基盤（最初にやる）
- [ ] `make` で `TurboGuild.rom`（ASCII8）が出る。`make run` で openMSX（turboR 機種、FDD 有効）が起動し「TurboGuild」と SCREEN5 に出る
- [ ] H.STKE 方式で起動しており、起動直後に `HIMEM` の値と空き RAM バイト数を画面に出す（ディスクROM 初期化後であることの確認）
- [ ] バンク切替テスト: 全コードバンク・データバンクを順に切り替え、先頭バイトのページ番号を読んで一致を確認する画面
- [ ] `sys/` 各モジュールの最小実装と、それぞれの動作確認用 `tools/demo_*.c`
- [ ] `build_data.py` → `link_rom.py` のパイプラインで cards.csv（10枚）が ROM 内テーブルとして読める
- [ ] `tests/` で `core/rng.c` と `core/time.c` が gcc で通る
- [ ] `mkblank_dsk.py` で作った `.dsk` に対し、セーブ→リセット→ロードで RTC 差分が正しく加算される。ディスク無しでも起動して遊べる
- [ ] turboR 以外（openMSX の MSX2 機種）で起動すると専用メッセージを出して止まる

### Phase 1: クエスト・AP・カード・ガチャ
- [ ] 500ステージ自動生成、エリア5まで実際に遊べる（エリア6〜50はデータだけ存在）
- [ ] カード40枚、デッキ編成、戦闘式 §6
- [ ] AP 回復、回復薬
- [ ] ガチャ3種（無料/ゴールド/プレミア）、初回10連リセマラ
- [ ] ログインボーナス7日
- [ ] **受け入れ**: 新規開始から30分遊んで落ちない。セーブ/ロード往復で所持カード・ステージ進行が一致。**実機（フラッシュカートリッジ＋セーブディスク）で同じことを確認**

### Phase 2: 合成・イベント・ランキング
- [ ] 合成/進化 §7、一括素材選択
- [ ] イベント kind=0 を1本、14ゲーム日で開始→終了→報酬配布まで自動
- [ ] ランキング（NPC は静的データで可、シミュレーションは Phase3）
- [ ] **受け入れ**: イベント期間を跨いだセーブを読み込むと、正しく終了処理と報酬が走る

### Phase 3: NPC・疑似オンライン・運営シミュレーション
- [ ] `npc_advance` §8、フィード、ランキング動的化
- [ ] 年表 §3.1 を SERVICE_END まで通す（`timeline.csv` 全行）
- [ ] インフレ乗数、新SSR追加（データ追加のみでコード変更なし、を確認）
- [ ] FM音源 BGM
- [ ] **受け入れ**: `tools/sim_economy.py` で現実 27日（ゲーム内270日）のプレイヤー資産推移を出し、SERVICE_END 時点で SSR 所持 3〜8 枚になる難易度に調整済み。実機で 1 週間放置→起動が 3 秒以内

### Phase 4（任意）: カード画像、サウンド強化、セーブディスクの FAT12 対応、ROM を 1MB まで拡張

---

## 11. コーディング規約（AI向け・短く）

- C99、`stdint.h` の `u8/u16/u32` typedef を `types.h` に置く。`int` を使わない。
- 乗除算は u16 までで済むよう設計。u32 乗算は時間計算と戦闘式のみ。
- `malloc` 禁止。配列は全部静的。RAM は約 10KB しか無いので、**RAM に置く変数は全部 `ram.h` に集約**し、合計サイズをビルド時に表示する（`make` の最後に `RAM used: xxxx / 10240`）。
- ROM 内テーブルは `const` を付けて必ず ROM に落とす。`const` を忘れると RAM にコピーされて即死する。
- データバンクを跨ぐポインタを保持しない。バンクを切り替えたらそのバンクのデータは無効。
- 文字列リテラルをゲームロジックに書かない（`text_id` で引く）。デバッグ出力だけ例外。
- 関数は 40 行以内。超えたら分ける。
- `//` コメントは「なぜ」だけ書く。「何を」はコードで分かるようにする。
- マジックナンバーは `config.h` に `#define` で集約（`AP_MAX`, `TIME_SCALE`, `DECK_SIZE` …）。
- 各 `.c` の先頭に 3 行以内で責務を書く。
- コミット単位は「1チェックボックス」。コミットメッセージ先頭にフェーズ番号（`P1: gacha pool filtering`）。

---

## 11.5 ライセンスと配布方針

- turboR 版は **MIT ライセンス**。リポジトリ直下に `LICENSE`（MIT）を置き、各ソースの先頭コメントに `SPDX-License-Identifier: MIT` を1行入れる。
- `core/` と `data/` はスマホ版（将来、寄付ウェア）と共有する前提。MIT のまま共有してよい。データだけ別ライセンスにする場合は `data/LICENSE` を別に置き、`build_data.py` が ROM 内にライセンス文字列を埋め込む。
- ROM イメージは無料配布（itch.io / GitHub Releases）。タイトル画面に MIT とリポジトリ URL を表示する画面を1つ設ける。
- README の冒頭とスタッフロール（§3.1）に、**コンセプト・ゲームデザイン／実装** の役割分担を明記する。表記は既存作『真零の咆哮』に揃える。企画書 `MSX_turboR_SocialRPG_Design.md` と本書もリポジトリに同梱する。
- 第三者ライブラリは使わない方針（§2）なので、ライセンス混在の心配はない。SDCC のランタイムライブラリは GPL+リンク例外で ROM 配布に問題なし。

## 12. やらないこと（スコープ外）

- 実通信、ネット対戦、フレンド申請の実処理
- カードごとの固有スキル・固有演出
- ストーリーの分岐
- リアルマネー要素の実装（石は無料配布＋ゲーム内獲得のみ。「課金」はお知らせ文面の中だけに存在する演出）
- MSX2/2+ への対応（turboR 専用で割り切る）

---

## 13. 実装者への最初の指示

1. この仕様書を読み、`docs/DECISIONS.md` を空で作る。
2. Phase 0 のチェックボックスを上から順に潰す。各項目が終わるたびに動作確認方法（openMSX のコマンドやスクリーンショット）を README に追記。
3. 不明点は **仮決めして進め、DECISIONS.md に書く**。質問で止まらない。
4. Phase 1 完了時点で `TurboGuild.rom`、`save.dsk`（openMSX 用）、openMSX の起動手順、フラッシュカートリッジへの書き込み手順（ASCII8 を指定すること）を納品する。
