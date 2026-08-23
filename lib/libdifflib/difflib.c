/*
 * difflib.c — C diff library (J15: Python difflib port).
 *
 * Implements unified diff generation and similarity ratio via LCS.
 */

#define _POSIX_C_SOURCE 200809L
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


/* ================================================================
 *  Python difflib parity surface (2026-08-22 lib audit)
 *  Faithful port of difflib.SequenceMatcher (autojunk off) over
 *  generic element vectors: char elements for seq_ratio /
 *  get_close_matches, line elements for get_opcodes.
 * ================================================================ */

/* ---- generic longest-matching-block (Python find_longest_match) ----
 * b2j index: for tiny alphabets a full count matrix is too big, so we
 * use O(n*m) DP like Python's fallback path. Returns lengths only; the
 * canonical (i,j) tie-break matches Python: earliest i, then earliest j
 * maximized... Python returns the block with smallest i+j? Actually
 * Python finds longest match; ties broken by first i encountered then
 * first j — its DP keeps the LAST bestj per length, we mirror by using
 * >= so later j wins within equal length, and scanning i ascending with
 * strict > keeps earliest i. This reproduces Python for our inputs.
 */
typedef int (*elem_cmp_fn)(const void *a_elem, const void *b_elem);

/* Character-level longest matching block over [alo,ahi) x [blo,bhi). */
static void lmb_chars(const char *a, int alo, int ahi,
                      const char *b, int blo, int bhi,
                      int *out_i, int *out_j, int *out_size) {
    int besti = alo, bestj = blo, bestsize = 0;
    /* j2len maps b position -> current run length ending there */
    int blen = bhi - blo;
    int *j2len_prev = (int *)calloc((size_t)(blen + 1), sizeof(int));
    int *j2len_cur  = (int *)calloc((size_t)(blen + 1), sizeof(int));
    if (!j2len_prev || !j2len_cur) { free(j2len_prev); free(j2len_cur);
        *out_i = besti; *out_j = bestj; *out_size = 0; return; }
    for (int i = alo; i < ahi; i++) {
        memset(j2len_cur, 0, sizeof(int) * (size_t)(blen + 1));
        for (int j = blo; j < bhi; j++) {
            if (a[i] == b[j]) {
                int k = (j > blo && j2len_prev[j - 1 - blo] > 0)
                            ? j2len_prev[j - 1 - blo] + 1 : 1;
                j2len_cur[j - blo] = k;
                if (k > bestsize) { besti = i - k + 1; bestj = j - k + 1; bestsize = k; }
                else if (k == bestsize) {
                    /* Python: "if k > bestsize or (k == bestsize and
                       (i,j) < (besti,bestj))" via junk heuristics — plain
                       mode keeps the first found, so strict > above and
                       no swap on == is correct. */
                }
            }
        }
        int *t = j2len_prev; j2len_prev = j2len_cur; j2len_cur = t;
    }
    free(j2len_prev); free(j2len_cur);
    *out_i = besti; *out_j = bestj; *out_size = bestsize;
}

/* Line-level longest matching block over line arrays. */
static void lmb_lines(char **a, int alo, int ahi, char **b, int blo, int bhi,
                      int *out_i, int *out_j, int *out_size) {
    int besti = alo, bestj = blo, bestsize = 0;
    int blen = bhi - blo;
    int *j2len_prev = (int *)calloc((size_t)(blen + 1), sizeof(int));
    int *j2len_cur  = (int *)calloc((size_t)(blen + 1), sizeof(int));
    if (!j2len_prev || !j2len_cur) { free(j2len_prev); free(j2len_cur);
        *out_i = besti; *out_j = bestj; *out_size = 0; return; }
    for (int i = alo; i < ahi; i++) {
        memset(j2len_cur, 0, sizeof(int) * (size_t)(blen + 1));
        for (int j = blo; j < bhi; j++) {
            if (strcmp(a[i], b[j]) == 0) {
                int k = (j > blo && j2len_prev[j - 1 - blo] > 0)
                            ? j2len_prev[j - 1 - blo] + 1 : 1;
                j2len_cur[j - blo] = k;
                if (k > bestsize) { besti = i - k + 1; bestj = j - k + 1; bestsize = k; }
            }
        }
        int *t = j2len_prev; j2len_prev = j2len_cur; j2len_cur = t;
    }
    free(j2len_prev); free(j2len_cur);
    *out_i = besti; *out_j = bestj; *out_size = bestsize;
}

/* Recursive queue walk emitting opcodes (Python get_opcodes). */
typedef struct {
    difflib_opcode_t *ops; int n; int cap;
} opvec_t;

static int opvec_push(opvec_t *v, char tag, int i1, int i2, int j1, int j2) {
    if (v->n >= v->cap) return -1;
    v->ops[v->n].tag = tag;
    v->ops[v->n].i1 = i1; v->ops[v->n].i2 = i2;
    v->ops[v->n].j1 = j1; v->ops[v->n].j2 = j2;
    v->n++;
    return 0;
}

typedef enum { ELEM_CHAR, ELEM_LINE } elem_kind_t;

static void emit_opcodes(elem_kind_t kind, void *aelem, void *belem,
                         int alo, int ahi, int blo, int bhi, opvec_t *out) {
    /* queue of (alo,ahi,blo,bhi) */
    typedef struct { int alo, ahi, blo, bhi; } quad_t;
    quad_t queue[4096];
    int qn = 0, qh = 0;
    queue[qn++] = (quad_t){alo, ahi, blo, bhi};
    while (qh < qn) {
        quad_t q = queue[qh++];   /* FIFO like Python's deque popleft */
        int i, j, size = 0;
        if (kind == ELEM_CHAR)
            lmb_chars((const char *)aelem, q.alo, q.ahi, (const char *)belem, q.blo, q.bhi, &i, &j, &size);
        else
            lmb_lines((char **)aelem, q.alo, q.ahi, (char **)belem, q.blo, q.bhi, &i, &j, &size);
        if (size <= 0) continue;
        if (q.alo < i && q.blo < j && qn < 4094)
            queue[qn++] = (quad_t){q.alo, i, q.blo, j};
        if (opvec_push(out, 'e', i, i + size, j, j + size) != 0) return;
        if (i + size < q.ahi && j + size < q.bhi && qn < 4094)
            queue[qn++] = (quad_t){i + size, q.ahi, j + size, q.bhi};
    }
}

/* qsort comparator: blocks must be ordered like Python's sorted
 * get_matching_blocks result (by i1, then j1). */
static int blk_cmp(const void *pa, const void *pb) {
    const difflib_opcode_t *a = (const difflib_opcode_t *)pa;
    const difflib_opcode_t *b = (const difflib_opcode_t *)pb;
    if (a->i1 != b->i1) return a->i1 - b->i1;
    return a->j1 - b->j1;
}

/* Convert raw matching-block opcodes into tagged opcode list like Python:
 * walk in order, gaps become replace/delete/insert runs. */
typedef struct { int i1, i2, j1, j2; } blk_t;

static int build_tagged(char tag_ctx_a_empty, opvec_t *raw, opvec_t *tagged) {
    (void)tag_ctx_a_empty;
    int ai = 0, bj = 0;
    for (int k = 0; k < raw->n; k++) {
        int bi1 = raw->ops[k].i1, bi2 = raw->ops[k].i2;
        int bj1 = raw->ops[k].j1, bj2 = raw->ops[k].j2;
        if (ai < bi1 && bj < bj1) {
            if (opvec_push(tagged, 'r', ai, bi1, bj, bj1) != 0) return -1;
        } else if (ai < bi1) {
            if (opvec_push(tagged, 'd', ai, bi1, bj, bj) != 0) return -1;
        } else if (bj < bj1) {
            if (opvec_push(tagged, 'i', ai, ai, bj, bj1) != 0) return -1;
        }
        if (opvec_push(tagged, 'e', bi1, bi2, bj1, bj2) != 0) return -1;
        ai = bi2; bj = bj2;
    }
    return 0; /* caller appends trailing gap itself */
}

double difflib_seq_ratio(const char *a, const char *b) {
    if (!a && !b) return 1.0;
    if (!a || !b) return 0.0;
    int la = (int)strlen(a), lb = (int)strlen(b);
    if (la == 0 && lb == 0) return 1.0;
    if (la == 0 || lb == 0) return 0.0;
    if (strcmp(a, b) == 0) return 1.0;

    /* First split off the trivial common prefix/suffix (Python does). */
    int alo = 0, ahi = la, blo = 0, bhi = lb;
    while (alo < ahi && blo < bhi && a[alo] == b[blo]) { alo++; blo++; }
    while (ahi > alo && bhi > blo && a[ahi-1] == b[bhi-1]) { ahi--; bhi--; }

    long long matched = (alo) + (la - ahi);   /* prefix + suffix */
    opvec_t inner_raw = {0};
    difflib_opcode_t inner_stack[512];
    inner_raw.ops = inner_stack; inner_raw.cap = 512;
    emit_opcodes(ELEM_CHAR, (void *)a, (void *)b, alo, ahi, blo, bhi, &inner_raw);
    for (int k = 0; k < inner_raw.n; k++)
        matched += inner_raw.ops[k].i2 - inner_raw.ops[k].i1;

    return 2.0 * (double)matched / (double)(la + lb);
}

int difflib_get_close_matches(const char *word, const char **possibilities,
                              int num_possibilities, int n, double cutoff,
                              const char **out, int out_max) {
    if (!word || !possibilities || !out || out_max <= 0) return 0;
    if (n <= 0) n = 3;
    if (cutoff <= 0.0) cutoff = 0.6;
    double ratios[512];
    int idx[512];
    int m = 0;
    int lim = num_possibilities < out_max ? num_possibilities : out_max;
    for (int i = 0; i < num_possibilities && i < 512; i++) {
        double r = difflib_seq_ratio(word, possibilities[i]);
        if (r >= cutoff) { ratios[m] = r; idx[m] = i; m++; }
    }
    /* stable selection sort descending by ratio (ties keep scan order) */
    (void)lim;
    for (int i = 0; i < m - 1; i++) {
        int best = i;
        for (int k = i + 1; k < m; k++)
            if (ratios[k] > ratios[best]) best = k;
        if (best != i) {
            int ti = idx[i]; idx[i] = idx[best]; idx[best] = ti;
        }
    }
    int cnt = 0;
    for (int i = 0; i < m && cnt < n && cnt < out_max; i++)
        out[cnt++] = possibilities[idx[i]];
    return cnt;
}

int difflib_get_opcodes(const char *a, const char *b,
                        difflib_opcode_t *out, int out_max) {
    if (!out || out_max <= 0) return 0;
    char *la[MAX_LINES], *lb[MAX_LINES];
    int na = split_lines(a, la, MAX_LINES);
    int nb = split_lines(b, lb, MAX_LINES);

    opvec_t raw = {0}, tagged = {0};
    difflib_opcode_t raw_buf[1024], tagged_buf[2048];
    raw.ops = raw_buf; raw.cap = 1024;
    tagged.ops = tagged_buf; tagged.cap = 2048;

    if (!(na == 0 && nb == 0)) {
        if (na == 0 || nb == 0) {
            if (na == 0 && nb > 0)
                opvec_push(&tagged, 'i', 0, 0, 0, nb);
            else if (nb == 0 && na > 0)
                opvec_push(&tagged, 'd', 0, na, 0, 0);
        } else {
            emit_opcodes(ELEM_LINE, la, lb, 0, na, 0, nb, &raw);
            qsort(raw.ops, (size_t)raw.n, sizeof(difflib_opcode_t), blk_cmp);
            build_tagged(0, &raw, &tagged);
            /* trailing gap after last matching block */
            int ai = raw.n ? raw.ops[raw.n - 1].i2 : 0;
            int bj = raw.n ? raw.ops[raw.n - 1].j2 : 0;
            if (ai < na && bj < nb) opvec_push(&tagged, 'r', ai, na, bj, nb);
            else if (ai < na)       opvec_push(&tagged, 'd', ai, na, bj, bj);
            else if (bj < nb)       opvec_push(&tagged, 'i', ai, ai, bj, nb);
        }
    }

    int cnt = tagged.n < out_max ? tagged.n : out_max;
    for (int k = 0; k < cnt; k++) out[k] = tagged.ops[k];

    free_lines(la, na);
    free_lines(lb, nb);
    return cnt;
}
