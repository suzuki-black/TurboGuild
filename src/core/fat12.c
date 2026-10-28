/* SPDX-License-Identifier: MIT */
/* fat12.c — セーブディスクの FAT12（fat12.h）。バッファは g_sector 1 枚だけで、FAT はセクタ単位で読み書きする。
 * 対応するのは BPB が 512 バイト/セクタ・クラスタ数 4085 未満（FAT12）のディスク。2DD の標準なら
 *   ブート 0 / FAT 1〜3・4〜6 / ルート 7〜13 / データ 14〜（1 クラスタ 2 セクタ）。
 * ファイルが無いときは、以前の版がセクタ 16〜24 に直接書いた形かを見る。作るときはその場所が空いていれば
 * そこをファイルとして FAT に登録するので、中身を移さずにそのまま続けられる。 */
#include "fat12.h"
#include "../far.h"
#include "../sys/disk.h"

#define NO_SEC   0xFFFFu
#define FAT_EOC  0xFFFu
#define ENT_SIZE 32

static const u8 file_name[11] = { 'T', 'G', 'S', 'A', 'V', 'E', ' ', ' ', 'D', 'A', 'T' };
static const u8 legacy_id[8] = { 'T', 'G', 'S', 'A', 'V', 'E', '0', '1' };

static u16 rd16(const u8 *p) { return (u16)(p[0] | (p[1] << 8)); }
static void wr16(u8 *p, u16 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); }

static u8 flush(void)
{
    if (!g_fat.dirty) return DISK_OK;
    g_fat.dirty = 0;
    return disk_write(g_fat.cache, 1, g_sector);
}

static u8 load(u16 s)
{
    u8 e;
    if (s == g_fat.cache) return DISK_OK;
    e = flush();
    if (e != DISK_OK) return e;
    g_fat.cache = NO_SEC;
    e = disk_read(s, 1, g_sector);
    if (e == DISK_OK) g_fat.cache = s;
    return e;
}

/* FAT12 の 1 項目は 12 ビット。クラスタ c は c × 1.5 バイト目から（セクタの境目をまたぐことがある） */
static u8 fat_get(u16 c, u16 *v)
{
    u16 off = (u16)(c + (c >> 1));
    u16 s = (u16)(g_fat.fat_start + (off >> 9));
    u16 i = off & 511;
    u8 lo, hi, e;
    if ((e = load(s)) != DISK_OK) return e;
    lo = g_sector[i];
    if (i == 511) {
        if ((e = load((u16)(s + 1))) != DISK_OK) return e;
        hi = g_sector[0];
    } else {
        hi = g_sector[i + 1];
    }
    *v = (u16)(lo | (hi << 8));
    *v = (c & 1) ? (u16)(*v >> 4) : (u16)(*v & 0x0FFF);
    return DISK_OK;
}

static u8 fat_put(u8 copy, u16 c, u16 v)
{
    u16 off = (u16)(c + (c >> 1));
    u16 s = (u16)(g_fat.fat_start + copy * g_fat.spf + (off >> 9));
    u16 i = off & 511;
    u8 e;
    if ((e = load(s)) != DISK_OK) return e;
    g_sector[i] = (c & 1) ? (u8)((g_sector[i] & 0x0F) | (v << 4)) : (u8)v;
    g_fat.dirty = 1;
    if (i == 511) {
        if ((e = load((u16)(s + 1))) != DISK_OK) return e;
        i = 0;
    } else {
        i++;
    }
    g_sector[i] = (c & 1) ? (u8)(v >> 4) : (u8)((g_sector[i] & 0xF0) | ((v >> 8) & 0x0F));
    g_fat.dirty = 1;
    return DISK_OK;
}

static u8 same(const u8 *a, const u8 *b, u8 n)
{
    while (n--) if (*a++ != *b++) return 0;
    return 1;
}

static u16 first_sec(u16 c) { return (u16)(g_fat.data_start + (c - 2) * g_fat.spc); }

/* ブートセクタの BPB から配置を求める。FAT12 でなければ 0 */
static u8 parse_bpb(void)
{
    const u8 *b = g_sector;
    u16 res = rd16(b + 14), ents = rd16(b + 17), total = rd16(b + 19);
    u8 spc = b[13];
    if (rd16(b + 11) != 512 || !spc || (spc & (spc - 1)) || !b[16] || b[16] > 2 || !ents || !total) return 0;
    g_fat.spc = spc;
    g_fat.nfat = b[16];
    g_fat.spf = rd16(b + 22);
    if (!g_fat.spf) return 0;
    g_fat.fat_start = res;
    g_fat.root_start = (u16)(res + g_fat.nfat * g_fat.spf);
    g_fat.root_secs = (u16)((ents + 15) >> 4);
    g_fat.data_start = (u16)(g_fat.root_start + g_fat.root_secs);
    if (total <= g_fat.data_start) return 0;
    g_fat.nclus = (u16)((total - g_fat.data_start) / spc);
    if (g_fat.nclus >= 4085) return 0;
    /* FAT 1 枚に全クラスタの項目が入ること（足りない BPB だと FAT の外＝ルートやデータを書き換えてしまう） */
    return (u8)((u32)(g_fat.nclus + 2) * 3 / 2 + 1 <= (u32)g_fat.spf * 512);
}

/* ルートで TGSAVE.DAT を探す（見つかれば先頭クラスタ、無ければ 0）。free には最初の空き（無ければ NO_SEC）を返す */
static u8 find_entry(u16 *clus, u16 *free_sec, u8 *free_k)
{
    u16 s;
    u8 k, e;
    *clus = 0;
    *free_sec = NO_SEC;
    for (s = 0; s < g_fat.root_secs; s++) {
        if ((e = load((u16)(g_fat.root_start + s))) != DISK_OK) return e;
        for (k = 0; k < 512 / ENT_SIZE; k++) {
            const u8 *p = g_sector + k * ENT_SIZE;
            if (p[0] == 0x00 || p[0] == 0xE5) {
                if (*free_sec == NO_SEC) { *free_sec = (u16)(g_fat.root_start + s); *free_k = k; }
                if (p[0] == 0x00) return DISK_OK;       /* ここから先は使われていない */
                continue;
            }
            if (p[11] & 0x18) continue;                 /* ボリューム名・ディレクトリ */
            if (same(p, file_name, 11)) { *clus = rd16(p + 26); return DISK_OK; }
        }
    }
    return DISK_OK;
}

FAR(u8, fat_open, (void))
{
    u16 c, fs;
    u8 n, k, fk, e;
    g_fat.state = FS_BAD;
    g_fat.cache = NO_SEC;
    g_fat.dirty = 0;
    if ((e = load(0)) != DISK_OK) return e;
    if (!parse_bpb()) return DISK_OK;
    if ((e = find_entry(&c, &fs, &fk)) != DISK_OK) return e;
    if (!c) {                                           /* ファイルが無い: 以前の版の形か */
        g_fat.state = FS_NONE;
        if (g_fat.data_start > FAT_LEGACY_SEC) return DISK_OK;
        if ((e = load(FAT_LEGACY_SEC)) != DISK_OK) return e;
        if (!same(g_sector, legacy_id, 8)) return DISK_OK;
        for (n = 0; n < FAT_FILE_SECS; n++) g_fat.sec[n] = (u16)(FAT_LEGACY_SEC + n);
        g_fat.state = FS_LEGACY;
        return DISK_OK;
    }
    for (n = 0; n < FAT_FILE_SECS; ) {                  /* クラスタの鎖をたどってセクタを並べる */
        if (c < 2 || c >= g_fat.nclus + 2) return DISK_OK;   /* 短すぎる・壊れている */
        for (k = 0; k < g_fat.spc && n < FAT_FILE_SECS; k++) g_fat.sec[n++] = (u16)(first_sec(c) + k);
        if (n < FAT_FILE_SECS && (e = fat_get(c, &c)) != DISK_OK) return e;
    }
    g_fat.state = FS_FILE;
    return DISK_OK;
}

/* 以前の版の場所（セクタ 16〜）のクラスタがすべて空いていれば、その先頭クラスタ。無理なら 0 */
static u16 legacy_clusters(u8 need)
{
    u16 c0, v;
    u8 j;
    if ((FAT_LEGACY_SEC - g_fat.data_start) % g_fat.spc) return 0;
    c0 = (u16)((FAT_LEGACY_SEC - g_fat.data_start) / g_fat.spc + 2);
    if (c0 + need > g_fat.nclus + 2) return 0;
    for (j = 0; j < need; j++) {
        if (fat_get((u16)(c0 + j), &v) != DISK_OK || v) return 0;
    }
    return c0;
}

FAR(u8, fat_create, (const DateTime *dt))
{
    u16 clus[FAT_FILE_SECS], ds, c, v;
    u8 need, n, j, k, dk, e;
    u8 *p;
    if (g_fat.state == FS_FILE) return DISK_OK;
    if (g_fat.state == FS_BAD) return DISK_ERROR;
    need = (u8)((FAT_FILE_SECS + g_fat.spc - 1) / g_fat.spc);
    if ((e = find_entry(&c, &ds, &dk)) != DISK_OK) return e;
    if (ds == NO_SEC) return DISK_ERROR;                /* ルートが満杯 */
    /* クラスタを選ぶ: 以前の版の場所が空いていればそこ（中身をそのまま引き継ぐ）、無ければ前から空きを */
    c = g_fat.state == FS_LEGACY ? legacy_clusters(need) : 0;
    if (c) {
        for (j = 0; j < need; j++) clus[j] = (u16)(c + j);
    } else {
        n = 0;
        for (c = 2; c < g_fat.nclus + 2 && n < need; c++) {
            if ((e = fat_get(c, &v)) != DISK_OK) return e;
            if (!v) clus[n++] = c;
        }
        if (n < need) return DISK_ERROR;                /* 空きが足りない */
    }
    for (k = 0; k < g_fat.nfat; k++) {                  /* FAT は全部の写しに同じ鎖を書く */
        for (j = 0; j < need; j++) {
            if ((e = fat_put(k, clus[j], j + 1 < need ? clus[j + 1] : FAT_EOC)) != DISK_OK) return e;
        }
    }
    if ((e = load(ds)) != DISK_OK) return e;
    p = g_sector + dk * ENT_SIZE;
    for (j = 0; j < ENT_SIZE; j++) p[j] = 0;
    for (j = 0; j < 11; j++) p[j] = file_name[j];
    p[11] = 0x20;                                       /* アーカイブ */
    wr16(p + 22, (u16)((dt->hour << 11) | (dt->min << 5) | (dt->sec >> 1)));
    wr16(p + 24, (u16)(((dt->year - 1980) << 9) | (dt->mon << 5) | dt->day));
    wr16(p + 26, clus[0]);
    wr16(p + 28, FAT_FILE_SECS * 512);
    g_fat.dirty = 1;
    if ((e = flush()) != DISK_OK) return e;
    for (n = 0, j = 0; j < need; j++) {
        for (k = 0; k < g_fat.spc && n < FAT_FILE_SECS; k++) g_fat.sec[n++] = (u16)(first_sec(clus[j]) + k);
    }
    g_fat.state = FS_FILE;
    return DISK_OK;
}
