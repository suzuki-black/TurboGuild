/* test_rng.c — xorshift32 の既知値・系列の独立性・範囲 */
#include "check.h"
#include "core/rng.h"

int main(void)
{
    u32 s = 1;
    u32 a = 0x2011C0DEUL, b = 0x2011C0DEUL, c = 12345;
    int i;
    /* xorshift32(13,17,5) の種 1 からの最初の値（Marsaglia の定義どおりに計算した値） */
    CHECK(rng_next(&s) == 270369UL);
    CHECK(rng_next(&s) == 67634689UL);
    /* 状態 0 は不動点なので 1 に逃がす */
    s = 0;
    CHECK(rng_next(&s) == 270369UL);
    /* 同じ種なら同じ系列（ガチャの再現性） */
    for (i = 0; i < 1000; i++) CHECK(rng_next(&a) == rng_next(&b));
    /* 範囲 */
    for (i = 0; i < 100000; i++) CHECK(rng_range(&c, 10000) < 10000);
    /* 偏りの粗い確認: 10 区間それぞれ 10% ± 1% */
    {
        long hist[10] = {0};
        for (i = 0; i < 100000; i++) hist[rng_range(&c, 10)]++;
        for (i = 0; i < 10; i++) CHECK(hist[i] > 9000 && hist[i] < 11000);
    }
    CHECK_DONE("rng");
    return 0;
}
