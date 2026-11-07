/* SPDX-License-Identifier: MIT */
/* sim_ops.c — 運営モード（core/opsim.c）を PC で回して数字を合わせる。
 *   sim_ops <選択 10 桁>   その選択で 270 日回し、10 日ごとの DAU・課金した人・引退・売上を出す（0000000000 = 史実）
 *   sim_ops all            選べる組み合わせをすべて回し、継続になる割合と売上の幅を出す */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "core/opsim.h"
#include "core/tables.h"
#include "config.h"

static u8 run(const u8 *pick, int verbose)
{
    OpsMeeting m;
    ops_start();
    while (g_ops.day < OPS_DAYS) {
        while (ops_meeting(&m)) ops_decide(pick[g_ops.next]);
        ops_step();
        if (verbose && (g_ops.day % 10 == 0 || g_ops.day == 1))
            printf("day %3u  reg %4u new %3u DAU %4u  pay %3u  ret %4u  sales/day %8lu  total %10lu  ad %8lu\n", g_ops.day, g_ops.reg, g_ops.fresh_users, g_ops.dau,
                   g_ops.payers, g_ops.retired, (unsigned long)g_ops.sales_day, (unsigned long)g_ops.sales_total, (unsigned long)g_ops.ad_total);
    }
    if (verbose)
        printf("profit %ld  dau_judge %u  points %u  verdict %s\n", (long)g_ops.profit_judge, g_ops.dau_judge, g_ops.points,
               g_ops.verdict == OPS_CONTINUE ? "CONTINUE" : "END");
    return g_ops.verdict;
}

int main(int argc, char **argv)
{
    static const u8 n[OPS_MEETINGS] = { 3, 2, 3, 3, 2, 3, 3, 3, 3, 3, 3, 1 };
    u8 pick[OPS_MEETINGS] = { 0 };
    int i;
    if (argc > 1 && strcmp(argv[1], "all")) {
        for (i = 0; i < OPS_MEETINGS && argv[1][i]; i++) pick[i] = (u8)(argv[1][i] - '0');
        run(pick, 1);
        return 0;
    }
    {
        unsigned long rmax = 0, dmax = 0;
        unsigned long total = 0, cont = 0, best = 0, worst = ~0UL, hist = 0;
        unsigned long cont_by[OPS_MEETINGS][3] = { { 0 } }, cnt_by[OPS_MEETINGS][3] = { { 0 } };
        char best_s[16] = "";
        for (;;) {
            u8 v = run(pick, 0);
            unsigned long s = g_ops.sales_total;
            total++;
            if (getenv("OPS_CSV")) { for (i = 0; i < OPS_MEETINGS; i++) putchar('0' + pick[i]); printf(",%lu,%ld,%u,%u,%u\n", s, (long)g_ops.profit_judge, g_ops.dau_judge, g_ops.retired, g_ops.points); }
            if (g_ops.profit_judge > (long)rmax) rmax = (unsigned long)g_ops.profit_judge;
            if (g_ops.dau_judge > dmax) dmax = g_ops.dau_judge;
                        if (v == OPS_CONTINUE) cont++;
            for (i = 0; i < OPS_MEETINGS; i++) { cnt_by[i][pick[i]]++; if (v == OPS_CONTINUE) cont_by[i][pick[i]]++; }
            if (s > best) { best = s; for (i = 0; i < OPS_MEETINGS; i++) best_s[i] = (char)('0' + pick[i]); }
            if (s < worst) worst = s;
            if (total == 1) hist = s;
            for (i = 0; i < OPS_MEETINGS; i++) {           /* 次の組み合わせ */
                if (++pick[i] < n[i]) break;
                pick[i] = 0;
            }
            if (i == OPS_MEETINGS) break;
        }
        printf("combos %lu  continue %lu (%.1f%%)  sales hist %lu best %lu (%s) worst %lu\n", total, cont,
               100.0 * cont / total, hist, best, best_s, worst);
        printf("profit max %lu  dau_judge max %lu\n", rmax, dmax);
        for (i = 0; i < OPS_MEETINGS - 1; i++) {
            int k;
            printf("m%d:", i);
            for (k = 0; k < n[i]; k++) printf("  [%d] %.1f%%", k + 1, 100.0 * cont_by[i][k] / cnt_by[i][k]);
            printf("\n");
        }
    }
    return 0;
}
