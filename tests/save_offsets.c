/* save_offsets.c — SaveData の各欄の位置を Tcl の配列で出す（自動試験が MSX のメモリを読むため）。
 * SDCC は構造体を詰めて並べるので、ここでも 1 バイト詰めで数える。 */
#include <stddef.h>
#include <stdio.h>
#pragma pack(push, 1)
#include "core/save.h"
#pragma pack(pop)

#define F(name, field) printf(" %s %u", name, (unsigned)offsetof(SaveData, field))

int main(void)
{
    printf("array set ::O {");
    F("gtime", g_time); F("level", level); F("gold", gold); F("gems", gems); F("ap", ap);
    F("potions", potions); F("stage", stage_cleared); F("cards_n", card_count); F("cards", cards);
    F("deck", deck); F("event_id", event_id); F("event_pt", event_pt); F("savecnt", save_count);
    F("funds", funds); F("spent", spent); F("last_event", last_event); F("last_rank", last_rank);
    F("guild", guild); F("gvg_last_war", gvg_last_war); F("gvg_last_rank", gvg_last_rank); F("raid_lv", raid_lv);
    printf(" card_size %u size %u}\n", (unsigned)sizeof(OwnedCard), (unsigned)sizeof(SaveData));
    return 0;
}
