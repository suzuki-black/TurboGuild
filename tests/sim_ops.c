/* SPDX-License-Identifier: MIT */
/* sim_ops.c — 運営モード（core/opsim.c）を PC で回して数字を合わせる。
 *   sim_ops <選択 10 桁>   その選択で 270 日回し、10 日ごとの DAU・課金した人・引退・売上を出す（0000000000 = 史実）
 *   sim_ops all            選べる組み合わせをすべて回し、継続になる割合と売上の幅を出す（企画会議は見送る）
 *   sim_ops histh          史実の選択の DAU と売上を、グラフの点（0..120）にした表を C のヘッダで出す（src/data/ops_hist.h, DECISIONS #100）
 *   sim_ops plans [N]      会議と企画会議（DECISIONS #99）を無作為に N 回（既定 20000）選び、施策ごとの継続の割合・利益・DAU を出す */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "core/opsim.h"
#include "core/tables.h"
#include "config.h"
#include "data/text_ids.h"

static u8 run(const u8 *pick, int verbose)
{
    OpsMeeting m;
    ops_start();
    while (g_ops.day < OPS_DAYS) {
        while (ops_meeting(&m)) ops_decide(m.text == T_OPS_PLAN ? 0 : pick[g_ops.next]);
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
    if (argc > 1 && !strcmp(argv[1], "histh")) {
        u8 dau[OPS_DAYS], sal[OPS_DAYS];
        OpsMeeting m;
        ops_start();
        while (g_ops.day < OPS_DAYS) {
            while (ops_meeting(&m)) ops_decide(0);
            ops_step();
            dau[g_ops.day - 1] = (u8)(g_ops.dau >= 1000 ? OPS_GRAPH_H : (u32)g_ops.dau * OPS_GRAPH_H / 1000);
            sal[g_ops.day - 1] = (u8)(g_ops.sales_day >= OPS_GRAPH_SALES ? OPS_GRAPH_H : g_ops.sales_day * OPS_GRAPH_H / OPS_GRAPH_SALES);
        }
        printf("/* SPDX-License-Identifier: MIT */\n/* ops_hist.h — 生成物: build/sim_ops histh（tests/sim_ops.c）。史実の選択で回したときの日ごとの DAU と売上を\n"
               " * グラフの点（0..OPS_GRAPH_H）にしたもの。運営モードのグラフに点線で重ねる（DECISIONS #100）。模型を変えたら作り直す（テストが比べる） */\n");
        printf("#ifndef DATA_OPS_HIST_H\n#define DATA_OPS_HIST_H\nstatic const u8 ops_hist[2][%d] = {\n", OPS_DAYS);
        for (i = 0; i < 2; i++) {
            int d;
            printf("    {");
            for (d = 0; d < OPS_DAYS; d++) printf("%s%s%u", d ? "," : "", d % 30 ? "" : "\n     ", i ? sal[d] : dau[d]);
            printf(" },\n");
        }
        printf("};\n#endif\n");
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "single")) {      /* 会議は史実、企画会議ではその施策が出たら必ず選ぶ */
        int a;
        for (a = 0; a < OPS_P_COUNT; a++) {
            OpsMeeting m;
            u8 times = 0;
            ops_start();
            while (g_ops.day < OPS_DAYS) {
                while (ops_meeting(&m)) {
                    u8 k = 0;
                    if (m.text == T_OPS_PLAN) for (k = OPS_PLAN_OPTS - 1; k && ops_plan_offer(k) != a; k--) {}
                    if (k) times++;
                    ops_decide(k);
                }
                ops_step();
            }
            printf("p%-2d x%-2u profit %6ld man  sales %6lu man  dau %4u  points %3u  %s\n", a, times, (long)(g_ops.profit_judge / 10000),
                   (unsigned long)(g_ops.sales_total / 10000), g_ops.dau_judge, g_ops.points, g_ops.verdict == OPS_CONTINUE ? "CONTINUE" : "end");
        }
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "plans")) {
        static const u8 n2[OPS_MEETINGS] = { 3, 2, 3, 3, 2, 3, 3, 3, 3, 3, 3, 1 };
        unsigned long runs = argc > 2 ? strtoul(argv[2], 0, 10) : 20000, r, cont = 0;
        unsigned long cnt[OPS_P_COUNT] = { 0 }, ccont[OPS_P_COUNT] = { 0 };
        double prof[OPS_P_COUNT] = { 0 }, dau[OPS_P_COUNT] = { 0 };
        unsigned seed = 12345;
        for (r = 0; r < runs; r++) {
            u8 used[OPS_P_COUNT] = { 0 };
            OpsMeeting m;
            ops_start();
            while (g_ops.day < OPS_DAYS) {
                while (ops_meeting(&m)) {
                    seed = seed * 1103515245u + 12345u;
                    if (m.text == T_OPS_PLAN) {
                        u8 k = (u8)((seed >> 16) % OPS_PLAN_OPTS);
                        used[ops_plan_offer(k)] = 1;
                        ops_decide(k);
                    } else ops_decide((u8)((seed >> 16) % n2[g_ops.next]));
                }
                ops_step();
            }
            if (g_ops.verdict == OPS_CONTINUE) cont++;
            for (i = 0; i < OPS_P_COUNT; i++) if (used[i]) {
                cnt[i]++; if (g_ops.verdict == OPS_CONTINUE) ccont[i]++;
                prof[i] += (double)g_ops.profit_judge; dau[i] += g_ops.dau_judge;
            }
        }
        printf("runs %lu  continue %.1f%%\n", runs, 100.0 * cont / runs);
        for (i = 0; i < OPS_P_COUNT; i++) if (cnt[i])
            printf("p%-2d used %6lu  continue %5.1f%%  profit %7.0f man  dau %5.0f\n", i, cnt[i], 100.0 * ccont[i] / cnt[i], prof[i] / cnt[i] / 10000, dau[i] / cnt[i]);
        return 0;
    }
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
