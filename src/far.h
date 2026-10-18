/* far.h — コードバンクに置く関数の定義に使う印（DECISIONS #14）
 *
 *   FAR(u8, battle_run, (const BattleIn *in)) { ... }
 *
 * SDCC では本体を battle_run_b という名前で作り、tools/gen_far.py がカーネルに
 * battle_run という中継（バンクを切り替えて呼ぶ）を生成する。呼ぶ側は普通に battle_run() と書く。
 * gcc（PC の単体テスト）では印は消え、普通の関数になる。
 *
 * 約束: 引数は 2 個まで、レジスタで渡る組み合わせだけ（u8 1 個 / 16bit 1 個 / u8 2 個 / 16bit 2 個）。
 *       別バンクの const（文字列・表）へのポインタを渡さない（呼び先の窓からは見えない）。
 *
 *   SCREEN(scr_home, home_init, home_update, home_draw, 0)
 * は画面表（Screen）をカーネルに生成させる印。中の関数は FAR で定義しておく。 */
#ifndef FAR_H
#define FAR_H
#ifdef __SDCC
#define FAR(ret, name, args) ret name##_b args
#else
#define FAR(ret, name, args) ret name args
#endif
#define SCREEN(var, init, update, draw, exit)
#endif
