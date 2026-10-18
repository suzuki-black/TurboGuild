/* battle.c — damage = max(1, 攻撃 - 敵防御), turns = ceil(敵HP / damage),
 * 被ダメージ = max(0, 敵攻撃 - 防御) × turns。被ダメージ < 自分の HP なら勝ち。 */
#include "battle.h"
#include "../far.h"

FAR(void, battle_run, (const BattleIn *in, BattleOut *out))
{
    u32 dmg = in->deck_atk > in->enemy_def ? in->deck_atk - in->enemy_def : 1;
    u32 turns = (in->enemy_hp + dmg - 1) / dmg;
    u32 taken = in->enemy_atk > in->deck_def ? in->enemy_atk - in->deck_def : 0;
    if (turns > 0xFFFF) turns = 0xFFFF;
    out->dmg_per_turn = dmg;
    out->turns = (u16)turns;
    out->taken_per_turn = taken;
    out->taken_total = taken * turns;
    out->win = (u8)(out->taken_total < in->player_hp);
}
