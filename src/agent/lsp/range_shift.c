/*
 * range_shift.c — C home for upstream Hermes agent/lsp/range_shift.py.
 * C11 behavior port of the diff-aware line-shift map (gate w25_rangeshift_920f52).
 *
 * PoP: w25_rangeshift_920f52_shift_line @ agent/lsp/range_shift.py:build_line_shift
 * PoP: w25_rangeshift_920f52_shift_diag @ agent/lsp/range_shift.py:shift_diagnostic_range
 * PoP: w25_rangeshift_920f52_shift_baseline @ agent/lsp/range_shift.py:shift_baseline
 *
 * The upstream map is a closure over ``difflib.SequenceMatcher(...,
 * autojunk=False).get_opcodes()``.  That matcher is ported here line for line
 * (find_longest_match / get_matching_blocks / get_opcodes, no junk heuristic,
 * no popular-element heuristic), because the shift is only as faithful as the
 * opcode split it reads.  Lines are ``str.splitlines()`` lines: breaks on
 * \n, \r\n, \r, \v, \f and \x1c-\x1e, with no empty trailing element.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_rangeshift_920f52.h"

/* ------------------------------------------------------------------ */
/* str.splitlines()                                                    */
/* ------------------------------------------------------------------ */

typedef struct {
    char **v;
    int n;
    int cap;
} rs_lines;

static int rs_is_break(char c) {
    return c == '\n' || c == '\r' || c == '\v' || c == '\f' ||
           c == '\x1c' || c == '\x1d' || c == '\x1e';
}

static int rs_push_line(rs_lines *ls, const char *start, size_t len) {
    char *copy;
    if (ls->n == ls->cap) {
        int cap = ls->cap ? ls->cap * 2 : 8;
        char **nv = (char **)realloc(ls->v, (size_t)cap * sizeof(char *));
        if (!nv) return 0;
        ls->v = nv;
        ls->cap = cap;
    }
    copy = (char *)malloc(len + 1);
    if (!copy) return 0;
    memcpy(copy, start, len);
    copy[len] = '\0';
    ls->v[ls->n++] = copy;
    return 1;
}

static void rs_splitlines(const char *text, rs_lines *ls) {
    const char *p, *start;
    ls->v = NULL;
    ls->n = 0;
    ls->cap = 0;
    if (!text || !text[0]) return;  /* ``splitlines() if text else []`` */
    p = text;
    start = text;
    while (*p) {
        if (rs_is_break(*p)) {
            size_t len = (size_t)(p - start);
            rs_push_line(ls, start, len);
            if (*p == '\r' && p[1] == '\n') p++;
            p++;
            start = p;
        } else {
            p++;
        }
    }
    if (p > start) rs_push_line(ls, start, (size_t)(p - start));
}

static void rs_free_lines(rs_lines *ls) {
    int i;
    for (i = 0; i < ls->n; i++) free(ls->v[i]);
    free(ls->v);
    ls->v = NULL;
    ls->n = ls->cap = 0;
}

static int rs_lines_equal(const rs_lines *a, const rs_lines *b) {
    int i;
    if (a->n != b->n) return 0;
    for (i = 0; i < a->n; i++)
        if (strcmp(a->v[i], b->v[i]) != 0) return 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/* difflib.SequenceMatcher (autojunk=False, isjunk=None)               */
/* ------------------------------------------------------------------ */

/* find_longest_match: no junk, so no junk extension passes. */
static void rs_find_longest_match(const rs_lines *a, int alo, int ahi,
                                  const rs_lines *b, int blo, int bhi,
                                  int *besti, int *bestj, int *bestsize) {
    int *prev, *cur, i, j;
    size_t slots = (size_t)b->n + 2;

    *besti = alo;
    *bestj = blo;
    *bestsize = 0;
    prev = (int *)calloc(slots, sizeof(int));
    cur = (int *)calloc(slots, sizeof(int));
    if (!prev || !cur) {
        free(prev);
        free(cur);
        return;
    }
    for (i = alo; i < ahi; i++) {
        memset(cur, 0, slots * sizeof(int));
        for (j = blo; j < bhi; j++) {
            if (strcmp(a->v[i], b->v[j]) == 0) {
                int k = (j > 0 ? prev[j - 1] : 0) + 1;
                cur[j] = k;
                if (k > *bestsize) {
                    *besti = i - k + 1;
                    *bestj = j - k + 1;
                    *bestsize = k;
                }
            }
        }
        {
            int *tmp = prev;
            prev = cur;
            cur = tmp;
        }
    }
    free(prev);
    free(cur);

    while (*besti > alo && *bestj > blo &&
           strcmp(a->v[*besti - 1], b->v[*bestj - 1]) == 0) {
        (*besti)--;
        (*bestj)--;
        (*bestsize)++;
    }
    while (*besti + *bestsize < ahi && *bestj + *bestsize < bhi &&
           strcmp(a->v[*besti + *bestsize], b->v[*bestj + *bestsize]) == 0) {
        (*bestsize)++;
    }
}

typedef struct {
    int i, j, k;
} rs_block;

typedef struct {
    int alo, ahi, blo, bhi;
} rs_box;

/* get_matching_blocks: queue of unknown boxes, sorted, adjacency-collapsed,
 * with the (la, lb, 0) sentinel last. */
static rs_block *rs_matching_blocks(const rs_lines *a, const rs_lines *b, int *count) {
    rs_box *queue;
    int qn = 0, qcap = 8;
    rs_block *raw;
    int rn = 0, rcap = 8;
    rs_block *out;
    int on = 0, oi1 = 0, oj1 = 0, ok1 = 0, idx;

    *count = 0;
    queue = (rs_box *)malloc((size_t)qcap * sizeof(rs_box));
    raw = (rs_block *)malloc((size_t)rcap * sizeof(rs_block));
    out = (rs_block *)malloc(((size_t)a->n + 2) * sizeof(rs_block));
    if (!queue || !raw || !out) {
        free(queue);
        free(raw);
        free(out);
        return NULL;
    }
    queue[qn].alo = 0;
    queue[qn].ahi = a->n;
    queue[qn].blo = 0;
    queue[qn].bhi = b->n;
    qn++;

    while (qn > 0) {
        rs_box bx = queue[--qn];
        int bi, bj, bk;
        rs_find_longest_match(a, bx.alo, bx.ahi, b, bx.blo, bx.bhi, &bi, &bj, &bk);
        if (!bk) continue;
        if (rn == rcap) {
            rs_block *nr;
            rcap *= 2;
            nr = (rs_block *)realloc(raw, (size_t)rcap * sizeof(rs_block));
            if (!nr) break;
            raw = nr;
        }
        raw[rn].i = bi;
        raw[rn].j = bj;
        raw[rn].k = bk;
        rn++;
        if (bx.alo < bi && bx.blo < bj) {
            if (qn == qcap) {
                rs_box *nq;
                qcap *= 2;
                nq = (rs_box *)realloc(queue, (size_t)qcap * sizeof(rs_box));
                if (!nq) break;
                queue = nq;
            }
            queue[qn].alo = bx.alo;
            queue[qn].ahi = bi;
            queue[qn].blo = bx.blo;
            queue[qn].bhi = bj;
            qn++;
        }
        if (bi + bk < bx.ahi && bj + bk < bx.bhi) {
            if (qn == qcap) {
                rs_box *nq;
                qcap *= 2;
                nq = (rs_box *)realloc(queue, (size_t)qcap * sizeof(rs_box));
                if (!nq) break;
                queue = nq;
            }
            queue[qn].alo = bi + bk;
            queue[qn].ahi = bx.ahi;
            queue[qn].blo = bj + bk;
            queue[qn].bhi = bx.bhi;
            qn++;
        }
    }

    /* matching_blocks.sort() over (i, j, k). */
    for (idx = 1; idx < rn; idx++) {
        rs_block key = raw[idx];
        int m = idx - 1;
        while (m >= 0 && (raw[m].i > key.i ||
                          (raw[m].i == key.i && (raw[m].j > key.j ||
                           (raw[m].j == key.j && raw[m].k > key.k))))) {
            raw[m + 1] = raw[m];
            m--;
        }
        raw[m + 1] = key;
    }

    for (idx = 0; idx < rn; idx++) {
        if (oi1 + ok1 == raw[idx].i && oj1 + ok1 == raw[idx].j) {
            ok1 += raw[idx].k;
        } else {
            if (ok1) {
                out[on].i = oi1;
                out[on].j = oj1;
                out[on].k = ok1;
                on++;
            }
            oi1 = raw[idx].i;
            oj1 = raw[idx].j;
            ok1 = raw[idx].k;
        }
    }
    if (ok1) {
        out[on].i = oi1;
        out[on].j = oj1;
        out[on].k = ok1;
        on++;
    }
    out[on].i = a->n;
    out[on].j = b->n;
    out[on].k = 0;
    on++;

    free(queue);
    free(raw);
    *count = on;
    return out;
}

typedef struct {
    int tag; /* 0 equal, 1 replace, 2 delete, 3 insert */
    int i1, i2, j1, j2;
} rs_op;

typedef struct {
    int identity;
    rs_lines pre;
    rs_lines post;
    rs_op *ops;
    int nops;
} rs_shift;

static void rs_op_push(rs_shift *sh, int *cap, int tag, int i1, int i2, int j1, int j2) {
    if (sh->nops == *cap) {
        int ncap = *cap ? *cap * 2 : 8;
        rs_op *no = (rs_op *)realloc(sh->ops, (size_t)ncap * sizeof(rs_op));
        if (!no) return;
        sh->ops = no;
        *cap = ncap;
    }
    sh->ops[sh->nops].tag = tag;
    sh->ops[sh->nops].i1 = i1;
    sh->ops[sh->nops].i2 = i2;
    sh->ops[sh->nops].j1 = j1;
    sh->ops[sh->nops].j2 = j2;
    sh->nops++;
}

static void rs_build(const char *pre_text, const char *post_text, rs_shift *sh) {
    int cap = 0, i = 0, j = 0, nblocks = 0, b;
    rs_block *blocks;

    memset(sh, 0, sizeof(*sh));
    rs_splitlines(pre_text, &sh->pre);
    rs_splitlines(post_text, &sh->post);
    if (rs_lines_equal(&sh->pre, &sh->post)) {
        sh->identity = 1;
        return;
    }
    blocks = rs_matching_blocks(&sh->pre, &sh->post, &nblocks);
    if (!blocks) return;
    for (b = 0; b < nblocks; b++) {
        int ai = blocks[b].i, bj = blocks[b].j, size = blocks[b].k;
        int tag = -1;
        if (i < ai && j < bj) tag = 1;
        else if (i < ai) tag = 2;
        else if (j < bj) tag = 3;
        if (tag >= 0) rs_op_push(sh, &cap, tag, i, ai, j, bj);
        i = ai + size;
        j = bj + size;
        if (size) rs_op_push(sh, &cap, 0, ai, i, bj, j);
    }
    free(blocks);
}

static void rs_release(rs_shift *sh) {
    rs_free_lines(&sh->pre);
    rs_free_lines(&sh->post);
    free(sh->ops);
    sh->ops = NULL;
    sh->nops = 0;
}

/* shift(pre_line) -> post_line; 0 when the line was deleted. */
static int rs_map(const rs_shift *sh, int line, int *out) {
    int k;
    if (sh->identity) {
        *out = line;
        return 1;
    }
    for (k = 0; k < sh->nops; k++) {
        const rs_op *op = &sh->ops[k];
        if (op->i1 <= line && line < op->i2) {
            if (op->tag == 0) {
                *out = line - op->i1 + op->j1;
                return 1;
            }
            return 0;
        }
        if (line < op->i1) break;
    }
    if (sh->post.n > 0) {
        *out = sh->post.n - 1 > 0 ? sh->post.n - 1 : 0;
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* int(v) over the JSON shapes a range field can hold                  */
/* ------------------------------------------------------------------ */

static int rs_int_value(const json_t *v, double *out) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NUMBER:
        *out = (double)(long long)v->num_val;
        return 1;
    case JSON_BOOL:
        *out = v->bool_val ? 1.0 : 0.0;
        return 1;
    case JSON_STRING: {
        const char *s = v->str_val ? v->str_val : "";
        char *end = NULL;
        long long parsed;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' ||
               *s == '\v' || *s == '\f')
            s++;
        parsed = strtoll(s, &end, 10);
        if (end == s) return 0;
        while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r' ||
               *end == '\v' || *end == '\f')
            end++;
        if (*end != '\0') return 0;
        *out = (double)parsed;
        return 1;
    }
    default:
        return 0;
    }
}

/* ------------------------------------------------------------------ */
/* Public surface                                                      */
/* ------------------------------------------------------------------ */

json_t *w25_rangeshift_920f52_shift_line(const char *pre_text,
                                         const char *post_text, int line) {
    rs_shift sh;
    int mapped;
    json_t *out;

    rs_build(pre_text, post_text, &sh);
    if (!rs_map(&sh, line, &mapped)) {
        rs_release(&sh);
        return json_null();
    }
    out = json_number((double)mapped);
    rs_release(&sh);
    return out;
}

static json_t *rs_shift_one(const rs_shift *sh, const json_t *diag) {
    const json_t *rng, *start, *end;
    double pre_start = 0.0, char_start = 0.0, char_end = 0.0;
    double end_line, mapped_start, mapped_end;
    int mapped;
    json_t *out, *range, *s, *e;

    if (!diag || diag->type != JSON_OBJECT) return json_null();
    rng = json_obj_get(diag, "range");
    if (rng && rng->type != JSON_OBJECT) rng = NULL; /* {} when falsy */
    start = rng ? json_obj_get(rng, "start") : NULL;
    if (start && start->type != JSON_OBJECT) start = NULL;
    end = rng ? json_obj_get(rng, "end") : NULL;
    if (end && end->type != JSON_OBJECT) end = NULL;

    if (start) rs_int_value(json_obj_get(start, "line"), &pre_start);
    if (!rs_map(sh, (int)pre_start, &mapped)) return json_null();
    mapped_start = (double)mapped;

    end_line = pre_start;
    if (end) rs_int_value(json_obj_get(end, "line"), &end_line);
    if (rs_map(sh, (int)end_line, &mapped)) mapped_end = (double)mapped;
    else mapped_end = mapped_start;

    if (start) rs_int_value(json_obj_get(start, "character"), &char_start);
    if (end) rs_int_value(json_obj_get(end, "character"), &char_end);

    out = json_copy(diag);
    if (!out || out->type != JSON_OBJECT) {
        json_free(out);
        return json_null();
    }
    range = json_object();
    s = json_object();
    json_set(s, "line", json_number(mapped_start));
    json_set(s, "character", json_number(char_start));
    e = json_object();
    json_set(e, "line", json_number(mapped_end));
    json_set(e, "character", json_number(char_end));
    json_set(range, "start", s);
    json_set(range, "end", e);
    json_set(out, "range", range);
    return out;
}

json_t *w25_rangeshift_920f52_shift_diag(const char *pre_text,
                                         const char *post_text,
                                         const json_t *diag) {
    rs_shift sh;
    json_t *out;

    rs_build(pre_text, post_text, &sh);
    out = rs_shift_one(&sh, diag);
    rs_release(&sh);
    return out;
}

json_t *w25_rangeshift_920f52_shift_baseline(const char *pre_text,
                                             const char *post_text,
                                             const json_t *baseline) {
    rs_shift sh;
    json_t *out;
    size_t i, n;

    rs_build(pre_text, post_text, &sh);
    out = json_array();
    n = baseline ? json_len(baseline) : 0;
    for (i = 0; i < n; i++) {
        const json_t *diag = json_get(baseline, i);
        json_t *shifted;
        if (!diag || diag->type != JSON_OBJECT) continue;
        shifted = rs_shift_one(&sh, diag);
        if (shifted && shifted->type != JSON_NULL) json_append(out, shifted);
        else json_free(shifted);
    }
    rs_release(&sh);
    return out;
}
