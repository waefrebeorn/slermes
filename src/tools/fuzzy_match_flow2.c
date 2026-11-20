/*
 * fuzzy_match_flow2.c -- FLOW-oracle port of the UNCLAIMED remainder of
 * tools/fuzzy_match.py (Slermes C11 parity port).
 *
 * PoP credits: strictly the 17 tools/fuzzy_match.py defs that had NO
 * pre-existing PoP annotation in src/ (the other 27 defs are claimed by
 * src/tools/fuzzy_match.c and fuzzy_match_helpers.c and are NOT re-claimed).
 * Each claim appears ONCE, as an inline annotation immediately above its
 * `static fm2_*` definition below; do not read this paragraph as a claim.
 *
 * All exported flow kernels are file-local `static fm2_*` (the harness
 * #includes this file directly; no port .o / build registration).
 *
 * FLOW model: pure unclaimed leaves are transcribed inline; claimed siblings
 * and callable collaborators are universal seams driven by in.io scripts:
 *   accept, transform, <strategy name>, format_match_locations,
 *   detect_escape_drift, maybe_unescape_new_string, apply_replacements,
 *   opcodes (difflib SequenceMatcher.get_opcodes),
 *   import_note_edit_match / note_edit_match.
 * Every seam records {ev,args,ret} in call order. A scripted
 * {"__raise__":cls,"msg":...} becomes a pending exception; callers propagate
 * it (no try/except) EXCEPT _note_edit_match's `except Exception` gate, which
 * uses the FULL builtin exception hierarchy below (BaseException trio passes
 * through). Fail-closed: an ABSENT seam script raises RuntimeError.
 *
 * DOMAIN (fixtures stay inside): byte offsets == code points (ASCII for every
 * function that slices content by span offset); str.strip() whitespace is the
 * ASCII set (space, \\t, \\n, \\r, \\f, \\v, 0x1c-0x1f) with no exotic Unicode
 * spaces in strip positions; non-empty needle for unicode replace; small
 * gutter line numbers; transform/opcodes scripts are well-shaped arrays.
 * Integral floats in str() positions are excluded (libjson has no int/float
 * identity, unlike Python).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes_json.h"

/* ── dynamic string ─────────────────────────────────────────────────────── */
typedef struct { char *s; size_t len, cap; } fm2_ds;
static void fm2_ds_free(fm2_ds *d) { free(d->s); d->s = NULL; d->len = d->cap = 0; }
static void fm2_ds_reserve(fm2_ds *d, size_t need) {
    if (d->cap >= need) return;
    size_t nc = d->cap ? d->cap * 2 : 64;
    while (nc < need) nc *= 2;
    d->s = realloc(d->s, nc);
    d->cap = nc;
}
static void fm2_ds_add(fm2_ds *d, const char *p, size_t n) {
    fm2_ds_reserve(d, d->len + n + 1);
    if (n) memcpy(d->s + d->len, p, n);
    d->len += n;
    d->s[d->len] = '\0';
}
static void fm2_ds_puts(fm2_ds *d, const char *s) { fm2_ds_add(d, s, strlen(s)); }
static void fm2_ds_ch(fm2_ds *d, char c) { fm2_ds_add(d, &c, 1); }
static void fm2_ds_num(fm2_ds *d, long n) {
    char b[48]; snprintf(b, sizeof(b), "%ld", n); fm2_ds_puts(d, b);
}
static char *fm2_ds_take(fm2_ds *d) {
    char *s = d->s ? d->s : strdup("");
    d->s = NULL; d->len = d->cap = 0;
    return s;
}

/* Python str.strip() whitespace (ASCII domain) and lstrip(" \t"). */
static bool fm2_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
           c == '\v' || (c >= 0x1c && c <= 0x1f);
}
static char *fm2_strip(const char *s) {
    if (!s) return strdup("");
    size_t a = 0, b = strlen(s);
    while (a < b && fm2_is_ws((unsigned char)s[a])) a++;
    while (b > a && fm2_is_ws((unsigned char)s[b - 1])) b--;
    char *r = malloc(b - a + 1);
    memcpy(r, s + a, b - a); r[b - a] = '\0';
    return r;
}
static char *fm2_lstrip_st(const char *s) {
    if (!s) return strdup("");
    size_t a = 0, b = strlen(s);
    while (a < b && (s[a] == ' ' || s[a] == '\t')) a++;
    char *r = malloc(b - a + 1);
    memcpy(r, s + a, b - a); r[b - a] = '\0';
    return r;
}
static bool fm2_all_ws(const char *s) {
    if (!s) return true;
    for (const char *p = s; *p; p++) if (!fm2_is_ws((unsigned char)*p)) return false;
    return true;
}
/* strip of only " \t\r" (used by _match_starts_mid_line) */
static bool fm2_nonblank_st(const char *s) {
    if (!s) return false;
    for (const char *p = s; *p; p++)
        if (*p != ' ' && *p != '\t' && *p != '\r') return true;
    return false;
}

/* split on '\n' (keeps empty segments), returns strdup'd segments */
typedef struct { char **v; size_t n, cap; } fm2_strs;
static void fm2_strs_init(fm2_strs *a) { a->v = NULL; a->n = a->cap = 0; }
static void fm2_strs_push(fm2_strs *a, const char *s, size_t n) {
    if (a->n == a->cap) {
        a->cap = a->cap ? a->cap * 2 : 8;
        a->v = realloc(a->v, a->cap * sizeof(char *));
    }
    a->v[a->n] = malloc(n + 1);
    memcpy(a->v[a->n], s, n); a->v[a->n][n] = '\0';
    a->n++;
}
static void fm2_strs_pushs(fm2_strs *a, const char *s) { fm2_strs_push(a, s, strlen(s)); }
static void fm2_strs_free(fm2_strs *a) {
    for (size_t i = 0; i < a->n; i++) free(a->v[i]);
    free(a->v); a->v = NULL; a->n = a->cap = 0;
}
static fm2_strs fm2_split_lines(const char *s) {
    fm2_strs a; fm2_strs_init(&a);
    if (!s) { fm2_strs_pushs(&a, ""); return a; }
    const char *p = s, *q;
    while ((q = strchr(p, '\n')) != NULL) {
        fm2_strs_push(&a, p, (size_t)(q - p));
        p = q + 1;
    }
    fm2_strs_pushs(&a, p);
    return a;
}
static char *fm2_join_lines(const fm2_strs *a) {
    fm2_ds d = {0};
    for (size_t i = 0; i < a->n; i++) {
        if (i) fm2_ds_ch(&d, '\n');
        fm2_ds_puts(&d, a->v[i]);
    }
    char *r = fm2_ds_take(&d); fm2_ds_free(&d); return r;
}
static json_t *fm2_strs_json(const fm2_strs *a) {
    json_t *r = json_array();
    for (size_t i = 0; i < a->n; i++) json_append(r, json_string(a->v[i]));
    return r;
}
static fm2_strs fm2_json_strs(const json_t *a) {
    fm2_strs r; fm2_strs_init(&r);
    if (a && a->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(a); i++) {
            const json_t *v = json_get(a, i);
            fm2_strs_pushs(&r, (v && v->type == JSON_STRING) ? v->str_val : "");
        }
    return r;
}
static bool fm2_strs_eq(const fm2_strs *a, const fm2_strs *b) {
    if (a->n != b->n) return false;
    for (size_t i = 0; i < a->n; i++) if (strcmp(a->v[i], b->v[i]) != 0) return false;
    return true;
}

/* substring count / contains (needles are non-empty literal ASCII/UTF-8) */
static long fm2_count(const char *s, const char *needle) {
    if (!s || !needle || !*needle) return 0;
    long n = 0; const char *p = s;
    while ((p = strstr(p, needle)) != NULL) { n++; p += strlen(needle); }
    return n;
}
static bool fm2_has(const char *s, const char *needle) {
    return s && needle && strstr(s, needle) != NULL;
}
/* Python str.replace(old,new) sequential replace */
static char *fm2_replace_all(const char *s, const char *old, const char *rep) {
    if (!s) return strdup("");
    if (!old || !*old) return strdup(s);
    fm2_ds d = {0};
    const char *p = s, *q;
    while ((q = strstr(p, old)) != NULL) {
        fm2_ds_add(&d, p, (size_t)(q - p));
        fm2_ds_puts(&d, rep);
        p = q + strlen(old);
    }
    fm2_ds_puts(&d, p);
    char *r = fm2_ds_take(&d); fm2_ds_free(&d); return r;
}

/* ── Unicode map (full UNICODE_MAP from fuzzy_match.py) ─────────────────── */
typedef struct { const char *u, *a; } fm2_uni;
static const fm2_uni FM2_UNI[] = {
    {"\xe2\x80\x9c", "\""}, {"\xe2\x80\x9d", "\""},
    {"\xe2\x80\x98", "'"},  {"\xe2\x80\x99", "'"},
    {"\xe2\x80\x94", "--"}, {"\xe2\x80\x93", "-"},
    {"\xe2\x80\xa6", "..."}, {"\xc2\xa0", " "},
    {"\xe2\x88\x92", "-"},
    {"\xe2\x80\x80", " "}, {"\xe2\x80\x81", " "}, {"\xe2\x80\x82", " "},
    {"\xe2\x80\x83", " "}, {"\xe2\x80\x84", " "}, {"\xe2\x80\x85", " "},
    {"\xe2\x80\x86", " "}, {"\xe2\x80\x87", " "}, {"\xe2\x80\x88", " "},
    {"\xe2\x80\x89", " "}, {"\xe2\x80\x8a", " "}, {"\xe2\x80\xaf", " "},
    {"\xe2\x81\x9f", " "}, {"\xe3\x80\x80", " "},
};
static const size_t FM2_UNI_N = sizeof(FM2_UNI) / sizeof(FM2_UNI[0]);
/* inline claimed pure leaf _unicode_normalize (no PoP re-claim) */
static char *fm2_unicode_normalize(const char *text) {
    char *r = strdup(text ? text : "");
    for (size_t i = 0; i < FM2_UNI_N; i++) {
        char *n = fm2_replace_all(r, FM2_UNI[i].u, FM2_UNI[i].a);
        free(r); r = n;
    }
    return r;
}

/* ── spans ──────────────────────────────────────────────────────────────── */
typedef struct { long a, b; } fm2_span;
typedef struct { fm2_span *v; size_t n, cap; } fm2_spans;
static void fm2_spans_init(fm2_spans *s) { s->v = NULL; s->n = s->cap = 0; }
static void fm2_spans_add(fm2_spans *s, long a, long b) {
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 8;
        s->v = realloc(s->v, s->cap * sizeof(fm2_span));
    }
    s->v[s->n].a = a; s->v[s->n].b = b; s->n++;
}
static json_t *fm2_spans_json(const fm2_spans *s) {
    json_t *r = json_array();
    for (size_t i = 0; i < s->n; i++) {
        json_t *p = json_array();
        json_append(p, json_number((double)s->v[i].a));
        json_append(p, json_number((double)s->v[i].b));
        json_append(r, p);
    }
    return r;
}
static fm2_spans fm2_json_spans(const json_t *a) {
    fm2_spans s; fm2_spans_init(&s);
    if (a && a->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(a); i++) {
            const json_t *p = json_get(a, i);
            if (p && p->type == JSON_ARRAY && json_len(p) >= 2) {
                const json_t *x = json_get(p, 0), *y = json_get(p, 1);
                fm2_spans_add(&s, x ? (long)x->num_val : 0, y ? (long)y->num_val : 0);
            }
        }
    return s;
}
static json_t *fm2_json_copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}

/* ── universal seam + context + exception hierarchy ────────────────────── */
typedef struct fm2_ctx {
    const json_t *in;
    json_t *trace;
    json_t *seqpos;
    json_t *exc;                 /* pending raise spec, or NULL */
    json_t *(*seam)(void *, const char *, const json_t *);
    void *seam_ud;
} fm2_ctx;

static bool fm2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool fm2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static void fm2_raise(fm2_ctx *c, const char *cls, const char *msg) {
    if (!c) return;
    if (c->exc) json_free(c->exc);
    c->exc = json_object();
    json_set(c->exc, "__raise__", json_string(cls));
    json_set(c->exc, "msg", json_string(msg ? msg : ""));
}
static void fm2_exc_clear(fm2_ctx *c) {
    if (c && c->exc) { json_free(c->exc); c->exc = NULL; }
}
static bool fm2_pending(fm2_ctx *c) { return c && c->exc != NULL; }
static const char *fm2_exc_class(fm2_ctx *c) {
    return json_get_str(c->exc, "__raise__", "");
}
/* seam call: records via ctx->seam, converts a raise ret into pending exc. */
static json_t *fm2_call(fm2_ctx *c, const char *name, const json_t *args) {
    json_t *r = c->seam(c->seam_ud, name, args);
    if (fm2_is_raise(r)) {
        const char *cls = json_get_str(r, "__raise__", "Exception");
        const char *msg = json_get_str(r, "msg", "");
        fm2_raise(c, cls, msg);
        json_free(r);
        return NULL;
    }
    return r; /* NULL pointer means a scripted JSON null (Python None) */
}
/* FULL builtin exception hierarchy: parent map, so except(Exception) matches
 * every Exception subclass but NOT the BaseException trio. */
static const char *fm2_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"},
        {"Exception", "BaseException"},
        {"BaseException", NULL},
        {"RuntimeError", "Exception"}, {"NotImplementedError", "RuntimeError"},
        {"RecursionError", "RuntimeError"},
        {"ValueError", "Exception"}, {"UnicodeError", "ValueError"},
        {"UnicodeDecodeError", "UnicodeError"}, {"UnicodeEncodeError", "UnicodeError"},
        {"UnicodeTranslateError", "UnicodeError"}, {"JSONDecodeError", "ValueError"},
        {"TypeError", "Exception"}, {"AttributeError", "Exception"},
        {"NameError", "Exception"}, {"UnboundLocalError", "NameError"},
        {"LookupError", "Exception"}, {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
        {"ArithmeticError", "Exception"}, {"FloatingPointError", "ArithmeticError"},
        {"ZeroDivisionError", "ArithmeticError"}, {"OverflowError", "ArithmeticError"},
        {"AssertionError", "Exception"}, {"SyntaxError", "Exception"},
        {"IndentationError", "SyntaxError"}, {"TabError", "IndentationError"},
        {"SystemError", "Exception"}, {"ReferenceError", "Exception"},
        {"MemoryError", "Exception"}, {"BufferError", "Exception"},
        {"EOFError", "Exception"}, {"ImportError", "Exception"},
        {"ModuleNotFoundError", "ImportError"},
        {"StopIteration", "Exception"}, {"StopAsyncIteration", "Exception"},
        {"OSError", "Exception"}, {"FileNotFoundError", "OSError"},
        {"PermissionError", "OSError"}, {"IsADirectoryError", "OSError"},
        {"NotADirectoryError", "OSError"}, {"FileExistsError", "OSError"},
        {"InterruptedError", "OSError"}, {"BlockingIOError", "OSError"},
        {"ChildProcessError", "OSError"}, {"ProcessLookupError", "OSError"},
        {"ConnectionError", "OSError"}, {"BrokenPipeError", "ConnectionError"},
        {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"},
        {"ConnectionResetError", "ConnectionError"}, {"TimeoutError", "OSError"},
        {"Warning", "Exception"}, {"UserWarning", "Warning"},
        {"DeprecationWarning", "Warning"}, {"FutureWarning", "Warning"},
        {"RuntimeWarning", "Warning"}, {"SyntaxWarning", "Warning"},
        {"ResourceWarning", "Warning"}, {"ImportWarning", "Warning"},
        {"UnicodeWarning", "Warning"}, {"BytesWarning", "Warning"},
        {"EncodingWarning", "Warning"}, {"PendingDeprecationWarning", "Warning"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (strcmp(T[i].c, c) == 0) return T[i].p;
    return NULL;
}
static bool fm2_exc_matches(fm2_ctx *c, const char *const *cls, size_t n) {
    if (!fm2_pending(c)) return false;
    const char *cur = fm2_exc_class(c);
    for (int depth = 0; depth < 64; depth++) {
        for (size_t i = 0; i < n; i++) if (strcmp(cur, cls[i]) == 0) return true;
        const char *p = fm2_exc_parent(cur);
        if (!p) return false;
        cur = p;
    }
    return false;
}
static bool fm2_exc_is_exception(fm2_ctx *c) {
    static const char *E[] = {"Exception"};
    return fm2_exc_matches(c, E, 1);
}

/* small arg builders */
static json_t *fm2_args(void) { return json_object(); }
static json_t *fm2_arg(json_t *o, const char *k, json_t *v) { json_set(o, k, v); return o; }
static json_t *fm2_argn(json_t *o, const char *k, long v) { return fm2_arg(o, k, json_number((double)v)); }

/* ── inline claimed pure leaf _calculate_line_positions ─────────────────── */
static fm2_span fm2_calc_positions(const fm2_strs *lines, long start, long end, long content_len) {
    long sp = 0, ep = 0;
    for (long i = 0; i < start && (size_t)i < lines->n; i++)
        sp += (long)strlen(lines->v[i]) + 1;
    for (long i = 0; i < end && (size_t)i < lines->n; i++)
        ep += (long)strlen(lines->v[i]) + 1;
    ep -= 1;
    fm2_span r = { sp, content_len < ep ? content_len : ep };
    return r;
}

/* ── accept callback type for window_spans ─────────────────────────────── */
typedef bool (*fm2_accept_fn)(fm2_ctx *, long, void *);

/* PoP: fm2_window_spans @ tools/fuzzy_match.py:_window_spans */
static json_t *fm2_window_spans(fm2_ctx *c, const char *content,
                                const fm2_strs *lines, long n,
                                fm2_accept_fn accept, void *ud) {
    fm2_spans out; fm2_spans_init(&out);
    long total = (long)lines->n;
    long content_len = (long)strlen(content ? content : "");
    for (long i = 0; i <= total - n; i++) {
        bool ok = accept(c, i, ud);
        if (fm2_pending(c)) { free(out.v); return NULL; }
        if (ok) {
            fm2_span sp = fm2_calc_positions(lines, i, i + n, content_len);
            fm2_spans_add(&out, sp.a, sp.b);
        }
    }
    json_t *r = fm2_spans_json(&out);
    free(out.v);
    return r;
}

/* direct _window_spans accept = seam("accept", {"i": i}) */
static bool fm2_accept_seam(fm2_ctx *c, long i, void *ud) {
    (void)ud;
    json_t *a = fm2_argn(json_object(), "i", i);
    json_t *v = fm2_call(c, "accept", a);
    if (fm2_pending(c)) return false;
    bool t = fm2_truthy(v);
    if (v) json_free(v);
    return t;
}

/* PoP: fm2_match_transformed_lines @ tools/fuzzy_match.py:_match_transformed_lines */
/* ud = { pattern_norm (fm2_strs*), content lines (fm2_strs*) } */
typedef struct { const fm2_strs *pat_norm; const fm2_strs *lines; } fm2_mtl_ud;
static bool fm2_accept_transform(fm2_ctx *c, long i, void *ud) {
    fm2_mtl_ud *u = (fm2_mtl_ud *)ud;
    long n = (long)u->pat_norm->n;
    fm2_strs block; fm2_strs_init(&block);
    for (long k = 0; k < n && (size_t)(i + k) < u->lines->n; k++)
        fm2_strs_pushs(&block, u->lines->v[i + k]);
    json_t *a = fm2_arg(json_object(), "lines", fm2_strs_json(&block));
    fm2_strs_free(&block);
    json_t *v = fm2_call(c, "transform", a);
    if (fm2_pending(c)) return false;
    fm2_strs got = fm2_json_strs(v);
    bool eq = fm2_strs_eq(&got, u->pat_norm);
    if (v) json_free(v);
    fm2_strs_free(&got);
    return eq;
}
static json_t *fm2_match_transformed_lines(fm2_ctx *c, const char *content, const char *pattern) {
    fm2_strs lines = fm2_split_lines(content);
    fm2_strs plines = fm2_split_lines(pattern);
    json_t *a = fm2_arg(json_object(), "lines", fm2_strs_json(&plines));
    json_t *tv = fm2_call(c, "transform", a);
    if (fm2_pending(c)) { fm2_strs_free(&lines); fm2_strs_free(&plines); return NULL; }
    fm2_strs pat_norm = fm2_json_strs(tv);
    if (tv) json_free(tv);
    long n = (long)pat_norm.n;
    fm2_mtl_ud ud = { &pat_norm, &lines };
    json_t *r = fm2_window_spans(c, content, &lines, n, fm2_accept_transform, &ud);
    fm2_strs_free(&pat_norm);
    fm2_strs_free(&plines);
    fm2_strs_free(&lines);
    return r;
}

/* PoP: fm2_strip_boundary @ tools/fuzzy_match.py:_strip_boundary */
static json_t *fm2_strip_boundary(fm2_ctx *c, const json_t *in) {
    if (!in || in->type != JSON_ARRAY || json_len(in) == 0) {
        fm2_raise(c, "IndexError", "list index out of range");
        return NULL;
    }
    json_t *r = json_array();
    for (size_t i = 0; i < json_len(in); i++) {
        const json_t *v = json_get(in, i);
        const char *s = (v && v->type == JSON_STRING) ? v->str_val : "";
        if (i == 0 || i + 1 == json_len(in)) {
            char *t = fm2_strip(s);
            json_append(r, json_string(t));
            free(t);
        } else {
            json_append(r, json_copy(v ? v : json_null()));
        }
    }
    return r;
}

/* PoP: fm2_invert_norm_map @ tools/fuzzy_match.py:_invert_norm_map */
static json_t *fm2_invert_norm_map(const json_t *orig) {
    json_t *r = json_object();
    if (!orig || orig->type != JSON_ARRAY) return r;
    size_t n = json_len(orig);
    if (n > 0) n--; /* orig_to_norm[:-1] */
    for (size_t i = 0; i < n; i++) {
        const json_t *v = json_get(orig, i);
        char key[48];
        snprintf(key, sizeof(key), "%.0f", v ? v->num_val : 0.0);
        if (!json_has(r, key)) json_set(r, key, json_number((double)i));
    }
    return r;
}

/* PoP: fm2_norm_end_to_orig @ tools/fuzzy_match.py:_norm_end_to_orig */
static long fm2_norm_end_to_orig(const json_t *orig, long orig_start, long norm_end) {
    long orig_len = orig && orig->type == JSON_ARRAY ? (long)json_len(orig) - 1 : -1;
    long e = orig_start;
    while (e < orig_len) {
        const json_t *v = json_get(orig, (size_t)e);
        double nv = v ? v->num_val : 0.0;
        if (!(nv < (double)norm_end)) break;
        e++;
    }
    return e;
}

/* PoP: fm2_boundary_note @ tools/fuzzy_match.py:boundary_note */
static json_t *fm2_boundary_note(const json_t *strategies,
                                 const char *target, const char *where) {
    static const char *SIM[] = {"block_anchor", "context_aware"};
    fm2_strs used; fm2_strs_init(&used);
    if (strategies && strategies->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(strategies); i++) {
            const json_t *v = json_get(strategies, i);
            if (!v || v->type != JSON_STRING) continue;
            bool is_sim = false;
            for (size_t k = 0; k < 2; k++) if (strcmp(v->str_val, SIM[k]) == 0) { is_sim = true; break; }
            if (!is_sim) continue;
            bool seen = false;
            for (size_t k = 0; k < used.n; k++) if (strcmp(used.v[k], v->str_val) == 0) { seen = true; break; }
            if (!seen) fm2_strs_pushs(&used, v->str_val);
        }
    if (used.n == 0) { fm2_strs_free(&used); return json_null(); }
    fm2_ds d = {0};
    fm2_ds_puts(&d, "Non-exact match (strategy: ");
    for (size_t i = 0; i < used.n; i++) {
        if (i) fm2_ds_puts(&d, ", ");
        fm2_ds_puts(&d, used.v[i]);
    }
    fm2_ds_puts(&d, ")");
    fm2_ds_puts(&d, where ? where : "");
    fm2_ds_puts(&d, ". The edit span was located approximately, not matched to ");
    fm2_ds_puts(&d, target ? target : "old_string's");
    fm2_ds_puts(&d, " literal boundaries \xe2\x80\x94 review the diff below to confirm the change "
                    "landed exactly where intended (a shifted boundary can duplicate or drop a "
                    "line just outside the edited region).");
    fm2_strs_free(&used);
    char *s = fm2_ds_take(&d); fm2_ds_free(&d);
    json_t *r = json_string(s); free(s);
    return r;
}

/* PoP: fm2_matched_regions @ tools/fuzzy_match.py:_matched_regions */
static char *fm2_matched_regions(const char *content, const fm2_spans *m) {
    fm2_ds d = {0};
    size_t L = strlen(content ? content : "");
    for (size_t i = 0; i < m->n; i++) {
        long a = m->v[i].a, b = m->v[i].b;
        if (a < 0) a = 0; if (b > (long)L) b = (long)L;
        if (a < b) fm2_ds_add(&d, content + a, (size_t)(b - a));
    }
    char *r = fm2_ds_take(&d); fm2_ds_free(&d); return r;
}

/* forward decls for inline orchestration helpers */
static json_t *fm2_run_strategy_chain(fm2_ctx *c, const char *content,
        const char *old_string, const char *new_string, bool replace_all,
        const json_t *strategies);
static void fm2_note_edit_match(fm2_ctx *c, const char *strategy, const char *miss);
static json_t *fm2_unicode_edit_plan(fm2_ctx *c, const char *old_string, const char *new_string);

static bool fm2_sim_name(const char *s) {
    return s && (strcmp(s, "block_anchor") == 0 || strcmp(s, "context_aware") == 0);
}

/* PoP: fm2_run_strategy_chain @ tools/fuzzy_match.py:_run_strategy_chain */
static json_t *fm2_run_strategy_chain(fm2_ctx *c, const char *content,
        const char *old_string, const char *new_string, bool replace_all,
        const json_t *strategies) {
    if (!strategies || strategies->type != JSON_ARRAY) return NULL; /* None */
    for (size_t si = 0; si < json_len(strategies); si++) {
        const json_t *nv = json_get(strategies, si);
        const char *name = (nv && nv->type == JSON_STRING) ? nv->str_val : "";
        json_t *a = fm2_arg(fm2_arg(json_object(), "content", json_string(content ? content : "")),
                            "old_string", json_string(old_string ? old_string : ""));
        json_t *mv = fm2_call(c, name, a);
        if (fm2_pending(c)) return NULL;
        fm2_spans matches = fm2_json_spans(mv);
        if (mv) json_free(mv);
        if (matches.n == 0) { free(matches.v); continue; }

        if (matches.n > 1 && !replace_all) {
            fm2_note_edit_match(c, NULL, "ambiguous");
            if (fm2_pending(c)) { free(matches.v); return NULL; }
            json_t *la = fm2_argn(fm2_arg(fm2_arg(json_object(), "content", json_string(content)),
                                 "matches", fm2_spans_json(&matches)), "cap", 5);
            json_t *lv = fm2_call(c, "format_match_locations", la);
            if (fm2_pending(c)) { free(matches.v); return NULL; }
            const char *loc = (lv && lv->type == JSON_STRING) ? lv->str_val : "";
            fm2_ds d = {0};
            fm2_ds_num(&d, (long)matches.n);
            char *s = fm2_ds_take(&d); fm2_ds_free(&d);
            char buf[512];
            snprintf(buf, sizeof(buf),
                "Found %s matches for old_string. Provide more context to make it unique, "
                "or use replace_all=True. Matches:\n%s", s, loc);
            free(s); if (lv) json_free(lv); free(matches.v);
            json_t *r = json_array();
            json_append(r, json_string(content ? content : ""));
            json_append(r, json_number(0));
            json_append(r, json_null());
            json_append(r, json_string(buf));
            return r;
        }
        if (replace_all && matches.n > 1 && fm2_sim_name(name)) {
            fm2_note_edit_match(c, NULL, "ambiguous");
            if (fm2_pending(c)) { free(matches.v); return NULL; }
            char buf[512];
            snprintf(buf, sizeof(buf),
                "Found %lu approximate matches via the '%s' strategy; replace_all only applies "
                "to exact matches. Provide the precise text (whitespace included) so an "
                "exact/line-trimmed match can be made.", (unsigned long)matches.n, name);
            free(matches.v);
            json_t *r = json_array();
            json_append(r, json_string(content ? content : ""));
            json_append(r, json_number(0));
            json_append(r, json_null());
            json_append(r, json_string(buf));
            return r;
        }
        if (strcmp(name, "exact") != 0) {
            json_t *da = fm2_arg(fm2_arg(fm2_arg(json_object(), "content", json_string(content)),
                             "matches", fm2_spans_json(&matches)), "old_string",
                             json_string(old_string ? old_string : ""));
            json_set(da, "new_string", json_string(new_string ? new_string : ""));
            json_t *dv = fm2_call(c, "detect_escape_drift", da);
            if (fm2_pending(c)) { free(matches.v); return NULL; }
            if (dv && dv->type == JSON_STRING && dv->str_val[0] != '\0') {
                char *msg = strdup(dv->str_val);
                free(matches.v); if (dv) json_free(dv);
                json_t *r = json_array();
                json_append(r, json_string(content ? content : ""));
                json_append(r, json_number(0)); json_append(r, json_null());
                json_append(r, json_string(msg));
                free(msg);
                return r;
            }
            if (dv) json_free(dv);
        }
        json_t *ua = fm2_arg(fm2_arg(fm2_arg(json_object(), "new_string",
                             json_string(new_string ? new_string : "")), "content",
                             json_string(content ? content : "")), "matches", fm2_spans_json(&matches));
        json_t *uv = fm2_call(c, "maybe_unescape_new_string", ua);
        if (fm2_pending(c)) { free(matches.v); return NULL; }
        const char *effective = (uv && uv->type == JSON_STRING) ? uv->str_val : "";
        char *eff_copy = strdup(effective);
        if (uv) json_free(uv);

        json_t *plan = NULL;
        if (strcmp(name, "unicode_normalized") == 0) {
            plan = fm2_unicode_edit_plan(c, old_string, eff_copy);
            if (fm2_pending(c)) { free(eff_copy); free(matches.v); return NULL; }
        }
        json_t *aa = fm2_arg(fm2_arg(fm2_arg(json_object(), "content", json_string(content)),
                             "matches", fm2_spans_json(&matches)), "new_string", json_string(eff_copy));
        json_set(aa, "old_string", strcmp(name, "exact") == 0 ? json_null()
                                                               : json_string(old_string ? old_string : ""));
        json_set(aa, "unicode_plan", plan ? plan : json_null());
        json_t *av = fm2_call(c, "apply_replacements", aa);
        if (fm2_pending(c)) { free(eff_copy); free(matches.v); return NULL; }
        const char *new_content = (av && av->type == JSON_STRING) ? av->str_val : "";
        char *nc_copy = strdup(new_content);
        if (av) json_free(av);
        fm2_note_edit_match(c, name, NULL);
        if (fm2_pending(c)) { free(nc_copy); free(eff_copy); free(matches.v); return NULL; }
        long n_match = (long)matches.n;
        free(eff_copy); free(matches.v);
        json_t *r = json_array();
        json_append(r, json_string(nc_copy));   /* nc_copy owns the bytes (av was freed) */
        free(nc_copy);
        json_append(r, json_number((double)n_match));
        json_append(r, json_string(name));
        json_append(r, json_null());
        return r;
    }
    return NULL;
}

/* ── gutter regex ^(\d+)\| at a line start ─────────────────────────────── */
/* returns true and sets *num / *end when line starts with digits+'|' */
static bool fm2_gutter_at(const char *line, long *num, size_t *end) {
    size_t i = 0;
    while (line[i] >= '0' && line[i] <= '9') i++;
    if (i == 0 || line[i] != '|') return false;
    char *tmp = malloc(i + 1);
    memcpy(tmp, line, i); tmp[i] = '\0';
    *num = atol(tmp); free(tmp);
    *end = i + 1;
    return true;
}
static bool fm2_gutter_search(const char *text) {
    fm2_strs lines = fm2_split_lines(text);
    bool found = false;
    for (size_t i = 0; i < lines.n; i++) {
        long num; size_t e;
        if (fm2_gutter_at(lines.v[i], &num, &e)) { found = true; break; }
    }
    fm2_strs_free(&lines);
    return found;
}

/* PoP: fm2_strip_line_number_gutter @ tools/fuzzy_match.py:_strip_line_number_gutter */
static char *fm2_strip_line_number_gutter(const char *text, long min_numbered) {
    fm2_strs lines = fm2_split_lines(text);
    fm2_strs out; fm2_strs_init(&out);
    bool has_expected = false;
    long expected = 0, numbered = 0;
    for (size_t i = 0; i < lines.n; i++) {
        long num; size_t e;
        char *line = lines.v[i];
        bool m = fm2_gutter_at(line, &num, &e);
        if (!m && fm2_all_ws(line) == false) {
            fm2_strs_free(&lines); fm2_strs_free(&out); return NULL;
        }
        bool has_expected_now = has_expected;
        if (m) {
            if (has_expected && num != expected) {
                fm2_strs_free(&lines); fm2_strs_free(&out); return NULL;
            }
            expected = num; numbered++;
            has_expected = true;  /* Python sets expected before the += 1 */
            fm2_strs_push(&out, line + e, strlen(line + e));
        } else {
            fm2_strs_pushs(&out, line);
        }
        if (has_expected) expected++;
        (void)has_expected_now;
    }
    char *r = NULL;
    if (numbered >= min_numbered) r = fm2_join_lines(&out);
    fm2_strs_free(&lines); fm2_strs_free(&out);
    return r;
}

/* PoP: fm2_gutter_stripped_old @ tools/fuzzy_match.py:_gutter_stripped_old */
static json_t *fm2_gutter_stripped_old(const char *content, const char *old_string) {
    char *stripped_old = fm2_strip_line_number_gutter(old_string, 1);
    if (!stripped_old || fm2_all_ws(stripped_old) || fm2_gutter_search(content)) {
        free(stripped_old); return NULL;
    }
    json_t *r = json_array();  /* Python returns a 2-tuple (stripped_old, strategies) */
    json_append(r, json_string(stripped_old));
    free(stripped_old);
    json_t *strats = json_array();
    char *two = fm2_strip_line_number_gutter(old_string, 2);
    if (two == NULL) {
        json_append(strats, json_string("line_trimmed"));
    } else {
        static const char *PREC[] = {"exact", "line_trimmed", "whitespace_normalized",
            "indentation_flexible", "escape_normalized", "trimmed_boundary", "unicode_normalized"};
        for (size_t i = 0; i < sizeof(PREC) / sizeof(PREC[0]); i++)
            json_append(strats, json_string(PREC[i]));
    }
    free(two);
    json_append(r, strats);
    return r;
}

/* PoP: fm2_gutter_recovery @ tools/fuzzy_match.py:_gutter_recovery */
static json_t *fm2_gutter_recovery(fm2_ctx *c, const char *content, const char *stripped_old,
        const json_t *strategies, const char *new_string, bool replace_all,
        bool has_notes, json_t **notes_io) {
    char *stripped_new = fm2_strip_line_number_gutter(new_string, 1);
    if (stripped_new == NULL) {
        if (fm2_gutter_search(new_string)) {
            fm2_note_edit_match(c, NULL, "no_match");
            if (fm2_pending(c)) return NULL;
            json_t *r = json_array();
            json_append(r, json_string(content));
            json_append(r, json_number(0)); json_append(r, json_null());
            json_append(r, json_string("old_string looks like read_file output with its `N|` "
                "line-number prefixes, but new_string's `N|` prefixes are not a clean read_file "
                "gutter (missing on some non-blank lines, or not numbered consecutively), so the "
                "intended replacement text is ambiguous. Resend both strings without any `N|` "
                "prefixes."));
            return r;
        }
        stripped_new = strdup(new_string);
    }
    if (strcmp(stripped_old, stripped_new) == 0) {
        free(stripped_new);
        json_t *r = json_array();
        json_append(r, json_string(content));
        json_append(r, json_number(0)); json_append(r, json_null());
        json_append(r, json_string("No edit was applied because old_string and new_string are "
            "identical. Provide the existing text to replace in old_string and the changed "
            "replacement text in new_string."));
        return r;
    }
    json_t *result = fm2_run_strategy_chain(c, content, stripped_old, stripped_new,
                                            replace_all, strategies);
    if (fm2_pending(c)) { free(stripped_new); return NULL; }
    if (has_notes && notes_io && result && result->type == JSON_ARRAY && json_len(result) >= 4) {
        const json_t *err = json_get(result, 3);
        if (!err || err->type == JSON_NULL) {
            json_append(*notes_io, json_string("old_string carried read_file's line-number "
                "prefixes (`N|`); they were stripped before matching. Send file text without the "
                "`N|` gutter next time."));
        }
    }
    free(stripped_new);
    return result;
}

/* PoP: fm2_note_edit_match @ tools/fuzzy_match.py:_note_edit_match */
static void fm2_note_edit_match(fm2_ctx *c, const char *strategy, const char *miss) {
    json_t *v = fm2_call(c, "import_note_edit_match", fm2_args());
    if (fm2_pending(c)) {
        /* `except Exception`: swallow every Exception family; BaseException passes */
        if (fm2_exc_is_exception(c)) fm2_exc_clear(c);
        (void)v;
        return;
    }
    if (v) json_free(v);
    json_t *a = fm2_args();
    json_set(a, "strategy", strategy ? json_string(strategy) : json_null());
    json_set(a, "miss", miss ? json_string(miss) : json_null());
    json_t *r = fm2_call(c, "note_edit_match", a);
    if (r) json_free(r);
}

/* PoP: fm2_detect_newline_literal_drift @ tools/fuzzy_match.py:_detect_newline_literal_drift */
static const char *FM2_NL_DRIFT =
    "Escape-drift detected: old_string contains more literal '\\\\n' sequences than the matched "
    "region of the file, which has real line breaks there instead. This is almost always a "
    "tool-call serialization artifact where a line break got escaped one extra time; new_string "
    "would write it as backslash + n. Re-read the file with read_file and pass "
    "old_string/new_string with actual line breaks, keeping only the '\\\\n' sequences that "
    "appear literally in the file.";
static json_t *fm2_detect_newline_literal_drift(const char *content, const fm2_spans *m,
        const char *old_string, const char *new_string) {
    if (!fm2_has(new_string, "\\n")) return json_null();
    long old_literals = fm2_count(old_string, "\\n");
    size_t L = strlen(content ? content : "");
    for (size_t i = 0; i < m->n; i++) {
        long a = m->v[i].a, b = m->v[i].b;
        if (a < 0) a = 0; if (b > (long)L) b = (long)L;
        if (a >= b) continue;
        char *region = malloc((size_t)(b - a) + 1);
        memcpy(region, content + a, (size_t)(b - a)); region[b - a] = '\0';
        bool has_real_nl = strchr(region, '\n') != NULL;
        long cnt = fm2_count(region, "\\n");
        free(region);
        if (has_real_nl && old_literals > cnt) return json_string(FM2_NL_DRIFT);
    }
    return json_null();
}

/* PoP: fm2_backslash_runs @ tools/fuzzy_match.py:_backslash_runs */
static json_t *fm2_backslash_runs(const char *s) {
    json_t *r = json_array();
    if (!s) return r;
    size_t i = 0, L = strlen(s);
    while (i < L) {
        if (s[i] == '\\') {
            size_t j = i; while (j < L && s[j] == '\\') j++;
            json_append(r, json_number((double)(j - i)));
            i = j;
        } else i++;
    }
    return r;
}
static bool fm2_runs_eq(const json_t *a, const json_t *b) {
    if (!a || !b || a->type != JSON_ARRAY || b->type != JSON_ARRAY) return false;
    if (json_len(a) != json_len(b)) return false;
    for (size_t i = 0; i < json_len(a); i++) {
        const json_t *x = json_get(a, i), *y = json_get(b, i);
        if ((x ? x->num_val : 0) != (y ? y->num_val : 0)) return false;
    }
    return true;
}

/* PoP: fm2_detect_backslash_doubling @ tools/fuzzy_match.py:_detect_backslash_doubling */
static const char *FM2_BS_DRIFT =
    "Escape-drift detected: every backslash run in old_string is exactly twice as long as in the "
    "matched region of the file (e.g. the file has `\\\\` where old_string has `\\\\\\\\`). The "
    "tool-call arguments were JSON-escaped one extra time; applying new_string verbatim would "
    "double every backslash in the file. Re-read the file with read_file and resend "
    "old_string/new_string with the backslash counts exactly as they appear in the file.";
static json_t *fm2_detect_backslash_doubling(const char *regions,
        const char *old_string, const char *new_string) {
    json_t *old_runs = fm2_backslash_runs(old_string);
    json_t *file_runs = fm2_backslash_runs(regions);
    json_t *new_runs = fm2_backslash_runs(new_string);
    bool ok = true;
    size_t no = json_len(old_runs), nf = json_len(file_runs);
    if (no == 0 || nf == 0 || no != nf || fm2_runs_eq(old_runs, file_runs) ||
        fm2_runs_eq(new_runs, file_runs)) ok = false;
    if (ok) {
        bool signal = false;
        for (size_t i = 0; i < nf; i++) {
            const json_t *f = json_get(file_runs, i), *o = json_get(old_runs, i);
            double fv = f ? f->num_val : 0;
            if (fv >= 2) signal = true;
            if ((o ? o->num_val : 0) != fv * 2) { ok = false; break; }
        }
        if (!signal && nf < 2) ok = false;
    }
    json_free(old_runs); json_free(file_runs); json_free(new_runs);
    return ok ? json_string(FM2_BS_DRIFT) : json_null();
}

/* PoP: fm2_match_starts_mid_line @ tools/fuzzy_match.py:_match_starts_mid_line */
static bool fm2_match_starts_mid_line(const char *content, long start, long end) {
    size_t L = strlen(content ? content : "");
    if (start < 0) start = 0; if (end > (long)L) end = (long)L;
    if (start > end) return false;
    char *region = malloc((size_t)(end - start) + 1);
    memcpy(region, content + start, (size_t)(end - start)); region[end - start] = '\0';
    fm2_strs lines = fm2_split_lines(region);
    long first_start = start;
    bool result = false;
    for (size_t i = 0; i < lines.n; i++) {
        if (fm2_all_ws(lines.v[i]) == false) {
            /* content.rfind('\n', 0, first_start) + 1 */
            long ls = 0;
            for (long k = first_start - 1; k >= 0; k--) {
                if (content[k] == '\n') { ls = k + 1; break; }
            }
            size_t seg = (size_t)(first_start - ls);
            char *pre = malloc(seg + 1);
            memcpy(pre, content + ls, seg); pre[seg] = '\0';
            result = fm2_nonblank_st(pre);
            free(pre);
            break;
        }
        first_start += (long)strlen(lines.v[i]) + 1;
    }
    fm2_strs_free(&lines);
    free(region);
    return result;
}

/* PoP: fm2_unicode_edit_plan @ tools/fuzzy_match.py:_unicode_edit_plan */
static json_t *fm2_unicode_edit_plan(fm2_ctx *c, const char *old_string, const char *new_string) {
    char *norm = fm2_unicode_normalize(old_string);
    json_t *a = fm2_arg(fm2_arg(json_object(), "a", json_string(norm)),
                        "b", json_string(new_string ? new_string : ""));
    free(norm);
    json_t *op = fm2_call(c, "opcodes", a);
    if (fm2_pending(c)) return NULL;
    json_t *r = json_array();
    /* norm_old recomputed for the returned tuple */
    char *norm2 = fm2_unicode_normalize(old_string);
    json_append(r, json_string(norm2));
    free(norm2);
    json_append(r, op ? op : json_null());
    return r;
}
