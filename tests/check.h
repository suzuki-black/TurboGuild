/* check.h — 最小のテスト用マクロ。失敗したら行番号を出して終了コード 1 */
#ifndef CHECK_H
#define CHECK_H
#include <stdio.h>
#include <stdlib.h>
static int check_count;
#define CHECK(cond) do { check_count++; if (!(cond)) { \
    fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); exit(1); } } while (0)
#define CHECK_DONE(name) printf("%-12s %d checks ok\n", name, check_count)
#endif
