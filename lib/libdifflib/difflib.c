/*
 * difflib.c — C diff library (J15: Python difflib port).
 *
 * Implements unified diff generation and similarity ratio via LCS.
 */

#include "difflib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ================================================================
 *  Line splitting
 * ================================================================ */

#define MAX_LINES 2048

/* Split text into lines. Returns count. Lines point into malloc'd copy. */
static int split_lines(const char *text, char **lines, int max) {
    if (!text || !text[0]) return 0;
    char *copy = strdup(text);
    if (!copy) return 0;

    int count = 0;
    char *save = NULL;
    char *line = strtok_r(copy, "\n", &save);
    while (line && count < max) {
        lines[count] = strdup(line);
        count++;
        line = strtok_r(NULL, "\n", &save);
    }
    free(copy);
    return count;
}

static void free_lines(char **lines, int count) {
    for (int i = 0; i < count; i++) free(lines[i]);
}

/* ================================================================
 *  Longest Common Subsequence (for ratio and diff)
 * ================================================================ */

/* LCS table size */
#define LCS_MAX 256

/* Compute LCS table for two line arrays */
static int lcs_length(char **a, int na, char **b, int nb) {
    /* Use DP with O(min(n,m)) space */
    int m = na, n = nb;
    if (m > LCS_MAX) m = LCS_MAX;
    if (n > LCS_MAX) n = LCS_MAX;

    /* Simple 2-row DP */
    int *prev = (int *)calloc((size_t)(n + 1), sizeof(int));
    int *curr = (int *)calloc((size_t)(n + 1), sizeof(int));
    if (!prev || !curr) { free(prev); free(curr); return 0; }

    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            if (strcmp(a[i], b[j]) == 0)
                curr[j + 1] = prev[j] + 1;
            else
                curr[j + 1] = (prev[j + 1] > curr[j]) ? prev[j + 1] : curr[j];
        }
        int *tmp = prev; prev = curr; curr = tmp;
    }

    int result = prev[n];
    free(prev);
    free(curr);
    return result;
}

/* ================================================================
 *  Public API
 * ================================================================ */

double difflib_ratio(const char *a, const char *b) {
    if (!a && !b) return 1.0;
    if (!a || !b) return 0.0;
    if (strcmp(a, b) == 0) return 1.0;

    /* If single-line, compare character-by-character */
    if (!strchr(a, '\n') && !strchr(b, '\n')) {
        int na = (int)strlen(a);
        int nb = (int)strlen(b);
        if (na == 0 && nb == 0) return 1.0;
        if (na == 0 || nb == 0) return 0.0;

        /* Simple LCS for character sequences */
        int m = na > LCS_MAX ? LCS_MAX : na;
        int n = nb > LCS_MAX ? LCS_MAX : nb;
        int *prev = (int *)calloc((size_t)(n + 1), sizeof(int));
        int *curr = (int *)calloc((size_t)(n + 1), sizeof(int));
        if (!prev || !curr) { free(prev); free(curr); return 0.0; }

        for (int i = 0; i < m; i++) {
            for (int j = 0; j < n; j++) {
                if (a[i] == b[j])
                    curr[j + 1] = prev[j] + 1;
                else
                    curr[j + 1] = (prev[j + 1] > curr[j]) ? prev[j + 1] : curr[j];
            }
            int *tmp = prev; prev = curr; curr = tmp;
        }

        int lcs = prev[n];
        free(prev);
        free(curr);
        return (na + nb > 0) ? (2.0 * lcs) / (na + nb) : 1.0;
    }

    /* Multi-line: compare line-by-line */
    char *la[MAX_LINES], *lb[MAX_LINES];
    int na = split_lines(a, la, MAX_LINES);
    int nb = split_lines(b, lb, MAX_LINES);

    /* Handle empty cases */
    if (na == 0 && nb == 0) { free_lines(la, na); free_lines(lb, nb); return 1.0; }
    if (na == 0 || nb == 0) { free_lines(la, na); free_lines(lb, nb); return 0.0; }

    int lcs = lcs_length(la, na, lb, nb);
    double ratio = (na + nb > 0) ? (2.0 * lcs) / (na + nb) : 1.0;

    free_lines(la, na);
    free_lines(lb, nb);
    return ratio;
}

char *difflib_unified_diff(const char *a, const char *b, int context_lines) {
    if (context_lines < 0) context_lines = 3;

    char *la[MAX_LINES], *lb[MAX_LINES];
    int na = split_lines(a, la, MAX_LINES);
    int nb = split_lines(b, lb, MAX_LINES);

    /* Dynamic output buffer (grows; no fixed-size overflow). */
    size_t cap = 65536, len = 0;
    char *result = (char *)malloc(cap);
    if (!result) { free_lines(la, na); free_lines(lb, nb); return strdup(""); }
    result[0] = '\0';

#define UD_APPEND(...) do {                                                  \
        int _need = snprintf(NULL, 0, __VA_ARGS__);                          \
        if (_need > 0) {                                                     \
            if (len + (size_t)_need + 1 > cap) {                             \
                while (len + (size_t)_need + 1 > cap) cap *= 2;              \
                char *_nr = (char *)realloc(result, cap);                    \
                if (!_nr) goto ud_done;                                      \
                result = _nr;                                                \
            }                                                                \
            len += (size_t)snprintf(result + len, cap - len, __VA_ARGS__);   \
        }                                                                    \
    } while (0)

    /* Full LCS DP table for a faithful diff (uint16 lengths: lines are
     * capped at MAX_LINES=2048 so they always fit). Table is
     * (na+1) x (nb+1) — at most ~8.4 MB transiently. */
    {
        size_t W = (size_t)nb + 1;
        unsigned short *dp = (unsigned short *)calloc(((size_t)na + 1) * W,
                                                      sizeof(unsigned short));
        /* match[i] = matched line in b for a[i], or -1 */
        int *match = (int *)malloc(sizeof(int) * (size_t)(na > 0 ? na : 1));
        if (!dp || !match) {
            free(dp); free(match);
            goto ud_done;
        }
        for (int i = na - 1; i >= 0; i--) {
            for (int j = nb - 1; j >= 0; j--) {
                if (strcmp(la[i], lb[j]) == 0)
                    dp[(size_t)i * W + j] = (unsigned short)(dp[((size_t)i + 1) * W + j + 1] + 1);
                else {
                    unsigned short d = dp[((size_t)i + 1) * W + j];
                    unsigned short r = dp[(size_t)i * W + j + 1];
                    dp[(size_t)i * W + j] = d > r ? d : r;
                }
            }
        }
        for (int i = 0; i < na; i++) match[i] = -1;
        {
            int i = 0, j = 0;
            while (i < na && j < nb) {
                if (strcmp(la[i], lb[j]) == 0) { match[i] = j; i++; j++; }
                else if (dp[((size_t)i + 1) * W + j] >= dp[(size_t)i * W + j + 1]) i++;
                else j++;
            }
        }
        free(dp);

        /* Build opcodes (equal / replace-delete-insert runs), then group
         * into hunks with `context_lines` context, like Python
         * difflib.unified_diff(get_grouped_opcodes). */
        UD_APPEND("--- original\n+++ modified\n");

        int i = 0, j = 0;
        while (i < na || j < nb) {
            /* Skip an equal run */
            int eq_start_i = i, eq_start_j = j;
            while (i < na && match[i] == j) { i++; j++; }
            int had_equal = (i > eq_start_i);
            (void)eq_start_j;

            if (i >= na && j >= nb) break;

            /* Start of a changed region: back up for leading context */
            int hunk_ai = i - context_lines;
            if (hunk_ai < eq_start_i && had_equal) hunk_ai = (i - context_lines < eq_start_i) ? ((i - context_lines) > 0 ? (i - context_lines) : 0) : hunk_ai;
            if (hunk_ai < 0) hunk_ai = 0;
            if (!had_equal) hunk_ai = i;
            int hunk_bj = j - (i - hunk_ai);

            /* Consume changed + interleaved short-equal runs (equal runs
             * shorter than 2*context merge adjacent hunks, like Python). */
            int ci = i, cj = j;      /* cursors */
            int last_change_i = i, last_change_j = j;
            while (ci < na || cj < nb) {
                if (ci < na && match[ci] == cj) {
                    /* equal run — measure it */
                    int run_i = ci, run_j = cj;
                    while (run_i < na && match[run_i] == run_j) { run_i++; run_j++; }
                    int run_len = run_i - ci;
                    if (run_i >= na && run_j >= nb) {
                        /* trailing equal run ends the hunk */
                        break;
                    }
                    if (run_len > 2 * context_lines) break;
                    /* short equal run: absorb into hunk and continue */
                    ci = run_i; cj = run_j;
                } else if (ci < na && (match[ci] < 0 || match[ci] < cj)) {
                    ci++; last_change_i = ci; last_change_j = cj;
                } else if (cj < nb) {
                    cj++; last_change_i = ci; last_change_j = cj;
                } else {
                    ci++; last_change_i = ci; last_change_j = cj;
                }
            }

            /* Trailing context */
            int hunk_end_i = last_change_i + context_lines;
            if (hunk_end_i > na) hunk_end_i = na;
            /* clamp to the end of the trailing equal run */
            {
                int run_i = last_change_i, run_j = last_change_j;
                while (run_i < na && match[run_i] == run_j &&
                       run_i < last_change_i + context_lines) { run_i++; run_j++; }
                hunk_end_i = run_i;
            }
            int hunk_end_j = last_change_j + (hunk_end_i - last_change_i);

            int alen = hunk_end_i - hunk_ai;
            int blen = hunk_end_j - hunk_bj;
            int astart = alen > 0 ? hunk_ai + 1 : hunk_ai;
            int bstart = blen > 0 ? hunk_bj + 1 : hunk_bj;
            if (alen == 1) UD_APPEND("@@ -%d ", astart);
            else UD_APPEND("@@ -%d,%d ", astart, alen);
            if (blen == 1) UD_APPEND("+%d @@\n", bstart);
            else UD_APPEND("+%d,%d @@\n", bstart, blen);

            /* Emit hunk body */
            int ei = hunk_ai, ej = hunk_bj;
            while (ei < hunk_end_i || ej < hunk_end_j) {
                if (ei < na && ei < hunk_end_i && match[ei] == ej) {
                    UD_APPEND(" %s\n", la[ei]);
                    ei++; ej++;
                } else if (ei < hunk_end_i && ei < na &&
                           (match[ei] < 0 || match[ei] >= hunk_end_j || match[ei] > ej)) {
                    /* deletions first, like Python replace order (-then+) */
                    if (match[ei] < 0 || match[ei] >= hunk_end_j) {
                        UD_APPEND("-%s\n", la[ei]);
                        ei++;
                    } else {
                        /* a[ei] matches later in b: emit inserts until we catch up */
                        UD_APPEND("+%s\n", lb[ej]);
                        ej++;
                    }
                } else if (ej < hunk_end_j && ej < nb) {
                    UD_APPEND("+%s\n", lb[ej]);
                    ej++;
                } else if (ei < hunk_end_i && ei < na) {
                    UD_APPEND("-%s\n", la[ei]);
                    ei++;
                } else break;
            }

            i = ci; j = cj;
        }
        free(match);
    }

ud_done:
#undef UD_APPEND
    free_lines(la, na);
    free_lines(lb, nb);
    return result;
}

char *difflib_simple_diff(const char *a, const char *b) {
    if (!a && !b) return strdup("");
    if (!a) return strdup(b);
    if (!b) return strdup(a);

    char *la[MAX_LINES], *lb[MAX_LINES];
    int na = split_lines(a, la, MAX_LINES);
    int nb = split_lines(b, lb, MAX_LINES);

    size_t buf_size = 65536;
    char *result = (char *)calloc(buf_size, 1);
    if (!result) { free_lines(la, na); free_lines(lb, nb); return strdup(""); }
    int pos = 0;

    int max_lines = (na > nb) ? na : nb;
    for (int i = 0; i < max_lines; i++) {
        char *line_a = (i < na) ? la[i] : "";
        char *line_b = (i < nb) ? lb[i] : "";

        if (strcmp(line_a, line_b) == 0) {
            pos += snprintf(result + pos, buf_size - (size_t)pos, " %s\n", line_a);
        } else {
            if (line_a[0]) pos += snprintf(result + pos, buf_size - (size_t)pos, "-%s\n", line_a);
            if (line_b[0]) pos += snprintf(result + pos, buf_size - (size_t)pos, "+%s\n", line_b);
        }
    }

    free_lines(la, na);
    free_lines(lb, nb);
    return result;
}
