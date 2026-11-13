/* SPDX-License-Identifier: MIT */
/* gen_border.c — 攻略スレの「正解」の表を作る（DECISIONS #104）。イベントごとに、終了の日の 10 位・100 位・500 位の点
 * （NPC だけで数えて、その点を取れば自分がその順位に入る最小の点）を、ROM と同じ式（core/ranking.c）で探して
 * C のヘッダで出す。src/data/border.h はこれの出力（Makefile が作り、テストが式と比べる）。 */
#include <stdio.h>
#include "core/ranking.h"
#include "core/save.h"
#include "core/tables.h"
#include "config.h"

static const u16 tiers[3] = { 10, 100, 500 };

/* 自分が rank 位以内に入る最小の点 */
static u32 border(u16 rank)
{
    u32 lo = 0, hi = 0x01000000UL;
    while (lo < hi) {
        u32 mid = lo + (hi - lo) / 2;
        g_save.event_pt = mid;
        if (ranking_final_rank() <= rank) hi = mid; else lo = mid + 1;
    }
    return lo;
}

int main(void)
{
    u8 ev, t;
    EventRec e;
    printf("/* SPDX-License-Identifier: MIT */\n/* border.h — 生成物: build/gen_border（tests/gen_border.c）。イベントごとの、終了の日の 10 位・100 位・500 位の\n"
           " * ボーダーの点。攻略スレの「正解」（DECISIONS #104）。式を変えたら作り直す（テストが比べる） */\n");
    printf("#ifndef DATA_BORDER_H\n#define DATA_BORDER_H\nstatic const u32 th_border[%d][3] = {\n", EVENT_COUNT);
    for (ev = 1; ev <= EVENT_COUNT; ev++) {
        tbl_event(ev, &e);
        g_save.event_id = ev;
        g_save.g_time = ((u32)e.start_day + e.duration) * GSEC_PER_DAY - 1;
        printf("    {");
        for (t = 0; t < 3; t++) printf("%s%luUL", t ? ", " : " ", (unsigned long)border(tiers[t]));
        printf(" },\n");
    }
    printf("};\n#endif\n");
    return 0;
}
