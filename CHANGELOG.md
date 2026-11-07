# Changelog / 更新履歴

## Unreleased / 未リリース

### English
- **Operator mode.** After the service ends, a fourth item on the title screen lets you run the same 270 days as the operator. Twelve meetings at the timeline's turning points (drop rate, pity, ads, rate-ups, featured slots and cross-promotion, new rarities, apology gems) each offer what history did plus other options, with a line saying why the operators chose it back then. Players arrive over time through word of mouth, platform promotion and ads; an ad campaign has a budget cap and a CVR cutoff, and stops by itself when either is hit. A graph shows DAU, new players, the paying rate, players who quit and daily sales for 1,000 NPCs. On day 240, profit (sales minus ad spend) and DAU decide whether the service goes on. History's choices fall short and the service closes. Your best profit is saved. The save format is now version 12; older saves still load.
- **Battle choices.** Press 0 on the explore screen or the event top to switch between Normal, Save (half AP, 60% attack) and All-out (double AP, 250% attack). Rewards stay the same, so you either farm easy stages cheaply or pay more AP to beat a stronger enemy. The first card in the deck is now the leader: cards of its element get +20% attack. You can keep three decks (A to C) and switch with 7 to 9 on the deck screen. The save format is now version 11; older saves still load.
- **Rival.** When you open the ranking or the event screen, the player just above you becomes your rival and stays your rival until you pass them. My Page shows the gap in the header ("Rival-320"). When you pass them, they react; during an event they may promise to pass you back on the last day. The marquee tells you whether, at your current pace, you will catch your rival by the last day, or whether the rival you passed will catch you. The save format is now version 10 (7 bytes added); older saves still load.
- **Guild board.** Each guild has seven or eight mates with a payment style and a verbal tic. Every six in-game hours one of them may post about the current event, guild war, rate-up, new rarity, the end-of-service notice, or their day. A few days before quitting, a mate starts to sound tired; on the day itself the board only says they have left the guild. Open it from News (SPACE). Posts also run on the My Page marquee. Nothing is saved: posts are worked out from the date.
- The explore screen and the event top now show the enemy's weakness ("[6] Weak Water"). Press 6 there to build the deck for that enemy, counting element advantage and event boon cards. Elements already affected battles; they were just not visible.

### 日本語
- **運営モード。** サービス終了後のタイトルに 4 つ目の項目が増え、同じ 270 日を運営の側から回せます。年表の節目で開く 12 回の会議（排出率・天井・広告・確率アップ・一押し枠と相互送客・新しいレア度・お詫びの石）では、史実の選択肢とほかの選択肢を選べ、当時なぜそうしたかが 1 行で分かります。人は口コミ・プラットフォームの送客・広告で少しずつ入ってきます。広告は予算の上限と打ち切りの CVR を決めて打ち、どちらかに当たると自動で止まります。NPC 1000 人の DAU・新規・課金率・引退・日ごとの売上をグラフで見ます。240 日目に、利益（売上 - 広告費）と DAU でサービスを続けるかが決まります。史実どおりでは届かず、終了します。利益の最高はセーブに残ります。セーブの版は 12 になりました。前の版のセーブも読めます。
- **戦う前の選択。** 探索の画面とイベントのトップで 0 を押すと、通常・節約（AP 半分・攻撃 60%）・全力（AP 2 倍・攻撃 250%）を替えられます。報酬は変わらないので、格下を安く回すか、AP を多く払って格上に勝つかを選ぶことになります。デッキの 1 枚目がリーダーになり、同じ属性のカードの攻撃が +20% になります。デッキは 3 つ（A〜C）持てて、デッキ画面の 7〜9 で替えます。セーブの版は 11 になりました。前の版のセーブも読めます。
- **ライバル。** ランキングかイベントの画面を開くと、自分のすぐ上の人がライバル（宿敵）になります。抜くまで替わりません。マイページの見出しに差（「宿敵-320」）を出します。抜くと相手がひとこと言い、イベント中なら「最終日に見てろよ」と返してくることもあります。流れるお知らせでは、いまのペースで最終日にライバルに届くか、抜いた元ライバルに抜き返されるかを伝えます。セーブの版は 10 になりました（7 バイト増）。前の版のセーブも読めます。
- **ギルド伝言板。** どのギルドにも、課金コースと口癖を持つ仲間が 7〜8 人います。ゲーム内の 6 時間ごとに、誰かがいまのイベント・対抗戦・確率アップ・新しいレア度・サービス終了の告知、またはその日のことを書き込みます。辞める数日前から弱音が増え、その日には「ギルドを脱退しました」とだけ流れます。お知らせの画面で SPACE を押すと開きます。マイページの流れるお知らせにも出ます。書き込みは日付から決まるので、セーブは増えていません。
- 探索の画面とイベントのトップに、敵の弱点（「[6] 弱点 水」）を出すようにしました。そこで 6 を押すと、属性の相性とイベントの特効を込みで、その敵向けにおまかせ編成します。属性は前から戦闘に効いていましたが、画面に出ていませんでした。

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
