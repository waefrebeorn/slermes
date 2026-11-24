/*
 * edit_check.c - C11 port of upstream agent/lsp/edit_check.py
 * (gate w23_editcheck_cd6717).
 *
 * PoP: w23_editcheck_cd6717_shift_baseline @ agent/lsp/edit_check.py:_check
 * PoP: w23_editcheck_cd6717_introduced @ agent/lsp/edit_check.py:_check
 * PoP: w23_editcheck_cd6717_diag_key @ agent/lsp/client.py:_diagnostic_key
 * PoP: w23_editcheck_cd6717_digest @ agent/lsp/edit_check.py:_digest
 * PoP: w23_editcheck_cd6717_inline_wait @ agent/lsp/edit_check.py:inline_wait
 * PoP: w23_editcheck_cd6717_cache_ops @ agent/lsp/edit_check.py:EditChecks.remember
 *
 * The language server, the service loop and the clock are seams: the caller
 * supplies the two snapshots (the bytes the write held, never a later read),
 * the diagnostics the server published, and "now".  What lives here is the
 * delta filter's own arithmetic: Python difflib's SequenceMatcher opcodes over
 * 0-indexed lines (autojunk=False), their application to the baseline, the
 * content-identity key diagnostics are compared by, and the bounded verdict
 * cache keyed by the sha256 of the disk-read text.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"
#include "noc_w23_editcheck_cd6717.h"

#define EC_MAX_LINES 512
#define EC_MAX_OPS (EC_MAX_LINES * 3 + 8)
#define EC_MAX_BLOCKS (EC_MAX_LINES * 2 + 8)
#define EC_MAX_STACK 256

typedef struct {
    char tag;             /* 'e' equal, 'r' replace, 'd' delete, 'i' insert */
    int i1, i2, j1, j2;   /* i-range in pre, j-range in post */
} ec_op;

/* -- str.splitlines() over a mutable copy --------------------------------- */

static int ec_split(const char *text, char *work, const char **lines, int max)
{
    size_t len, i = 0, start = 0;
    int n = 0;

    if (!text) return 0;
    len = strlen(text);
    memcpy(work, text, len + 1);
    if (len == 0) return 0;

    while (i < len) {
        char c = work[i];
        size_t termlen = 0, k;

        if (c == '\r') termlen = (i + 1 < len && work[i + 1] == '\n') ? 2 : 1;
        else if (c == '\n' || c == '\v' || c == '\f' || c == '\x1c' || c == '\x1d' ||
                 c == '\x1e') termlen = 1;
        if (!termlen) { i++; continue; }
        for (k = 0; k < termlen; k++) work[i + k] = '\0';
        if (n >= max) return max;
        lines[n++] = work + start;
        i += termlen;
        start = i;
    }
    if (start < len) {
        if (n >= max) return max;
        lines[n++] = work + start;
    }
    return n;
}

/* -- difflib.SequenceMatcher(a=..., b=..., autojunk=False) ---------------- */

static void ec_flm(const char **a, int alo, int ahi, const char **b, int blo, int bhi,
                   int *pbi, int *pbj, int *pbs)
{
    int j2len[EC_MAX_LINES + 1], newj2len[EC_MAX_LINES + 2];
    int besti = alo, bestj = blo, bestsize = 0;
    int m = bhi - blo, i, k;

    if (m > EC_MAX_LINES) m = EC_MAX_LINES;
    for (k = 0; k <= m; k++) j2len[k] = 0;
    for (i = alo; i < ahi; i++) {
        for (k = 0; k <= m; k++) newj2len[k] = 0;
        for (k = 0; k < m; k++) {
            int v;
            if (strcmp(a[i], b[blo + k]) != 0) continue;
            v = (k > 0 ? j2len[k - 1] : 0) + 1;
            newj2len[k] = v;
            if (v > bestsize) {
                besti = i - v + 1;
                bestj = blo + k - v + 1;
                bestsize = v;
            }
        }
        for (k = 0; k <= m; k++) j2len[k] = newj2len[k];
    }
    *pbi = besti;
    *pbj = bestj;
    *pbs = bestsize;
}

/* get_matching_blocks() then get_opcodes().  Returns the opcode count. */
static int ec_opcodes(const char **a, int na, const char **b, int nb, ec_op *ops, int maxops)
{
    int stack[EC_MAX_STACK][4];
    int blocks[EC_MAX_BLOCKS][3];
    int sp = 0, nblocks = 0, nops = 0, i, j, t;

    if (na == 0 && nb == 0) return 0;
    stack[sp][0] = 0; stack[sp][1] = na; stack[sp][2] = 0; stack[sp][3] = nb; sp++;
    while (sp > 0) {
        int q[4], bi, bj, bs;
        sp--;
        for (t = 0; t < 4; t++) q[t] = stack[sp][t];
        ec_flm(a, q[0], q[1], b, q[2], q[3], &bi, &bj, &bs);
        if (bs == 0) continue;
        if (nblocks < EC_MAX_BLOCKS) {
            blocks[nblocks][0] = bi;
            blocks[nblocks][1] = bj;
            blocks[nblocks][2] = bs;
            nblocks++;
        }
        if (q[0] < bi && q[2] < bj && sp < EC_MAX_STACK) {
            stack[sp][0] = q[0]; stack[sp][1] = bi; stack[sp][2] = q[2]; stack[sp][3] = bj; sp++;
        }
        if (bi + bs < q[1] && bj + bs < q[3] && sp < EC_MAX_STACK) {
            stack[sp][0] = bi + bs; stack[sp][1] = q[1];
            stack[sp][2] = bj + bs; stack[sp][3] = q[3]; sp++;
        }
    }
    if (nblocks < EC_MAX_BLOCKS) {
        blocks[nblocks][0] = na;
        blocks[nblocks][1] = nb;
        blocks[nblocks][2] = 0;
        nblocks++;
    }

    /* sort by (i, j, size) */
    for (i = 1; i < nblocks; i++) {
        int cur[3];
        for (t = 0; t < 3; t++) cur[t] = blocks[i][t];
        j = i - 1;
        while (j >= 0 && (blocks[j][0] > cur[0] ||
                          (blocks[j][0] == cur[0] && (blocks[j][1] > cur[1] ||
                           (blocks[j][1] == cur[1] && blocks[j][2] > cur[2]))))) {
            for (t = 0; t < 3; t++) blocks[j + 1][t] = blocks[j][t];
            j--;
        }
        for (t = 0; t < 3; t++) blocks[j + 1][t] = cur[t];
    }

    /* collapse adjacent blocks */
    {
        int out[EC_MAX_BLOCKS][3];
        int nout = 0, i1 = 0, j1 = 0, size1 = 0;
        for (i = 0; i < nblocks; i++) {
            int bi = blocks[i][0], bj = blocks[i][1], size = blocks[i][2];
            if (i1 + size1 == bi && j1 + size1 == bj) { size1 += size; continue; }
            if (size1 && nout < EC_MAX_BLOCKS) {
                out[nout][0] = i1; out[nout][1] = j1; out[nout][2] = size1; nout++;
            }
            i1 = bi; j1 = bj; size1 = size;
        }
        if (size1 && nout < EC_MAX_BLOCKS) {
            out[nout][0] = i1; out[nout][1] = j1; out[nout][2] = size1; nout++;
        }
        for (i = 0; i < nout; i++)
            for (t = 0; t < 3; t++) blocks[i][t] = out[i][t];
        nblocks = nout;
    }

    i = 0;
    j = 0;
    for (t = 0; t < nblocks; t++) {
        int ai = blocks[t][0], bj = blocks[t][1], size = blocks[t][2];
        char tag = 0;
        if (i < ai && j < bj) tag = 'r';
        else if (i < ai) tag = 'd';
        else if (j < bj) tag = 'i';
        if (tag && nops < maxops) {
            ops[nops].tag = tag;
            ops[nops].i1 = i; ops[nops].i2 = ai;
            ops[nops].j1 = j; ops[nops].j2 = bj;
            nops++;
        }
        i = ai + size;
        j = bj + size;
        if (size && nops < maxops) {
            ops[nops].tag = 'e';
            ops[nops].i1 = ai; ops[nops].i2 = i;
            ops[nops].j1 = bj; ops[nops].j2 = j;
            nops++;
        }
    }
    return nops;
}

/* range_shift.build_line_shift(): the map is state, applied by ec_shift_line. */
typedef struct {
    ec_op ops[EC_MAX_OPS];
    int nops;
    int identity;
    int post_count;
} ec_shift;

static int ec_shift_line(const ec_shift *s, int line)
{
    int k;

    if (s->identity) return line;
    for (k = 0; k < s->nops; k++) {
        const ec_op *o = &s->ops[k];
        if (o->i1 <= line && line < o->i2)
            return o->tag == 'e' ? line - o->i1 + o->j1 : -1;
        if (line < o->i1) break;
    }
    return s->post_count > 0 ? s->post_count - 1 : -1;
}

static int ec_build_shift(const char *pre_text, const char *post_text, ec_shift *s)
{
    size_t plen = pre_text ? strlen(pre_text) : 0;
    size_t olen = post_text ? strlen(post_text) : 0;
    char *pwork = (char *)malloc(plen + 1);
    char *owork = (char *)malloc(olen + 1);
    const char *plines[EC_MAX_LINES], *olines[EC_MAX_LINES];
    int np = 0, no = 0, k;

    memset(s, 0, sizeof(*s));
    if (!pwork || !owork) { free(pwork); free(owork); return -1; }
    np = ec_split(pre_text, pwork, plines, EC_MAX_LINES);
    no = ec_split(post_text, owork, olines, EC_MAX_LINES);
    s->post_count = no;
    s->identity = (np == no);
    for (k = 0; k < np && s->identity; k++)
        if (strcmp(plines[k], olines[k]) != 0) s->identity = 0;
    if (!s->identity) s->nops = ec_opcodes(plines, np, olines, no, s->ops, EC_MAX_OPS);
    free(pwork);
    free(owork);
    return 0;
}

/* int(node.get(key, default)) for the LSP range fields. */
static int ec_int_field(const json_t *obj, const char *key, int dflt)
{
    json_t *v = obj ? json_obj_get(obj, key) : NULL;
    if (!v) return dflt;
    if (v->type == JSON_NUMBER) return (int)v->num_val;
    if (v->type == JSON_STRING && v->str_val) return atoi(v->str_val);
    return dflt;
}

static json_t *ec_shift_range(const json_t *diag, ec_shift *s)
{
    json_t *rng = json_obj_get(diag, "range");
    json_t *start, *end, *out, *nr, *ns, *ne;
    int pre_start, new_start, new_end;

    if (!rng || rng->type != JSON_OBJECT) rng = NULL;
    start = rng ? json_obj_get(rng, "start") : NULL;
    end = rng ? json_obj_get(rng, "end") : NULL;
    if (!start || start->type != JSON_OBJECT) start = NULL;
    if (!end || end->type != JSON_OBJECT) end = NULL;

    pre_start = ec_int_field(start, "line", 0);
    new_start = ec_shift_line(s, pre_start);
    if (new_start < 0) return NULL;
    new_end = ec_shift_line(s, ec_int_field(end, "line", pre_start));
    if (new_end < 0) new_end = new_start;

    out = json_copy(diag);
    nr = json_object();
    ns = json_object();
    ne = json_object();
    json_set(ns, "line", json_number((double)new_start));
    json_set(ns, "character", json_number((double)ec_int_field(start, "character", 0)));
    json_set(ne, "line", json_number((double)new_end));
    json_set(ne, "character", json_number((double)ec_int_field(end, "character", 0)));
    json_set(nr, "start", ns);
    json_set(nr, "end", ne);
    json_set(out, "range", nr);
    return out;
}

/* -- str() of a JSON value the way Python prints it ---------------------- */

static void ec_num_text(const json_t *v, char *buf, size_t cap)
{
    long long n = (long long)v->num_val;
    if ((double)n == v->num_val) snprintf(buf, cap, "%lld", n);
    else snprintf(buf, cap, "%g", v->num_val);
}

/* Falsy for Python: None, False, 0, "", [], {}. */
static int ec_falsy(const json_t *v)
{
    if (!v) return 1;
    switch (v->type) {
        case JSON_NULL: return 1;
        case JSON_BOOL: return v->bool_val ? 0 : 1;
        case JSON_NUMBER: return v->num_val == 0.0;
        case JSON_STRING: return !v->str_val || v->str_val[0] == '\0';
        case JSON_ARRAY: return (size_t)json_len(v) == 0;
        case JSON_OBJECT: return json_object_size(v) == 0;
        default: return 1;
    }
}

static int ec_ws(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* str(value); NULL for None/unprintable. */
static char *ec_text_of(const json_t *v)
{
    char buf[64];
    if (!v || v->type == JSON_NULL) return NULL;
    switch (v->type) {
        case JSON_STRING: {
            size_t n = strlen(v->str_val ? v->str_val : "");
            char *o = (char *)malloc(n + 1);
            if (o) memcpy(o, v->str_val ? v->str_val : "", n + 1);
            return o;
        }
        case JSON_NUMBER:
            ec_num_text(v, buf, sizeof(buf));
            break;
        case JSON_BOOL:
            snprintf(buf, sizeof(buf), "%s", v->bool_val ? "True" : "False");
            break;
        default:
            return NULL;
    }
    {
        size_t n = strlen(buf);
        char *o = (char *)malloc(n + 1);
        if (o) memcpy(o, buf, n + 1);
        return o;
    }
}

/* str(v or fallback). */
static char *ec_str_or(const json_t *v, const char *fallback)
{
    char *s = ec_falsy(v) ? NULL : ec_text_of(v);
    if (!s) {
        size_t n = strlen(fallback);
        s = (char *)malloc(n + 1);
        if (s) memcpy(s, fallback, n + 1);
    }
    return s;
}

static void ec_strip(char *s)
{
    size_t n, a = 0;
    if (!s) return;
    while (s[a] && ec_ws(s[a])) a++;
    n = strlen(s);
    while (n > a && ec_ws(s[n - 1])) n--;
    if (a) memmove(s, s + a, n - a);
    s[n - a] = '\0';
}

char *w23_editcheck_cd6717_diag_key(const json_t *diag)
{
    char *sev, *code, *src, *msg, *key;
    int sline = 0, schar = 0, eline = 0, echar = 0;
    size_t need, klen;

    if (!diag || diag->type != JSON_OBJECT) return NULL;
    sev = ec_str_or(json_obj_get(diag, "severity"), "1");
    {
        json_t *c = json_obj_get(diag, "code");
        code = (c && c->type != JSON_NULL) ? ec_text_of(c) : NULL;
        if (!code) {
            code = (char *)malloc(1);
            if (code) code[0] = '\0';
        }
    }
    src = ec_str_or(json_obj_get(diag, "source"), "");
    msg = ec_str_or(json_obj_get(diag, "message"), "");
    ec_strip(msg);
    {
        json_t *rng = json_obj_get(diag, "range");
        json_t *start = (rng && rng->type == JSON_OBJECT) ? json_obj_get(rng, "start") : NULL;
        json_t *end = (rng && rng->type == JSON_OBJECT) ? json_obj_get(rng, "end") : NULL;
        if (start && start->type != JSON_OBJECT) start = NULL;
        if (end && end->type != JSON_OBJECT) end = NULL;
        sline = ec_int_field(start, "line", 0);
        schar = ec_int_field(start, "character", 0);
        eline = ec_int_field(end, "line", 0);
        echar = ec_int_field(end, "character", 0);
    }
    need = strlen(sev) + strlen(code) + strlen(src) + strlen(msg) + 96;
    key = (char *)malloc(need);
    if (key) {
        /* Python joins the five fields with a literal NUL byte; a C string
         * cannot carry one, so every side of the comparison uses 0x1f here
         * (a JSON string cannot hold that either, so no field can collide). */
        klen = (size_t)snprintf(key, need, "%s%c%s%c%s%c%s%c%d:%d-%d:%d",
                                sev, '\x1f', code, '\x1f', src, '\x1f', msg, '\x1f',
                                sline, schar, eline, echar);
        (void)klen;
    }
    free(sev);
    free(code);
    free(src);
    free(msg);
    return key;
}

json_t *w23_editcheck_cd6717_shift_baseline(const json_t *baseline, const char *pre_text,
                                            const char *post_text)
{
    json_t *out = json_array();
    ec_shift sh;
    size_t i, n;

    if (!out) return NULL;
    if (ec_build_shift(pre_text, post_text, &sh) != 0) return out;
    if (!baseline || baseline->type != JSON_ARRAY) return out;
    n = json_len(baseline);
    for (i = 0; i < n; i++) {
        json_t *d = json_get(baseline, i);
        json_t *moved;
        if (!d || d->type != JSON_OBJECT) continue;
        moved = ec_shift_range(d, &sh);
        if (moved) json_append(out, moved);
    }
    return out;
}

json_t *w23_editcheck_cd6717_introduced(const json_t *diags, const json_t *baseline,
                                        const char *pre_text, const char *post_text)
{
    json_t *out = json_array();
    json_t *shifted;
    size_t i, n, nb, j;
    char **seen = NULL;
    int nseen = 0;

    if (!out) return NULL;
    if (!pre_text || !post_text) {
        if (diags && diags->type == JSON_ARRAY) {
            n = json_len(diags);
            for (i = 0; i < n; i++) {
                json_t *d = json_get(diags, i);
                if (d) json_append(out, json_copy(d));
            }
        }
        return out;
    }
    shifted = w23_editcheck_cd6717_shift_baseline(baseline, pre_text, post_text);
    if (shifted) {
        nb = json_len(shifted);
        seen = (char **)calloc(nb ? nb : 1, sizeof(char *));
        for (i = 0; i < nb; i++) {
            char *k = w23_editcheck_cd6717_diag_key(json_get(shifted, i));
            if (k && seen) seen[nseen++] = k;
            else free(k);
        }
    }
    if (diags && diags->type == JSON_ARRAY) {
        n = json_len(diags);
        for (i = 0; i < n; i++) {
            json_t *d = json_get(diags, i);
            char *k = w23_editcheck_cd6717_diag_key(d);
            int hit = 0;
            if (!k) { if (d) json_append(out, json_copy(d)); continue; }
            for (j = 0; j < (size_t)nseen; j++)
                if (strcmp(seen[j], k) == 0) { hit = 1; break; }
            free(k);
            if (!hit && d) json_append(out, json_copy(d));
        }
    }
    for (i = 0; i < (size_t)nseen; i++) free(seen[i]);
    free(seen);
    json_free(shifted);
    return out;
}

char *w23_editcheck_cd6717_digest(const char *text)
{
    const unsigned char *p = (const unsigned char *)(text ? text : "");
    size_t n = strlen((const char *)p), i, o = 0;
    char *norm;
    char *hex;

    if (n >= 3 && p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF) { p += 3; n -= 3; }
    norm = (char *)malloc(n + 1);
    if (!norm) return NULL;
    for (i = 0; i < n; i++) {
        if (p[i] == '\r') {
            if (i + 1 < n && p[i + 1] == '\n') i++;
            norm[o++] = '\n';
        } else {
            norm[o++] = (char)p[i];
        }
    }
    norm[o] = '\0';
    hex = hash_sha256_hex((const unsigned char *)norm, o);
    free(norm);
    return hex;
}

json_t *w23_editcheck_cd6717_consts(void)
{
    json_t *o = json_object();
    json_set(o, "write_budget", json_number(W23_EDITCHECK_CD6717_WRITE_BUDGET));
    json_set(o, "inline_grace", json_number(W23_EDITCHECK_CD6717_INLINE_GRACE));
    json_set(o, "verdict_cap", json_number((double)W23_EDITCHECK_CD6717_VERDICT_CAP));
    return o;
}

double w23_editcheck_cd6717_inline_wait(const json_t *started, double now)
{
    double left;
    if (!started || started->type != JSON_NUMBER) return W23_EDITCHECK_CD6717_INLINE_GRACE;
    left = started->num_val + W23_EDITCHECK_CD6717_WRITE_BUDGET - now;
    if (left < 0.0 || left == 0.0) return 0.0;
    if (left > W23_EDITCHECK_CD6717_INLINE_GRACE) left = W23_EDITCHECK_CD6717_INLINE_GRACE;
    return left;
}

/* -- EditChecks: the bounded per-file verdict cache ---------------------- */

typedef struct {
    char *path;
    char *digest;
    json_t *diags;
} ec_entry;

typedef struct {
    /* CAP + 1: remember() appends first, then evicts the oldest, exactly like
     * the OrderedDict it mirrors. */
    ec_entry e[W23_EDITCHECK_CD6717_VERDICT_CAP + 1];
    int n;
} ec_cache;

static void ec_cache_drop(ec_cache *c, int at)
{
    int i;
    free(c->e[at].path);
    free(c->e[at].digest);
    json_free(c->e[at].diags);
    for (i = at; i + 1 < c->n; i++) c->e[i] = c->e[i + 1];
    c->n--;
}

/* remember(): pop-then-set keeps a re-remembered path youngest; the cap
 * evicts the oldest entry (OrderedDict.popitem(last=False)). */
static void ec_cache_remember(ec_cache *c, const char *path, const char *text, const json_t *diags)
{
    int i;
    ec_entry ent;

    if (!path) return;
    for (i = 0; i < c->n; i++)
        if (strcmp(c->e[i].path, path) == 0) { ec_cache_drop(c, i); break; }
    ent.path = (char *)malloc(strlen(path) + 1);
    if (!ent.path) return;
    memcpy(ent.path, path, strlen(path) + 1);
    ent.digest = w23_editcheck_cd6717_digest(text);
    ent.diags = diags ? json_copy(diags) : json_array();
    c->e[c->n++] = ent;
    while (c->n > W23_EDITCHECK_CD6717_VERDICT_CAP) ec_cache_drop(c, 0);
}

/* recall(): a hit only when the path's stored digest is this text's. */
static json_t *ec_cache_recall(ec_cache *c, const char *path, const char *text)
{
    char *d;
    int i;

    if (!path) return json_null();
    d = w23_editcheck_cd6717_digest(text);
    if (!d) return json_null();
    for (i = 0; i < c->n; i++) {
        if (strcmp(c->e[i].path, path) != 0) continue;
        if (strcmp(c->e[i].digest, d) == 0) { free(d); return json_copy(c->e[i].diags); }
        break;
    }
    free(d);
    return json_null();
}

json_t *w23_editcheck_cd6717_cache_ops(const json_t *ops)
{
    ec_cache *c = (ec_cache *)calloc(1, sizeof(ec_cache));
    json_t *results = json_array();
    json_t *out = json_object();
    size_t i, n;
    int k;

    if (!c || !results || !out) {
        free(c);
        json_free(results);
        json_free(out);
        return NULL;
    }
    n = (ops && ops->type == JSON_ARRAY) ? json_len(ops) : 0;
    for (i = 0; i < n; i++) {
        json_t *op = json_get(ops, i);
        const char *kind;
        const char *path;
        if (!op || op->type != JSON_OBJECT) { json_append(results, json_null()); continue; }
        kind = json_get_str(op, "op", "");
        path = json_get_str(op, "path", NULL);
        if (!strcmp(kind, "remember")) {
            ec_cache_remember(c, path, json_get_str(op, "text", ""), json_obj_get(op, "diags"));
            json_append(results, json_null());
        } else if (!strcmp(kind, "recall")) {
            json_append(results, ec_cache_recall(c, path, json_get_str(op, "text", "")));
        } else {
            json_append(results, json_null());
        }
    }
    json_set(out, "results", results);
    json_set(out, "size", json_number((double)c->n));
    for (k = 0; k < c->n; k++) {
        free(c->e[k].path);
        free(c->e[k].digest);
        json_free(c->e[k].diags);
    }
    free(c);
    return out;
}
