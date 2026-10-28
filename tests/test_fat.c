/* SPDX-License-Identifier: MIT */
/* test_fat.c — セーブディスクの FAT12（core/fat12.c）。2DD のイメージを作り、FAT は別に書いた読み手で確かめる */
#include <stdio.h>
#include <string.h>
#include "core/fat12.h"
#include "sys/disk.h"

extern u8 *g_pc_disk;
extern u16 g_pc_disk_secs;

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("NG %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

#define SECS 1440
static u8 img[SECS * 512];
static const DateTime dt = { 2026, 10, 28, 12, 34, 56 };

/* tools/mkblank_dsk.py と同じ 2DD: ブート 0 / FAT 1〜3・4〜6 / ルート 7〜13（112 件）/ データ 14〜 */
static void blank(void)
{
    u8 *b = img;
    memset(img, 0, sizeof img);
    b[0] = 0xEB; b[1] = 0xFE; b[2] = 0x90;
    b[11] = 0x00; b[12] = 0x02; b[13] = 2; b[14] = 1; b[15] = 0; b[16] = 2;
    b[17] = 112; b[18] = 0; b[19] = SECS & 255; b[20] = SECS >> 8; b[21] = 0xF9; b[22] = 3; b[23] = 0;
    img[512] = img[4 * 512] = 0xF9; img[513] = img[514] = img[4 * 512 + 1] = img[4 * 512 + 2] = 0xFF;
    g_pc_disk = img;
    g_pc_disk_secs = SECS;
}

/* 別に書いた FAT12 の読み書き（テストの側の正解） */
static unsigned get(int copy, unsigned c)
{
    const u8 *f = img + (1 + copy * 3) * 512;
    unsigned v = f[c * 3 / 2] | f[c * 3 / 2 + 1] << 8;
    return c & 1 ? v >> 4 : v & 0xFFF;
}
static void put(int copy, unsigned c, unsigned v)
{
    u8 *f = img + (1 + copy * 3) * 512;
    unsigned o = c * 3 / 2;
    if (c & 1) { f[o] = (u8)((f[o] & 0x0F) | (v << 4)); f[o + 1] = (u8)(v >> 4); }
    else { f[o] = (u8)v; f[o + 1] = (u8)((f[o + 1] & 0xF0) | (v >> 8)); }
}
static void put2(unsigned c, unsigned v) { put(0, c, v); put(1, c, v); }

static u8 *entry(int k) { return img + 7 * 512 + k * 32; }
static void add_file(int k, const char *name11, unsigned clus)
{
    memcpy(entry(k), name11, 11);
    entry(k)[11] = 0x20;
    entry(k)[26] = (u8)clus; entry(k)[27] = (u8)(clus >> 8);
}
static int find(const char *name11)
{
    int k;
    for (k = 0; k < 112; k++) if (!memcmp(entry(k), name11, 11)) return k;
    return -1;
}
static unsigned sec_of(unsigned c) { return 14 + (c - 2) * 2; }

/* 鎖をたどって 9 セクタ分の並びが g_fat.sec と一致し、FAT の写しが同じか */
static int chain_ok(unsigned c)
{
    int n = 0, k;
    for (;;) {
        for (k = 0; k < 2 && n < FAT_FILE_SECS; k++, n++) if (g_fat.sec[n] != sec_of(c) + k) return 0;
        if (get(0, c) != get(1, c)) return 0;
        if (n == FAT_FILE_SECS) return get(0, c) >= 0xFF8;
        c = get(0, c);
        if (c < 2 || c >= 0xFF0) return 0;
    }
}

static void test_blank(void)
{
    int k;
    blank();
    CHECK(fat_open() == DISK_OK && g_fat.state == FS_NONE);
    CHECK(g_fat.spc == 2 && g_fat.root_start == 7 && g_fat.root_secs == 7 && g_fat.data_start == 14 && g_fat.nclus == 713);
    CHECK(fat_create(&dt) == DISK_OK && g_fat.state == FS_FILE);
    k = find("TGSAVE  DAT");
    CHECK(k == 0);
    CHECK(entry(k)[11] == 0x20 && (entry(k)[26] | entry(k)[27] << 8) == 2);
    CHECK((entry(k)[28] | entry(k)[29] << 8) == 4608 && entry(k)[30] == 0 && entry(k)[31] == 0);
    CHECK((entry(k)[24] | entry(k)[25] << 8) == (((2026 - 1980) << 9) | (10 << 5) | 28));
    CHECK((entry(k)[22] | entry(k)[23] << 8) == ((12 << 11) | (34 << 5) | 28));
    CHECK(chain_ok(2) && g_fat.sec[0] == 14 && g_fat.sec[8] == 22);
    CHECK(get(0, 0) == 0xFF9 && get(0, 1) == 0xFFF && get(0, 7) == 0);   /* 前後を壊していない */
    /* 開き直すと同じ場所 */
    memset(&g_fat, 0, sizeof g_fat);
    CHECK(fat_open() == DISK_OK && g_fat.state == FS_FILE && chain_ok(2));
    /* もう一度作ろうとしても何もしない */
    CHECK(fat_create(&dt) == DISK_OK && find("TGSAVE  DAT") == 0 && entry(1)[0] == 0);
}

static void test_other_files(void)
{
    blank();
    add_file(0, "AUTOEXECBAS", 2); put2(2, 3); put2(3, 0xFFF);   /* クラスタ 2〜3 は使用中 */
    add_file(1, "OLD     TXT", 4); entry(1)[0] = 0xE5; put2(4, 0xFFF); /* 消したファイル（FAT は残っている） */
    put2(6, 0xFF7);                                                /* 不良クラスタ */
    CHECK(fat_open() == DISK_OK && g_fat.state == FS_NONE);
    CHECK(fat_create(&dt) == DISK_OK);
    CHECK(find("TGSAVE  DAT") == 1);                               /* 消した項目の跡を使う */
    CHECK(get(0, 5) == 7 && get(0, 7) == 8 && get(0, 8) == 9 && get(0, 9) == 10 && get(0, 10) >= 0xFF8);
    CHECK(chain_ok(5) && g_fat.sec[0] == sec_of(5) && g_fat.sec[2] == sec_of(7));
    CHECK(get(0, 2) == 3 && get(0, 6) == 0xFF7 && find("AUTOEXECBAS") == 0);
}

static void test_fragmented(void)
{
    blank();
    entry(0)[0] = entry(1)[0] = 0xE5;                              /* 消したファイルの後ろ（00h は「ここで終わり」） */
    add_file(2, "TGSAVE  DAT", 40);                                /* DOS が飛び飛びに置いたコピー */
    put2(40, 12); put2(12, 100); put2(100, 101); put2(101, 30); put2(30, 0xFFF);
    CHECK(fat_open() == DISK_OK && g_fat.state == FS_FILE);
    CHECK(g_fat.sec[0] == sec_of(40) && g_fat.sec[1] == sec_of(40) + 1 && g_fat.sec[2] == sec_of(12));
    CHECK(g_fat.sec[4] == sec_of(100) && g_fat.sec[6] == sec_of(101) && g_fat.sec[8] == sec_of(30));
    put2(101, 0xFFF);                                              /* 短すぎるファイルは壊れている扱い */
    CHECK(fat_open() == DISK_OK && g_fat.state == FS_BAD);
    CHECK(fat_create(&dt) == DISK_ERROR);
}

/* 以前の版: セクタ 16 に識別、17〜24 にスロット。FAT には何も無い */
static void legacy(void)
{
    int s;
    blank();
    memcpy(img + 16 * 512, "TGSAVE01 TurboGuild", 19);
    for (s = 17; s <= 24; s++) img[s * 512] = (u8)s;
}

static void test_legacy(void)
{
    int s;
    legacy();
    CHECK(fat_open() == DISK_OK && g_fat.state == FS_LEGACY && g_fat.sec[0] == 16 && g_fat.sec[8] == 24);
    CHECK(fat_create(&dt) == DISK_OK && g_fat.state == FS_FILE);
    CHECK((entry(0)[26] | entry(0)[27] << 8) == 3 && chain_ok(3));   /* その場所（クラスタ 3〜7）をそのまま登録 */
    CHECK(g_fat.sec[0] == 16 && g_fat.sec[8] == 24 && !memcmp(img + 16 * 512, "TGSAVE01", 8));
    for (s = 17; s <= 24; s++) CHECK(img[s * 512] == (u8)s);       /* 中身はそのまま */
    CHECK(get(0, 2) == 0);
    /* その場所が使われていたら、空いている所に作る（中身は移さない。呼び元が識別と今のセーブを書く） */
    legacy();
    add_file(0, "GAME    BIN", 4); put2(4, 0xFFF);
    CHECK(fat_open() == DISK_OK && g_fat.state == FS_LEGACY);
    CHECK(fat_create(&dt) == DISK_OK && find("TGSAVE  DAT") == 1);
    CHECK(chain_ok(2) && g_fat.sec[0] == 14 && g_fat.sec[2] == sec_of(3) && g_fat.sec[4] == sec_of(5));
    CHECK(get(0, 4) == 0xFFF);
}

static void test_boundary(void)
{
    unsigned c;
    blank();
    for (c = 2; c < 340; c++) put2(c, 0xFFF);                      /* クラスタ 341 の項目はセクタの境目をまたぐ */
    CHECK(fat_open() == DISK_OK && fat_create(&dt) == DISK_OK);
    CHECK(get(0, 340) == 341 && get(0, 341) == 342 && get(1, 341) == 342 && get(0, 344) >= 0xFF8);
    CHECK(chain_ok(340) && get(0, 339) == 0xFFF && get(0, 345) == 0);
    memset(&g_fat, 0, sizeof g_fat);
    CHECK(fat_open() == DISK_OK && g_fat.state == FS_FILE && chain_ok(340));
}

static void test_errors(void)
{
    int k;
    unsigned c;
    blank();
    for (k = 0; k < 112; k++) add_file(k, "FILLER  TXT", 0);       /* ルートが満杯 */
    CHECK(fat_open() == DISK_OK && g_fat.state == FS_NONE && fat_create(&dt) == DISK_ERROR);
    CHECK(get(0, 2) == 0);                                         /* 失敗しても FAT は汚さない */
    blank();
    for (c = 2; c < 713 + 2 - 3; c++) put2(c, 0xFFF);              /* 空きが 3 クラスタしか無い */
    CHECK(fat_open() == DISK_OK && fat_create(&dt) == DISK_ERROR && find("TGSAVE  DAT") < 0);
    blank();
    memset(img, 0, 512);                                           /* FAT でないディスク */
    CHECK(fat_open() == DISK_OK && g_fat.state == FS_BAD && fat_create(&dt) == DISK_ERROR);
    blank();
    img[22] = 1;                                                   /* FAT が全クラスタ分の大きさに足りない BPB */
    CHECK(fat_open() == DISK_OK && g_fat.state == FS_BAD && fat_create(&dt) == DISK_ERROR);
    g_pc_disk = 0;                                                 /* ディスク無し */
    CHECK(fat_open() == DISK_NODISK);
}

int main(void)
{
    test_blank();
    test_other_files();
    test_fragmented();
    test_legacy();
    test_boundary();
    test_errors();
    printf("fat          %d checks %s\n", checks, fails ? "NG" : "ok");
    return fails ? 1 : 0;
}
