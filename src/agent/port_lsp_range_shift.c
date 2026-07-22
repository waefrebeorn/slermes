/*
 * port_lsp_range_shift.c — pure helpers ported from
 * agent/lsp/range_shift.py (diff-aware LSP line-shift map). Self-contained;
 * implements a faithful SequenceMatcher(autojunk=False).get_opcodes() and uses
 * libjson to round-trip diagnostics.
 *
 *   - build_line_shift        -> lsp_build_line_shift
 *   - (closure)             -> lsp_line_shift
 *   - shift_diagnostic_range -> lsp_shift_diagnostic_range
 *   - shift_baseline         -> lsp_shift_baseline_json
 */

#include "lsp_range_shift.h"
#include "libjson/json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct json_t json_t;

/* ---- line splitting ---- */
static char **split_lines(const char *text, int *out_n)
{
    if (!text || !*text) { *out_n = 0; return NULL; }
    /* count lines */
    int n = 1;
    for (const char *p = text; *p; p++) if (*p == '\n') n++;
    char **lines = calloc((size_t)n, sizeof(char *));
    int idx = 0;
    const char *start = text;
    for (const char *p = text; ; p++) {
        if (*p == '\n' || *p == '\0') {
            size_t len = (size_t)(p - start);
            char *ln = malloc(len + 1);
            memcpy(ln, start, len);
            ln[len] = '\0';
            lines[idx++] = ln;
            start = p + 1;
            if (*p == '\0') break;
        }
    }
    *out_n = idx;
    return lines;
}

static void free_lines(char **lines, int n)
{
    if (!lines) return;
    for (int i = 0; i < n; i++) free(lines[i]);
    free(lines);
}

/* ---- opcode generation (SequenceMatcher) ---- */
typedef struct json_t json_t;

/* find the longest matching block in [alo,ahi) x [blo,bhi). Returns
 * (besti, bestj, bestsize). */
static void find_longest_match(char **pre, char **post,
                               int alo, int ahi, int blo, int bhi,
                               int *bi, int *bj, int *bsz)
{
    int besti = alo, bestj = blo, bestsize = 0;
    /* j2len map: simple O(m) scan per i (test-sized inputs) */
    int *j2len = calloc((size_t)(bhi > blo ? bhi - blo : 1), sizeof(int));
    for (int i = alo; i < ahi; i++) {
        int *newj = calloc((size_t)(bhi > blo ? bhi - blo : 1), sizeof(int));
        for (int j = blo; j < bhi; j++) {
            int k = 0;
            if (strcmp(pre[i], post[j]) == 0) {
                int prev = (j > blo) ? j2len[j - 1 - blo] : 0;
                k = prev + 1;
            }
            newj[j - blo] = k;
            if (k > bestsize) { bestsize = k; besti = i - k + 1; bestj = j - k + 1; }
        }
        free(j2len);
        j2len = newj;
    }
    free(j2len);
    *bi = besti; *bj = bestj; *bsz = bestsize;
}

static void get_matching_blocks(char **pre, char **post,
                                int alo, int ahi, int blo, int bhi,
                                opcode_t **out, int *out_n, int *cap)
{
    int bi, bj, bsz;
    find_longest_match(pre, post, alo, ahi, blo, bhi, &bi, &bj, &bsz);
    if (bsz == 0) return;
    /* before */
    get_matching_blocks(pre, post, alo, bi, blo, bj, out, out_n, cap);
    if (*out_n >= *cap) { *cap *= 2; *out = realloc(*out, (size_t)*cap * sizeof(opcode_t)); }
    (*out)[(*out_n)++] = (opcode_t){0, bi, bj, bsz}; /* placeholder, fixed below */
    /* mark as equal block: store i1,j1,size in i1,i2,j1 */
    (*out)[(*out_n) - 1].i1 = bi;
    (*out)[(*out_n) - 1].j1 = bj;
    (*out)[(*out_n) - 1].i2 = bsz; /* size */
    /* after */
    get_matching_blocks(pre, post, bi + bsz, ahi, bj + bsz, bhi, out, out_n, cap);
}

static lsp_range_shift_t *build_opcodes(char **pre, int pn, char **post, int qn)
{
    int cap = 16, n = 0;
    opcode_t *mb = calloc((size_t)cap, sizeof(opcode_t));
    get_matching_blocks(pre, post, 0, pn, 0, qn, &mb, &n, &cap);
    /* sentinel */
    if (n >= cap) { cap++; mb = realloc(mb, (size_t)cap * sizeof(opcode_t)); }
    mb[n++] = (opcode_t){0, pn, qn, 0}; /* sentinel (alen,blen,0) */

    lsp_range_shift_t *sh = calloc(1, sizeof(lsp_range_shift_t));
    sh->pre_n = pn; sh->post_n = qn;
    sh->opcap = n + 1;
    sh->ops = calloc((size_t)sh->opcap, sizeof(opcode_t));

    int la = 0, lb = 0, on = 0;
    for (int k = 0; k < n; k++) {
        int i = mb[k].i1, j = mb[k].j1, sz = mb[k].i2; /* equal block */
        if (i > la || j > lb) {
            int tag;
            if (i > la && j > lb) tag = 1;       /* replace */
            else if (i > la) tag = 2;            /* delete */
            else tag = 3;                       /* insert */
            sh->ops[on++] = (opcode_t){tag, la, i, lb, j};
        }
        if (sz) {
            sh->ops[on++] = (opcode_t){0, i, i + sz, j, j + sz};
        }
        la = i + sz; lb = j + sz;
    }
    sh->opn = on;
    free(mb);
    return sh;
}

/* PoP: build_line_shift @ agent/lsp/range_shift.py:build_line_shift */
/* Returns a shift map; lsp_line_shift() applies it. Caller frees with
 * lsp_free_line_shift. Trivial (identical) content -> identity. */
lsp_range_shift_t *lsp_build_line_shift(const char *pre_text, const char *post_text)
{
    int pn = 0, qn = 0;
    char **pre = split_lines(pre_text, &pn);
    char **post = split_lines(post_text, &qn);

    if (pn == qn) {
        int same = 1;
        for (int i = 0; i < pn; i++)
            if (strcmp(pre[i], post[i]) != 0) { same = 0; break; }
        if (same) {
            lsp_range_shift_t *sh = calloc(1, sizeof(lsp_range_shift_t));
            sh->pre_n = pn; sh->post_n = qn; sh->identity = 1;
            free_lines(pre, pn); free_lines(post, qn);
            return sh;
        }
    }
    lsp_range_shift_t *sh = build_opcodes(pre, pn, post, qn);
    sh->pre_lines = pre; sh->post_lines = post; /* kept for free */
    return sh;
}

/* PoP: build_line_shift closure @ agent/lsp/range_shift.py:shift
 * Apply the shift map to a pre-edit 0-indexed line. Returns the post-edit
 * line, or -1 if the line was deleted (no post counterpart). */
int lsp_line_shift(const lsp_range_shift_t *sh, int line)
{
    if (!sh) return -1;
    if (sh->identity) return line;
    for (int k = 0; k < sh->opn; k++) {
        const opcode_t *o = &sh->ops[k];
        if (o->i1 <= line && line < o->i2) {
            if (o->tag == 0) return line - o->i1 + o->j1;   /* equal */
            return -1;                                       /* delete/replace */
        }
        if (line < o->i1) break;
    }
    /* past last region */
    return sh->post_n > 0 ? (sh->post_n - 1) : -1;
}

/* PoP: shift_diagnostic_range @ agent/lsp/range_shift.py:shift_diagnostic_range */
/* Parse diag JSON, remap start.line/end.line through shift. Returns a malloc'd
 * new JSON string, or NULL if start maps to deleted (caller drops it). */
char *lsp_shift_diagnostic_range(const char *diag_json, const lsp_range_shift_t *sh)
{
    if (!diag_json) return NULL;
    char *err = NULL;
    json_t *diag = json_parse(diag_json, &err);
    if (err) { free(err); return NULL; }
    if (!diag || diag->type != JSON_OBJECT) { if (diag) json_free(diag); return NULL; }

    const json_t *rng = json_obj_get(diag, "range");
    if (!rng || rng->type != JSON_OBJECT) { json_free(diag); return NULL; }
    const json_t *start = json_obj_get(rng, "start");
    const json_t *end = json_obj_get(rng, "end");
    if (!start || !end) { json_free(diag); return NULL; }

    int pre_start = (int)json_get_num(start, "line", 0);
    int pre_end = (int)json_get_num(end, "line", pre_start);
    int pre_start_ch = (int)json_get_num(start, "character", 0);
    int pre_end_ch = (int)json_get_num(end, "character", 0);

    int new_start = lsp_line_shift(sh, pre_start);
    if (new_start < 0) { json_free(diag); return NULL; }
    int new_end = lsp_line_shift(sh, pre_end);
    if (new_end < 0) new_end = new_start;   /* straddle -> collapse to start */

    /* build a fresh object mirroring diag with remapped range */
    json_t *out = json_object();
    /* copy all top-level keys except range */
    for (size_t i = 0; i < diag->c.count; i++) {
        const char *k = diag->c.keys[i];
        if (k && strcmp(k, "range") == 0) continue;
        json_set(out, k, json_copy(diag->c.items[i]));
    }
    json_t *r = json_object();
    json_t *rs = json_object(); json_set(rs, "line", json_number(new_start)); json_set(rs, "character", json_number(pre_start_ch));
    json_t *re = json_object(); json_set(re, "line", json_number(new_end)); json_set(re, "character", json_number(pre_end_ch));
    json_set(r, "start", rs); json_set(r, "end", re);
    json_set(out, "range", r);

    char *ser = json_serialize(out);
    json_free(out); json_free(diag);
    return ser;
}

/* PoP: shift_baseline @ agent/lsp/range_shift.py:shift_baseline */
/* Apply shift to every diagnostic in the baseline JSON array, dropping deleted
 * entries. Returns a malloc'd JSON array string (possibly "[]"). */
char *lsp_shift_baseline_json(const char *baseline_json, const lsp_range_shift_t *sh)
{
    char *err = NULL;
    json_t *base = json_parse(baseline_json ? baseline_json : "[]", &err);
    if (err) { free(err); return strdup("[]"); }
    if (!base || base->type != JSON_ARRAY) { if (base) json_free(base); return strdup("[]"); }

    json_t *out = json_array();
    for (size_t i = 0; i < base->c.count; i++) {
        const json_t *d = base->c.items[i];
        if (!d || d->type != JSON_OBJECT) continue;
        char *dser = json_serialize(d);
        char *shifted = lsp_shift_diagnostic_range(dser, sh);
        free(dser);
        if (shifted) {
            char *e2 = NULL;
            json_t *so = json_parse(shifted, &e2);
            if (e2) free(e2);
            if (so) json_append(out, so);   /* takes ownership */
            free(shifted);
        }
    }
    char *ser = json_serialize(out);
    json_free(out); json_free(base);
    return ser;
}

void lsp_free_line_shift(lsp_range_shift_t *sh)
{
    if (!sh) return;
    if (!sh->identity) {
        free_lines(sh->pre_lines, sh->pre_n);
        free_lines(sh->post_lines, sh->post_n);
        free(sh->ops);
    }
    free(sh);
}
