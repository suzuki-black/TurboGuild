# Changelog / 更新履歴

## v0.1.0 — 2026-10-31

The first release. This is a foundation for further development: a prototype that has had little play-testing, and whose numbers and balance will change.
Checked in openMSX only; **not yet checked on real hardware**.

最初のリリースです。これから発展させるための土台で、テストプレイはまだ十分ではなく、数値やバランスは今後変わります。
openMSX でのみ確認していて、**実機ではまだ確認していません**。

### English
- A single-player RPG for the MSX turboR that recreates a 2011-era Japanese feature-phone social game, fully offline: 1,000 simulated players, rankings, operator news.
- Gacha with a different opening animation for each type; event box gacha; no pity system.
- 121 cards, fusion, evolution, deck building, and an in-game wallet in place of real money.
- Three event types (hunt, raid, battle), guilds and guild wars.
- A 270-day service timeline: rate-ups, power creep, USR on day 90 and XUSR on day 180, the end-of-service notice, and a staff roll.
- FM sound (MSX-MUSIC) background music, 5 tracks.
- English (`make`) and Japanese (`make LANG=ja`) builds.
- Saves go to `TGSAVE.DAT` on a FAT12 2DD floppy. The game asks before creating the file and re-reads the disk before every save.

### 日本語
- 2011 年ごろの日本のガラケーのソーシャルゲームを MSX turboR で再現した、一人用の RPG。1000 人の架空プレイヤー・ランキング・運営のお知らせまで、完全オフラインで動きます。
- 種類ごとに開封演出が違うガチャ。イベントは BOX ガチャで、天井はありません。
- カード 121 枚、強化合成、進化合成、デッキ編成。現実のお金の代わりにゲーム内の軍資金。
- イベント 3 種類（討伐・レイド・バトル）、ギルドとギルド対抗戦。
- 270 日の運営年表: 確率アップ、インフレ、90 日目の USR、180 日目の XUSR、サービス終了の告知、スタッフロール。
- FM 音源（MSX-MUSIC）の BGM 5 曲。
- 英語版（`make`）と日本語版（`make LANG=ja`）。
- セーブは FAT12 の 2DD フロッピーの `TGSAVE.DAT`。ファイルを作る前に確認し、保存のたびにディスクを読み直します。
