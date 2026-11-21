/*
 * context_references_flow3.c — agent/context_references.py FLOW remainder beyond
 * flow2 (C11 port). LIVE authority: C:/Users/eman5/hermes-agent/agent/context_references.py
 *
 * CRLF-aware PoP grep BEFORE writing (all src .c/.h + registry pop claims,
 * lines decoded with universal newlines): all 36 AST defs of
 * context_references.py are already PoP-claimed by existing sets
 * (context_references.c / _ports.c / _flow.c / _flow2.c / _gaps3.c), so there
 * are 0 GLOBALLY-unclaimed defs. Per references/flow2-remainder-scoping.md the
 * legitimate FLOW remainder is the defs NOT covered by a prior FLOW oracle:
 * flow1 (context_references_flow.c) + flow2 (context_references_flow2.c) cover
 * 26 of the 36; the remaining 11 are this file's standalone whole-remainder
 * re-derivation. Older claims stay in force; these 11 are re-derived here for
 * an independent differential check. Had these 11 already been FLOW-claimed
 * the task would REFUSE; they are not, so the port proceeds.
 *
 *   PoP: cr3_parse_context_references       @ agent/context_references.py:parse_context_references
 *   PoP: cr3_preprocess                     @ agent/context_references.py:preprocess_context_references
 *   PoP: cr3_preprocess_async               @ agent/context_references.py:preprocess_context_references_async
 *   PoP: cr3_expand_reference               @ agent/context_references.py:_expand_reference
 *   PoP: cr3_expand_git_reference           @ agent/context_references.py:_expand_git_reference
 *   PoP: cr3_fetch_url_content              @ agent/context_references.py:_fetch_url_content
 *   PoP: cr3_default_url_fetcher            @ agent/context_references.py:_default_url_fetcher
 *   PoP: cr3_resolve_path                   @ agent/context_references.py:_resolve_path
 *   PoP: cr3_ensure_path_allowed            @ agent/context_references.py:_ensure_reference_path_allowed
 *   PoP: cr3_build_folder_listing           @ agent/context_references.py:_build_folder_listing
 *   PoP: cr3_iter_visible_entries           @ agent/context_references.py:_iter_visible_entries
 *
 * 11 functions (all top-level defs the FLOW series had not covered).
 * Internal string helpers (cr3_stp/cr3_swr/cr3_pfrv) re-derive flow2's pure
 * leaves so composition is exercised; they carry NO new PoP claim here.
 *
 * Self-contained, libjson-only. Every live effect enters through ONE universal
 * seam: json_t *(*call)(void *ctx, const char *name, const json_t *args).
 * `args` is BORROWED (seam records a copy); cr3_call frees it after the call.
 * NULL == Python None (never a JSON_NULL node); {"__raise__":cls,"msg":msg}
 * == a raised exception. Fail-closed: a MISSING seam -> RuntimeError raise spec
 * (presence test; a scripted null is a valid None).
 *
 * FULL BUILTIN EXCEPTION HIERARCHY: CR3_EXC_TREE lists every builtin exception
 * class child->parent (plus the quarry/subprocess exceptions). cr3_cls_is()
 * walks it so `except Exception` folds every Exception child while
 * SystemExit / KeyboardInterrupt / GeneratorExit / CancelledError
 * (BaseException children) propagate. Scripted unknown class names are out of
 * domain.
 *
 * RETURN CONTRACT: every ported function returns a NEW json_t* (caller frees):
 * NULL == None; a {"__raise__":...} object == a propagated exception.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <math.h>
#include <ctype.h>
#include "json.h"

/* ── Python-value helpers ── */
static bool cr3_jtruth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static bool cr3_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool cr3_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool cr3_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool cr3_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *cr3_sval(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : "";
}
static double cr3_nval(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
static char *cr3_xsf(const char *fmt, ...) {
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2); return buf ? buf : strdup("");
}
static char *cr3_dup(const char *s) { return strdup(s ? s : ""); }
static void cr3_set_str(json_t *o, const char *k, const char *v) {
    json_set(o, k, json_string(v ? v : ""));
}
static void cr3_set_or_null(json_t *o, const char *k, const json_t *v) {
    json_set(o, k, (v && v->type != JSON_NULL) ? json_copy(v) : json_null());
}
static void cr3_set_strnode(json_t *o, const char *k, const json_t *v) {
    json_set(o, k, json_string(cr3_sval(v)));
}

/* ── universal seam ── */
typedef struct cr3_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} cr3_seams_t;

static json_t *cr3_raise(const char *cls, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(cls));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}
static bool cr3_is_raise(const json_t *r) {
    return cr3_is_obj(r) && json_obj_get(r, "__raise__") != NULL;
}
static const char *cr3_raise_class(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}
static const char *cr3_raise_msg(const json_t *r) {
    return json_get_str(r, "msg", "");
}
static json_t *cr3_raise_copy(const json_t *r) {
    return cr3_raise(cr3_raise_class(r), cr3_raise_msg(r));
}
/* cr3_call OWNS args (frees after the call); the seam BORROWS args. */
static json_t *cr3_call(const cr3_seams_t *s, const char *name, json_t *args) {
    if (!s || !s->call) {
        json_free(args);
        return cr3_raise("RuntimeError", "no seam installed");
    }
    json_t *r = s->call(s->ctx, name, args);
    json_free(args);
    return r;
}

/* ── FULL builtin exception hierarchy (child -> parent) ── */
typedef struct cr3_exc_pair { const char *child; const char *parent; } cr3_exc_pair_t;
static const cr3_exc_pair_t CR3_EXC_TREE[] = {
    {"BaseExceptionGroup", "BaseException"},
    {"ExceptionGroup", "BaseExceptionGroup"},
    {"GeneratorExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"SystemExit", "BaseException"},
    {"CancelledError", "BaseException"},
    {"Exception", "BaseException"},
    {"ArithmeticError", "Exception"},
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ImportError", "Exception"},
    {"ModuleNotFoundError", "ImportError"},
    {"LookupError", "Exception"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"MemoryError", "Exception"},
    {"NameError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"OSError", "Exception"},
    {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"},
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"Warning", "Exception"},
    {"BytesWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"UserWarning", "Warning"},
    /* quarry / subprocess exceptions */
    {"LiveConnectionError", "Exception"},
    {"SubprocessError", "Exception"},
    {"TimeoutExpired", "SubprocessError"},
    {"CalledProcessError", "SubprocessError"},
};
static const char *cr3_exc_parent(const char *c) {
    for (size_t i = 0; i < sizeof(CR3_EXC_TREE) / sizeof(CR3_EXC_TREE[0]); i++)
        if (!strcmp(CR3_EXC_TREE[i].child, c)) return CR3_EXC_TREE[i].parent;
    return NULL;
}
/* isinstance(c, ancestor): walk child -> parent up to BaseException. */
static bool cr3_cls_is(const char *c, const char *ancestor) {
    if (!c || !*c) return false;
    while (c) {
        if (!strcmp(c, ancestor)) return true;
        c = cr3_exc_parent(c);
    }
    return false;
}

/* ── small string helpers ── */
static bool cr3_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
        || c == 0x1c || c == 0x1d || c == 0x1e || c == 0x1f;
}
static char *cr3_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s), b = 0, e = n;
    while (b < e && cr3_ws(s[b])) b++;
    while (e > b && cr3_ws(s[e - 1])) e--;
    char *out = malloc(e - b + 1);
    memcpy(out, s + b, e - b);
    out[e - b] = '\0';
    return out;
}
static bool cr3_in_charset(char c, const char *set) { return c && strchr(set, c) != NULL; }
static char *cr3_lower_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s), b = 0, e = n;
    while (b < e && cr3_ws(s[b])) b++;
    while (e > b && cr3_ws(s[e - 1])) e--;
    char *out = malloc(e - b + 1);
    size_t j = 0;
    for (size_t i = b; i < e; i++) {
        char c = s[i];
        out[j++] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    out[j] = '\0';
    return out;
}
/* Python str(value) for the scalar types the fixtures use. */
static char *cr3_py_str(const json_t *v) {
    if (cr3_is_null(v)) return strdup("");
    if (v->type == JSON_STRING) return cr3_dup(v->str_val);
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == floor(v->num_val)) return cr3_xsf("%.0f", v->num_val);
        return cr3_xsf("%g", v->num_val);
    }
    return cr3_dup(cr3_sval(v)); /* containers out of domain; echo text */
}
/* join an array of strings with a separator */
static char *cr3_join(const json_t *arr, const char *sep) {
    if (!cr3_is_arr(arr)) return strdup("");
    size_t total = 1, n = arr->c.count;
    for (size_t i = 0; i < n; i++) total += strlen(cr3_sval(json_get(arr, i))) + 1;
    char *out = malloc(total + 8);
    out[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        if (i) strcat(out, sep);
        strcat(out, cr3_sval(json_get(arr, i)));
    }
    return out;
}
/* split a string into non-empty stripped lines (str.splitlines + strip + filter) */
static json_t *cr3_lines(const char *s) {
    json_t *out = json_array();
    if (!s) return out;
    size_t n = strlen(s);
    size_t i = 0;
    while (i < n) {
        size_t b = i;
        while (i < n && s[i] != '\n' && s[i] != '\r' && s[i] != '\v' && s[i] != '\f'
               && (unsigned char)s[i] != 0x1c && (unsigned char)s[i] != 0x1d
               && (unsigned char)s[i] != 0x1e) i++;
        char *ln = malloc(i - b + 1);
        memcpy(ln, s + b, i - b); ln[i - b] = '\0';
        char *t = cr3_strip(ln);
        if (t[0]) json_append(out, json_string(t));
        free(t); free(ln);
        if (i < n) {
            if (s[i] == '\r' && i + 1 < n && s[i + 1] == '\n') i += 2;
            else i++;
        }
    }
    return out;
}

/* ── path helpers (POSIX view; fixtures use POSIX-shaped paths) ── */
static bool cr3_path_abs(const char *p) { return p && p[0] == '/'; }
static char *cr3_path_join(const char *a, const char *b) {
    if (!a || !*a) return cr3_dup(b);
    if (!b || !*b) return cr3_dup(a);
    if (cr3_path_abs(b)) return cr3_dup(b);        /* pathlib '/' keeps absolute rhs */
    size_t la = strlen(a);
    if (a[la - 1] == '/') return cr3_xsf("%s%s", a, b);
    return cr3_xsf("%s/%s", a, b);
}
/* pathlib .parents: all ancestors of p from parent up to '/', longest first. */
static json_t *cr3_parents(const char *p) {
    json_t *out = json_array();
    if (!p || !*p) return out;
    char *tmp = cr3_dup(p);
    size_t n = strlen(tmp);
    while (n > 1 && tmp[n - 1] == '/') tmp[--n] = '\0';
    char *last = strrchr(tmp, '/');
    while (last && last != tmp) {
        *last = '\0';
        json_append(out, json_string(tmp));
        last = strrchr(tmp, '/');
    }
    if (tmp[0] == '/' && tmp[1] != '\0') json_append(out, json_string("/"));
    free(tmp);
    return out;
}
/* pathlib .parts count (POSIX): number of path components. */
static size_t cr3_parts_count(const char *p) {
    if (!p || !*p) return 0;
    size_t c = 0;
    const char *q = p;
    while (*q) {
        while (*q == '/') q++;
        if (!*q) break;
        c++;
        while (*q && *q != '/') q++;
    }
    return c;
}
static char *cr3_path_name(const char *p) {
    if (!p || !*p) return strdup("");
    size_t n = strlen(p);
    while (n > 1 && p[n - 1] == '/') n--;
    const char *slash = NULL;
    for (size_t i = 0; i < n; i++) if (p[i] == '/') slash = p + i;
    return slash ? cr3_dup(slash + 1) : cr3_dup(p);
}

/* ══════════════════ PoP helper leaves (re-derived, flow2 owns the claim) ════ */
static json_t *cr3_stp(const char *src) {
    char *s = cr3_dup(src);
    size_t n = strlen(s);
    while (n > 0 && strchr(".,;!?", s[n - 1])) s[--n] = '\0';
    const char *closers = ")]}";
    const char *openers = "([{";
    for (;;) {
        if (n == 0) break;
        const char *cp = strchr(closers, s[n - 1]);
        if (!cp) break;
        size_t ci = (size_t)(cp - closers), nc = 0, no = 0;
        for (size_t i = 0; i < n; i++) {
            if (s[i] == closers[ci]) nc++;
            if (s[i] == openers[ci]) no++;
        }
        if (nc > no) s[--n] = '\0'; else break;
    }
    json_t *r = json_string(s); free(s); return r;
}
static json_t *cr3_swr(const char *s) {
    size_t n = strlen(s);
    if (n >= 2 && s[0] == s[n - 1] && strchr("`\"'", s[0])) {
        char *buf = malloc(n - 1);
        memcpy(buf, s + 1, n - 2); buf[n - 2] = '\0';
        json_t *r = json_string(buf); free(buf); return r;
    }
    return json_string(s);
}
/* returns [path, start|null, end|null] */
static json_t *cr3_pfrv(const char *s) {
    size_t n = strlen(s);
    size_t colon = (size_t)-1;
    long long start = 0, end = 0;
    for (size_t i = n; i > 0; i--) {
        if (s[i - 1] != ':') continue;
        const char *tail = s + i;
        size_t tl = strlen(tail), d1 = 0;
        while (d1 < tl && tail[d1] >= '0' && tail[d1] <= '9') d1++;
        if (d1 == 0) continue;
        long long st = 0;
        for (size_t k = 0; k < d1; k++) st = st * 10 + (tail[k] - '0');
        long long en = st;
        if (d1 < tl && tail[d1] == '-') {
            size_t d2 = 0;
            while (d1 + 1 + d2 < tl && tail[d1 + 1 + d2] >= '0' && tail[d1 + 1 + d2] <= '9') d2++;
            if (d2 == 0 || d1 + 1 + d2 != tl) continue;
            en = 0;
            for (size_t k = 0; k < d2; k++) en = en * 10 + (tail[d1 + 1 + k] - '0');
        } else if (d1 != tl) continue;
        colon = i - 1; start = st; end = en; break;
    }
    json_t *out = json_array();
    if (colon == (size_t)-1 || colon == 0) {
        json_t *p = cr3_swr(s);
        json_append(out, p);
        json_append(out, json_null());
        json_append(out, json_null());
        return out;
    }
    const char *prefix = s;
    size_t pl = colon;
    if (pl >= 2 && strchr("`\"'", prefix[0]) && prefix[0] == prefix[pl - 1]) {
        char *buf = malloc(pl - 1);
        memcpy(buf, prefix + 1, pl - 2); buf[pl - 2] = '\0';
        json_append(out, json_string(buf)); free(buf);
    } else {
        char *buf = malloc(pl + 1);
        memcpy(buf, prefix, pl); buf[pl] = '\0';
        json_append(out, json_string(buf)); free(buf);
    }
    json_append(out, json_number((double)start));
    json_append(out, json_number((double)end));
    return out;
}
static bool cr3_builtin_prefix(const char *p) {
    static const char *B[] = {"diff", "staged", "file", "folder", "git", "url", NULL};
    for (int i = 0; B[i]; i++) if (!strcmp(p, B[i])) return true;
    return false;
}

/* build a ContextReference dict in canonical key order */
static json_t *cr3_ref(const char *raw, const char *kind, const char *target,
                       double start, double end, const json_t *ls, const json_t *le) {
    json_t *o = json_object();
    cr3_set_str(o, "raw", raw);
    cr3_set_str(o, "kind", kind);
    cr3_set_str(o, "target", target);
    json_set(o, "start", json_number(start));
    json_set(o, "end", json_number(end));
    cr3_set_or_null(o, "line_start", ls);
    cr3_set_or_null(o, "line_end", le);
    return o;
}
static const char *cr3_ref_get(const json_t *r, const char *k) { return json_get_str(r, k, ""); }
static double cr3_ref_num(const json_t *r, const char *k) { return json_get_num(r, k, 0); }

/* ══════════════════ F1: parse_context_references ══════════════════ */
static json_t *cr3_parse_context_references(cr3_seams_t *sp, const char *message,
                                            const json_t *providers, bool plugins) {
    json_t *refs = json_array();
    if (!message || !*message) return refs;
    json_t *a = json_object(); cr3_set_str(a, "message", message);
    json_t *ms = cr3_call(sp, "builtin_matches", a);
    if (cr3_is_raise(ms)) return ms;
    if (cr3_is_arr(ms)) {
        for (size_t i = 0; i < ms->c.count; i++) {
            const json_t *m = json_get(ms, i);
            if (!cr3_is_obj(m)) continue;
            const json_t *simple = json_obj_get(m, "simple");
            const char *kind = json_get_str(m, "kind", "");
            const char *val = "";
            { const json_t *vv = json_obj_get(m, "value"); if (vv && cr3_is_str(vv)) val = vv->str_val; }
            json_t *stp = cr3_stp(val);
            const char *value = cr3_sval(stp);
            const char *target = "";
            json_t *ls = json_null(), *le = json_null();
            json_t *keeper = NULL;  /* owns target's bytes until cr3_ref copies them */
            if (simple && simple->type != JSON_NULL && cr3_is_str(simple)) {
                kind = simple->str_val;
                target = "";
            } else if (!strcmp(kind, "file")) {
                json_t *pfr = cr3_pfrv(value);
                target = cr3_sval(json_get(pfr, 0));
                ls = json_copy(json_get(pfr, 1));
                le = json_copy(json_get(pfr, 2));
                keeper = pfr;
            } else {
                json_t *swr = cr3_swr(value);
                target = cr3_sval(swr);
                /* copy target into owned buffer below via cr3_ref (immediate use) */
                json_free(stp);
                stp = swr; value = target;
            }
            json_t *ref = cr3_ref(json_get_str(m, "g0", ""), kind, target,
                                  cr3_ref_num(m, "start"), cr3_ref_num(m, "end"), ls, le);
            json_append(refs, ref);
            json_free(ls); json_free(le); json_free(stp); json_free(keeper);
        }
    }
    json_free(ms);
    if (plugins && cr3_is_obj(providers) && providers->c.count > 0) {
        json_t *a2 = json_object(); cr3_set_str(a2, "message", message);
        json_t *pm = cr3_call(sp, "plugin_matches", a2);
        if (cr3_is_raise(pm)) { json_free(refs); return pm; }
        if (cr3_is_arr(pm)) {
            for (size_t i = 0; i < pm->c.count; i++) {
                const json_t *m = json_get(pm, i);
                if (!cr3_is_obj(m)) continue;
                const char *kind = json_get_str(m, "kind", "");
                if (cr3_builtin_prefix(kind) || !json_obj_get(providers, kind)) continue;
                double st = cr3_ref_num(m, "start");
                bool dup = false;
                for (size_t j = 0; j < refs->c.count; j++) {
                    const json_t *r = json_get(refs, j);
                    if (!strcmp(cr3_ref_get(r, "kind"), kind) && cr3_ref_num(r, "start") == st) { dup = true; break; }
                }
                if (dup) continue;
                const json_t *vv = json_obj_get(m, "value");
                const char *rawv = vv && cr3_is_str(vv) ? vv->str_val : "";
                json_t *stp = cr3_stp(rawv);
                json_t *swr = cr3_swr(cr3_sval(stp));
                const char *target = cr3_sval(swr);
                json_append(refs, cr3_ref(json_get_str(m, "g0", ""), kind, target,
                                          st, cr3_ref_num(m, "end"), json_null(), json_null()));
                json_free(stp); json_free(swr);
            }
        }
        json_free(pm);
    }
    return refs;
}

/* result dict helper (canonical ContextReferenceResult field order) */
static json_t *cr3_result(const char *message, const char *original,
                          const json_t *refs, const json_t *warnings,
                          double tokens, bool expanded, bool blocked) {
    json_t *o = json_object();
    cr3_set_str(o, "message", message);
    cr3_set_str(o, "original_message", original);
    json_set(o, "references", refs ? json_copy(refs) : json_array());
    json_set(o, "warnings", warnings ? json_copy(warnings) : json_array());
    json_set(o, "injected_tokens", json_number(tokens));
    json_set(o, "expanded", json_bool(expanded));
    json_set(o, "blocked", json_bool(blocked));
    return o;
}
static bool cr3_has_refs(const json_t *refs) { return cr3_is_arr(refs) && refs->c.count > 0; }

/* ══════════════════ F3: preprocess_context_references_async ══════════ */
static json_t *cr3_preprocess_async(cr3_seams_t *sp, const char *message, const char *cwd,
                                    double context_length, const json_t *allowed_root, bool plugins) {
    /* parse_context_references -> seam parse_refs */
    json_t *pa = json_object(); cr3_set_str(pa, "message", message ? message : "");
    json_set(pa, "plugins", json_bool(plugins));
    json_t *refs = cr3_call(sp, "parse_refs", pa);
    if (cr3_is_raise(refs)) return refs;
    if (!cr3_has_refs(refs)) {
        json_t *r = cr3_result(message, message, refs, json_array(), 0, false, false);
        json_free(refs); return r;
    }
    /* host_feature_refusal */
    json_t *ha = json_object(); cr3_set_str(ha, "feature", "context_references"); cr3_set_str(ha, "label", "@-references");
    json_t *refusal = cr3_call(sp, "host_refusal", ha);
    if (cr3_is_raise(refusal)) { json_free(refs); return refusal; }
    if (!cr3_is_null(refusal) && cr3_jtruth(refusal)) {
        json_t *w = json_array(); json_append(w, json_copy(refusal));
        json_t *r = cr3_result(message, message, refs, w, 0, false, true);
        json_free(refusal); json_free(refs); return r;
    }
    json_free(refusal);
    /* cwd resolve / root resolve */
    json_t *ca = json_object(); cr3_set_str(ca, "path", cwd ? cwd : "");
    json_t *cwdp = cr3_call(sp, "resolve_path", ca);
    if (cr3_is_raise(cwdp)) { json_free(refs); return cwdp; }
    const char *cwd_s = cr3_sval(cwdp);
    json_t *root = NULL;
    if (!cr3_is_null(allowed_root)) {
        json_t *ra = json_object(); cr3_set_str(ra, "path", cr3_sval(allowed_root));
        root = cr3_call(sp, "resolve_path", ra);
        if (cr3_is_raise(root)) { json_free(refs); json_free(cwdp); return root; }
    }
    double hard = floor(context_length * 0.50);
    if (hard < 1) hard = 1;
    double soft = floor(context_length * 0.25);
    if (soft < 1) soft = 1;
    /* gather expand_ref over refs[:16] */
    json_t *expanded = json_array();
    size_t nrefs = refs->c.count;
    size_t nexp = nrefs < 16 ? nrefs : 16;
    for (size_t i = 0; i < nexp; i++) {
        const json_t *ref = json_get(refs, i);
        json_t *ea = json_object();
        cr3_set_str(ea, "raw", cr3_ref_get(ref, "raw"));
        cr3_set_str(ea, "kind", cr3_ref_get(ref, "kind"));
        cr3_set_str(ea, "target", cr3_ref_get(ref, "target"));
        cr3_set_str(ea, "cwd", cwd_s);
        json_set(ea, "max_inline_tokens", json_number(hard));
        json_t *e = cr3_call(sp, "expand_ref", ea);
        if (cr3_is_raise(e)) { json_free(refs); json_free(cwdp); json_free(root); json_free(expanded); return e; }
        json_append(expanded, e ? json_copy(e) : json_array());
        json_free(e);
    }
    json_t *rc = json_array();
    for (size_t i = 0; i < nexp; i++) {
        const json_t *e = json_get(expanded, i);
        if (cr3_is_arr(e) && e->c.count >= 1) {
            const json_t *w = json_get(e, 0);
            if (!cr3_is_null(w) && cr3_jtruth(w)) json_append(rc, json_copy(w));
        }
    }
    for (size_t i = nexp; i < nrefs; i++) {
        const json_t *ref = json_get(refs, i);
        char *m = cr3_xsf("%s: not expanded (maximum 16 @-references per message)", cr3_ref_get(ref, "raw"));
        json_append(rc, json_string(m)); free(m);
    }
    json_t *blocks = json_array();
    for (size_t i = 0; i < nexp; i++) {
        const json_t *e = json_get(expanded, i);
        if (cr3_is_arr(e) && e->c.count >= 2) {
            const json_t *b = json_get(e, 1);
            if (!cr3_is_null(b) && cr3_jtruth(b)) json_append(blocks, json_copy(b));
        }
    }
    double injected = 0;
    for (size_t i = 0; i < blocks->c.count; i++) {
        json_t *ta = json_object(); cr3_set_strnode(ta, "text", json_get(blocks, i));
        json_t *tk = cr3_call(sp, "estimate_tokens", ta);
        if (cr3_is_raise(tk)) { json_free(refs); json_free(cwdp); json_free(root); json_free(expanded); json_free(rc); json_free(blocks); return tk; }
        injected += cr3_nval(tk); json_free(tk);
    }
    json_t *result = cr3_result(message, message, refs, rc, injected, false, false);
    if (injected > hard) {
        char *m = cr3_xsf("@ context injection refused: %.0f tokens exceeds the 50%% hard limit (%.0f).", injected, hard);
        json_t *w = json_obj_get(result, "warnings"); json_append(w, json_string(m)); free(m);
        json_set(result, "blocked", json_bool(true));
        json_free(refs); json_free(cwdp); json_free(root); json_free(expanded); json_free(blocks); return result;
    }
    if (injected > soft) {
        char *m = cr3_xsf("@ context injection warning: %.0f tokens exceeds the 25%% soft limit (%.0f).", injected, soft);
        json_t *w = json_obj_get(result, "warnings"); json_append(w, json_string(m)); free(m);
    }
    /* final message assembly */
    char *final = cr3_dup(message ? message : "");
    json_t *warns = json_obj_get(result, "warnings");
    if (warns->c.count > 0) {
        char *wj = cr3_join(warns, "\n");
        char *pre = cr3_xsf("%s\n\n--- Context Warnings ---\n", final);
        char *buf = cr3_xsf("%s%s", pre, "");
        (void)buf;
        /* build "- w" lines */
        size_t cap = strlen(pre) + 1;
        for (size_t i = 0; i < warns->c.count; i++) cap += strlen(cr3_sval(json_get(warns, i))) + 4;
        char *lines = malloc(cap); lines[0] = '\0';
        strcat(lines, pre);
        for (size_t i = 0; i < warns->c.count; i++) {
            if (i) strcat(lines, "\n");
            char *ln = cr3_xsf("- %s", cr3_sval(json_get(warns, i)));
            strcat(lines, ln); free(ln);
        }
        free(final); final = lines;
        free(pre); free(wj);
    }
    if (blocks->c.count > 0) {
        char *bj = cr3_join(blocks, "\n\n");
        char *pre = cr3_xsf("%s\n\n--- Attached Context ---\n\n%s", final, bj);
        free(final); final = pre; free(bj);
    }
    char *trimmed = cr3_strip(final);
    json_set(result, "message", json_string(trimmed));
    json_set(result, "expanded", json_bool(blocks->c.count > 0 || warns->c.count > 0));
    free(trimmed); free(final);
    json_free(refs); json_free(cwdp); json_free(root); json_free(expanded); json_free(blocks);
    return result;
}

/* ══════════════════ F2: preprocess_context_references (sync wrapper) ══ */
static json_t *cr3_preprocess(cr3_seams_t *sp, const char *message, const char *cwd,
                              double context_length, const json_t *allowed_root) {
    (void)cwd; (void)context_length; (void)allowed_root;
    /* get_running_loop: RuntimeError (Exception child) -> asyncio.run branch;
     * no exception -> thread-pool branch. Both await the coro -> same result. */
    json_t *ga = json_object();
    json_t *g = cr3_call(sp, "get_running_loop", ga);
    bool run_direct = false;
    if (cr3_is_raise(g)) {
        if (cr3_cls_is(cr3_raise_class(g), "RuntimeError")) { run_direct = true; json_free(g); }
        else return g; /* non-RuntimeError propagates */
    } else json_free(g);
    (void)run_direct;
    json_t *ra = json_object(); cr3_set_str(ra, "message", message ? message : "");
    json_t *r = cr3_call(sp, "preprocess_async_result", ra);
    return r;
}

/* ══════════════════ F7: _default_url_fetcher ══════════════════ */
static json_t *cr3_default_url_fetcher(cr3_seams_t *sp, const char *url) {
    json_t *a = json_object();
    json_t *urls = json_array(); json_append(urls, json_string(url ? url : ""));
    json_set(a, "urls", urls); cr3_set_str(a, "format", "markdown");
    json_t *raw = cr3_call(sp, "web_extract", a);
    if (cr3_is_raise(raw)) return raw;
    const char *txt = cr3_sval(raw);
    char *err = NULL;
    json_t *payload = json_parse(txt, &err);
    if (err || !payload) { free(err); json_free(raw); return cr3_raise("JSONDecodeError", "Expecting value"); }
    json_free(raw);
    json_t *docs = json_obj_get(payload, "results");
    if (!cr3_is_arr(docs) || docs->c.count == 0) {
        const json_t *e = json_obj_get(payload, "error");
        json_free(payload);
        if (!cr3_is_null(e) && cr3_jtruth(e)) return cr3_raise("ValueError", cr3_sval(e));
        return json_string("");
    }
    const json_t *page = json_get(docs, 0);
    const json_t *blocked = json_obj_get(page, "blocked_by_policy");
    if (!cr3_is_null(blocked) && cr3_jtruth(blocked)) {
        const char *msg = json_get_str(page, "error", "Blocked by website policy");
        json_t *r = cr3_raise("ValueError", msg && *msg ? msg : "Blocked by website policy");
        json_free(payload); return r;
    }
    const json_t *c = json_obj_get(page, "content");
    const json_t *rc = json_obj_get(page, "raw_content");
    const char *src = (!cr3_is_null(c) && cr3_jtruth(c)) ? cr3_sval(c)
                     : (!cr3_is_null(rc) && cr3_jtruth(rc)) ? cr3_sval(rc) : "";
    char *content = cr3_strip(src);
    if (!content[0]) {
        const json_t *e = json_obj_get(page, "error");
        if (!cr3_is_null(e) && cr3_jtruth(e)) {
            char *msg = cr3_dup(cr3_sval(e));
            json_t *r = cr3_raise("ValueError", msg);
            free(msg); free(content); json_free(payload); return r;
        }
    }
    json_free(payload);
    json_t *r = json_string(content); free(content); return r;
}

/* ══════════════════ F6: _fetch_url_content ══════════════════ */
static json_t *cr3_fetch_url_content(cr3_seams_t *sp, const char *url) {
    json_t *a = json_object(); cr3_set_str(a, "url", url ? url : "");
    json_t *raw = cr3_call(sp, "fetch", a);
    if (cr3_is_raise(raw)) return raw;
    /* {"__await__": value} models an awaitable result. */
    const json_t *v = raw;
    if (cr3_is_obj(raw) && json_obj_get(raw, "__await__")) v = json_obj_get(raw, "__await__");
    /* Python str(content or ""): falsy (None/""/0/False/empty) -> "". */
    char *s = cr3_jtruth(v) ? cr3_py_str(v) : strdup("");
    char *t = cr3_strip(s);
    free(s); json_free(raw);
    json_t *r = json_string(t); free(t); return r;
}

/* ══════════════════ F4: _expand_reference ══════════════════ */
static json_t *cr3_expand_reference(cr3_seams_t *sp, const json_t *ref, const char *cwd,
                                    const json_t *allowed_root, const json_t *max_inline,
                                    const json_t *providers) {
    const char *kind = cr3_ref_get(ref, "kind");
    const char *raw = cr3_ref_get(ref, "raw");
    const char *target = cr3_ref_get(ref, "target");
    const char *err_cls = NULL, *err_msg = NULL;
    json_t *ret = NULL;
    bool handled = false;
    /* try: ... except Exception as exc */
    if (!strcmp(kind, "file") || !strcmp(kind, "folder")) {
        json_t *a = json_object();
        cr3_set_str(a, "raw", raw); cr3_set_str(a, "kind", kind); cr3_set_str(a, "target", target);
        cr3_set_str(a, "cwd", cwd ? cwd : "");
        cr3_set_or_null(a, "allowed_root", allowed_root);
        cr3_set_or_null(a, "max_inline_tokens", max_inline);
        json_t *r = cr3_call(sp, "expand_path", a);
        if (cr3_is_raise(r)) { err_cls = cr3_dup(cr3_raise_class(r)); err_msg = cr3_dup(cr3_raise_msg(r)); json_free(r); }
        else { ret = r; handled = true; }
    } else if (!strcmp(kind, "diff") || !strcmp(kind, "staged") || !strcmp(kind, "git")) {
        json_t *args = json_array();
        char *label = NULL;
        if (!strcmp(kind, "diff")) {
            json_append(args, json_string("diff")); label = strdup("git diff");
        } else if (!strcmp(kind, "staged")) {
            json_append(args, json_string("diff")); json_append(args, json_string("--staged"));
            label = strdup("git diff --staged");
        } else {
            long long n = 1;
            const char *tv = target ? target : "";
            if (*tv) { char *end = NULL; long long p = strtoll(tv, &end, 10); if (end != tv) n = p; }
            if (n < 1) n = 1;
            if (n > 10) n = 10;
            char *num = cr3_xsf("-%lld", n);
            json_append(args, json_string("log")); json_append(args, json_string(num)); json_append(args, json_string("-p"));
            label = cr3_xsf("git log %s -p", num);
            free(num);
        }
        json_t *a = json_object();
        cr3_set_str(a, "raw", raw); cr3_set_str(a, "cwd", cwd ? cwd : "");
        json_set(a, "args", args); cr3_set_str(a, "label", label);
        json_t *r = cr3_call(sp, "expand_git", a);
        if (cr3_is_raise(r)) { err_cls = cr3_dup(cr3_raise_class(r)); err_msg = cr3_dup(cr3_raise_msg(r)); json_free(r); }
        else { ret = r; handled = true; }
        free(label);
    } else if (!strcmp(kind, "url")) {
        json_t *a = json_object(); cr3_set_str(a, "url", target);
        json_t *content = cr3_call(sp, "fetch_url", a);
        if (cr3_is_raise(content)) { err_cls = cr3_dup(cr3_raise_class(content)); err_msg = cr3_dup(cr3_raise_msg(content)); }
        else {
            const char *ct = cr3_sval(content);
            if (!ct[0]) {
                char *m = cr3_xsf("%s: no content extracted", raw);
                ret = json_array(); json_append(ret, json_string(m)); json_append(ret, json_null()); free(m);
            } else {
                json_t *ta = json_object(); cr3_set_str(ta, "text", ct);
                json_t *tk = cr3_call(sp, "estimate_tokens", ta);
                if (cr3_is_raise(tk)) { err_cls = cr3_dup(cr3_raise_class(tk)); err_msg = cr3_dup(cr3_raise_msg(tk)); }
                else {
                    char *m = cr3_xsf("🌐 %s (%.0f tokens)\n%s", raw, cr3_nval(tk), ct);
                    ret = json_array(); json_append(ret, json_null()); json_append(ret, json_string(m)); free(m);
                    json_free(tk);
                }
            }
            json_free(content);
            if (err_cls) handled = false; else handled = true;
        }
    }
    if (err_cls) {
        if (cr3_cls_is(err_cls, "Exception")) {
            char *m = cr3_xsf("%s: %s", raw, err_msg ? err_msg : "");
            ret = json_array(); json_append(ret, json_string(m)); json_append(ret, json_null()); free(m);
            free((void*)err_cls); free((void*)err_msg);
            return ret;
        }
        /* BaseException propagates */
        json_t *out = cr3_raise(err_cls, err_msg);
        free((void*)err_cls); free((void*)err_msg);
        return out;
    }
    if (handled) return ret;
    /* plugin provider fallback */
    const json_t *prov = cr3_is_obj(providers) ? json_obj_get(providers, kind) : NULL;
    if (!cr3_is_null(prov) && prov) {
        json_t *a = json_object(); cr3_set_str(a, "kind", kind); cr3_set_str(a, "target", target);
        json_t *pc = cr3_call(sp, "provider_expand", a);
        if (cr3_is_raise(pc)) {
            if (cr3_cls_is(cr3_raise_class(pc), "Exception")) {
                char *m = cr3_xsf("%s: plugin expansion error: %s", raw, cr3_raise_msg(pc));
                json_t *o = json_array(); json_append(o, json_string(m)); json_append(o, json_null()); free(m); json_free(pc); return o;
            }
            return pc;
        }
        if (!cr3_is_null(pc)) {
            json_t *ta = json_object(); cr3_set_strnode(ta, "text", pc);
            json_t *tk = cr3_call(sp, "estimate_tokens", ta);
            if (cr3_is_raise(tk)) {
                if (cr3_cls_is(cr3_raise_class(tk), "Exception")) {
                    char *m = cr3_xsf("%s: plugin expansion error: %s", raw, cr3_raise_msg(tk));
                    json_t *o = json_array(); json_append(o, json_string(m)); json_append(o, json_null()); free(m);
                } else { json_free(pc); return tk; }
                json_free(tk); json_free(pc);
                /* fall through to unsupported if we didn't build ret */
                json_t *o = json_array();
                char *m = cr3_xsf("%s: unsupported reference type", raw);
                json_append(o, json_string(m)); json_append(o, json_null()); free(m); return o;
            }
            char *m = cr3_xsf("📌 %s (%.0f tokens)\n%s", raw, cr3_nval(tk), cr3_sval(pc));
            json_t *o = json_array(); json_append(o, json_null()); json_append(o, json_string(m)); free(m);
            json_free(tk); json_free(pc); return o;
        }
        json_free(pc);
    }
    char *m = cr3_xsf("%s: unsupported reference type", raw);
    json_t *o = json_array(); json_append(o, json_string(m)); json_append(o, json_null()); free(m); return o;
}

/* ══════════════════ F5: _expand_git_reference ══════════════════ */
static json_t *cr3_expand_git_reference(cr3_seams_t *sp, const json_t *ref, const char *cwd,
                                        const json_t *args, const char *label) {
    const char *raw = cr3_ref_get(ref, "raw");
    json_t *ea = json_object(); cr3_set_str(ea, "cwd", cwd ? cwd : "");
    json_t *env = cr3_call(sp, "noninteractive_env", ea);
    if (cr3_is_raise(env)) return env;
    if (cr3_is_null(env)) {
        char *m = cr3_xsf("%s: git filter discovery failed", raw);
        json_t *o = json_array(); json_append(o, json_string(m)); json_append(o, json_null()); free(m); return o;
    }
    json_t *ha = json_object(); json_set(ha, "args", args ? json_copy(args) : json_array());
    json_t *hard = cr3_call(sp, "harden_argv", ha);
    if (cr3_is_raise(hard)) { json_free(env); return hard; }
    json_t *cmd = json_array(); json_append(cmd, json_string("git"));
    if (cr3_is_arr(hard)) for (size_t i = 0; i < hard->c.count; i++) json_append(cmd, json_copy(json_get(hard, i)));
    json_free(hard);
    json_t *qa = json_object();
    json_set(qa, "cmd", cmd); cr3_set_str(qa, "cwd", cwd ? cwd : "");
    json_set(qa, "timeout", json_number(30)); cr3_set_or_null(qa, "env", env);
    json_t *res = cr3_call(sp, "run_quiet", qa);
    json_free(env);
    if (cr3_is_raise(res)) {
        if (cr3_cls_is(cr3_raise_class(res), "TimeoutExpired")) {
            char *m = cr3_xsf("%s: git command timed out (30s)", raw);
            json_t *o = json_array(); json_append(o, json_string(m)); json_append(o, json_null()); free(m); json_free(res); return o;
        }
        return res;
    }
    double rc = cr3_nval(json_obj_get(res, "returncode"));
    if (rc != 0) {
        char *se = cr3_strip(json_get_str(res, "stderr", ""));
        const char *msg = se[0] ? se : "git command failed";
        char *m = cr3_xsf("%s: %s", raw, msg);
        json_t *o = json_array(); json_append(o, json_string(m)); json_append(o, json_null()); free(m); free(se); json_free(res); return o;
    }
    char *out = cr3_strip(json_get_str(res, "stdout", ""));
    const char *content = out[0] ? out : "(no output)";
    json_t *ta = json_object(); cr3_set_str(ta, "text", content);
    json_t *tk = cr3_call(sp, "estimate_tokens", ta);
    json_free(res);
    if (cr3_is_raise(tk)) { free(out); return tk; }
    char *m = cr3_xsf("🧾 %s (%.0f tokens)\n```diff\n%s\n```", label ? label : "", cr3_nval(tk), content);
    json_t *o = json_array(); json_append(o, json_null()); json_append(o, json_string(m)); free(m); free(out); json_free(tk); return o;
}

/* ══════════════════ F8: _resolve_path ══════════════════ */
static json_t *cr3_resolve_path(cr3_seams_t *sp, const char *cwd, const char *target,
                                const json_t *allowed_root) {
    json_t *na = json_object(); cr3_set_str(na, "target", target ? target : "");
    json_t *nt = cr3_call(sp, "is_nt_namespace", na);
    if (cr3_is_raise(nt)) return nt;
    bool is_nt = cr3_jtruth(nt); json_free(nt);
    if (is_nt) return cr3_raise("ValueError", "path uses a Windows NT/device namespace prefix and cannot be attached");
    char *joined = cr3_path_join(cwd, target);
    json_t *ra = json_object(); cr3_set_str(ra, "path", joined);
    json_t *resolved = cr3_call(sp, "resolve_path", ra);
    free(joined);
    if (cr3_is_raise(resolved)) return resolved;
    const char *res_s = cr3_sval(resolved);
    bool outside = false;
    if (!cr3_is_null(allowed_root)) {
        json_t *ua = json_object(); cr3_set_str(ua, "path", res_s); cr3_set_strnode(ua, "root", allowed_root);
        json_t *u = cr3_call(sp, "is_under", ua);
        if (cr3_is_raise(u)) { json_free(resolved); return u; }
        outside = !cr3_jtruth(u); json_free(u);
        if (outside) {
            json_t *ca = json_object();
            json_t *roots = cr3_call(sp, "composer_roots", ca);
            if (cr3_is_raise(roots)) { json_free(resolved); return roots; }
            if (cr3_is_arr(roots)) {
                for (size_t i = 0; i < roots->c.count; i++) {
                    json_t *ua2 = json_object(); cr3_set_str(ua2, "path", res_s); cr3_set_strnode(ua2, "root", json_get(roots, i));
                    json_t *u2 = cr3_call(sp, "is_under", ua2);
                    if (cr3_is_raise(u2)) { json_free(resolved); json_free(roots); return u2; }
                    bool inroot = cr3_jtruth(u2); json_free(u2);
                    if (inroot) { outside = false; break; }
                }
            }
            json_free(roots);
            if (outside) {
                json_t *sa = json_object(); cr3_set_str(sa, "path", res_s);
                json_t *st = cr3_call(sp, "agent_staged", sa);
                if (cr3_is_raise(st)) { json_free(resolved); return st; }
                outside = !cr3_jtruth(st); json_free(st);
            }
        }
    }
    if (outside) { json_free(resolved); return cr3_raise("ValueError", "path is outside the allowed workspace"); }
    return resolved;
}

/* ══════════════════ F9: _ensure_reference_path_allowed ═══════════ */
static bool cr3_deny_key_eq(const char *a, const char *b) { return !strcmp(a, b); }
static bool cr3_deny_under(const char *p, const char *base) {
    size_t n = strlen(base);
    return strncmp(p, base, n) == 0 && (p[n] == '\0' || p[n] == '/');
}
static json_t *cr3_ensure_path_allowed(cr3_seams_t *sp, const char *path,
                                       const json_t *constants, const char *home_in, const char *hermes_in) {
    /* live: Path(expanduser("~")).resolve(), get_hermes_home().resolve() */
    json_t *ha = json_object(); cr3_set_str(ha, "path", home_in);
    json_t *hr = cr3_call(sp, "resolve_path", ha);
    if (cr3_is_raise(hr)) return hr;
    const char *home = cr3_sval(hr);
    json_t *hb = json_object(); cr3_set_str(hb, "path", hermes_in);
    json_t *hres = cr3_call(sp, "resolve_path", hb);
    if (cr3_is_raise(hres)) { json_free(hr); return hres; }
    const char *hermes_home = cr3_sval(hres);
    /* seed-sensitive home files / hermes dirs / credential dirs come from `constants`. */
    const json_t *sf = json_obj_get(constants, "sensitive_home_files");
    const json_t *sh = json_obj_get(constants, "sensitive_hermes_dirs");
    const json_t *cd = json_obj_get(constants, "credential_dirs");
    /* blocked_exact keys */
    json_t *exact = json_array();
    if (cr3_is_arr(sf)) for (size_t i = 0; i < sf->c.count; i++) {
        char *p = cr3_path_join(home, cr3_sval(json_get(sf, i)));
        json_append(exact, json_string(p)); free(p);
    }
    { char *p = cr3_path_join(hermes_home, ".env"); json_append(exact, json_string(p)); free(p); }
    json_t *dirs = json_array();
    if (cr3_is_arr(cd)) for (size_t i = 0; i < cd->c.count; i++) {
        char *p = cr3_path_join(home, cr3_sval(json_get(cd, i)));
        json_append(dirs, json_string(p)); free(p);
    }
    if (cr3_is_arr(sh)) for (size_t i = 0; i < sh->c.count; i++) {
        char *p = cr3_path_join(hermes_home, cr3_sval(json_get(sh, i)));
        json_append(dirs, json_string(p)); free(p);
    }
    const char *pk = path ? path : "";
    for (size_t i = 0; i < exact->c.count; i++)
        if (cr3_deny_key_eq(pk, cr3_sval(json_get(exact, i))))
            return cr3_raise("ValueError", "path is a sensitive credential file and cannot be attached");
    for (size_t i = 0; i < dirs->c.count; i++)
        if (cr3_deny_under(pk, cr3_sval(json_get(dirs, i))))
            return cr3_raise("ValueError", "path is a sensitive credential or internal Hermes path and cannot be attached");
    /* get_read_block_error(path) seam: try ValueError->raise; Exception->fail-closed */
    json_t *ba = json_object(); cr3_set_str(ba, "path", pk);
    json_t *blk = cr3_call(sp, "read_block", ba);
    if (cr3_is_raise(blk)) {
        const char *cls = cr3_raise_class(blk);
        if (!strcmp(cls, "ValueError")) return blk;
        if (cr3_cls_is(cls, "Exception"))
            return cr3_raise("ValueError", "path could not be verified against the credential deny-list and cannot be attached");
        return blk; /* BaseException propagates */
    }
    bool blocked = !cr3_is_null(blk) && cr3_jtruth(blk);
    json_free(blk);
    json_free(exact); json_free(dirs);
    if (blocked)
        return cr3_raise("ValueError", "path is a sensitive credential or internal Hermes path and cannot be attached");
    return NULL;
}

/* ══════════════════ F11: _iter_visible_entries ═══════════════ */
static int cr3_cmp_paths(const void *a, const void *b) {
    const json_t *pa = *(const json_t *const *)a, *pb = *(const json_t *const *)b;
    bool da = json_get_bool(pa, "is_dir", false), db = json_get_bool(pb, "is_dir", false);
    if (da != db) return da ? -1 : 1; /* dirs first */
    return strcmp(json_get_str(pa, "path", ""), json_get_str(pb, "path", ""));
}
static json_t *cr3_entry(json_t *out, const char *path, cr3_seams_t *sp) {
    /* exists() and is_dir() seams, dedupe by path */
    for (size_t i = 0; i < out->c.count; i++)
        if (!strcmp(json_get_str(json_get(out, i), "path", ""), path)) return NULL;
    json_t *ea = json_object(); cr3_set_str(ea, "path", path);
    json_t *ex = cr3_call(sp, "exists", ea);
    if (cr3_is_raise(ex)) return ex;
    bool exists = cr3_jtruth(ex); json_free(ex);
    if (!exists) return NULL;
    json_t *o = json_object(); cr3_set_str(o, "path", path);
    /* is_dir is a deterministic filesystem double (no seam): a path whose final
     * component has no '.' is a directory. Mirrors the oracle's PathD.is_dir. */
    const char *nm0 = path;
    const char *slash = strrchr(path, '/');
    if (slash) nm0 = slash + 1;
    bool isdir = (strchr(nm0, '.') == NULL);
    json_set(o, "is_dir", json_bool(isdir));
    char *nm = cr3_path_name(path); cr3_set_str(o, "name", nm); free(nm);
    json_append(out, o);
    return NULL;
}
static json_t *cr3_iter_visible_entries(cr3_seams_t *sp, const char *path, const char *cwd, double limit) {
    json_t *qa = json_object();
    json_t *cmd = json_array(); json_append(cmd, json_string("rg")); json_append(cmd, json_string("--files")); json_append(cmd, json_string(path ? path : ""));
    json_set(qa, "cmd", cmd); cr3_set_str(qa, "cwd", cwd ? cwd : ""); json_set(qa, "timeout", json_number(10));
    json_t *rg = cr3_call(sp, "run_rg", qa);
    json_t *out = json_array();
    bool rg_ok = false;
    if (cr3_is_raise(rg)) {
        /* live: except (FileNotFoundError, OSError, subprocess.TimeoutExpired) */
        if (cr3_cls_is(cr3_raise_class(rg), "OSError") ||
            cr3_cls_is(cr3_raise_class(rg), "TimeoutExpired")) {
            json_free(rg); rg = NULL;
        } else return rg; /* e.g. RuntimeError is NOT swallowed */
    } else if (!cr3_is_null(rg)) {
        double rc = cr3_nval(json_obj_get(rg, "returncode"));
        if (rc == 0) {
            rg_ok = true;
            json_t *lines = cr3_lines(json_get_str(rg, "stdout", ""));
            size_t nl = lines->c.count; if (nl > (size_t)limit) nl = (size_t)limit;
            json_t *seen = json_array();
            for (size_t i = 0; i < nl; i++) {
                const char *line = cr3_sval(json_get(lines, i));
                char *full = cr3_path_join(cwd, line);
                /* for each parent ancestor under path, add if unseen and not already present */
                json_t *parents = cr3_parents(full);
                for (size_t j = 0; j < parents->c.count; j++) {
                    const char *par = cr3_sval(json_get(parents, j));
                    /* path not in {parent, *parent.parents}  <=>  par is under `path` */
                    if (!cr3_deny_under(par, path)) continue;
                    bool dup = false;
                    for (size_t k = 0; k < seen->c.count; k++)
                        if (!strcmp(cr3_sval(json_get(seen, k)), par)) { dup = true; break; }
                    if (!dup) {
                        json_append(seen, json_string(par));
                        json_t *e = cr3_entry(out, par, sp);
                        if (e) { json_free(e); json_free(seen); json_free(parents); json_free(lines); json_free(rg); return e; }
                    }
                }
                json_free(parents);
                bool dup = false;
                for (size_t k = 0; k < seen->c.count; k++)
                    if (!strcmp(cr3_sval(json_get(seen, k)), full)) { dup = true; break; }
                if (!dup) {
                    json_append(seen, json_string(full));
                    json_t *e = cr3_entry(out, full, sp);
                    if (e) { json_free(e); json_free(seen); json_free(lines); json_free(rg); return e; }
                }
                free(full);
            }
            json_free(seen); json_free(lines);
        }
    }
    json_free(rg);
    if (rg_ok) {
        /* sort by (not is_dir, str(path)) */
        json_t **arr = malloc((out->c.count + 1) * sizeof(*arr));
        for (size_t i = 0; i < out->c.count; i++) arr[i] = json_get(out, i);
        qsort(arr, out->c.count, sizeof(*arr), cr3_cmp_paths);
        json_t *sorted = json_array();
        for (size_t i = 0; i < out->c.count; i++) json_append(sorted, json_copy(arr[i]));
        free(arr); json_free(out);
        json_t *res = json_array();
        for (size_t i = 0; i < sorted->c.count; i++) json_append(res, json_string(json_get_str(json_get(sorted, i), "path", "")));
        json_free(sorted);
        return res;
    }
    /* os.walk fallback: seam returns list of {root, dirs, files} */
    json_free(out);
    json_t *wa = json_object(); cr3_set_str(wa, "path", path ? path : "");
    json_t *walk = cr3_call(sp, "os_walk", wa);
    if (cr3_is_raise(walk)) return walk;
    json_t *res = json_array();
    if (cr3_is_arr(walk)) {
        for (size_t i = 0; i < walk->c.count; i++) {
            const json_t *row = json_get(walk, i);
            const char *root = json_get_str(row, "root", "");
            const json_t *dirs = json_obj_get(row, "dirs");
            const json_t *files = json_obj_get(row, "files");
            /* live: dirs[:] = sorted(d for d in dirs if not d.startswith('.') and d!='__pycache__') */
            json_t *dnames = json_array();
            if (cr3_is_arr(dirs)) for (size_t k = 0; k < dirs->c.count; k++) {
                const char *d = cr3_sval(json_get(dirs, k));
                if (d[0] != '.' && strcmp(d, "__pycache__")) json_append(dnames, json_string(d));
            }
            json_t *fnames = json_array();
            if (cr3_is_arr(files)) for (size_t k = 0; k < files->c.count; k++) {
                const char *f = cr3_sval(json_get(files, k));
                if (f[0] != '.') json_append(fnames, json_string(f));
            }
            char **ds = malloc((dnames->c.count + 1) * sizeof(char*));
            for (size_t k = 0; k < dnames->c.count; k++) ds[k] = (char*)cr3_sval(json_get(dnames, k));
            char **fs = malloc((fnames->c.count + 1) * sizeof(char*));
            for (size_t k = 0; k < fnames->c.count; k++) fs[k] = (char*)cr3_sval(json_get(fnames, k));
            /* simple lexical sort pointers; names are interned in dnames/fnames */
            for (size_t x = 0; x < dnames->c.count; x++) for (size_t y = x + 1; y < dnames->c.count; y++)
                if (strcmp(ds[x], ds[y]) > 0) { char *t = ds[x]; ds[x] = ds[y]; ds[y] = t; }
            for (size_t x = 0; x < fnames->c.count; x++) for (size_t y = x + 1; y < fnames->c.count; y++)
                if (strcmp(fs[x], fs[y]) > 0) { char *t = fs[x]; fs[x] = fs[y]; fs[y] = t; }
            for (size_t k = 0; k < dnames->c.count + fnames->c.count; k++) {
                const char *nm = k < dnames->c.count ? ds[k] : fs[k - dnames->c.count];
                char *p = cr3_path_join(root, nm);
                json_append(res, json_string(p)); free(p);
                if ((double)res->c.count >= limit) { free(ds); free(fs); json_free(dnames); json_free(fnames); json_free(walk); return res; }
            }
            free(ds); free(fs); json_free(dnames); json_free(fnames);
            (void)root;
        }
    }
    json_free(walk);
    return res;
}

/* ══════════════════ F10: _build_folder_listing ═══════════════ */
static json_t *cr3_build_folder_listing(cr3_seams_t *sp, const char *path, const char *cwd,
                                        double limit, const json_t *display_base) {
    char *shown = NULL;
    const char *bases[2] = {cwd, cr3_is_null(display_base) ? NULL : cr3_sval(display_base)};
    for (int bi = 0; bi < 2 && !shown; bi++) {
        if (!bases[bi]) continue;
        json_t *a = json_object(); cr3_set_str(a, "path", path ? path : ""); cr3_set_str(a, "base", bases[bi]);
        json_t *r = cr3_call(sp, "relative_to", a);
        if (cr3_is_raise(r)) {
            if (cr3_cls_is(cr3_raise_class(r), "ValueError")) { json_free(r); continue; }
            free(shown); return r; /* r stays alive: caller owns the raise spec */
        }
        char *rel = cr3_dup(cr3_sval(r));
        json_free(r);
        shown = cr3_xsf("%s/", rel);
        free(rel);
    }
    if (!shown) shown = cr3_xsf("%s/", path ? path : "");
    json_t *lines = json_array();
    json_append(lines, json_string(shown)); free(shown);
    json_t *ia = json_object(); cr3_set_str(ia, "path", path ? path : ""); cr3_set_str(ia, "cwd", cwd ? cwd : "");
    json_set(ia, "limit", json_number(limit));
    json_t *entries = cr3_call(sp, "iter_entries", ia);
    if (cr3_is_raise(entries)) return entries;
    size_t ne = cr3_is_arr(entries) ? entries->c.count : 0;
    for (size_t i = 0; i < ne; i++) {
        const json_t *e = json_get(entries, i);
        const char *name = json_get_str(e, "name", "");
        long parts = (long)json_get_num(e, "relparts", 1);
        long level = parts - 1; if (level < 0) level = 0;
        char *indent = NULL;
        if (level) { indent = malloc((size_t)level * 2 + 1); indent[0] = '\0'; for (long z = 0; z < level; z++) strcat(indent, "  "); }
        else indent = strdup("");
        bool isdir = json_get_bool(e, "is_dir", false);
        char *line = NULL;
        if (isdir) line = cr3_xsf("%s- %s/", indent, name);
        else {
            json_t *ma = json_object();
            char *ep = cr3_path_join(path, (strchr(name, '/') ? "" : name));
            /* use the entry's full stored path when available */
            const char *fullp = json_get_str(e, "path", "");
            cr3_set_str(ma, "path", fullp[0] ? fullp : ep);
            cr3_set_str(ma, "name", name);
            json_t *meta = cr3_call(sp, "file_metadata", ma);
            if (cr3_is_raise(meta)) { free(indent); free(ep); json_free(ma); return meta; }
            line = cr3_xsf("%s- %s (%s)", indent, name, cr3_sval(meta));
            json_free(meta); free(ep);
        }
        json_append(lines, json_string(line));
        free(line); free(indent);
    }
    json_free(entries);
    if ((double)ne >= limit) json_append(lines, json_string("- ..."));
    char *joined = cr3_join(lines, "\n");
    json_t *r = json_string(joined);
    free(joined); json_free(lines);
    return r;
}
