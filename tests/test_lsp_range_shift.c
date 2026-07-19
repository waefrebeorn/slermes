/*
 * test_lsp_range_shift.c — unit tests for the pure agent/lsp/range_shift.py
 * helpers. Invariants derived from a Python oracle.
 */

#include "lsp_range_shift.h"
#include "libjson/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;

#define CHK(cond, lbl) do { \
    if (!(cond)) { printf("FAIL: %s\n", lbl); g_fail++; } \
    else printf("ok: %s\n", lbl); \
} while (0)

static void expect_shift(const char *lbl, const char *pre, const char *post,
                         int n, const int *want)
{
    lsp_range_shift_t *sh = lsp_build_line_shift(pre, post);
    int ok = 1;
    for (int i = 0; i < n; i++) {
        int got = lsp_line_shift(sh, i);
        if (got != want[i]) {
            printf("FAIL: %s line %d got %d want %d\n", lbl, i, got, want[i]);
            g_fail++; ok = 0;
        }
    }
    if (ok) printf("ok: %s\n", lbl);
    lsp_free_line_shift(sh);
}

int main(void)
{
    /* insert X at middle: 2 -> 3 */
    int w1[] = {0,1,3,4,5};
    expect_shift("insert-mid", "a\nb\nc\nd\ne", "a\nb\nX\nc\nd\ne", 5, w1);

    /* delete c: line 2 removed */
    int w2[] = {0,1,-1,2,3};
    expect_shift("delete-mid", "a\nb\nc\nd\ne", "a\nb\nd\ne", 5, w2);

    /* replace b->Z: lines 1,2 gone, 3->2 */
    int w3[] = {0,-1,-1,2};
    expect_shift("replace", "a\nb\nc\nd", "a\nZ\nd", 4, w3);

    /* identical -> identity */
    int w4[] = {0,1,2};
    expect_shift("identical", "x\ny\nz", "x\ny\nz", 3, w4);

    /* shift_diagnostic_range: pre line2 -> post line3 */
    lsp_range_shift_t *sh1 = lsp_build_line_shift("a\nb\nc\nd\ne", "a\nb\nX\nc\nd\ne");
    char *r1 = lsp_shift_diagnostic_range(
        "{\"range\":{\"start\":{\"line\":2,\"character\":1},\"end\":{\"line\":2,\"character\":5}},\"severity\":1,\"message\":\"oops\"}", sh1);
    CHK(r1 && strstr(r1, "\"line\":3") && strstr(r1, "\"character\":1") &&
         strstr(r1, "oops") && strstr(r1, "severity"), "shift_diagnostic_range R1");
    free(r1);
    lsp_free_line_shift(sh1);

    /* shift_diagnostic_range: deleted line -> NULL */
    lsp_range_shift_t *sh2 = lsp_build_line_shift("a\nb\nc\nd\ne", "a\nb\nd\ne");
    char *r2 = lsp_shift_diagnostic_range(
        "{\"range\":{\"start\":{\"line\":2,\"character\":0},\"end\":{\"line\":2,\"character\":3}}}", sh2);
    CHK(r2 == NULL, "shift_diagnostic_range R2 (deleted->NULL)");
    lsp_free_line_shift(sh2);

    /* shift_baseline: line0->0, line2 dropped */
    lsp_range_shift_t *sh3 = lsp_build_line_shift("a\nb\nc\nd\ne", "a\nb\nd\ne");
    char *b1 = lsp_shift_baseline_json(
        "[{\"range\":{\"start\":{\"line\":0,\"character\":0},\"end\":{\"line\":0,\"character\":1}},"
        "{\"range\":{\"start\":{\"line\":2,\"character\":0},\"end\":{\"line\":2,\"character\":1}}}]", sh3);
    CHK(b1 && strstr(b1, "\"line\":0") && !strstr(b1, "\"line\":2"), "shift_baseline B1");
    free(b1);
    lsp_free_line_shift(sh3);

    if (g_fail) { printf("\n%d FAIL\n", g_fail); return 1; }
    printf("\nALL PASSED\n");
    return 0;
}
