/* tables_pc.c — core/tables.h の PC 版。build/ の .bin（ROM に入るのと同じもの）を読む */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/tables.h"
#include "core/save.h"

SaveData g_save;

static unsigned char *load(const char *name)
{
    static unsigned char *cache[8];
    static const char *names[8];
    char path[256];
    FILE *f;
    long n;
    int i;
    for (i = 0; i < 8 && names[i]; i++) if (!strcmp(names[i], name)) return cache[i];
    snprintf(path, sizeof path, "build/%s", name);
    f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "tables_pc: %s が無い（make を先に）\n", path); exit(2); }
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    cache[i] = malloc((size_t)n);
    if (fread(cache[i], 1, (size_t)n, f) != (size_t)n) exit(2);
    fclose(f);
    names[i] = name;
    return cache[i];
}

static u16 cnt(const char *name) { unsigned char *b = load(name); return (u16)(b[0] | b[1] << 8); }
static void rec(const char *name, u16 index, void *out, size_t size)
{
    memcpy(out, load(name) + 2 + index * size, size);
}

u16 tbl_card_count(void) { return cnt("cards.bin"); }
u8 tbl_card(u16 id, CardRec *out)
{
    if (id == 0 || id > cnt("cards.bin")) return 0;
    rec("cards.bin", id - 1, out, sizeof *out);
    return 1;
}
void tbl_area(u8 a, AreaRec *out)    { rec("areas.bin", a - 1, out, sizeof *out); }
void tbl_stage(u16 n, StageRec *out) { rec("stages.bin", n - 1, out, sizeof *out); }
void tbl_gacha(u8 id, GachaRec *out) { rec("gacha.bin", id - 1, out, sizeof *out); }
void tbl_login(u8 d, LoginRec *out)  { rec("login.bin", d - 1, out, sizeof *out); }
