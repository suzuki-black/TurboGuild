/* tables.c — データ窓 8000h に表のページを出し、1 レコードを RAM へ写して返す。
 * 表は「u16 件数 + 固定長レコード」の形で、ページ見出しの直後（+10h）から置かれている。 */
#include "../core/tables.h"
#include "bank.h"
#include "../config.h"
#include "../data/rompages.h"

static const u8 *rec(u8 page, u16 index, u8 size)
{
    bank_dataA(page);
    return (const u8 *)(WIN_DATAA + PAGE_HDR_SIZE + 2 + index * size);
}

static u16 count(u8 page)
{
    bank_dataA(page);
    return *(const u16 *)(WIN_DATAA + PAGE_HDR_SIZE);
}

static void copy(void *dst, const u8 *src, u8 n)
{
    u8 *d = (u8 *)dst;
    while (n--) *d++ = *src++;
}

u16 tbl_card_count(void) { return count(PAGE_CARDS); }

u8 tbl_card(u16 id, CardRec *out)
{
    if (id == 0 || id > count(PAGE_CARDS)) return 0;
    copy(out, rec(PAGE_CARDS, id - 1, sizeof(CardRec)), sizeof(CardRec));
    return 1;
}

void tbl_area(u8 area, AreaRec *out)   { copy(out, rec(PAGE_AREAS, area - 1, sizeof(AreaRec)), sizeof(AreaRec)); }
void tbl_stage(u16 no, StageRec *out)  { copy(out, rec(PAGE_STAGES, no - 1, sizeof(StageRec)), sizeof(StageRec)); }
void tbl_gacha(u8 id, GachaRec *out)   { copy(out, rec(PAGE_GACHA, id - 1, sizeof(GachaRec)), sizeof(GachaRec)); }
void tbl_login(u8 day, LoginRec *out)  { copy(out, rec(PAGE_LOGIN, day - 1, sizeof(LoginRec)), sizeof(LoginRec)); }
void tbl_course(u8 id, CourseRec *out) { copy(out, rec(PAGE_COURSES, id - 1, sizeof(CourseRec)), sizeof(CourseRec)); }
void tbl_pack(u8 id, PackRec *out)     { copy(out, rec(PAGE_PACKS, id - 1, sizeof(PackRec)), sizeof(PackRec)); }
