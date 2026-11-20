/* lifecycle_guard_flow2.c — FLOW port of the UNCLAIMED remainder of
 * cron/lifecycle_guard.py. The 29 CLAIMED siblings (in src/cron/lifecycle_guard.c
 * / lifecycle_guard_gaps.c) and every external/stdlib operation (shlex, Path,
 * os, sqlite-safe-read, heredoc helpers, logger) are SEAMED via lg2_call; the
 * ported defs call each other REAL. The harness scripts every seam and records
 * the ordered {ev,args,ret} trace the oracle diffs against the live module.
 *
 * Representation contract (shared with tests/oracle_lifecycle_guard_flow2.py):
 *   - None -> JSON null; tuple -> array; Path -> its string spelling; bytes ->
 *     ASCII string in the fixture domain; integral doubles fold to ints.
 *   - scripts: absent key -> None; {"$seq":[...]} per-call queue (exhaustion
 *     raises RuntimeError("script exhausted: <name>"));
 *     {"__raise__":cls,"msg":m} raises a real exception (trace ret is the WHOLE
 *     marker); any other value is reused for every call.
 *   - None-valued seam-arg keys are dropped on BOTH sides ("" is kept).
 *   - except Exception never catches SystemExit/KeyboardInterrupt/GeneratorExit.
 *   - FULL builtin exception parent table: every common builtin maps up to
 *     Exception -> BaseException; only the BaseException trio maps straight to
 *     BaseException so it propagates through `except Exception`.
 *   - fixture domain: read data / paths / heredoc text are ASCII; regex uses
 *     PCRE2 (Python re compatible for the patterns this module compiles).
 *
 * libjson-only (plus pcre2, which the oracle runner links). Self-contained:
 * harness #includes "cron/lifecycle_guard_flow2.c". All symbols are static
 * with the lg2_ prefix.
 */
#include "hermes_json.h"
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>

typedef struct lg2_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *env;       /* borrowed: scripted constants/flags */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} lg2_ctx;

/* ── tiny value helpers ──────────────────────────────────────────────────── */
static bool lg2_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool lg2_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool lg2_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool lg2_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool lg2_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool lg2_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool lg2_boolean(const json_t *v) { return v && v->type == JSON_BOOL; }
static const char *lg2_sval(const json_t *v) { return lg2_str(v) && v->str_val ? v->str_val : ""; }
static double lg2_nval(const json_t *v) { return lg2_num(v) ? v->num_val : 0.0; }
static bool lg2_bval(const json_t *v) { return lg2_boolean(v) && v->bool_val; }
static json_t *lg2_get(const json_t *o, const char *k) { return lg2_obj(o) ? json_obj_get(o, k) : NULL; }
static json_t *lg2_null(void) { return json_null(); }
static json_t *lg2_bool(bool b) { return json_bool(b); }
static json_t *lg2_strnode(const char *s) { return json_string(s ? s : ""); }
static json_t *lg2_intnode(long n) { return json_number((double)n); }

static bool lg2_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: return v->c.count > 0;
        case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

/* ── exception plumbing (FULL builtin hierarchy) ────────────────────────── */
static bool lg2_exc_pending(const lg2_ctx *ctx) { return ctx && ctx->exc != NULL; }
static void lg2_exc_clear(lg2_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}
static void lg2_raise(lg2_ctx *ctx, const char *cls, const char *msg) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
    json_set(ctx->exc, "msg", json_string(msg ? msg : ""));
}
static const char *lg2_exc_class(const lg2_ctx *ctx) {
    return json_get_str(ctx->exc, "__raise__", "");
}
static const char *lg2_exc_msg(const lg2_ctx *ctx) {
    return json_get_str(ctx->exc, "msg", "");
}
/* Complete parent chain. Only the BaseException trio maps straight to
 * BaseException; everything else ultimately reaches Exception. */
static const char *lg2_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"},
        {"Exception", "BaseException"},
        {"BaseException", NULL},
        {"RuntimeError", "Exception"}, {"NotImplementedError", "RuntimeError"},
        {"RecursionError", "RuntimeError"},
        {"ValueError", "Exception"}, {"UnicodeError", "ValueError"},
        {"UnicodeDecodeError", "UnicodeError"}, {"UnicodeEncodeError", "UnicodeError"},
        {"UnicodeTranslateError", "UnicodeError"},
        {"JSONDecodeError", "ValueError"},
        {"TypeError", "Exception"}, {"AttributeError", "Exception"},
        {"NameError", "Exception"}, {"UnboundLocalError", "NameError"},
        {"LookupError", "Exception"}, {"IndexError", "LookupError"},
        {"KeyError", "LookupError"},
        {"ArithmeticError", "Exception"}, {"ZeroDivisionError", "ArithmeticError"},
        {"OverflowError", "ArithmeticError"}, {"FloatingPointError", "ArithmeticError"},
        {"OSError", "Exception"}, {"BlockingIOError", "OSError"},
        {"ChildProcessError", "OSError"}, {"ConnectionError", "OSError"},
        {"BrokenPipeError", "ConnectionError"}, {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
        {"FileExistsError", "OSError"}, {"FileNotFoundError", "OSError"},
        {"InterruptedError", "OSError"}, {"IsADirectoryError", "OSError"},
        {"NotADirectoryError", "OSError"}, {"PermissionError", "OSError"},
        {"ProcessLookupError", "OSError"}, {"TimeoutError", "OSError"},
        {"EnvironmentError", "OSError"}, {"IOError", "OSError"}, {"WindowsError", "OSError"},
        {"StopIteration", "Exception"}, {"StopAsyncIteration", "Exception"},
        {"AssertionError", "Exception"}, {"SyntaxError", "Exception"},
        {"IndentationError", "SyntaxError"}, {"TabError", "IndentationError"},
        {"SystemError", "Exception"}, {"ReferenceError", "Exception"},
        {"MemoryError", "Exception"}, {"BufferError", "Exception"},
        {"EOFError", "Exception"}, {"ImportError", "Exception"},
        {"ModuleNotFoundError", "ImportError"},
        {"Warning", "Exception"}, {"UserWarning", "Warning"},
        {"DeprecationWarning", "Warning"}, {"PendingDeprecationWarning", "Warning"},
        {"SyntaxWarning", "Warning"}, {"RuntimeWarning", "Warning"},
        {"FutureWarning", "Warning"}, {"ImportWarning", "Warning"},
        {"UnicodeWarning", "Warning"}, {"BytesWarning", "Warning"},
        {"ResourceWarning", "Warning"},
        {"StopAsyncIterationError", "StopAsyncIteration"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (lg2_str_eq(T[i].c, c)) return T[i].p;
    return NULL;
}
static bool lg2_exc_matches(const lg2_ctx *ctx, const char *const *classes, size_t n) {
    if (!lg2_exc_pending(ctx)) return false;
    const char *cls = lg2_exc_class(ctx);
    for (int depth = 0; depth < 32 && cls; depth++) {
        for (size_t i = 0; i < n; i++) if (lg2_str_eq(cls, classes[i])) return true;
        cls = lg2_exc_parent(cls);
    }
    return false;
}
static bool lg2_is_base_exc(const lg2_ctx *ctx) {
    static const char *B[] = {"SystemExit", "KeyboardInterrupt", "GeneratorExit"};
    return lg2_exc_matches(ctx, B, 3);
}
static bool lg2_exc_is_exception(const lg2_ctx *ctx) {
    static const char *E[] = {"Exception"};
    return lg2_exc_matches(ctx, E, 1);
}
static bool lg2_exc_is(const lg2_ctx *ctx, const char *cls) {
    return lg2_exc_matches(ctx, &cls, 1);
}
static bool lg2_exc_is_oserror(const lg2_ctx *ctx) {
    static const char *E[] = {"OSError"};
    return lg2_exc_matches(ctx, E, 1);
}
static bool lg2_exc_is_value_or_runtime(const lg2_ctx *ctx) {
    static const char *E[] = {"ValueError", "RuntimeError"};
    return lg2_exc_matches(ctx, E, 2);
}

/* ── universal seam ──────────────────────────────────────────────────────── */
static json_t *lg2_script_result(lg2_ctx *ctx, const json_t *script) {
    if (!script || script->type == JSON_NULL) return NULL;
    if (script->type == JSON_OBJECT) {
        json_t *r = json_obj_get(script, "__raise__");
        if (r && r->type == JSON_STRING) {
            if (ctx->exc) json_free(ctx->exc);
            ctx->exc = json_copy(script);
            return NULL;
        }
    }
    return json_copy(script);
}
/* args ownership is TRANSFERRED (NULL -> empty object). Result is NEW; a
 * scripted raise parks the marker in ctx->exc and returns NULL. */
static json_t *lg2_call(lg2_ctx *ctx, const char *name, json_t *args) {
    if (!args) args = json_object();
    json_t *ret = NULL;
    json_t *script = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *chosen = script;
    if (script && script->type == JSON_OBJECT) {
        json_t *seq = json_obj_get(script, "$seq");
        if (seq && seq->type == JSON_ARRAY) {
            double cur = json_get_num(ctx->counters, name, 0);
            size_t idx = (size_t)cur;
            if (idx >= seq->c.count) {
                if (ctx->exc) json_free(ctx->exc);
                ctx->exc = json_object();
                json_set(ctx->exc, "__raise__", json_string("RuntimeError"));
                char msg[256];
                snprintf(msg, sizeof msg, "script exhausted: %s", name);
                json_set(ctx->exc, "msg", json_string(msg));
                chosen = ctx->exc;
            } else {
                json_set(ctx->counters, name, json_number(cur + 1));
                chosen = seq->c.items[idx];
            }
        }
    }
    if (chosen != ctx->exc) ret = lg2_script_result(ctx, chosen);
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args);
    if (ctx->exc && (chosen == ctx->exc ||
        (chosen && chosen->type == JSON_OBJECT && json_obj_get(chosen, "__raise__"))))
        json_set(ev, "ret", json_copy(ctx->exc));
    else
        json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(ctx->trace, ev);
    return ret;
}
static void lg2_arg(json_t *args, const char *key, const json_t *val) {
    if (!val || val->type == JSON_NULL) return;
    json_set(args, key, json_copy(val));
}
static void lg2_arg_str(json_t *args, const char *key, const char *val) {
    if (!val) return;
    json_set(args, key, json_string(val));
}
static void lg2_arg_bool(json_t *args, const char *key, bool val) {
    json_set(args, key, json_bool(val));
}
static void lg2_arg_num(json_t *args, const char *key, double val) {
    json_set(args, key, json_number(val));
}
static json_t *lg2_exc_arg(const lg2_ctx *ctx) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(lg2_exc_class(ctx)));
    json_set(e, "msg", json_string(lg2_exc_msg(ctx)));
    return e;
}
static void lg2_log(lg2_ctx *ctx, const char *level, const char *msg, json_t *args_arr, bool exc_info) {
    json_t *a = json_object();
    json_set(a, "msg", json_string(msg));
    json_set(a, "args", args_arr ? args_arr : json_array());
    if (exc_info) json_set(a, "exc_info", json_bool(true));
    char name[48];
    snprintf(name, sizeof name, "logger.%s", level);
    json_t *r = lg2_call(ctx, name, a);
    json_free(r);
}
static json_t *lg2_pair(json_t *a, json_t *b) {
    json_t *r = json_array();
    json_append(r, a ? a : json_null());
    json_append(r, b ? b : json_null());
    return r;
}


/* ── string helpers (all caller-frees where noted) ──────────────────────── */
static bool lg2_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
        || (c >= 0x1c && c <= 0x1f);
}
static char *lg2_strip(const char *s) {
    if (!s) return strdup("");
    const unsigned char *b = (const unsigned char *)s;
    while (*b && lg2_is_ws(*b)) b++;
    const unsigned char *e = b + strlen((const char *)b);
    while (e > b && lg2_is_ws(e[-1])) e--;
    size_t n = (size_t)(e - b);
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, b, n); r[n] = '\0';
    return r;
}
static char *lg2_strip_chars(const char *s, const char *chars) {
    if (!s) return strdup("");
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && strchr(chars, s[a])) a++;
    while (b > a && strchr(chars, s[b - 1])) b--;
    char *r = (char *)malloc(b - a + 1);
    if (!r) return NULL;
    memcpy(r, s + a, b - a); r[b - a] = '\0';
    return r;
}
static char *lg2_lstrip_chars(const char *s, const char *chars) {
    if (!s) return strdup("");
    size_t n = strlen(s), a = 0;
    while (a < n && strchr(chars, s[a])) a++;
    return strdup(s + a);
}
static char *lg2_lower_dup(const char *s) {
    char *r = strdup(s ? s : "");
    if (!r) return NULL;
    for (char *p = r; *p; p++) *p = (char)tolower((unsigned char)*p);
    return r;
}
static char *lg2_upper_dup(const char *s) {
    char *r = strdup(s ? s : "");
    if (!r) return NULL;
    for (char *p = r; *p; p++) *p = (char)toupper((unsigned char)*p);
    return r;
}
static bool lg2_ends_with(const char *s, const char *suf) {
    if (!s || !suf) return false;
    size_t a = strlen(s), b = strlen(suf);
    return b <= a && strcmp(s + a - b, suf) == 0;
}
static bool lg2_starts_with(const char *s, const char *pre) {
    return s && pre && strncmp(s, pre, strlen(pre)) == 0;
}
/* Python str.removesuffix / removeprefix (caller frees). */
static char *lg2_remove_suffix(const char *s, const char *suf) {
    if (!s) return strdup("");
    if (suf && *suf && lg2_ends_with(s, suf)) {
        size_t n = strlen(s) - strlen(suf);
        char *r = (char *)malloc(n + 1);
        if (!r) return NULL;
        memcpy(r, s, n); r[n] = '\0'; return r;
    }
    return strdup(s);
}
static char *lg2_remove_prefix(const char *s, const char *pre) {
    if (!s) return strdup("");
    if (pre && *pre && lg2_starts_with(s, pre)) return strdup(s + strlen(pre));
    return strdup(s);
}
static size_t lg2_count_char(const char *s, char ch) {
    size_t n = 0;
    if (s) for (const char *p = s; *p; p++) if (*p == ch) n++;
    return n;
}
static bool lg2_contains(const char *s, const char *sub) {
    return s && sub && *sub && strstr(s, sub) != NULL;
}
/* Python str.partition(sep) -> 3 caller-freed strings. sep is one byte. */
static void lg2_partition(const char *s, char sep, char **a, char **b, char **c) {
    const char *p = s ? strchr(s, sep) : NULL;
    if (!p) { *a = strdup(s ? s : ""); *b = strdup(""); *c = strdup(""); return; }
    size_t n = (size_t)(p - s);
    *a = (char *)malloc(n + 1); memcpy(*a, s, n); (*a)[n] = '\0';
    *b = (char *)malloc(2); (*b)[0] = sep; (*b)[1] = '\0';
    *c = strdup(p + 1);
}
/* Split into a JSON array of strings (Python s.split(ch)). */
static json_t *lg2_split_char(const char *s, char ch) {
    json_t *out = json_array();
    if (!s) return out;
    const char *start = s;
    for (const char *p = s; ; p++) {
        if (*p == ch || *p == '\0') {
            size_t n = (size_t)(p - start);
            char *t = (char *)malloc(n + 1);
            if (t) { memcpy(t, start, n); t[n] = '\0'; json_append(out, json_string(t)); free(t); }
            if (*p == '\0') break;
            start = p + 1;
        }
    }
    return out;
}
/* str.split() on ASCII whitespace (fixture domain) -> JSON array of strings. */
static json_t *lg2_split_ws(const char *s) {
    json_t *out = json_array();
    if (!s) return out;
    const char *p = s;
    while (*p) {
        while (*p && lg2_is_ws((unsigned char)*p)) p++;
        if (!*p) break;
        const char *a = p;
        while (*p && !lg2_is_ws((unsigned char)*p)) p++;
        size_t n = (size_t)(p - a);
        char *t = (char *)malloc(n + 1);
        if (t) { memcpy(t, a, n); t[n] = '\0'; json_append(out, json_string(t)); free(t); }
    }
    return out;
}
/* Python str.isascii() and str.isdecimal() (ASCII digits only for fixtures). */
static bool lg2_is_ascii_decimal(const char *s) {
    if (!s || !*s) return false;
    for (const char *p = s; *p; p++) if (*p < '0' || *p > '9') return false;
    return true;
}
/* Python Path(token).name: last component; "" for "" / "." / ".." / "/". */
static const char *lg2_path_name_ptr(const char *token) {
    if (!token || !*token) return "";
    const char *end = token + strlen(token);
    while (end > token && end[-1] == '/') end--;
    if (end == token) return "";
    const char *slash = NULL;
    for (const char *p = token; p < end; p++) if (*p == '/') slash = p;
    const char *base = slash ? slash + 1 : token;
    if (lg2_str_eq(base, ".") || lg2_str_eq(base, "..")) return "";
    return base;
}
static char *lg2_path_name(const char *token) { return strdup(lg2_path_name_ptr(token)); }
static bool lg2_path_is_absolute(const char *s) {
    return s && ((s[0] == '/') || (strlen(s) >= 3 && isalpha((unsigned char)s[0]) && s[1] == ':' && s[2] == '\\'));
}
/* POSIX-ish join mirroring pathlib for our ASCII fixture domain. */
static char *lg2_path_join(const char *a, const char *b) {
    if (!a || !*a) return strdup(b ? b : "");
    if (!b || !*b) return strdup(a);
    size_t na = strlen(a), nb = strlen(b);
    bool need = a[na - 1] != '/' && b[0] != '/';
    char *r = (char *)malloc(na + nb + 2);
    if (!r) return NULL;
    memcpy(r, a, na);
    if (need) r[na++] = '/';
    memcpy(r + na, b, nb + 1);
    return r;
}
/* Python Path.parts -> JSON array of component strings (ASCII fixture domain). */
static json_t *lg2_path_parts(const char *s) {
    json_t *out = json_array();
    if (!s || !*s) return out;
    const char *p = s;
    if (*p == '/') { json_append(out, json_string("/")); while (*p == '/') p++; }
    while (*p) {
        const char *a = p;
        while (*p && *p != '/') p++;
        size_t n = (size_t)(p - a);
        if (n) {
            char *t = (char *)malloc(n + 1);
            if (t) { memcpy(t, a, n); t[n] = '\0'; json_append(out, json_string(t)); free(t); }
        }
        while (*p == '/') p++;
    }
    return out;
}

/* ── PCRE2 wrapper ───────────────────────────────────────────────────────── */
#define LG2_RE_CACHE 96
static struct { const char *pat; pcre2_code *code; } lg2_re_cache[LG2_RE_CACHE];
static int lg2_re_cache_n = 0;
/* Translate Python \uXXXX into PCRE2 \x{XXXX}; return malloc'd pattern. */
static char *lg2_re_translate(const char *pat) {
    size_t n = strlen(pat);
    char *out = (char *)malloc(n * 8 + 8);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        if (pat[i] == '\\' && i + 5 < n && pat[i + 1] == 'u') {
            char hex[5] = { pat[i+2], pat[i+3], pat[i+4], pat[i+5], 0 };
            int ok = 1;
            for (int k = 0; k < 4; k++) if (!isxdigit((unsigned char)hex[k])) ok = 0;
            if (ok) {
                o += (size_t)snprintf(out + o, n * 8 + 8 - o, "\\x{%s}", hex);
                i += 5;
                continue;
            }
        }
        out[o++] = pat[i];
    }
    out[o] = '\0';
    return out;
}
static pcre2_code *lg2_re_compile(const char *pat) {
    for (int i = 0; i < lg2_re_cache_n; i++)
        if (lg2_re_cache[i].pat == pat) return lg2_re_cache[i].code;
    char *tr = lg2_re_translate(pat);
    int err; PCRE2_SIZE off;
    uint32_t opts = PCRE2_UTF | PCRE2_UCP;
    pcre2_code *code = pcre2_compile((PCRE2_SPTR)tr, PCRE2_ZERO_TERMINATED, opts, &err, &off, NULL);
    free(tr);
    if (!code) return NULL;
    if (lg2_re_cache_n < LG2_RE_CACHE) {
        lg2_re_cache[lg2_re_cache_n].pat = pat;
        lg2_re_cache[lg2_re_cache_n].code = code;
        lg2_re_cache_n++;
    }
    return code;
}
typedef struct { char *g[4]; PCRE2_SIZE start, end; bool ok; } lg2_match;
static void lg2_match_free(lg2_match *m) {
    for (int i = 0; i < 4; i++) { free(m->g[i]); m->g[i] = NULL; }
    m->ok = false;
}
/* Python pattern.search(s) (anchored=false) or pattern.match(s) (anchored=true). */
static bool lg2_re_find(const char *pat, const char *s, bool anchored, size_t startoff,
                        lg2_match *m, int ncap) {
    memset(m, 0, sizeof(*m));
    pcre2_code *code = lg2_re_compile(pat);
    if (!code || !s) return false;
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(code, NULL);
    uint32_t opts = anchored ? PCRE2_ANCHORED : 0;
    int rc = pcre2_match(code, (PCRE2_SPTR)s, PCRE2_ZERO_TERMINATED, startoff, opts, md, NULL);
    if (rc < 0) { pcre2_match_data_free(md); return false; }
    PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);
    m->ok = true;
    m->start = ov[0]; m->end = ov[1];
    int nc = rc - 1;
    for (int i = 0; i <= nc && i < 4; i++) {
        if (ov[2*i] == PCRE2_UNSET) { m->g[i] = NULL; continue; }
        size_t len = (size_t)(ov[2*i+1] - ov[2*i]);
        m->g[i] = (char *)malloc(len + 1);
        if (m->g[i]) { memcpy(m->g[i], s + ov[2*i], len); m->g[i][len] = '\0'; }
    }
    (void)ncap;
    pcre2_match_data_free(md);
    return true;
}
static bool lg2_re_search(const char *pat, const char *s, lg2_match *m) {
    return lg2_re_find(pat, s, false, 0, m, 3);
}
static bool lg2_re_match(const char *pat, const char *s, lg2_match *m) {
    return lg2_re_find(pat, s, true, 0, m, 3);
}
static bool lg2_re_bool(const char *pat, const char *s) {
    lg2_match m; bool r = lg2_re_search(pat, s, &m); lg2_match_free(&m); return r;
}
/* re.sub(pat, repl, s) with a fixed ASCII replacement string. */
static char *lg2_re_sub(const char *pat, const char *repl, const char *s) {
    pcre2_code *code = lg2_re_compile(pat);
    if (!code || !s) return strdup(s ? s : "");
    size_t cap = strlen(s) * 4 + 64, used = 0;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t pos = 0, slen = strlen(s);
    while (pos <= slen) {
        lg2_match m;
        if (!lg2_re_find(pat, s, false, pos, &m, 3) || !m.ok) break;
        size_t gs = (size_t)m.start, ge = (size_t)m.end;
        size_t need = used + gs - pos + strlen(repl) + 1;
        if (need > cap) {
            while (need > cap) cap *= 2;
            char *n = (char *)realloc(out, cap);
            if (!n) { free(out); lg2_match_free(&m); return NULL; }
            out = n;
        }
        memcpy(out + used, s + pos, gs - pos); used += gs - pos;
        memcpy(out + used, repl, strlen(repl)); used += strlen(repl);
        pos = ge > gs ? ge : gs + 1;
        if (ge == gs && pos > slen) break;
        lg2_match_free(&m);
    }
    if (pos < slen) {
        size_t need = used + slen - pos + 1;
        if (need > cap) { char *n = (char *)realloc(out, need); if (n) { out = n; cap = need; } }
        memcpy(out + used, s + pos, slen - pos); used += slen - pos;
    }
    out[used] = '\0';
    return out;
}
/* re.findall-style iteration: return JSON array of match group descriptors. */
static json_t *lg2_re_finditer(const char *pat, const char *s, int ncap) {
    json_t *out = json_array();
    if (!s) return out;
    size_t pos = 0, slen = strlen(s);
    while (pos <= slen) {
        lg2_match m;
        if (!lg2_re_find(pat, s, false, pos, &m, ncap) || !m.ok) break;
        json_t *d = json_object();
        for (int i = 0; i <= ncap && i < 4; i++) {
            char key[8]; snprintf(key, sizeof key, "g%d", i);
            json_set(d, key, m.g[i] ? json_string(m.g[i]) : json_null());
        }
        json_append(out, d);
        size_t ge = (size_t)m.end;
        pos = ge > (size_t)m.start ? ge : (size_t)m.start + 1;
        lg2_match_free(&m);
    }
    return out;
}


/* ── module constants (faithful to cron/lifecycle_guard.py) ──────────────── */
#define LG2_MAX_READ_BYTES 1048576
#define LG2_MAX_DEPTH 8
#define LG2_SNIFF_BYTES 4096
static const char *LG2_PAT_INTERPRETER = "^pythonw?(?:\\d+(?:\\.\\d+)*)?(?:\\.exe)?$";
static const char *LG2_PAT_ERE_TAIL = "[.*+?\\\\\\[\\](){}|].*$";
static const char *LG2_PAT_ERE_WILDCARD = "^[.*+?$\\s]*$";
static const char *LG2_PAT_KILL_VERB = "(?i)\\b(?:kill|taskkill|stop-process)\\b";
static const char *LG2_PAT_CMDLINE = "(?i)hermes_cli|\\bhermes\\b[^\\n]*\\bgateway\\b|\\bgateway\\b[^\\n]*\\bhermes\\b";
static const char *LG2_PAT_LINE_CONT = "\\\\\\r?\\n[ \\t]*";
static const char *LG2_PAT_ARGV_PUNCT = "[\\[\\],]+";
static const char *LG2_PAT_REDIRECT = "\\d*(?:<<<|<<-|<<|<>|>>|>\\||<|>)";
static const char *LG2_PAT_DOT_COMMAND = "^\\.[A-Za-z]";
static const char *LG2_PAT_WHATIF_DISQUAL = "(?i)[^\\S \\t]|[;|&{}()@$#`,%\\\\<>\\u2010-\\u201f]|--|powershell|pwsh";
static const char *LG2_PAT_WHATIF_BOUND = "(?i)-(?:wh[a-z]*|wi):";
static const char *LG2_PAT_SUBST = "`([^`]*)`|\\$\\(([^()]*)\\)";
static const char *LG2_PAT_TASKKILL_FILTER = "(?i)\\s*['\"]?imagename\\s+eq\\s+(\\S+)";
static const char *LG2_PAT_GATEWAY_LIFECYCLE =
    "(?i)"
    "(?:(?<![/\\w.\\-])hermes(?:\\.(?:exe|cmd|bat|com|ps1))?\\s+gateway\\s+(?:restart|stop|uninstall)\\b)"
    "|(?:launchctl\\s+(?:kickstart|unload|load|stop|restart|submit|bootstrap|bootout|remove|disable)\\b[^\\n]*\\bhermes[.\\-]?gateway)"
    "|(?:systemctl\\s+(?:-\\S+\\s+)*(?:restart|stop|start)\\b[^\\n]*\\bhermes[.\\-]?gateway)"
    "|(?:\\b(?:p?kill|taskkill|stop-process)\\b[^\\n]*\\bhermes\\b[^\\n]*\\bgateway)"
    "|(?:\\b(?:p?kill|taskkill|stop-process)\\b[^\\n]*\\bgateway\\b[^\\n]*\\bhermes)";

static const char *lg2_host_interp(lg2_ctx *ctx) {
    const char *v = json_get_str(ctx->env, "host_interpreter_name", "python.exe");
    return v && *v ? v : "python.exe";
}
static bool lg2_in_strset(const char *s, const char *const *set, size_t n) {
    for (size_t i = 0; i < n; i++) if (lg2_str_eq(s, set[i])) return true;
    return false;
}
static bool lg2_set_has(const json_t *obj, const char *key) {
    return lg2_obj(obj) && json_obj_get(obj, key) != NULL;
}
static bool lg2_arr_has_str(const json_t *arr, const char *s) {
    if (!lg2_arr(arr)) return false;
    for (size_t i = 0; i < arr->c.count; i++)
        if (lg2_str(arr->c.items[i]) && lg2_str_eq(arr->c.items[i]->str_val, s)) return true;
    return false;
}
static char *lg2_join_tokens(const json_t *arr, const char *sep) {
    size_t cap = 16, used = 0;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';
    if (!lg2_arr(arr)) return out;
    for (size_t i = 0; i < arr->c.count; i++) {
        const char *t = lg2_sval(arr->c.items[i]);
        size_t sl = strlen(sep), tl = strlen(t);
        if (used + sl + tl + 1 > cap) {
            while (used + sl + tl + 1 > cap) cap *= 2;
            char *n = (char *)realloc(out, cap);
            if (!n) { free(out); return NULL; }
            out = n;
        }
        if (i) { memcpy(out + used, sep, sl); used += sl; }
        memcpy(out + used, t, tl); used += tl; out[used] = '\0';
    }
    return out;
}
static char *lg2_replace_literal(const char *s, const char *needle, const char *repl) {
    if (!s || !needle || !*needle) return strdup(s ? s : "");
    size_t cap = strlen(s) * 3 + 16, used = 0;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    const char *p = s;
    while (*p) {
        const char *q = strstr(p, needle);
        size_t seg = q ? (size_t)(q - p) : strlen(p);
        size_t rl = strlen(repl);
        if (used + seg + rl + 1 > cap) {
            while (used + seg + rl + 1 > cap) cap *= 2;
            char *n = (char *)realloc(out, cap);
            if (!n) { free(out); return NULL; }
            out = n;
        }
        memcpy(out + used, p, seg); used += seg;
        if (!q) break;
        memcpy(out + used, repl, rl); used += rl;
        p = q + strlen(needle);
    }
    out[used] = '\0';
    return out;
}
/* Python f"`{path}` ..." style formatter with one substitution. */
static char *lg2_fmt1(const char *fmt, const char *v) {
    const char *p = strstr(fmt, "%s");
    if (!p) return strdup(fmt);
    size_t pre = (size_t)(p - fmt), post = strlen(p + 2);
    size_t n = pre + strlen(v) + post;
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, fmt, pre);
    memcpy(r + pre, v, strlen(v));
    memcpy(r + pre + strlen(v), p + 2, post);
    r[n] = '\0';
    return r;
}

/* ── budget (the _LifecycleScanBudget class) ────────────────────────────── */
static json_t *lg2_budget_new(lg2_ctx *ctx) {
    json_t *b = json_object();
    long bytes = (long)json_get_num(ctx->env, "max_scan_bytes", LG2_MAX_READ_BYTES);
    long lines = (long)json_get_num(ctx->env, "max_scan_lines", 16384);
    long paths = (long)json_get_num(ctx->env, "max_scan_paths", 1024);
    long remote = (long)json_get_num(ctx->env, "max_remote_reads", 64);
    json_set(b, "bytes_remaining", json_number((double)bytes));
    json_set(b, "lines_remaining", json_number((double)lines));
    json_set(b, "paths_remaining", json_number((double)paths));
    json_set(b, "remote_reads_remaining", json_number((double)remote));
    json_set(b, "refusal", json_null());
    json_set(b, "scanned_bodies", json_object());
    return b;
}
static json_t *lg2_budget_charge_text(lg2_ctx *ctx, json_t *budget, const char *text, bool written_body) {
    (void)ctx;
    json_t *scanned = json_obj_get(budget, "scanned_bodies");
    if (written_body && !(scanned && json_obj_get(scanned, text))) {
        if (scanned) json_set(scanned, text, json_bool(true));
        return lg2_bool(true);
    }
    size_t chars = strlen(text ? text : "");
    double remain = json_get_num(budget, "bytes_remaining", 0);
    if ((double)chars > remain) return lg2_bool(false);
    size_t encoded = chars; /* ASCII fixture domain */
    if ((double)encoded > remain) return lg2_bool(false);
    size_t lines = lg2_count_char(text, '\n') + 1;
    if ((double)lines > json_get_num(budget, "lines_remaining", 0)) return lg2_bool(false);
    size_t longest = 0;
    const char *p = text ? text : "";
    size_t cur = 0;
    for (const char *q = p; ; q++) {
        if (*q == '\n' || *q == '\0') { if (cur > longest) longest = cur; cur = 0; if (!*q) break; }
        else cur++;
    }
    long max_line = (long)json_get_num(ctx->env, "max_line_bytes", 65536);
    if ((long)longest > max_line) return lg2_bool(false);
    json_set(budget, "bytes_remaining", json_number(remain - (double)encoded));
    json_set(budget, "lines_remaining", json_number(json_get_num(budget, "lines_remaining", 0) - (double)lines));
    return lg2_bool(true);
}
static json_t *lg2_budget_charge_path(lg2_ctx *ctx, json_t *budget) {
    (void)ctx;
    double n = json_get_num(budget, "paths_remaining", 0);
    if (n <= 0) return lg2_bool(false);
    json_set(budget, "paths_remaining", json_number(n - 1));
    return lg2_bool(true);
}
static json_t *lg2_budget_charge_remote(lg2_ctx *ctx, json_t *budget) {
    (void)ctx;
    double n = json_get_num(budget, "remote_reads_remaining", 0);
    if (n <= 0) return lg2_bool(false);
    json_set(budget, "remote_reads_remaining", json_number(n - 1));
    return lg2_bool(true);
}
static json_t *lg2_budget_exhausted(lg2_ctx *ctx, json_t *budget, const char *what, long depth) {
    json_t *a = json_array();
    json_append(a, json_string(what));
    json_append(a, json_number((double)depth));
    lg2_log(ctx, "warning",
        "lifecycle guard scan budget exhausted (%s at depth %d); failing closed "
        "\xe2\x80\x94 see _MAX_LIFECYCLE_SCAN_* in cron/lifecycle_guard.py", a, false);
    char *reason = NULL;
    size_t n = snprintf(NULL, 0, "the scan budget was exhausted (%s at depth %ld)", what, depth);
    reason = (char *)malloc(n + 1);
    if (reason) {
        snprintf(reason, n + 1, "the scan budget was exhausted (%s at depth %ld)", what, depth);
        json_set(budget, "refusal", json_string(reason));
        free(reason);
    }
    return lg2_bool(true);
}
static json_t *lg2_refuse_unreadable(lg2_ctx *ctx, json_t *budget, const char *path, const char *reason) {
    json_t *a = json_array();
    json_append(a, json_string(path));
    json_append(a, json_string(reason));
    lg2_log(ctx, "warning", "lifecycle guard cannot scan referenced script %s: %s; failing closed", a, false);
    json_set(budget, "refusal", json_string(reason ? reason : ""));
    return lg2_bool(true);
}

/* ── _is_interpreter_image / _pattern_reaches_host_interpreter ──────────── */
static json_t *lg2_is_interpreter_image(lg2_ctx *ctx, const char *value, bool substring);
static bool lg2_is_interp(lg2_ctx *ctx, const char *value, bool substring) {
    json_t *r = lg2_is_interpreter_image(ctx, value, substring);
    bool b = lg2_bval(r);
    json_free(r);
    return b;
}
/* PoP: lg2_is_interpreter_image @ cron/lifecycle_guard.py:_is_interpreter_image */
static json_t *lg2_is_interpreter_image(lg2_ctx *ctx, const char *value, bool substring) {
    char *n1 = lg2_strip(value ? value : "");
    char *n2 = lg2_strip_chars(n1, "\"'");
    char *n3 = lg2_lower_dup(n2);
    char *name = lg2_remove_suffix(n3, ".exe");
    free(n1); free(n2); free(n3);
    if (!name || !*name) { free(name); return lg2_bool(false); }
    size_t nl = strlen(name);
    if (name[nl - 1] == '*') {
        char *prefix = lg2_strip_chars(name, "*");
        bool hit = lg2_starts_with("python", prefix) || lg2_starts_with(lg2_host_interp(ctx), prefix);
        free(prefix); free(name);
        return lg2_bool(hit);
    }
    lg2_match im; bool match = lg2_re_match(LG2_PAT_INTERPRETER, name, &im); lg2_match_free(&im);
    char *hn = lg2_remove_suffix(lg2_host_interp(ctx), ".exe");
    bool eq = lg2_str_eq(name, hn);
    free(hn);
    if (match || eq) { free(name); return lg2_bool(true); }
    bool ok = substring && nl >= 2 && lg2_starts_with("python", name);
    free(name);
    return lg2_bool(ok);
}
/* PoP: lg2_pattern_reaches_host_interpreter @ cron/lifecycle_guard.py:_pattern_reaches_host_interpreter */
static json_t *lg2_pattern_reaches_host_interpreter(lg2_ctx *ctx, const char *pattern, bool full_cmdline, bool exact) {
    char *c1 = lg2_strip(pattern ? pattern : "");
    char *c2 = lg2_strip_chars(c1, "\"'");
    char *c3 = lg2_lstrip_chars(c2, "^");
    char *core = lg2_remove_suffix(c3, "$");
    free(c1); free(c2); free(c3);
    char *head = NULL, *sep = NULL, *rest = NULL;
    if (full_cmdline) lg2_partition(core, ' ', &head, &sep, &rest);
    else { head = strdup(core); sep = strdup(""); rest = strdup(""); }
    free(sep);
    bool head_is_interp = lg2_is_interp(ctx, head, !exact);
    lg2_match m;
    bool found = head_is_interp ? false : lg2_re_search(LG2_PAT_ERE_TAIL, head, &m);
    if (found && m.ok) {
        size_t st = (size_t)m.start;
        size_t hl = strlen(head);
        char *tail = (char *)malloc(hl - st + 1);
        char *nh = (char *)malloc(st + 1);
        if (tail && nh) {
            memcpy(tail, head + st, hl - st); tail[hl - st] = '\0';
            memcpy(nh, head, st); nh[st] = '\0';
            char *nr = (char *)malloc(strlen(tail) + strlen(rest) + 2);
            if (nr) {
                snprintf(nr, strlen(tail) + strlen(rest) + 2, "%s %s", tail, rest);
                free(rest); rest = nr;
                free(head); head = nh; nh = NULL;
            } else { free(nh); }
            free(tail);
        } else { free(tail); free(nh); }
    }
    if (found) lg2_match_free(&m);
    if (!lg2_is_interp(ctx, head, !exact)) {
        bool ok = full_cmdline && lg2_re_bool(LG2_PAT_CMDLINE, core);
        free(core); free(head); free(rest);
        return lg2_bool(ok);
    }
    char *rs = lg2_strip(rest);
    lg2_match wm; memset(&wm, 0, sizeof(wm));
    bool ok = !*rs || lg2_re_match(LG2_PAT_ERE_WILDCARD, rs, &wm);
    lg2_match_free(&wm);
    if (!ok) {
        char *rl = lg2_lower_dup(rs);
        ok = lg2_contains(rl, "hermes");
        free(rl);
    }
    free(rs); free(core); free(head); free(rest);
    return lg2_bool(ok);
}


/* ── claimed-sibling / external seam wrappers ───────────────────────────── */
static json_t *lg2_seam_segments(lg2_ctx *ctx, const char *command) {
    json_t *a = json_object();
    json_set(a, "command", json_string(command ? command : ""));
    json_t *r = lg2_call(ctx, "iter_command_segments", a);
    if (!r || r->type != JSON_ARRAY) { json_free(r); return json_array(); }
    return r;
}
static json_t *lg2_seam_shlex(lg2_ctx *ctx, const char *line, const char *punct) {
    json_t *a = json_object();
    json_set(a, "line", json_string(line ? line : ""));
    json_set(a, "punctuation", json_string(punct ? punct : ";&|()"));
    json_t *r = lg2_call(ctx, "shlex_tokens", a);
    if (!r || r->type != JSON_ARRAY) { json_free(r); return json_array(); }
    return r;
}
static char *lg2_seam_executable_name(lg2_ctx *ctx, const char *token) {
    json_t *a = json_object();
    json_set(a, "token", json_string(token ? token : ""));
    json_t *r = lg2_call(ctx, "executable_name", a);
    char *s = strdup(lg2_sval(r));
    json_free(r);
    return s;
}
static long lg2_seam_index(lg2_ctx *ctx, const char *name, const json_t *segment) {
    json_t *a = json_object();
    json_set(a, "segment", json_copy(segment));
    json_t *r = lg2_call(ctx, name, a);
    long v = (r && r->type == JSON_NUMBER) ? (long)r->num_val : -1;
    json_free(r);
    return v;
}
static char *lg2_seam_str(lg2_ctx *ctx, const char *name, const char *key, const char *val) {
    json_t *a = json_object();
    json_set(a, key, json_string(val ? val : ""));
    json_t *r = lg2_call(ctx, name, a);
    char *s = strdup(lg2_sval(r));
    json_free(r);
    return s;
}
static bool lg2_seam_bool(lg2_ctx *ctx, const char *name, const char *key, const char *val) {
    json_t *a = json_object();
    json_set(a, key, json_string(val ? val : ""));
    json_t *r = lg2_call(ctx, name, a);
    bool b = lg2_bval(r);
    json_free(r);
    return b;
}

/* ── _killer_targets_host_interpreter + helpers ─────────────────────────── */
static bool lg2_killer_value_option(const char *name, const char *tok) {
    static const char *const pkill[] = {
        "-g","--pgroup","-G","--group","-P","--parent","-s","--session","-t","--terminal",
        "-u","--euid","-U","--uid","-F","--pidfile","--ns","--nslist","-d","--delimiter",
        "--signal","-O","--older","-r","--runstates","--cgroup","-A","--ignore-ancestors"};
    static const char *const killall[] = {
        "-s","--signal","-u","--user","-o","--older-than","-y","--younger-than",
        "-n","--ns","-Z","--context"};
    static const char *const pidof[] = {"-o","--omit-pid","-S","--separator"};
    if (lg2_str_eq(name, "pkill") || lg2_str_eq(name, "pgrep")) return lg2_in_strset(tok, pkill, sizeof(pkill)/sizeof(pkill[0]));
    if (lg2_str_eq(name, "killall")) return lg2_in_strset(tok, killall, sizeof(killall)/sizeof(killall[0]));
    if (lg2_str_eq(name, "pidof")) return lg2_in_strset(tok, pidof, sizeof(pidof)/sizeof(pidof[0]));
    return false;
}
static json_t *lg2_killer_targets_host_interpreter(lg2_ctx *ctx, const char *name, const json_t *args);
/* PoP: lg2_killer_targets_host_interpreter @ cron/lifecycle_guard.py:_killer_targets_host_interpreter */
static json_t *lg2_killer_targets_host_interpreter(lg2_ctx *ctx, const char *name, const json_t *args) {
    if (lg2_str_eq(name, "pkill") || lg2_str_eq(name, "pgrep") ||
        lg2_str_eq(name, "killall") || lg2_str_eq(name, "pidof")) {
        bool any_regexp = false, any_full = false;
        size_t n = lg2_arr(args) ? args->c.count : 0;
        for (size_t i = 0; i < n; i++) {
            const char *t = lg2_sval(args->c.items[i]);
            if (lg2_str_eq(t, "-r") || lg2_str_eq(t, "--regexp") || lg2_str_eq(t, "-x") || lg2_str_eq(t, "--exact")) any_regexp = true;
            if (lg2_str_eq(t, "-f") || lg2_str_eq(t, "--full")) any_full = true;
        }
        bool exact = lg2_str_eq(name, "pidof") || ((lg2_str_eq(name, "killall")) != any_regexp);
        bool full = (lg2_str_eq(name, "pkill") || lg2_str_eq(name, "pgrep")) && any_full;
        json_t *operands = json_array();
        size_t pos = 0;
        while (pos < n) {
            const char *tok = lg2_sval(args->c.items[pos]);
            if (lg2_str_eq(tok, "--")) {
                for (size_t i = pos + 1; i < n; i++) json_append(operands, json_copy(args->c.items[i]));
                break;
            }
            if (lg2_killer_value_option(name, tok)) { pos += 2; continue; }
            if (tok[0] != '-') json_append(operands, json_string(tok));
            pos++;
        }
        bool hit = false;
        for (size_t i = 0; i < operands->c.count && !hit; i++) {
            json_t *r = lg2_pattern_reaches_host_interpreter(ctx, lg2_sval(operands->c.items[i]), full, exact);
            hit = lg2_bval(r); json_free(r);
        }
        json_free(operands);
        return lg2_bool(hit);
    }
    if (lg2_str_eq(name, "taskkill")) {
        size_t n = lg2_arr(args) ? args->c.count : 0;
        for (size_t i = 0; i + 1 < n; i++) {
            char *opt0 = lg2_lower_dup(lg2_sval(args->c.items[i]));
            const char *opt = opt0;
            while (*opt == '/' || *opt == '-') opt++;
            const char *val = lg2_sval(args->c.items[i + 1]);
            bool hit = false;
            if (lg2_str_eq(opt, "im")) hit = lg2_is_interp(ctx, val, false);
            if (lg2_str_eq(opt, "fi")) {
                lg2_match fm;
                if (lg2_re_match(LG2_PAT_TASKKILL_FILTER, val, &fm) && fm.g[1]) hit = lg2_is_interp(ctx, fm.g[1], false);
                lg2_match_free(&fm);
            }
            free(opt0);
            if (hit) return lg2_bool(true);
        }
        return lg2_bool(false);
    }
    /* stop-process / get-process */
    json_t *values = json_array();
    size_t n = lg2_arr(args) ? args->c.count : 0;
    for (size_t i = 0; i < n; i++) {
        const char *tok = lg2_sval(args->c.items[i]);
        char *o2 = NULL, *c2 = NULL, *v2 = NULL;
        lg2_partition(tok, ':', &o2, &c2, &v2);
        char *ol = lg2_lower_dup(o2);
        bool is_name_opt = lg2_str_eq(ol, "-name") || lg2_str_eq(ol, "-processname") || lg2_str_eq(ol, "-n");
        if (is_name_opt) {
            if (*v2) json_append(values, json_string(v2));
            else if (i + 1 < n) json_append(values, json_copy(args->c.items[i + 1]));
            else json_append(values, json_string(""));
        } else if (lg2_str_eq(name, "get-process") && tok[0] != '-' &&
                   (i == 0 || lg2_sval(args->c.items[i - 1])[0] != '-')) {
            json_append(values, json_string(tok));
        }
        free(o2); free(c2); free(v2); free(ol);
    }
    bool hit = false;
    for (size_t i = 0; i < values->c.count && !hit; i++) {
        json_t *parts = lg2_split_char(lg2_sval(values->c.items[i]), ',');
        for (size_t j = 0; j < parts->c.count && !hit; j++) hit = lg2_is_interp(ctx, lg2_sval(parts->c.items[j]), false);
        json_free(parts);
    }
    json_free(values);
    return lg2_bool(hit);
}


/* ── shell segmentation (native split; shlex is seamed) ─────────────────── */
/* internal helper (claimed sibling; no PoP claim) */
static json_t *lg2_split_logical_lines(const char *text) {
    json_t *out = json_array();
    size_t cap = 64, len = 0;
    char *cur = (char *)malloc(cap);
    if (!cur) return out;
    bool in_single = false, in_double = false, escape = false;
    for (const char *p = text ? text : ""; *p; p++) {
        char ch = *p;
        if (len + 2 > cap) {
            cap *= 2; char *n = (char *)realloc(cur, cap);
            if (!n) break; cur = n;
        }
        if (escape) { cur[len++] = ch; escape = false; continue; }
        if (ch == '\\') { escape = true; cur[len++] = ch; continue; }
        if (ch == '\'' && !in_double) { in_single = !in_single; cur[len++] = ch; continue; }
        if (ch == '"' && !in_single) { in_double = !in_double; cur[len++] = ch; continue; }
        if (ch == '\n' && !in_single && !in_double) {
            cur[len] = '\0'; json_append(out, json_string(cur)); len = 0; continue;
        }
        cur[len++] = ch;
    }
    if (len > 0) { cur[len] = '\0'; json_append(out, json_string(cur)); }
    free(cur);
    return out;
}
static bool lg2_token_is_control(const char *t) {
    if (!t || !*t) return false;
    for (const char *p = t; *p; p++) if (!strchr(";&|()", *p)) return false;
    return true;
}
/* PoP: lg2_split_segments @ cron/lifecycle_guard.py:_split_segments */
static json_t *lg2_split_segments(const json_t *tokens, bool keep_controls) {
    json_t *out = json_array();
    json_t *seg = json_array();
    size_t n = lg2_arr(tokens) ? tokens->c.count : 0;
    for (size_t i = 0; i < n; i++) {
        const char *tok = lg2_sval(tokens->c.items[i]);
        if (lg2_token_is_control(tok)) {
            if (seg->c.count) { json_append(out, seg); seg = json_array(); }
            if (keep_controls) { json_t *one = json_array(); json_append(one, json_copy(tokens->c.items[i])); json_append(out, one); }
            continue;
        }
        json_append(seg, json_copy(tokens->c.items[i]));
    }
    if (seg->c.count) json_append(out, seg); else json_free(seg);
    return out;
}
/* internal helper (claimed sibling; no PoP claim) */
static json_t *lg2_iter_command_segments(lg2_ctx *ctx, const char *command) {
    json_t *out = json_array();
    char *collapsed = lg2_replace_literal(command ? command : "", "\\\n", "");
    json_t *lines = lg2_split_logical_lines(collapsed);
    free(collapsed);
    for (size_t li = 0; li < lines->c.count; li++) {
        const char *line = lg2_sval(lines->c.items[li]);
        json_t *tokens = lg2_seam_shlex(ctx, line, ";&|()");
        if (lg2_exc_pending(ctx) && lg2_exc_is(ctx, "ValueError")) {
            lg2_exc_clear(ctx);
            json_free(tokens);
            json_t *phys = lg2_split_char(line, '\n');
            for (size_t pi = 0; pi < phys->c.count; pi++) {
                json_t *pt = lg2_seam_shlex(ctx, lg2_sval(phys->c.items[pi]), ";&|()");
                if (lg2_exc_pending(ctx) && lg2_exc_is(ctx, "ValueError")) { lg2_exc_clear(ctx); json_free(pt); continue; }
                json_t *segs = lg2_split_segments(pt, false);
                for (size_t si = 0; si < segs->c.count; si++) json_append(out, json_copy(segs->c.items[si]));
                json_free(segs); json_free(pt);
            }
            json_free(phys);
            continue;
        }
        json_t *segs = lg2_split_segments(tokens, false);
        for (size_t si = 0; si < segs->c.count; si++) json_append(out, json_copy(segs->c.items[si]));
        json_free(segs); json_free(tokens);
    }
    json_free(lines);
    return out;
}
/* PoP: lg2_executed_command_index @ cron/lifecycle_guard.py:_executed_command_index */
static long lg2_executed_command_index(lg2_ctx *ctx, const json_t *segment) {
    long idx = lg2_seam_index(ctx, "command_token_index", segment);
    if (idx < 0) return -1;
    long peeled = lg2_seam_index(ctx, "peel_transparent_prefixes", segment);
    return (peeled < (long)(lg2_arr(segment) ? segment->c.count : 0)) ? peeled : -1;
}
static char *lg2_exe_base(lg2_ctx *ctx, const char *token) {
    char *raw = lg2_seam_executable_name(ctx, token);
    char *lo = lg2_lower_dup(raw);
    char *base = lg2_remove_suffix(lo, ".exe");
    free(raw); free(lo);
    return base;
}
static bool lg2_in_name_set(const char *name, bool killers) {
    static const char *const K[] = {"pkill","killall","taskkill","stop-process"};
    static const char *const E[] = {"pgrep","pidof","get-process"};
    return killers ? lg2_in_strset(name, K, 4) : lg2_in_strset(name, E, 3);
}
/* PoP: lg2_segment_names_host_interpreter @ cron/lifecycle_guard.py:_segment_names_host_interpreter */
static json_t *lg2_segment_names_host_interpreter(lg2_ctx *ctx, const json_t *tokens, bool killers) {
    size_t n = lg2_arr(tokens) ? tokens->c.count : 0;
    long idx = lg2_executed_command_index(ctx, tokens);
    if (lg2_exc_pending(ctx)) return NULL;
    json_t *candidates = json_array();
    if (idx >= 0) json_append(candidates, json_number((double)idx));
    for (size_t i = 0; i < n; i++) {
        char *base = lg2_exe_base(ctx, lg2_sval(tokens->c.items[i]));
        if (lg2_exc_pending(ctx)) { free(base); json_free(candidates); return NULL; }
        if (lg2_in_name_set(base, killers)) json_append(candidates, json_number((double)i));
        free(base);
    }
    for (size_t c = 0; c < candidates->c.count; c++) {
        size_t pos = (size_t)lg2_nval(candidates->c.items[c]);
        char *base = lg2_exe_base(ctx, lg2_sval(tokens->c.items[pos]));
        if (lg2_exc_pending(ctx)) { free(base); json_free(candidates); return NULL; }
        bool in = lg2_in_name_set(base, killers);
        if (!in) { free(base); continue; }
        json_t *rest = json_array();
        for (size_t i = pos + 1; i < n; i++) json_append(rest, json_copy(tokens->c.items[i]));
        json_t *hit = lg2_killer_targets_host_interpreter(ctx, base, rest);
        if (lg2_exc_pending(ctx)) { json_free(hit); json_free(rest); free(base); json_free(candidates); return NULL; }
        bool b = lg2_bval(hit);
        json_free(hit); json_free(rest); free(base);
        if (b) { json_free(candidates); return lg2_bool(true); }
    }
    json_free(candidates);
    return lg2_bool(false);
}


/* PoP: lg2_contains_host_interpreter_kill @ cron/lifecycle_guard.py:contains_host_interpreter_kill */
static json_t *lg2_contains_host_interpreter_kill(lg2_ctx *ctx, const char *text) {
    char *normalized = lg2_re_sub(LG2_PAT_LINE_CONT, " ", text ? text : "");
    bool kill_verb = lg2_re_bool(LG2_PAT_KILL_VERB, normalized);
    json_t *segments = lg2_seam_segments(ctx, normalized);
    if (lg2_exc_pending(ctx)) { json_free(segments); free(normalized); return NULL; }
    bool hit = false;
    for (size_t i = 0; i < segments->c.count && !hit; i++) {
        json_t *seg = segments->c.items[i];
        char *joined = lg2_join_tokens(seg, " ");
        char *stripped = lg2_re_sub(LG2_PAT_ARGV_PUNCT, " ", joined);
        json_t *variants = json_array();
        json_append(variants, json_copy(seg));
        if (!lg2_str_eq(stripped, joined)) {
            json_t *toks = lg2_split_ws(stripped);
            json_append(variants, toks);
        } else free(stripped);
        for (size_t v = 0; v < variants->c.count && !hit; v++) {
            json_t *r = lg2_segment_names_host_interpreter(ctx, variants->c.items[v], true);
            if (lg2_exc_pending(ctx)) { json_free(r); json_free(variants); free(joined); json_free(segments); free(normalized); return NULL; }
            hit = lg2_bval(r); json_free(r);
            if (!hit && kill_verb) {
                r = lg2_segment_names_host_interpreter(ctx, variants->c.items[v], false);
                if (lg2_exc_pending(ctx)) { json_free(r); json_free(variants); free(joined); json_free(segments); free(normalized); return NULL; }
                hit = lg2_bval(r); json_free(r);
            }
        }
        json_free(variants); free(joined);
    }
    json_free(segments); free(normalized);
    return lg2_bool(hit);
}
/* PoP: lg2_first_gateway_lifecycle_match @ cron/lifecycle_guard.py:first_gateway_lifecycle_match */
static json_t *lg2_first_gateway_lifecycle_match(lg2_ctx *ctx, const char *text) {
    char *t = lg2_seam_str(ctx, "strip_inert_heredoc_bodies", "text", text ? text : "");
    char *normalized = lg2_re_sub(LG2_PAT_LINE_CONT, " ", t ? t : "");
    free(t);
    lg2_match m;
    if (lg2_re_search(LG2_PAT_GATEWAY_LIFECYCLE, normalized, &m) && m.g[0]) {
        json_t *r = lg2_strnode(m.g[0]);
        lg2_match_free(&m); free(normalized);
        return r;
    }
    lg2_match_free(&m);
    json_t *segs = lg2_seam_segments(ctx, normalized);
    for (size_t i = 0; i < segs->c.count; i++) {
        char *joined = lg2_join_tokens(segs->c.items[i], " ");
        char *stripped = lg2_re_sub(LG2_PAT_ARGV_PUNCT, " ", joined);
        for (int k = 0; k < 2; k++) {
            const char *cand = k == 0 ? joined : stripped;
            lg2_match cm;
            if (lg2_re_search(LG2_PAT_GATEWAY_LIFECYCLE, cand, &cm) && cm.g[0]) {
                json_t *r = lg2_strnode(cm.g[0]);
                lg2_match_free(&cm); free(stripped); free(joined);
                json_free(segs); free(normalized);
                return r;
            }
            lg2_match_free(&cm);
        }
        free(stripped); free(joined);
    }
    json_free(segs); free(normalized);
    return lg2_null();
}
/* PoP: lg2_capped_read_limit @ cron/lifecycle_guard.py:_capped_read_limit */
static json_t *lg2_capped_read_limit(const json_t *max_bytes) {
    if (lg2_none(max_bytes) || !max_bytes) return lg2_intnode(LG2_MAX_READ_BYTES);
    long v = (long)lg2_nval(max_bytes);
    if (v < 0) v = 0;
    if (v > LG2_MAX_READ_BYTES) v = LG2_MAX_READ_BYTES;
    return lg2_intnode(v);
}
/* PoP: lg2_unreadable_reason @ cron/lifecycle_guard.py:_unreadable_reason */
static json_t *lg2_unreadable_reason(lg2_ctx *ctx, const char *path) {
    json_t *a = json_object();
    json_set(a, "path", json_string(path ? path : ""));
    json_t *live = lg2_call(ctx, "has_live_connection", a);
    bool is_live = lg2_bval(live);
    json_free(live);
    if (is_live) {
        char *s = lg2_fmt1("`%s` is a SQLite database open in this gateway process", path);
        json_t *r = lg2_strnode(s); free(s); return r;
    }
    a = json_object(); json_set(a, "path", json_string(path ? path : ""));
    json_t *st = lg2_call(ctx, "os.stat", a);
    if (lg2_exc_pending(ctx) && lg2_exc_is_oserror(ctx)) {
        lg2_exc_clear(ctx); json_free(st);
        char *s = lg2_fmt1("`%s` could not be read", path);
        json_t *r = lg2_strnode(s); free(s); return r;
    }
    /* S_ISREG(mode): (mode & 0170000) == 0100000 (scripted os.stat node). */
    long mode = (long)json_get_num(st, "st_mode", 0100000);
    json_free(st);
    bool regular = (mode & 0170000) == 0100000;
    if (!regular) {
        char *s = lg2_fmt1("`%s` is not a regular file", path);
        json_t *r = lg2_strnode(s); free(s); return r;
    }
    char buf[160];
    snprintf(buf, sizeof buf,
        "`%s` is larger than the scan cap (%d bytes) or the remaining walk budget",
        path ? path : "", LG2_MAX_READ_BYTES);
    return lg2_strnode(buf);
}

/* ── signal / PID helpers ────────────────────────────────────────────────── */
static bool lg2_nonlethal_signals_init = false;
static json_t *lg2_nonlethal_signals = NULL;
/* PoP: lg2_is_nonlethal_signal @ cron/lifecycle_guard.py:_is_nonlethal_signal */
static bool lg2_is_nonlethal_signal(const char *spec) {
    if (!lg2_nonlethal_signals_init) {
        lg2_nonlethal_signals = json_object();
        json_set(lg2_nonlethal_signals, "0", json_bool(true));
        /* The gate host's Python has no signal.SIGUSR2 (Windows), so the live
         * _NONLETHAL_KILL_SIGNALS set is exactly {"0"} there. */
        lg2_nonlethal_signals_init = true;
    }
    char *u = lg2_upper_dup(spec ? spec : "");
    char *sig = lg2_remove_prefix(u, "SIG");
    bool hit = (sig && json_obj_get(lg2_nonlethal_signals, sig)) ? true : false;
    free(u); free(sig);
    return hit;
}
/* PoP: lg2_is_nonlethal_signal_option @ cron/lifecycle_guard.py:_is_nonlethal_signal_option */
static json_t *lg2_is_nonlethal_signal_option(const char *arg) {
    if (!arg || arg[0] != '-') return lg2_bool(false);
    char *x = lg2_remove_prefix(arg, "--signal=");
    char *y = lg2_lstrip_chars(x, "-");
    bool hit = lg2_is_nonlethal_signal(y);
    free(x); free(y);
    if (!hit && strlen(arg) >= 2 && (arg[1] == 's' || arg[1] == 'n')) hit = lg2_is_nonlethal_signal(arg + 2);
    return lg2_bool(hit);
}
/* PoP: lg2_is_pid_operand @ cron/lifecycle_guard.py:_is_pid_operand */
static bool lg2_is_pid_operand(const char *token, long pid, bool group) {
    if (!token || !*token) return false;
    const char *t = token;
    if (t[0] == '+' || (group && t[0] == '-')) t++;
    if (!lg2_is_ascii_decimal(t)) return false;
    while (*t == '0' && t[1]) t++;
    char buf[32]; snprintf(buf, sizeof buf, "%ld", pid);
    return lg2_str_eq(t, buf);
}
/* PoP: lg2_kill_targets_pid @ cron/lifecycle_guard.py:_kill_targets_pid */
static json_t *lg2_kill_targets_pid(lg2_ctx *ctx, const json_t *args, long pid) {
    size_t n = lg2_arr(args) ? args->c.count : 0;
    size_t pos = 0;
    bool signal_given = false;
    static const char *const LIST[] = {"-l","-L","--list","--table"};
    static const char *const SIGVAL[] = {"-s","-n","--signal"};
    while (pos < n) {
        const char *arg = lg2_sval(args->c.items[pos]);
        if (lg2_str_eq(arg, "--")) {
            for (size_t i = pos + 1; i < n; i++)
                if (lg2_is_pid_operand(lg2_sval(args->c.items[i]), pid, true)) return lg2_bool(true);
            return lg2_bool(false);
        }
        bool in_list = lg2_in_strset(arg, LIST, 4);
        json_t *sopt = lg2_is_nonlethal_signal_option(arg);
        bool nonlethal = lg2_bval(sopt); json_free(sopt);
        if (in_list || (!signal_given && nonlethal)) return lg2_bool(false);
        if (lg2_in_strset(arg, SIGVAL, 3)) {
            const char *nxt = pos + 1 < n ? lg2_sval(args->c.items[pos + 1]) : "";
            bool nl = lg2_is_nonlethal_signal(nxt);
            if (nl) return lg2_bool(false);
            signal_given = true; pos += 2; continue;
        }
        if (lg2_is_pid_operand(arg, pid, signal_given)) return lg2_bool(true);
        lg2_match rm;
        if (lg2_re_match(LG2_PAT_REDIRECT, arg, &rm) && rm.ok) {
            size_t end = (size_t)rm.end;
            lg2_match_free(&rm);
            pos += (end == strlen(arg)) ? 2 : 1;
            continue;
        }
        lg2_match_free(&rm);
        if (arg[0] != '-') {
            for (size_t i = pos + 1; i < n; i++)
                if (lg2_is_pid_operand(lg2_sval(args->c.items[i]), pid, true)) return lg2_bool(true);
            return lg2_bool(false);
        }
        signal_given = true; pos++;
    }
    (void)ctx;
    return lg2_bool(false);
}


/* PoP: lg2_preview_words @ cron/lifecycle_guard.py:_preview_words */
static json_t *lg2_preview_words(const char *command) {
    json_t *words = json_array();
    size_t cap = 64, len = 0;
    char *cur = (char *)malloc(cap);
    if (!cur) return json_null();
    char quote = 0;
    bool quoted = false;
    const char *p = command ? command : "";
    for (; *p; p++) {
        char ch = *p;
        if (quote || (ch != ' ' && ch != '\t')) {
            if (!quote && (ch == '\'' || ch == '"')) { quote = ch; quoted = true; }
            else if (ch == quote) quote = 0;
            if (len + 2 > cap) { cap *= 2; char *n = (char *)realloc(cur, cap); if (!n) break; cur = n; }
            cur[len++] = ch;
        } else if (len) {
            cur[len] = '\0';
            json_t *pair = json_array();
            json_append(pair, json_string(cur));
            json_append(pair, json_bool(quoted));
            json_append(words, pair);
            len = 0; quoted = false;
        }
    }
    if (quote) { free(cur); json_free(words); return json_null(); }
    if (len) {
        cur[len] = '\0';
        json_t *pair = json_array();
        json_append(pair, json_string(cur));
        json_append(pair, json_bool(quoted));
        json_append(words, pair);
    }
    free(cur);
    return words;
}
static bool lg2_whatif_switch(const char *s) {
    static const char *const W[] = {"-whatif","-wi","-whatif:$true","-wi:$true"};
    return lg2_in_strset(s, W, 4);
}
static bool lg2_whatif_command(const char *s) {
    static const char *const W[] = {"stop-process","spps","kill"};
    return lg2_in_strset(s, W, 3);
}
static bool lg2_stop_switch(const char *s) {
    if (lg2_whatif_switch(s)) return true;
    static const char *const S[] = {"-force","-passthru","-confirm","-cf","-verbose","-vb","-debug","-db"};
    return lg2_in_strset(s, S, 8);
}
/* PoP: lg2_is_stop_process_whatif_preview @ cron/lifecycle_guard.py:_is_stop_process_whatif_preview */
static json_t *lg2_is_stop_process_whatif_preview(lg2_ctx *ctx, const char *command) {
    (void)ctx;
    json_t *words = lg2_preview_words(command);
    if (lg2_none(words) || words->c.count == 0) { json_free(words); return lg2_bool(false); }
    json_t *first = words->c.items[0];
    const char *w0 = lg2_sval(first->c.items[0]);
    bool q0 = lg2_bval(first->c.items[1]);
    char *w0l = lg2_lower_dup(w0);
    if (q0 || !lg2_whatif_command(w0l)) { free(w0l); json_free(words); return lg2_bool(false); }
    free(w0l);
    for (size_t i = 0; i < words->c.count; i++) {
        char *w = lg2_lower_dup(lg2_sval(words->c.items[i]->c.items[0]));
        bool hit;
        lg2_match bm;
        hit = lg2_re_search(LG2_PAT_WHATIF_BOUND, w, &bm); lg2_match_free(&bm);
        if (!hit) { hit = lg2_re_search(LG2_PAT_WHATIF_DISQUAL, w, &bm); lg2_match_free(&bm); }
        free(w);
        if (hit && !lg2_whatif_switch(lg2_sval(words->c.items[i]->c.items[0]))) {
            json_free(words); return lg2_bool(false);
        }
    }
    for (size_t i = 0; i + 1 < words->c.count; i++) {
        char *prev = lg2_lower_dup(lg2_sval(words->c.items[i]->c.items[0]));
        const char *word = lg2_sval(words->c.items[i + 1]->c.items[0]);
        bool quoted = lg2_bval(words->c.items[i + 1]->c.items[1]);
        bool after_value_param = false;
        if (prev[0] == '-') {
            bool ends_colon = prev[strlen(prev) - 1] == ':';
            bool has_colon = strchr(prev, ':') != NULL;
            after_value_param = ends_colon || (!has_colon && !lg2_stop_switch(prev));
        }
        char *wl = lg2_lower_dup(word);
        if (!quoted && lg2_whatif_switch(wl) && !after_value_param) {
            free(prev); free(wl); json_free(words); return lg2_bool(true);
        }
        free(prev); free(wl);
    }
    json_free(words);
    return lg2_bool(false);
}
/* PoP: lg2_windows_killer_targets_pid @ cron/lifecycle_guard.py:_windows_killer_targets_pid */
static json_t *lg2_windows_killer_targets_pid(lg2_ctx *ctx, const char *name, const json_t *args,
                                              long pid, bool whatif) {
    char *joined = lg2_join_tokens(args, " ");
    char *rejoined = lg2_re_sub("\\s*,\\s*", ",", joined);
    json_t *argv = lg2_split_ws(rejoined);
    free(joined); free(rejoined);
    size_t n = argv->c.count;
    char **low = (char **)malloc((n ? n : 1) * sizeof(char *));
    for (size_t i = 0; i < n; i++) low[i] = lg2_lower_dup(lg2_sval(argv->c.items[i]));
    if (lg2_str_eq(name, "stop-process") && whatif) { for (size_t i=0;i<n;i++) free(low[i]); free(low); json_free(argv); return lg2_bool(false); }
    bool remote_host = false;
    if (lg2_str_eq(name, "taskkill"))
        for (size_t i = 0; i < n; i++) if (lg2_str_eq(low[i], "/s")) remote_host = true;
    if (remote_host) {
        for (size_t i=0;i<n;i++) free(low[i]); free(low); json_free(argv); return lg2_bool(false);
    }
    bool taskkill = lg2_str_eq(name, "taskkill");
    json_t *values = json_array();
    size_t pos = 0;
    static const char *const VPARAMS[] = {"-id","-name","-n","-processname","-inputobject"};
    while (pos < n) {
        char *o = NULL, *c = NULL, *inl = NULL;
        lg2_partition(low[pos], ':', &o, &c, &inl);
        bool flag = (taskkill && (lg2_str_eq(o, "/pid") || lg2_str_eq(o, "-pid"))) ||
                    (!taskkill && lg2_str_eq(o, "-id"));
        if (flag) {
            if (*c) json_append(values, json_string(inl));
            else if (pos + 1 < n) json_append(values, json_copy(argv->c.items[pos + 1]));
            else json_append(values, json_string(""));
        } else if (!taskkill && low[pos][0] != '-') {
            json_append(values, json_copy(argv->c.items[pos]));
        }
        bool vparam = !taskkill && lg2_in_strset(o, VPARAMS, 5) && !*c;
        free(o); free(c); free(inl);
        if (vparam) pos++;
        pos++;
    }
    bool hit = false;
    for (size_t i = 0; i < values->c.count && !hit; i++) {
        json_t *parts = lg2_split_char(lg2_sval(values->c.items[i]), ',');
        for (size_t j = 0; j < parts->c.count && !hit; j++) hit = lg2_is_pid_operand(lg2_sval(parts->c.items[j]), pid, false);
        json_free(parts);
    }
    for (size_t i=0;i<n;i++) free(low[i]);
    free(low); json_free(argv); json_free(values);
    (void)ctx;
    return lg2_bool(hit);
}
/* PoP: lg2_string_runner_payload @ cron/lifecycle_guard.py:_string_runner_payload */
static json_t *lg2_string_runner_payload(const char *name, const json_t *args) {
    const char *const *flags = NULL;
    size_t nf = 0;
    static const char *const NONE[] = {NULL};
    static const char *const PS[] = {"-c","-command"};
    static const char *const CMD[] = {"/c","/k"};
    if (lg2_str_eq(name, "eval")) { flags = NONE; nf = 0; }
    else if (lg2_str_eq(name, "powershell") || lg2_str_eq(name, "pwsh")) { flags = PS; nf = 2; }
    else if (lg2_str_eq(name, "cmd")) { flags = CMD; nf = 2; }
    else return lg2_null();
    if (!nf) {
        char *s = lg2_join_tokens(args, " ");
        json_t *r = lg2_strnode(s); free(s); return r;
    }
    size_t n = lg2_arr(args) ? args->c.count : 0;
    for (size_t i = 0; i < n; i++) {
        char *l = lg2_lower_dup(lg2_sval(args->c.items[i]));
        if (lg2_in_strset(l, flags, nf)) {
            json_t *tail = json_array();
            for (size_t j = i + 1; j < n; j++) json_append(tail, json_copy(args->c.items[j]));
            char *s = lg2_join_tokens(tail, " ");
            json_t *r = lg2_strnode(s); free(s); json_free(tail); free(l);
            return r;
        }
        free(l);
    }
    return lg2_null();
}
static bool lg2_targets_pid_rec(lg2_ctx *ctx, const char *command, long pid, int depth);
/* PoP: lg2_targets_gateway_pid @ cron/lifecycle_guard.py:_targets_gateway_pid */
static json_t *lg2_targets_gateway_pid(lg2_ctx *ctx, const char *command, long pid, int depth) {
    return lg2_bool(lg2_targets_pid_rec(ctx, command, pid, depth));
}
static bool lg2_targets_pid_rec(lg2_ctx *ctx, const char *command, long pid, int depth) {
    bool whatif = depth == 0 && lg2_bval(lg2_is_stop_process_whatif_preview(ctx, command));
    char *cmd = lg2_seam_str(ctx, "strip_inert_heredoc_bodies", "text", command ? command : "");
    if (depth < LG2_MAX_DEPTH) {
        json_t *subs = lg2_re_finditer(LG2_PAT_SUBST, cmd ? cmd : "", 2);
        for (size_t i = 0; i < subs->c.count; i++) {
            json_t *d = subs->c.items[i];
            json_t *g1 = json_obj_get(d, "g1"), *g2 = json_obj_get(d, "g2");
            const char *body = (g1 && !lg2_none(g1)) ? lg2_sval(g1) : lg2_sval(g2);
            if (lg2_targets_pid_rec(ctx, body, pid, depth + 1)) { json_free(subs); free(cmd); return true; }
        }
        json_free(subs);
    }
    json_t *segs = lg2_seam_segments(ctx, cmd ? cmd : "");
    free(cmd);
    static const char *const SYNTAX[] = {"then","do","else","elif","if","while","until","!","{","time"};
    for (size_t i = 0; i < segs->c.count; i++) {
        json_t *seg = segs->c.items[i];
        size_t nseg = seg->c.count, off = 0;
        while (off < nseg) {
            const char *tok = lg2_sval(seg->c.items[off]);
            if (lg2_in_strset(tok, SYNTAX, 10)) {
                bool tm = lg2_str_eq(tok, "time") && off + 1 < nseg && lg2_str_eq(lg2_sval(seg->c.items[off + 1]), "-p");
                off += tm ? 2 : 1;
            } else {
                lg2_match rm;
                bool isredir = lg2_re_match(LG2_PAT_REDIRECT, tok, &rm);
                size_t end = isredir ? (size_t)rm.end : 0;
                if (isredir) { lg2_match_free(&rm); off += (end == strlen(tok)) ? 2 : 1; }
                else { lg2_match_free(&rm); break; }
            }
        }
        json_t *trimmed = json_array();
        for (size_t k = off; k < nseg; k++) json_append(trimmed, json_copy(seg->c.items[k]));
        long idx = lg2_executed_command_index(ctx, trimmed);
        if (idx < 0) { json_free(trimmed); continue; }
        char *base = lg2_exe_base(ctx, lg2_sval(trimmed->c.items[idx]));
        if (lg2_str_eq(base, "spps")) { char *n = strdup("stop-process"); free(base); base = n; }
        json_t *rest = json_array();
        for (size_t k = (size_t)idx + 1; k < trimmed->c.count; k++) json_append(rest, json_copy(trimmed->c.items[k]));
        json_t *hit;
        bool b = false;
        if (lg2_str_eq(base, "kill")) { hit = lg2_kill_targets_pid(ctx, rest, pid); b = lg2_bval(hit); json_free(hit); }
        if (!b && (lg2_str_eq(base, "taskkill") || lg2_str_eq(base, "stop-process"))) {
            hit = lg2_windows_killer_targets_pid(ctx, base, rest, pid, whatif);
            b = lg2_bval(hit); json_free(hit);
        }
        if (!b && depth < LG2_MAX_DEPTH &&
            (lg2_str_eq(base, "eval") || lg2_str_eq(base, "powershell") || lg2_str_eq(base, "pwsh") || lg2_str_eq(base, "cmd"))) {
            json_t *payload = lg2_string_runner_payload(base, rest);
            if (!lg2_none(payload) && lg2_truthy(payload)) b = lg2_targets_pid_rec(ctx, lg2_sval(payload), pid, depth + 1);
            json_free(payload);
        }
        json_free(rest); json_free(trimmed); free(base);
        if (b) { json_free(segs); return true; }
    }
    json_free(segs);
    return false;
}


/* ── path handling ───────────────────────────────────────────────────────── */
/* PoP: lg2_resolve_lenient @ cron/lifecycle_guard.py:_resolve_lenient */
static char *lg2_resolve_lenient(lg2_ctx *ctx, const char *path) {
    json_t *a = json_object();
    json_set(a, "path", json_string(path ? path : ""));
    json_t *r = lg2_call(ctx, "path.resolve", a);
    if (lg2_exc_pending(ctx) && (lg2_exc_is_value_or_runtime(ctx) || lg2_exc_is_oserror(ctx))) {
        lg2_exc_clear(ctx); json_free(r); return strdup(path ? path : "");
    }
    char *s = strdup(lg2_sval(r));
    json_free(r);
    return s;
}
/* claimed sibling transcribed as an internal helper (no PoP claim) */
static json_t *lg2_is_cloud_placeholder_path(const char *path) {
    json_t *parts = lg2_path_parts(path);
    bool hit = false;
    for (size_t i = 1; i < parts->c.count; i++) {
        const char *prev = lg2_sval(parts->c.items[i - 1]);
        const char *cur = lg2_sval(parts->c.items[i]);
        if (lg2_str_eq(prev, "Library") &&
            (lg2_str_eq(cur, "Mobile Documents") || lg2_str_eq(cur, "CloudStorage"))) { hit = true; break; }
    }
    json_free(parts);
    return lg2_bool(hit);
}
/* PoP: lg2_on_cloud_path @ cron/lifecycle_guard.py:_on_cloud_path */
static json_t *lg2_on_cloud_path(lg2_ctx *ctx, const char *path) {
    if (lg2_bval(lg2_is_cloud_placeholder_path(path))) return lg2_bool(true);
    char *resolved = lg2_resolve_lenient(ctx, path);
    json_t *r = lg2_is_cloud_placeholder_path(resolved);
    bool b = lg2_bval(r);
    json_free(r); free(resolved);
    return lg2_bool(b);
}
/* claimed sibling transcribed as an internal helper (no PoP claim) */
static char *lg2_expand_candidate_path(lg2_ctx *ctx, const char *candidate) {
    if (!candidate || !*candidate) return NULL;  /* NUL-bearing tokens are outside the fixture domain */
    json_t *a = json_object();
    json_set(a, "path", json_string(candidate));
    json_t *r = lg2_call(ctx, "path.expanduser", a);
    if (lg2_exc_pending(ctx) && lg2_exc_is_value_or_runtime(ctx)) { lg2_exc_clear(ctx); json_free(r); return NULL; }
    if (lg2_exc_pending(ctx) && lg2_exc_is_oserror(ctx)) { lg2_exc_clear(ctx); json_free(r); return NULL; }
    char *s = strdup(lg2_sval(r));
    json_free(r);
    return s;
}
/* PoP: lg2_resolved_or_nothing @ cron/lifecycle_guard.py:_resolved_or_nothing */
static json_t *lg2_resolved_or_nothing(lg2_ctx *ctx, const char *candidate, const char *cwd) {
    json_t *out = json_array();
    char *p = lg2_expand_candidate_path(ctx, candidate);
    if (!p) return out;
    if (!lg2_path_is_absolute(p)) {
        char *base = NULL;
        if (cwd && *cwd) base = strdup(cwd);
        else {
            json_t *a = json_object();
            json_t *r = lg2_call(ctx, "path.cwd", a);
            if (lg2_exc_pending(ctx) && lg2_exc_is_oserror(ctx)) { lg2_exc_clear(ctx); json_free(r); free(p); return out; }
            base = strdup(lg2_sval(r));
            json_free(r);
        }
        char *joined = lg2_path_join(base, p);
        free(p); free(base); p = joined;
    }
    json_append(out, json_string(p ? p : ""));
    free(p);
    return out;
}
/* PoP: lg2_heredoc_write_targets @ cron/lifecycle_guard.py:_heredoc_write_targets */
static json_t *lg2_heredoc_write_targets(lg2_ctx *ctx, const char *unit) {
    json_t *out = json_array();
    json_t *tokens = lg2_seam_shlex(ctx, unit, ";&|()<>");
    if (lg2_exc_pending(ctx) && lg2_exc_is(ctx, "ValueError")) { lg2_exc_clear(ctx); json_free(tokens); return out; }
    json_t *segs = lg2_split_segments(tokens, false);
    static const char *const REDIRS[] = {">",">>",">|","&>","&>>"};
    for (size_t i = 0; i < segs->c.count; i++) {
        json_t *seg = segs->c.items[i];
        long idx = lg2_executed_command_index(ctx, seg);
        if (idx < 0) continue;
        char *exe = lg2_seam_executable_name(ctx, lg2_sval(seg->c.items[idx]));
        if (!lg2_str_eq(exe, "cat") && !lg2_str_eq(exe, "tee")) { free(exe); continue; }
        const char *op = NULL;
        for (size_t k = (size_t)idx + 1; k < seg->c.count; k++) {
            const char *tok = lg2_sval(seg->c.items[k]);
            if (op) {
                if (lg2_in_strset(op, REDIRS, 5)) json_append(out, json_copy(seg->c.items[k]));
                op = NULL;
            } else {
                bool all_op = *tok != '\0';
                for (const char *p = tok; *p; p++) if (!strchr("<>|&", *p)) all_op = false;
                if (all_op) op = tok;
                else {
                    char *each = lg2_seam_executable_name(ctx, lg2_sval(seg->c.items[idx]));
                    bool is_tee = lg2_str_eq(each, "tee");
                    free(each);
                    if (is_tee && tok[0] != '-') json_append(out, json_copy(seg->c.items[k]));
                }
            }
        }
        free(exe);
    }
    json_free(segs); json_free(tokens);
    return out;
}
/* PoP: lg2_collect_heredoc_written_scripts @ cron/lifecycle_guard.py:_collect_heredoc_written_scripts */
static json_t *lg2_collect_heredoc_written_scripts(lg2_ctx *ctx, const char *command,
                                                   const char *cwd, json_t *written) {
    if (!written) written = json_object();
    json_t *a = json_object();
    json_set(a, "command", json_string(command ? command : ""));
    json_t *units = lg2_call(ctx, "iter_heredoc_bodies", a);
    if (!lg2_arr(units)) { json_free(units); return written; }
    for (size_t i = 0; i < units->c.count; i++) {
        json_t *pair = units->c.items[i];
        if (!lg2_arr(pair) || pair->c.count < 2) continue;
        const char *unit = lg2_sval(pair->c.items[0]);
        const char *body = lg2_sval(pair->c.items[1]);
        json_t *targets = lg2_heredoc_write_targets(ctx, unit);
        for (size_t t = 0; t < targets->c.count; t++) {
            json_t *paths = lg2_resolved_or_nothing(ctx, lg2_sval(targets->c.items[t]), cwd);
            for (size_t p = 0; p < paths->c.count; p++) {
                char *key = lg2_resolve_lenient(ctx, lg2_sval(paths->c.items[p]));
                json_t *bodies = json_obj_get(written, key);
                if (!bodies) { bodies = json_array(); json_set(written, key, bodies); }
                bool present = false;
                for (size_t b = 0; b < bodies->c.count; b++) if (lg2_str_eq(lg2_sval(bodies->c.items[b]), body)) present = true;
                if (!present) json_append(bodies, json_string(body));
                free(key);
            }
            json_free(paths);
        }
        json_free(targets);
    }
    json_free(units);
    return written;
}
/* PoP: lg2_walk_candidates @ cron/lifecycle_guard.py:_walk_candidates */
static json_t *lg2_walk_candidates(lg2_ctx *ctx, const char *command, const char *walk_command,
                                   const char *cwd, bool executed) {
    json_t *out = json_array();
    json_t *a = json_object();
    json_set(a, "command", json_string(walk_command ? walk_command : ""));
    if (cwd && *cwd) json_set(a, "cwd", json_string(cwd));
    json_t *paths = lg2_call(ctx, "iter_referenced_shell_scripts", a);
    if (lg2_arr(paths)) for (size_t i = 0; i < paths->c.count; i++) {
        json_t *pair = json_array();
        json_append(pair, json_copy(paths->c.items[i])); json_append(pair, json_bool(executed));
        json_append(out, pair);
    }
    json_free(paths);
    if (!lg2_str_eq(command, walk_command)) {
        a = json_object();
        json_set(a, "command", json_string(command ? command : ""));
        if (cwd && *cwd) json_set(a, "cwd", json_string(cwd));
        paths = lg2_call(ctx, "iter_referenced_shell_scripts", a);
        if (lg2_arr(paths)) for (size_t i = 0; i < paths->c.count; i++) {
            json_t *pair = json_array();
            json_append(pair, json_copy(paths->c.items[i])); json_append(pair, json_bool(false));
            json_append(out, pair);
        }
        json_free(paths);
    }
    return out;
}
/* claimed sibling transcribed as an internal helper (no PoP claim) */
static json_t *lg2_has_binary_magic(const char *data) {
    if (!data) return lg2_bool(false);
    if (lg2_starts_with(data, "#!")) return lg2_bool(false);
    static const char *const ascii_magics[] = {"MZ", "!<arch>"};
    for (size_t i = 0; i < 2; i++) if (lg2_starts_with(data, ascii_magics[i])) return lg2_bool(true);
    static const unsigned char elf[] = {0x7f,'E','L','F'};
    static const unsigned char gz[] = {0x1f,0x8b};
    static const unsigned char zip[] = {'P','K',0x03,0x04};
    size_t n = strlen(data);
    if (n >= 4 && memcmp(data, elf, 4) == 0) return lg2_bool(true);
    if (n >= 2 && memcmp(data, gz, 2) == 0) return lg2_bool(true);
    if (n >= 4 && memcmp(data, zip, 4) == 0) return lg2_bool(true);
    return lg2_bool(false);
}


/* PoP: lg2_read_referenced_script_unlocked @ cron/lifecycle_guard.py:_read_referenced_script_unlocked */
static json_t *lg2_read_referenced_script_unlocked(lg2_ctx *ctx, const char *path, const json_t *max_bytes) {
    long limit = (long)lg2_nval(lg2_capped_read_limit(max_bytes));
    if (lg2_bval(lg2_on_cloud_path(ctx, path))) return lg2_pair(lg2_null(), lg2_bool(true));
    json_t *a = json_object();
    json_set(a, "path", json_string(path ? path : ""));
    json_set(a, "flags", json_number(0));
    json_t *fdnode = lg2_call(ctx, "os.open", a);
    if (lg2_exc_pending(ctx) && (lg2_exc_is_oserror(ctx) || lg2_exc_is(ctx, "ValueError"))) {
        lg2_exc_clear(ctx); json_free(fdnode); return lg2_pair(lg2_null(), lg2_bool(false));
    }
    long fd = (long)lg2_nval(fdnode);
    json_free(fdnode);
    a = json_object(); json_set(a, "fd", json_number((double)fd));
    json_t *st = lg2_call(ctx, "os.fstat", a);
    if (lg2_exc_pending(ctx) && lg2_exc_is_oserror(ctx)) {
        lg2_exc_clear(ctx); json_free(st); return lg2_pair(lg2_null(), lg2_bool(false));
    }
    long mode = (long)json_get_num(st, "st_mode", 0100000);
    long size = (long)json_get_num(st, "st_size", 0);
    json_free(st);
    bool regular = (mode & 0170000) == 0100000;
    bool dir = (mode & 0170000) == 0040000;
    if (!regular) {
        a = json_object(); json_set(a, "fd", json_number((double)fd));
        json_t *cl = lg2_call(ctx, "os.close", a); json_free(cl);
        return lg2_pair(lg2_null(), lg2_bool(!dir));
    }
    a = json_object(); json_set(a, "fd", json_number((double)fd)); json_set(a, "n", json_number(LG2_SNIFF_BYTES));
    json_t *dnode = lg2_call(ctx, "os.read", a);
    if (lg2_exc_pending(ctx) && lg2_exc_is_oserror(ctx)) {
        lg2_exc_clear(ctx); json_free(dnode); return lg2_pair(lg2_null(), lg2_bool(false));
    }
    const char *ds = lg2_sval(dnode);
    size_t cap = strlen(ds) + (size_t)limit + 64, used = strlen(ds);
    char *data = (char *)malloc(cap);
    if (!data) { json_free(dnode); return lg2_pair(lg2_null(), lg2_bool(false)); }
    memcpy(data, ds, used); data[used] = '\0';
    json_free(dnode);
    if (lg2_bval(lg2_has_binary_magic(data))) {
        free(data);
        a = json_object(); json_set(a, "fd", json_number((double)fd)); lg2_call(ctx, "os.close", a);
        return lg2_pair(lg2_null(), lg2_bool(false));
    }
    if (size > limit) {
        free(data);
        a = json_object(); json_set(a, "fd", json_number((double)fd)); lg2_call(ctx, "os.close", a);
        return lg2_pair(lg2_null(), lg2_bool(true));
    }
    while ((long)used <= limit) {
        long want = limit + 1 - (long)used;
        if (want <= 0) break;
        a = json_object(); json_set(a, "fd", json_number((double)fd)); json_set(a, "n", json_number((double)want));
        json_t *chunk = lg2_call(ctx, "os.read", a);
        if (lg2_exc_pending(ctx) && lg2_exc_is_oserror(ctx)) { lg2_exc_clear(ctx); json_free(chunk); break; }
        const char *cs = lg2_sval(chunk);
        if (!*cs) { json_free(chunk); break; }
        size_t cl = strlen(cs);
        if (used + cl + 1 > cap) {
            while (used + cl + 1 > cap) cap *= 2;
            char *n = (char *)realloc(data, cap);
            if (!n) break; data = n;
        }
        memcpy(data + used, cs, cl); used += cl; data[used] = '\0';
        json_free(chunk);
    }
    a = json_object(); json_set(a, "fd", json_number((double)fd));
    json_t *closed = lg2_call(ctx, "os.close", a); json_free(closed);
    if (lg2_bval(lg2_has_binary_magic(data))) { free(data); return lg2_pair(lg2_null(), lg2_bool(false)); }
    if ((long)used > limit) { free(data); return lg2_pair(lg2_null(), lg2_bool(true)); }
    /* NUL strip (ASCII fixture domain: no embedded NULs in JSON strings). */
    json_t *text = lg2_strnode(data);
    free(data);
    return lg2_pair(text, lg2_bool(false));
}
/* PoP: lg2_scan_written_bodies @ cron/lifecycle_guard.py:_scan_written_bodies */
static json_t *lg2_scan_written_bodies(lg2_ctx *ctx, const json_t *bodies, const char *cwd, bool executed) {
    json_t *uniq = json_array();
    for (size_t i = 0; bodies && i < bodies->c.count; i++) {
        const char *b = lg2_sval(bodies->c.items[i]);
        if (!*b) continue;
        bool seen = false;
        for (size_t j = 0; j < uniq->c.count; j++) if (lg2_str_eq(lg2_sval(uniq->c.items[j]), b)) seen = true;
        if (!seen) json_append(uniq, json_string(b));
    }
    for (size_t i = 0; i < uniq->c.count; i++) {
        json_t *a = json_object();
        json_set(a, "text", json_copy(uniq->c.items[i]));
        if (cwd && *cwd) json_set(a, "cwd", json_string(cwd));
        json_set(a, "executed", json_bool(executed));
        json_set(a, "charge", json_bool(false));
        json_t *r = lg2_call(ctx, "recurse", a);
        bool b = lg2_bval(r); json_free(r);
        if (b) { json_free(uniq); return lg2_bool(true); }
    }
    if (uniq->c.count > 1) {
        char *joined = lg2_join_tokens(uniq, "");
        json_t *a = json_object();
        json_set(a, "text", json_string(joined));
        if (cwd && *cwd) json_set(a, "cwd", json_string(cwd));
        json_set(a, "executed", json_bool(executed));
        json_set(a, "charge", json_bool(true));
        json_t *r = lg2_call(ctx, "recurse", a);
        bool b = lg2_bval(r); json_free(r);
        free(joined); json_free(uniq);
        return lg2_bool(b);
    }
    json_free(uniq);
    return lg2_bool(false);
}
/* PoP: lg2_scan_gateway_lifecycle @ cron/lifecycle_guard.py:scan_gateway_lifecycle */
static json_t *lg2_scan_gateway_lifecycle(lg2_ctx *ctx, const char *command, const char *cwd,
                                          const json_t *gateway_pid) {
    json_t *budget = lg2_budget_new(ctx);
    json_t *a = json_object();
    json_set(a, "command", json_string(command ? command : ""));
    if (cwd && *cwd) json_set(a, "cwd", json_string(cwd));
    json_set(a, "depth", json_number(0));
    json_t *res = lg2_call(ctx, "_contains_unsafe_gateway_action", a);
    if (lg2_exc_pending(ctx) && lg2_exc_is_exception(ctx) && !lg2_is_base_exc(ctx)) {
        lg2_exc_clear(ctx);
        json_free(res);
        lg2_log(ctx, "warning",
            "lifecycle guard referenced-script walk failed; falling back to direct-scan verdict",
            NULL, true);
        if (lg2_exc_pending(ctx) && !lg2_is_base_exc(ctx)) lg2_exc_clear(ctx);
        bool unsafe = false;
        json_t *dargs = json_object();
        json_set(dargs, "command", json_string(command ? command : ""));
        json_t *d = lg2_call(ctx, "_direct_lifecycle_scan", dargs);
        unsafe = lg2_bval(d); json_free(d);
        if (!unsafe && gateway_pid && !lg2_none(gateway_pid)) {
            unsafe = lg2_bval(lg2_targets_gateway_pid(ctx, command, (long)lg2_nval(gateway_pid), 0));
        }
        if (lg2_exc_pending(ctx) && lg2_exc_is_exception(ctx) && !lg2_is_base_exc(ctx)) {
            lg2_exc_clear(ctx);
            bool u = lg2_seam_bool(ctx, "contains_gateway_lifecycle_command", "text", command);
            if (!u) u = lg2_seam_bool(ctx, "contains_launchctl_submit_command", "command", command);
            unsafe = u;
        }
        json_free(budget);
        return lg2_pair(lg2_bool(unsafe), lg2_null());
    }
    bool unsafe = false;
    const char *refusal = NULL;
    unsafe = lg2_arr(res) ? (res->c.count >= 1 && lg2_bval(res->c.items[0])) : lg2_bval(res);
    if (unsafe && lg2_arr(res) && res->c.count >= 2 && !lg2_none(res->c.items[1]))
        refusal = lg2_sval(res->c.items[1]);
    json_free(res);
    json_t *r = lg2_pair(lg2_bool(unsafe), lg2_null());
    (void)refusal;
    json_free(budget);
    return r;
}
/* ── direct-drive wrappers for the budget class methods ─────────────────── */
/* PoP: lg2_budget_init @ cron/lifecycle_guard.py:__init__ */
static json_t *lg2_budget_init(lg2_ctx *ctx) { return lg2_budget_new(ctx); }
/* PoP: lg2_budget_charge_text_fn @ cron/lifecycle_guard.py:charge_text */
static json_t *lg2_budget_charge_text_fn(lg2_ctx *ctx, json_t *budget, const char *text, bool written) {
    return lg2_budget_charge_text(ctx, budget, text, written);
}
/* PoP: lg2_budget_charge_path_fn @ cron/lifecycle_guard.py:charge_path */
static json_t *lg2_budget_charge_path_fn(lg2_ctx *ctx, json_t *budget) {
    return lg2_budget_charge_path(ctx, budget);
}
/* PoP: lg2_budget_charge_remote_fn @ cron/lifecycle_guard.py:charge_remote_read */
static json_t *lg2_budget_charge_remote_fn(lg2_ctx *ctx, json_t *budget) {
    return lg2_budget_charge_remote(ctx, budget);
}


/* ── dispatcher: func name -> ported call (harness emits func/in/out) ───── */
static json_t *lg2_dispatch(lg2_ctx *ctx, const char *func, const json_t *in, json_t **state_out) {
    *state_out = NULL;
    const json_t *v;
    /* budget class methods (mutate in.budget in place; echo it as state) */
    if (lg2_str_eq(func, "__init__")) {
        json_t *b = lg2_budget_new(ctx); *state_out = b; return json_copy(b);
    }
    if (lg2_str_eq(func, "charge_text") || lg2_str_eq(func, "charge_path") ||
        lg2_str_eq(func, "charge_remote_read")) {
        json_t *b = lg2_get(in, "budget");
        b = b ? json_copy(b) : lg2_budget_new(ctx);
        json_t *r;
        if (lg2_str_eq(func, "charge_text"))
            r = lg2_budget_charge_text_fn(ctx, b, json_get_str(in, "text", ""), lg2_bval(lg2_get(in, "written_body")));
        else if (lg2_str_eq(func, "charge_path")) r = lg2_budget_charge_path_fn(ctx, b);
        else r = lg2_budget_charge_remote_fn(ctx, b);
        *state_out = json_copy(b);
        return r;
    }
    if (lg2_str_eq(func, "_is_interpreter_image"))
        return lg2_is_interpreter_image(ctx, json_get_str(in, "value", ""), lg2_bval(lg2_get(in, "substring")));
    if (lg2_str_eq(func, "_pattern_reaches_host_interpreter"))
        return lg2_pattern_reaches_host_interpreter(ctx, json_get_str(in, "pattern", ""),
            lg2_bval(lg2_get(in, "full_cmdline")), lg2_bval(lg2_get(in, "exact")));
    if (lg2_str_eq(func, "_killer_targets_host_interpreter"))
        return lg2_killer_targets_host_interpreter(ctx, json_get_str(in, "name", ""), lg2_get(in, "args"));
    if (lg2_str_eq(func, "_segment_names_host_interpreter"))
        return lg2_segment_names_host_interpreter(ctx, lg2_get(in, "tokens"), lg2_bval(lg2_get(in, "killers")));
    if (lg2_str_eq(func, "contains_host_interpreter_kill"))
        return lg2_contains_host_interpreter_kill(ctx, json_get_str(in, "text", ""));
    if (lg2_str_eq(func, "first_gateway_lifecycle_match"))
        return lg2_first_gateway_lifecycle_match(ctx, json_get_str(in, "text", ""));
    if (lg2_str_eq(func, "_capped_read_limit"))
        return lg2_capped_read_limit(lg2_get(in, "max_bytes"));
    if (lg2_str_eq(func, "lifecycle_scan_root_within_budget")) {
        json_t *b = lg2_budget_new(ctx);
        json_t *r = lg2_budget_charge_text(ctx, b, json_get_str(in, "text", ""), false);
        bool ok = lg2_bval(r);
        json_free(r); json_free(b);
        if (lg2_exc_pending(ctx) && lg2_exc_is_exception(ctx) && !lg2_is_base_exc(ctx)) { lg2_exc_clear(ctx); return lg2_bool(false); }
        return lg2_bool(ok);
    }
    if (lg2_str_eq(func, "_budget_exhausted")) {
        json_t *b = lg2_get(in, "budget");
        b = b ? json_copy(b) : lg2_budget_new(ctx);
        json_t *r = lg2_budget_exhausted(ctx, b, json_get_str(in, "what", "text"),
                                          (long)json_get_num(in, "depth", 0));
        *state_out = json_copy(b);
        return r;
    }
    if (lg2_str_eq(func, "_unreadable_reason"))
        return lg2_unreadable_reason(ctx, json_get_str(in, "path", ""));
    if (lg2_str_eq(func, "_refuse_unreadable")) {
        json_t *b = lg2_get(in, "budget");
        b = b ? json_copy(b) : lg2_budget_new(ctx);
        json_t *r = lg2_refuse_unreadable(ctx, b, json_get_str(in, "path", ""), json_get_str(in, "reason", ""));
        *state_out = json_copy(b);
        return r;
    }
    if (lg2_str_eq(func, "_shlex_tokens"))
        return lg2_seam_shlex(ctx, json_get_str(in, "line", ""), json_get_str(in, "punctuation", ";&|()"));
    if (lg2_str_eq(func, "_split_logical_lines"))
        return lg2_split_logical_lines(json_get_str(in, "text", ""));
    if (lg2_str_eq(func, "_split_segments"))
        return lg2_split_segments(lg2_get(in, "tokens"), lg2_bval(lg2_get(in, "keep_controls")));
    if (lg2_str_eq(func, "_executed_command_index")) {
        long i = lg2_executed_command_index(ctx, lg2_get(in, "segment"));
        return i < 0 ? lg2_null() : lg2_intnode(i);
    }
    if (lg2_str_eq(func, "_is_nonlethal_signal"))
        return lg2_bool(lg2_is_nonlethal_signal(json_get_str(in, "spec", "")));
    if (lg2_str_eq(func, "_is_nonlethal_signal_option"))
        return lg2_is_nonlethal_signal_option(json_get_str(in, "arg", ""));
    if (lg2_str_eq(func, "_is_pid_operand"))
        return lg2_bool(lg2_is_pid_operand(json_get_str(in, "token", ""),
            (long)json_get_num(in, "pid", 0), lg2_bval(lg2_get(in, "group"))));
    if (lg2_str_eq(func, "_kill_targets_pid"))
        return lg2_kill_targets_pid(ctx, lg2_get(in, "args"), (long)json_get_num(in, "pid", 0));
    if (lg2_str_eq(func, "_preview_words"))
        return lg2_preview_words(json_get_str(in, "command", ""));
    if (lg2_str_eq(func, "_is_stop_process_whatif_preview"))
        return lg2_is_stop_process_whatif_preview(ctx, json_get_str(in, "command", ""));
    if (lg2_str_eq(func, "_windows_killer_targets_pid"))
        return lg2_windows_killer_targets_pid(ctx, json_get_str(in, "name", ""), lg2_get(in, "args"),
            (long)json_get_num(in, "pid", 0), lg2_bval(lg2_get(in, "whatif_preview")));
    if (lg2_str_eq(func, "_string_runner_payload")) {
        const char *nm = json_get_str(in, "name", "");
        if (!lg2_str_eq(nm, "eval") && !lg2_str_eq(nm, "powershell") && !lg2_str_eq(nm, "pwsh") && !lg2_str_eq(nm, "cmd")) {
            char msg[256]; snprintf(msg, sizeof msg, "'%s'", nm);
            lg2_raise(ctx, "KeyError", msg);
            return lg2_null();
        }
        return lg2_string_runner_payload(nm, lg2_get(in, "args"));
    }
    if (lg2_str_eq(func, "_targets_gateway_pid"))
        return lg2_targets_gateway_pid(ctx, json_get_str(in, "command", ""),
            (long)json_get_num(in, "pid", 0), (int)json_get_num(in, "depth", 0));
    if (lg2_str_eq(func, "_resolve_lenient")) {
        char *s = lg2_resolve_lenient(ctx, json_get_str(in, "path", ""));
        json_t *r = lg2_strnode(s); free(s); return r;
    }
    if (lg2_str_eq(func, "_is_cloud_placeholder_path"))
        return lg2_is_cloud_placeholder_path(json_get_str(in, "path", ""));
    if (lg2_str_eq(func, "_on_cloud_path"))
        return lg2_on_cloud_path(ctx, json_get_str(in, "path", ""));
    if (lg2_str_eq(func, "_expand_candidate_path")) {
        char *s = lg2_expand_candidate_path(ctx, json_get_str(in, "candidate", ""));
        if (!s) return lg2_null();
        json_t *r = lg2_strnode(s); free(s); return r;
    }
    if (lg2_str_eq(func, "_resolved_or_nothing"))
        return lg2_resolved_or_nothing(ctx, json_get_str(in, "candidate", ""), json_get_str(in, "cwd", ""));
    if (lg2_str_eq(func, "_heredoc_write_targets"))
        return lg2_heredoc_write_targets(ctx, json_get_str(in, "unit", ""));
    if (lg2_str_eq(func, "_collect_heredoc_written_scripts")) {
        json_t *w = lg2_get(in, "written");
        json_t *b = w ? json_copy(w) : json_object();
        json_t *r = lg2_collect_heredoc_written_scripts(ctx, json_get_str(in, "command", ""), json_get_str(in, "cwd", ""), b);
        *state_out = json_copy(b);
        return r;
    }
    if (lg2_str_eq(func, "_walk_candidates"))
        return lg2_walk_candidates(ctx, json_get_str(in, "command", ""), json_get_str(in, "walk_command", ""),
            json_get_str(in, "cwd", ""), lg2_bval(lg2_get(in, "executed")));
    if (lg2_str_eq(func, "_has_binary_magic"))
        return lg2_has_binary_magic(json_get_str(in, "data", ""));
    if (lg2_str_eq(func, "_read_referenced_script_unlocked"))
        return lg2_read_referenced_script_unlocked(ctx, json_get_str(in, "path", ""), lg2_get(in, "max_bytes"));
    if (lg2_str_eq(func, "_scan_written_bodies"))
        return lg2_scan_written_bodies(ctx, lg2_get(in, "bodies"), json_get_str(in, "cwd", ""),
                                       lg2_bval(lg2_get(in, "executed")));
    if (lg2_str_eq(func, "scan_gateway_lifecycle"))
        return lg2_scan_gateway_lifecycle(ctx, json_get_str(in, "command", ""), json_get_str(in, "cwd", ""),
                                          lg2_get(in, "gateway_pid"));
    (void)v;
    return lg2_null();
}
