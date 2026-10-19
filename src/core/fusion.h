/* fusion.h — 合成（強化）と進化（仕様 §7）。素材は所持カードの添字のビット集合で選ぶ */
#ifndef CORE_FUSION_H
#define CORE_FUSION_H
#include "../types.h"
#include "../config.h"

#define SEL_BYTES ((CARD_MAX + 7) / 8)

#define F_OK         0
#define F_NO_GOLD    1
#define F_NONE       2     /* 素材が選ばれていない */
#define F_BAD        3     /* ベースを素材にした・デッキのカードを素材にした */
#define F_MAX_LV     4     /* もう最大 Lv */
#define F_NOT_EVO    5     /* 進化できない組み合わせ */

typedef struct {
    u16 base;              /* 入力: ベースの添字 */
    u8  sel[SEL_BYTES];    /* 入力: 素材の添字のビット集合 */
    u8  count;             /* 以下は fusion_preview が埋める */
    u32 gain;              /* 得る経験値 */
    u16 gold;              /* 払うゴールド */
    u8  lv_before, lv_after;
    u8  status;
} FuseReq;

#define SEL_GET(s, i) ((s)[(i) >> 3] & (1 << ((i) & 7)))
#define SEL_SET(s, i) ((s)[(i) >> 3] |= (u8)(1 << ((i) & 7)))
#define SEL_CLR(s, i) ((s)[(i) >> 3] &= (u8)~(1 << ((i) & 7)))

void fusion_preview(FuseReq *f);    /* 結果の見込み（何も変えない） */
void fusion_apply(FuseReq *f);      /* 支払い → 素材を消す → ベースに経験値。f->base は詰めた後の添字に直す */
void fusion_select_all_n(FuseReq *f);   /* 「N カードを全部」（デッキとベースは除く） */
u8   evolve_partner_ok(u16 base, u16 partner);
u8   evolve(u16 base, u16 partner); /* F_OK なら base が進化先に変わる（Lv1）。partner は消える */
u16  fusion_need(u16 index);        /* そのカードの次の Lv までに要る経験値 */
#endif
