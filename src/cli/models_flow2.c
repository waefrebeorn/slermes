/* models_flow2.c — FLOW + leaves port of the UNCLAIMED remainder of
 * hermes_cli/models.py (the claimed siblings live in port_models_catalog_flow.c
 * / port_models_validate.c / models_pricing / models_catalog_static and are
 * SEAMED here).
 *
 * Decision-sequence port: Nous Portal pricing/union helpers, free-tier cache
 * read, Anthropic frontier/fast-mode/ultrafast predicates, /v1/models URL +
 * cursor helpers, provider cache-row builders, the per-provider catalog
 * fetchers (codex/nous/anthropic), the static-catalog tail, the locked
 * cache-row writer and the disk-cached custom-endpoint fetch
 * (cached_fetch_api_models). ALL collaborator I/O (claimed siblings,
 * foreign-module imports, credential/pool/clock reads, logger) rides the
 * injected universal seam md2_call; the harness scripts results and records
 * the ordered trace the oracle diffs against LIVE hermes_cli/models.py under
 * the same scripts. Ported functions call each other REAL (internal
 * composition: _live_result_entry -> _cache_entry, _model_requires_account_discovery
 * -> _normalized_cache_slug, _static_catalog -> _chat_catalog_rows,
 * cached_fetch_api_models -> _cache_entry/_chat_catalog_rows/_store_cache_entry,
 * get_cached_nous_free_tier -> _pricing_profile_key).
 *
 * Representation contract (shared with tests/oracle_models_flow2.py):
 *   - tuple/set -> array (sets sorted by code point); None -> JSON null;
 *     integral floats collapse to ints on both sides; Path -> str.
 *   - CuratedFallbackModels (list subclass) -> {"$curated": [items...]};
 *     _NativePickerModelList (list subclass) -> {"$native": [items...]}.
 *     list(x) on a tagged value strips the tag (plain array copy).
 *   - uncaught exception -> {"__raise__": cls}; an exception passed as a
 *     logger arg projects to {"__raise__": cls, "msg": str(e)}.
 *   - scripts: absent key -> None; {"$seq":[...]} per-call queue (exhaustion
 *     raises RuntimeError("script exhausted: <name>")); {"__raise__":cls,"msg":m}
 *     raises (trace ret is the WHOLE marker); any other value reused.
 *     None-valued seam arg keys dropped on BOTH sides ("" kept).
 *   - except Exception never catches the BaseException trio
 *     (SystemExit/KeyboardInterrupt/GeneratorExit) — those propagate.
 *   - fixture domain: str()/float() operands are ASCII strings, numbers,
 *     bools or null (container-repr stringification and non-ASCII whitespace
 *     are out of scope); float() tokens exclude underscores; URLs are
 *     scheme://host[:port]/path ASCII (urlparse hostname); base_url_origin
 *     results are 3-element arrays; cache rows carry fp/at/models.
 *   - object doubles (creds/runtime/entry) are attribute dicts read via
 *     json_obj_get; the background_caller() context manager and the
 *     write_under_cache_lock callback are modeled inertly (the lock seam
 *     records, then the port runs the load/mutate/save row steps in order).
 *
 * libjson-only (default oracle deps suffice). Self-contained: the harness
 * #includes "cli/models_flow2.c". mcat_ prefix is taken by flow1; this port
 * uses md2_.
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>

/* ── context + seam plumbing ─────────────────────────────────────────────── */

typedef struct md2_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *env;       /* borrowed: scripted constants/flags */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} md2_ctx;

static bool md2_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool md2_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool md2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool md2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool md2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool md2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool md2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

/* list-subclass tags */
static bool md2_is_curated(const json_t *v) {
    return md2_is_obj(v) && json_obj_get(v, "$curated") != NULL;
}
static bool md2_is_native(const json_t *v) {
    return md2_is_obj(v) && json_obj_get(v, "$native") != NULL;
}
/* borrowed items array of a list value (plain array or tagged subclass) */
static json_t *md2_items(const json_t *v) {
    if (!v) return NULL;
    if (v->type == JSON_ARRAY) return (json_t *)v;
    if (v->type == JSON_OBJECT) {
        json_t *t = json_obj_get(v, "$curated");
        if (t) return t;
        return json_obj_get(v, "$native");
    }
    return NULL;
}

static bool md2_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: return v->c.count > 0;
        case JSON_OBJECT: {
            json_t *items = md2_items(v);
            if (md2_is_curated(v) || md2_is_native(v))
                return items && items->type == JSON_ARRAY && items->c.count > 0;
            return v->c.count > 0;
        }
        default: return false;
    }
}

static const char *md2_jstr(const json_t *v) {
    return (md2_is_str(v) && v->str_val) ? v->str_val : "";
}
static double md2_num(const json_t *v) {
    return md2_is_num(v) ? v->num_val : 0.0;
}

/* borrowed object field or NULL */
static json_t *md2_get(const json_t *obj, const char *key) {
    return md2_is_obj(obj) ? json_obj_get(obj, key) : NULL;
}
/* dict.get(key, default): present-null keeps the null node; absent -> default.
 * Returns a BORROWED node (the default node is borrowed too). */
static json_t *md2_get_def(const json_t *obj, const char *key, json_t *def) {
    if (md2_is_obj(obj)) {
        json_t *v = json_obj_get(obj, key);
        if (v) return v;
    }
    return def;
}
static json_t *md2_copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}
/* list(x): plain array copy, stripping any list-subclass tag. */
static json_t *md2_list_copy(const json_t *v) {
    json_t *out = json_array();
    json_t *items = md2_items(v);
    if (items && items->type == JSON_ARRAY)
        for (size_t i = 0; i < items->c.count; i++)
            json_append(out, json_copy(items->c.items[i]));
    return out;
}
static json_t *md2_obj_copy(const json_t *v) {
    json_t *out = json_object();
    if (md2_is_obj(v))
        for (size_t i = 0; i < v->c.count; i++)
            json_set(out, v->c.keys[i], json_copy(v->c.items[i]));
    return out;
}
static json_t *md2_curated_wrap(const json_t *items_src) {
    json_t *out = json_object();
    json_set(out, "$curated", md2_list_copy(items_src));
    return out;
}
static json_t *md2_native_wrap(const json_t *items_src) {
    json_t *out = json_object();
    json_set(out, "$native", md2_list_copy(items_src));
    return out;
}

static bool md2_arr_has_str(const json_t *arr, const char *s) {
    if (!md2_is_arr(arr)) return false;
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *it = arr->c.items[i];
        if (md2_is_str(it) && md2_str_eq(it->str_val, s)) return true;
    }
    return false;
}

/* ── string helpers (caller frees) ───────────────────────────────────────── */

static bool md2_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static char *md2_strip(const char *s) {
    if (!s) return strdup("");
    const char *b = s;
    while (*b && md2_is_space(*b)) b++;
    const char *e = b + strlen(b);
    while (e > b && md2_is_space(e[-1])) e--;
    size_t n = (size_t)(e - b);
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, b, n); r[n] = '\0';
    return r;
}
static char *md2_lower(const char *s) {
    char *r = strdup(s ? s : "");
    if (!r) return NULL;
    for (char *p = r; *p; p++) *p = (char)tolower((unsigned char)*p);
    return r;
}
static char *md2_rstrip(const char *s, const char *chars) {
    if (!s) return strdup("");
    size_t n = strlen(s);
    while (n > 0 && strchr(chars, s[n - 1])) n--;
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, s, n); r[n] = '\0';
    return r;
}
/* str(v) for a scalar (null -> "None"). Caller frees. */
static char *md2_pystr(const json_t *v) {
    if (md2_is_none(v)) return strdup("None");
    if (md2_is_bool(v)) return strdup(v->bool_val ? "True" : "False");
    if (md2_is_num(v)) {
        char buf[64]; double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
        else snprintf(buf, sizeof buf, "%g", d);
        return strdup(buf);
    }
    if (md2_is_str(v)) return strdup(v->str_val ? v->str_val : "");
    return strdup("");
}
/* str(v or "") for a scalar (falsy -> ""). Caller frees. */
static char *md2_str_or_empty(const json_t *v) {
    if (!md2_truthy(v)) return strdup("");
    return md2_pystr(v);
}
/* first segment of s.split(sep)[0] (caller frees) */
static char *md2_split_first(const char *s, char sep) {
    const char *p = strchr(s, sep);
    size_t n = p ? (size_t)(p - s) : strlen(s);
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, s, n); r[n] = '\0';
    return r;
}
static bool md2_starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

/* float(v) coercion. err: 0 ok, 1 TypeError, 2 ValueError. ASCII token domain
 * (no underscores; inf/infinity/nan accepted case-insensitively). */
static double md2_float(const json_t *v, int *err) {
    *err = 0;
    if (md2_is_none(v)) { *err = 1; return 0.0; }
    if (md2_is_bool(v)) return v->bool_val ? 1.0 : 0.0;
    if (md2_is_num(v)) return v->num_val;
    if (!md2_is_str(v)) { *err = 1; return 0.0; }
    char *t = md2_strip(v->str_val ? v->str_val : "");
    if (!t) { *err = 2; return 0.0; }
    if (!t[0]) { free(t); *err = 2; return 0.0; }
    /* special literals: [+-]?(inf|infinity|nan) */
    const char *body = t;
    double sign = 1.0;
    if (*body == '+' || *body == '-') { if (*body == '-') sign = -1.0; body++; }
    char *lb = md2_lower(body);
    if (md2_str_eq(lb, "inf") || md2_str_eq(lb, "infinity")) {
        free(lb); free(t); return sign * INFINITY;
    }
    if (md2_str_eq(lb, "nan")) { free(lb); free(t); return NAN; }
    free(lb);
    /* numeric grammar: [+-]?(\d+(\.\d*)?|\.\d+)([eE][+-]?\d+)? */
    const char *p = t;
    if (*p == '+' || *p == '-') p++;
    bool any = false, dot = false;
    while (isdigit((unsigned char)*p)) { any = true; p++; }
    if (*p == '.') { dot = true; p++; while (isdigit((unsigned char)*p)) { any = true; p++; } }
    if (!any) { free(t); *err = 2; return 0.0; }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        bool ed = false;
        while (isdigit((unsigned char)*p)) { ed = true; p++; }
        if (!ed) { free(t); *err = 2; return 0.0; }
    }
    if (*p != '\0') { free(t); *err = 2; return 0.0; }
    (void)dot;
    double d = strtod(t, NULL);
    free(t);
    return d;
}

/* urlparse(s).hostname (lowercased host, no port/userinfo) or NULL.
 * ASCII scheme://host[:port]/path domain (protocol-relative //host too). */
static char *md2_url_hostname(const char *s) {
    if (!s) return NULL;
    const char *p = strstr(s, "://");
    if (p) p += 3;
    else if (s[0] == '/' && s[1] == '/') p = s + 2;
    else return NULL;
    const char *e = p;
    while (*e && *e != '/' && *e != '?' && *e != '#') e++;
    size_t n = (size_t)(e - p);
    char *host = (char *)malloc(n + 1);
    if (!host) return NULL;
    memcpy(host, p, n); host[n] = '\0';
    /* strip userinfo (last '@') */
    char *at = strrchr(host, '@');
    if (at) {
        char *h2 = strdup(at + 1);
        free(host);
        host = h2;
        if (!host) return NULL;
    }
    /* strip port (last ':' outside brackets — simple-host domain) */
    char *colon = strrchr(host, ':');
    if (colon && !strchr(host, ']')) *colon = '\0';
    char *low = md2_lower(host);
    free(host);
    if (low && !low[0]) { free(low); return NULL; }
    return low;
}

/* urllib.parse.quote_plus(s) — ASCII unreserved kept, space -> '+', else %XX. */
static char *md2_quote_plus(const char *s) {
    size_t n = strlen(s);
    char *r = (char *)malloc(n * 3 + 1);
    if (!r) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (isalnum(c) || c == '_' || c == '.' || c == '-' || c == '~') r[o++] = (char)c;
        else if (c == ' ') r[o++] = '+';
        else { static const char H[] = "0123456789ABCDEF";
               r[o++] = '%'; r[o++] = H[c >> 4]; r[o++] = H[c & 15]; }
    }
    r[o] = '\0';
    return r;
}

/* ── exception plumbing ──────────────────────────────────────────────────── */

static bool md2_exc_pending(const md2_ctx *ctx) { return ctx && ctx->exc != NULL; }
static void md2_exc_clear(md2_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}
static void md2_raise(md2_ctx *ctx, const char *cls, const char *msg) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
    json_set(ctx->exc, "msg", json_string(msg ? msg : ""));
}
static const char *md2_exc_class(const md2_ctx *ctx) {
    return json_get_str(ctx->exc, "__raise__", "");
}
static const char *md2_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        {"FileNotFoundError", "OSError"}, {"PermissionError", "OSError"},
        {"IsADirectoryError", "OSError"}, {"NotADirectoryError", "OSError"},
        {"FileExistsError", "OSError"}, {"InterruptedError", "OSError"},
        {"BlockingIOError", "OSError"}, {"ChildProcessError", "OSError"},
        {"ProcessLookupError", "OSError"}, {"ConnectionError", "OSError"},
        {"BrokenPipeError", "ConnectionError"}, {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
        {"TimeoutError", "OSError"},
        {"JSONDecodeError", "ValueError"}, {"UnicodeDecodeError", "UnicodeError"},
        {"UnicodeError", "ValueError"},
        {"ZeroDivisionError", "ArithmeticError"}, {"OverflowError", "ArithmeticError"},
        {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"},
        /* base Exception hierarchy (so `except Exception` catches the common
         * builtin raises; the BaseException trio above never matches Exception) */
        {"Exception", "BaseException"},
        {"RuntimeError", "Exception"}, {"NotImplementedError", "RuntimeError"},
        {"RecursionError", "RuntimeError"},
        {"ValueError", "Exception"}, {"TypeError", "Exception"},
        {"AttributeError", "Exception"}, {"NameError", "Exception"},
        {"UnboundLocalError", "NameError"}, {"LookupError", "Exception"},
        {"ArithmeticError", "Exception"}, {"FloatingPointError", "ArithmeticError"},
        {"OSError", "Exception"}, {"StopIteration", "Exception"},
        {"StopAsyncIteration", "Exception"}, {"AssertionError", "Exception"},
        {"SyntaxError", "Exception"}, {"IndentationError", "SyntaxError"},
        {"TabError", "IndentationError"}, {"SystemError", "Exception"},
        {"ReferenceError", "Exception"}, {"MemoryError", "Exception"},
        {"BufferError", "Exception"}, {"EOFError", "Exception"},
        {"ImportError", "Exception"}, {"ModuleNotFoundError", "ImportError"},
        {"Warning", "Exception"}, {"UserWarning", "Warning"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (md2_str_eq(T[i].c, c)) return T[i].p;
    return NULL;
}
static bool md2_exc_matches(const md2_ctx *ctx, const char *const *classes, size_t n) {
    if (!md2_exc_pending(ctx)) return false;
    const char *cls = md2_exc_class(ctx);
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (md2_str_eq(cls, classes[i])) return true;
        const char *p = md2_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}
/* except Exception (NOT the BaseException trio). */
static bool md2_exc_is_exception(const md2_ctx *ctx) {
    static const char *E[] = {"Exception"};
    return md2_exc_matches(ctx, E, 1);
}
static bool md2_exc_type_value(const md2_ctx *ctx) {
    static const char *E[] = {"TypeError", "ValueError"};
    return md2_exc_matches(ctx, E, 2);
}

/* ── universal seam ──────────────────────────────────────────────────────── */

static json_t *md2_script_result(md2_ctx *ctx, const json_t *script) {
    if (!script || script->type == JSON_NULL) return NULL;
    if (script->type == JSON_OBJECT) {
        json_t *raise = json_obj_get(script, "__raise__");
        if (raise && raise->type == JSON_STRING) {
            if (ctx->exc) json_free(ctx->exc);
            ctx->exc = json_copy(script);
            return NULL;
        }
    }
    return json_copy(script);
}

/* args ownership is TRANSFERRED (NULL -> empty object). Result is NEW. */
static json_t *md2_call(md2_ctx *ctx, const char *name, json_t *args) {
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
    if (chosen != ctx->exc) ret = md2_script_result(ctx, chosen);
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args);
    if (ctx->exc && chosen == ctx->exc) json_set(ev, "ret", json_copy(ctx->exc));
    else if (ctx->exc && chosen && chosen->type == JSON_OBJECT && json_obj_get(chosen, "__raise__"))
        json_set(ev, "ret", json_copy(ctx->exc));
    else json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(ctx->trace, ev);
    return ret;
}

/* Arg builders: None-valued keys dropped on BOTH sides ("" kept). Borrowed val
 * is copied; the caller keeps ownership of the source node. */
static void md2_arg(json_t *args, const char *key, const json_t *val) {
    if (!val || val->type == JSON_NULL) return;
    json_set(args, key, json_copy(val));
}
static void md2_arg_str(json_t *args, const char *key, const char *val) {
    if (!val) return;
    json_set(args, key, json_string(val));
}
static void md2_arg_bool(json_t *args, const char *key, bool val) {
    json_set(args, key, json_bool(val));
}
static void md2_arg_num(json_t *args, const char *key, double val) {
    json_set(args, key, json_number(val));
}
/* logger seam: {"msg","args"}. args_arr ownership transferred. */
static void md2_log(md2_ctx *ctx, const char *level, const char *msg, json_t *args_arr) {
    json_t *a = json_object();
    json_set(a, "msg", json_string(msg));
    json_set(a, "args", args_arr ? args_arr : json_array());
    char name[32];
    snprintf(name, sizeof name, "logger.%s", level);
    json_t *r = md2_call(ctx, name, a);
    json_free(r);
}
/* project the pending exception as a logger arg node {"__raise__","msg"}. */
static json_t *md2_exc_arg(const md2_ctx *ctx) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(md2_exc_class(ctx)));
    json_set(e, "msg", json_string(json_get_str(ctx->exc, "msg", "")));
    return e;
}

/* tuple result helpers */
static json_t *md2_pair(json_t *a, json_t *b) {
    json_t *r = json_array();
    json_append(r, a ? a : json_null());
    json_append(r, b ? b : json_null());
    return r;
}
static json_t *md2_pair_str(const char *a, const char *b) {
    json_t *r = json_array();
    json_append(r, json_string(a ? a : ""));
    json_append(r, json_string(b ? b : ""));
    return r;
}

/* ── env table accessors ─────────────────────────────────────────────────── */

static json_t *md2_env_obj(const md2_ctx *ctx, const char *key) {
    json_t *v = ctx->env ? json_obj_get(ctx->env, key) : NULL;
    return md2_is_obj(v) ? v : NULL;
}
static json_t *md2_env_arr(const md2_ctx *ctx, const char *key) {
    json_t *v = ctx->env ? json_obj_get(ctx->env, key) : NULL;
    return md2_is_arr(v) ? v : NULL;
}
static double md2_env_num(const md2_ctx *ctx, const char *key, double def) {
    return ctx->env ? json_get_num(ctx->env, key, def) : def;
}
static const char *md2_env_str(const md2_ctx *ctx, const char *key, const char *def) {
    return ctx->env ? json_get_str(ctx->env, key, def) : def;
}
static bool md2_env_set_has(const md2_ctx *ctx, const char *key, const char *val) {
    return md2_arr_has_str(md2_env_arr(ctx, key), val);
}

/* ── pricing / Portal union cluster ──────────────────────────────────────── */

/* PoP: md2_zero_priced @ hermes_cli/models.py:_zero_priced */
static json_t *md2_zero_priced(md2_ctx *ctx, json_t *in) {
    (void)ctx;
    json_t *pricing = md2_get(in, "pricing");
    json_t *keys = md2_get(in, "keys");
    const char *def = json_get_str(in, "default", "1");
    if (!md2_is_obj(pricing)) return json_bool(false);
    json_t *def_node = json_string(def);
    bool result = true;
    if (md2_is_arr(keys)) {
        for (size_t i = 0; i < keys->c.count; i++) {
            const char *k = md2_jstr(keys->c.items[i]);
            json_t *v = md2_get_def(pricing, k, def_node);
            int err = 0;
            double d = md2_float(v, &err);
            if (err) { result = false; break; }   /* except (TypeError, ValueError) */
            if (d != 0.0) { result = false; break; } /* all() short-circuit */
        }
    }
    json_free(def_node);
    return json_bool(result);
}

/* PoP: md2_is_subscription_billed @ hermes_cli/models.py:_is_subscription_billed */
static json_t *md2_is_subscription_billed(md2_ctx *ctx, json_t *in) {
    (void)ctx;
    json_t *entry = md2_get(in, "entry");
    bool r = md2_is_obj(entry) && md2_str_eq(json_get_str(entry, "billing_mode", NULL), "subscription");
    return json_bool(r);
}

/* PoP: md2_union_with_portal_recommendations @ hermes_cli/models.py:_union_with_portal_recommendations */
static json_t *md2_union_with_portal_recommendations(md2_ctx *ctx, json_t *in) {
    const char *tier_key = json_get_str(in, "tier_key", "");
    json_t *curated_ids = md2_get(in, "curated_ids");
    json_t *pricing = md2_get(in, "pricing");
    const char *portal = json_get_str(in, "portal_base_url", "");
    bool force_refresh = json_get_bool(in, "force_refresh", false);
    bool synth = json_get_bool(in, "synthesize_free_pricing", false);

    /* try: payload = fetch_nous_recommended_models(...) except Exception: None */
    json_t *a = json_object();
    md2_arg_str(a, "portal_base_url", portal);
    md2_arg_bool(a, "force_refresh", force_refresh);
    json_t *payload = md2_call(ctx, "fetch_nous_recommended_models", a);
    if (md2_exc_pending(ctx)) {
        if (!md2_exc_is_exception(ctx)) { json_free(payload); return NULL; }
        md2_exc_clear(ctx);
        json_free(payload);
        payload = NULL;
    }
    json_t *block = md2_is_obj(payload) ? md2_get(payload, tier_key) : NULL;
    json_t *entries = md2_is_arr(block) ? block : NULL;
    json_t *portal_ids = json_array();
    if (entries) {
        for (size_t i = 0; i < entries->c.count; i++) {
            json_t *ea = json_object();
            md2_arg(ea, "entry", entries->c.items[i]);
            json_t *name = md2_call(ctx, "extract_model_name", ea);
            if (md2_exc_pending(ctx)) {  /* comprehension is OUTSIDE the try */
                json_free(name); json_free(portal_ids); json_free(payload);
                return NULL;
            }
            if (md2_truthy(name)) json_append(portal_ids, json_copy(name));
            json_free(name);
        }
    }
    json_free(payload);
    if (portal_ids->c.count == 0) {
        json_free(portal_ids);
        return md2_pair(md2_list_copy(curated_ids), md2_obj_copy(pricing));
    }
    json_t *augmented = md2_obj_copy(pricing);
    if (synth) {
        for (size_t i = 0; i < portal_ids->c.count; i++) {
            const char *mid = md2_jstr(portal_ids->c.items[i]);
            if (!json_has(augmented, mid)) {
                json_t *z = json_object();
                json_set(z, "prompt", json_string("0"));
                json_set(z, "completion", json_string("0"));
                json_set(augmented, mid, z);
            }
        }
    }
    json_t *out_list = md2_list_copy(curated_ids);
    for (size_t i = 0; i < portal_ids->c.count; i++) {
        const char *mid = md2_jstr(portal_ids->c.items[i]);
        if (!md2_arr_has_str(curated_ids, mid))
            json_append(out_list, json_string(mid));
    }
    json_free(portal_ids);
    return md2_pair(out_list, augmented);
}

/* on_sale row: (-discount_pct, model_id) sorted ascending (Python tuple sort) */
typedef struct md2_sale { double neg_pct; char *mid; } md2_sale;
static int md2_sale_cmp(const void *a, const void *b) {
    const md2_sale *x = (const md2_sale *)a, *y = (const md2_sale *)b;
    if (x->neg_pct != y->neg_pct) return (x->neg_pct < y->neg_pct) ? -1 : 1;
    return strcmp(x->mid, y->mid);
}

/* PoP: md2_union_with_nous_on_sale_models @ hermes_cli/models.py:union_with_nous_on_sale_models */
static json_t *md2_union_with_nous_on_sale_models(md2_ctx *ctx, json_t *in) {
    json_t *curated_ids = md2_get(in, "curated_ids");
    json_t *pricing = md2_get(in, "pricing");
    md2_sale *rows = NULL;
    size_t n = 0, cap = 0;
    json_t *empty_str = json_string("");
    json_t *p0 = md2_truthy(pricing) ? pricing : NULL;  /* (pricing or {}) */
    if (md2_is_obj(p0)) {
        for (size_t i = 0; i < p0->c.count; i++) {
            const char *mid = p0->c.keys[i];
            json_t *entry = p0->c.items[i];
            json_t *tools = md2_get(entry, "tools");
            if (md2_arr_has_str(curated_ids, mid) || !md2_is_obj(entry)
                || (md2_is_bool(tools) && !tools->bool_val)   /* is False identity */
                || md2_truthy(md2_get(entry, "generation")))
                continue;
            json_t *sa = json_object();
            md2_arg(sa, "prompt", md2_get_def(entry, "prompt", empty_str));
            md2_arg(sa, "completion", md2_get_def(entry, "completion", empty_str));
            md2_arg(sa, "original", md2_get(entry, "original"));
            json_t *sale = md2_call(ctx, "compute_sale_discount", sa);
            if (md2_exc_pending(ctx)) goto fail;
            /* paid = any(price_float(...) is not None and isfinite(...) ...) */
            bool paid = false;
            json_t *pa = json_object();
            md2_arg(pa, "raw", md2_get(entry, "prompt"));
            md2_arg_bool(pa, "positive", true);
            json_t *rate = md2_call(ctx, "price_float", pa);
            if (md2_exc_pending(ctx)) { json_free(sale); goto fail; }
            if (!md2_is_none(rate) && isfinite(md2_num(rate))) paid = true;
            json_free(rate);
            if (!paid) {
                json_t *pb = json_object();
                md2_arg(pb, "raw", md2_get(entry, "completion"));
                md2_arg_bool(pb, "positive", true);
                json_t *rate2 = md2_call(ctx, "price_float", pb);
                if (md2_exc_pending(ctx)) { json_free(sale); goto fail; }
                if (!md2_is_none(rate2) && isfinite(md2_num(rate2))) paid = true;
                json_free(rate2);
            }
            if (!md2_is_none(sale) && md2_is_obj(md2_get(entry, "original")) && paid) {
                json_t *pct = md2_is_arr(sale) ? json_get(sale, 0) : NULL;
                if (n == cap) { cap = cap ? cap * 2 : 8;
                    rows = (md2_sale *)realloc(rows, cap * sizeof(md2_sale)); }
                rows[n].neg_pct = -md2_num(pct);
                rows[n].mid = strdup(mid);
                n++;
            }
            json_free(sale);
        }
    }
    json_free(empty_str);
    if (rows) qsort(rows, n, sizeof(md2_sale), md2_sale_cmp);
    json_t *out = md2_list_copy(curated_ids);
    for (size_t i = 0; i < n; i++) { json_append(out, json_string(rows[i].mid)); free(rows[i].mid); }
    free(rows);
    return out;
fail:
    json_free(empty_str);
    for (size_t i = 0; i < n; i++) free(rows[i].mid);
    free(rows);
    return NULL;
}

/* PoP: md2_pricing_profile_key @ hermes_cli/models.py:_pricing_profile_key */
static json_t *md2_pricing_profile_key(md2_ctx *ctx, json_t *in) {
    (void)in;
    return md2_call(ctx, "hermes_home_key", json_object());
}

/* PoP: md2_get_cached_nous_free_tier @ hermes_cli/models.py:get_cached_nous_free_tier */
static json_t *md2_get_cached_nous_free_tier(md2_ctx *ctx, json_t *in) {
    json_t *key = md2_pricing_profile_key(ctx, in);
    if (md2_exc_pending(ctx)) { json_free(key); return NULL; }
    json_t *cache = md2_env_obj(ctx, "free_tier_cache");
    json_t *cached = md2_get(cache, md2_jstr(key));
    json_free(key);
    if (md2_is_none(cached)) return NULL;
    json_t *mono = md2_call(ctx, "time.monotonic", json_object());
    if (md2_exc_pending(ctx)) { json_free(mono); return NULL; }
    json_t *ts = md2_is_arr(cached) ? json_get(cached, 1) : NULL;
    double ttl = md2_env_num(ctx, "free_tier_ttl", 180);
    double age = md2_num(mono) - md2_num(ts);
    json_free(mono);
    if (age >= ttl) return NULL;
    json_t *res = md2_is_arr(cached) ? json_get(cached, 0) : NULL;
    return md2_copy_or_null(res);
}

/* ── Anthropic frontier / fast-mode / ultrafast cluster ──────────────────── */

/* PoP: md2_is_anthropic_frontier_tier @ hermes_cli/models.py:_is_anthropic_frontier_tier */
static json_t *md2_is_anthropic_frontier_tier(md2_ctx *ctx, json_t *in) {
    char *raw0 = md2_str_or_empty(md2_get(in, "model_id"));
    json_t *a = json_object();
    md2_arg_str(a, "model_id", raw0);
    free(raw0);
    json_t *r = md2_call(ctx, "strip_vendor_prefix", a);
    if (md2_exc_pending(ctx)) { json_free(r); return NULL; }
    char *seg = md2_split_first(md2_jstr(r), ':');
    json_free(r);
    char *base = md2_lower(seg);
    free(seg);
    bool result = false;
    if (md2_starts_with(base, "claude-"))
        result = strstr(base, "opus-") != NULL || strstr(base, "fable-") != NULL;
    free(base);
    return json_bool(result);
}

/* PoP: md2_anthropic_models_url @ hermes_cli/models.py:_anthropic_models_url */
static json_t *md2_anthropic_models_url(md2_ctx *ctx, json_t *in) {
    (void)ctx;
    json_t *bu = md2_get(in, "base_url");
    char *endpoint;
    if (md2_truthy(bu)) {
        char *s = md2_pystr(bu);
        char *st = md2_strip(s);
        free(s);
        endpoint = md2_rstrip(st, "/");
        free(st);
    } else {
        endpoint = strdup("https://api.anthropic.com");
    }
    size_t en = strlen(endpoint);
    bool ends_v1 = en >= 3 && strcmp(endpoint + en - 3, "/v1") == 0;
    const char *suffix = ends_v1 ? "/models" : "/v1/models";
    const char *after_id = json_get_str(in, "after_id", NULL);
    bool has_after = after_id && after_id[0];
    /* url + ("&" if "?" in url else "?") + urlencode(params) */
    bool has_q = strchr(endpoint, '?') != NULL;
    char *qafter = has_after ? md2_quote_plus(after_id) : NULL;
    size_t total = en + strlen(suffix) + 1 + strlen("limit=1000")
                 + (has_after ? strlen("&after_id=") + strlen(qafter) : 0) + 1;
    char *url = (char *)malloc(total);
    if (url) {
        snprintf(url, total, "%s%s%s%s%s%s%s", endpoint, suffix,
                 has_q ? "&" : "?", "limit=1000",
                 has_after ? "&after_id=" : "", has_after ? qafter : "", "");
    }
    free(endpoint);
    free(qafter);
    json_t *out = json_string(url ? url : "");
    free(url);
    return out;
}

/* PoP: md2_anthropic_next_cursor @ hermes_cli/models.py:_anthropic_next_cursor */
static json_t *md2_anthropic_next_cursor(md2_ctx *ctx, json_t *in, json_t *seen) {
    (void)ctx;
    json_t *page = md2_get(in, "page");
    json_t *hm = md2_get(page, "has_more");
    if (!md2_is_obj(page) || !(md2_is_bool(hm) && hm->bool_val))  /* is not True */
        return NULL;
    json_t *last = md2_get(page, "last_id");
    if (!md2_is_str(last) || !last->str_val || !last->str_val[0]) return NULL;
    if (json_has(seen, last->str_val)) return NULL;
    json_set(seen, last->str_val, json_bool(true));
    return json_string(last->str_val);
}

/* PoP: md2_fast_mode_route_supported @ hermes_cli/models.py:_fast_mode_route_supported */
static json_t *md2_fast_mode_route_supported(md2_ctx *ctx, json_t *in) {
    json_t *model_id = md2_get(in, "model_id");
    const char *provider = json_get_str(in, "provider", NULL);
    json_t *base_url = md2_get(in, "base_url");
    json_t *tier = md2_get(in, "tier");

    /* allowed map per branch: keys (providers) + values (hosts) */
    const char *akeys[3]; const char *avals[3]; size_t an = 0;
    /* effective base_url string for the host gate (region branch may rewrite) */
    const char *eff_base = md2_is_none(base_url) ? "" : md2_jstr(base_url);

    json_t *fa = json_object();
    md2_arg(fa, "model_id", model_id);
    json_t *is_anth = md2_call(ctx, "is_anthropic_fast_model", fa);
    if (md2_exc_pending(ctx)) { json_free(is_anth); return NULL; }
    bool anth = md2_truthy(is_anth);
    json_free(is_anth);
    if (anth) {
        akeys[0] = "anthropic"; avals[0] = "api.anthropic.com"; an = 1;
    } else {
        char *mid0 = md2_str_or_empty(model_id);
        json_t *ga = json_object();
        md2_arg_str(ga, "model_id", mid0);
        free(mid0);
        json_t *grok = md2_call(ctx, "is_grok_46_family", ga);
        if (md2_exc_pending(ctx)) { json_free(grok); return NULL; }
        bool is_grok = md2_truthy(grok);
        json_free(grok);
        if (is_grok) {
            akeys[0] = "xai"; avals[0] = "api.x.ai"; an = 1;
        } else {
            json_t *ra = json_object();
            md2_arg(ra, "base_url", base_url);
            md2_arg(ra, "model_id", model_id);
            md2_arg(ra, "tier", tier);
            json_t *allows = md2_call(ctx, "openai_fast_region_allows", ra);
            if (md2_exc_pending(ctx)) { json_free(allows); return NULL; }
            /* Eligibility only; the runtime URL stays regional. */
            if (md2_truthy(allows)) eff_base = "https://api.openai.com";
            json_free(allows);
            akeys[0] = "openai"; avals[0] = "api.openai.com";
            akeys[1] = "openai-api"; avals[1] = "api.openai.com";
            akeys[2] = "openai-codex"; avals[2] = "chatgpt.com"; an = 3;
        }
    }
    /* provider gate: if provider and normalize_provider(provider) not in allowed */
    if (provider && provider[0]) {
        json_t *na = json_object();
        md2_arg_str(na, "provider", provider);
        json_t *norm = md2_call(ctx, "normalize_provider", na);
        if (md2_exc_pending(ctx)) { json_free(norm); return NULL; }
        const char *ns = md2_jstr(norm);
        bool ok = false;
        for (size_t i = 0; i < an; i++) if (md2_str_eq(ns, akeys[i])) ok = true;
        json_free(norm);
        if (!ok) return json_bool(false);
    }
    /* host gate: not host or host in allowed.values() */
    char *host = md2_url_hostname(eff_base);
    bool result;
    if (!host || !host[0]) result = true;
    else {
        result = false;
        for (size_t i = 0; i < an; i++) if (md2_str_eq(host, avals[i])) result = true;
    }
    free(host);
    return json_bool(result);
}

/* PoP: md2_model_supports_ultrafast @ hermes_cli/models.py:model_supports_ultrafast */
static json_t *md2_model_supports_ultrafast(md2_ctx *ctx, json_t *in) {
    char *mid0 = md2_str_or_empty(md2_get(in, "model_id"));
    json_t *sa = json_object();
    md2_arg_str(sa, "model_id", mid0);
    free(mid0);
    json_t *stripped = md2_call(ctx, "strip_codex_context_variant_suffix", sa);
    if (md2_exc_pending(ctx)) { json_free(stripped); return NULL; }
    json_t *va = json_object();
    md2_arg_str(va, "model_id", md2_jstr(stripped));
    json_free(stripped);
    json_t *vend = md2_call(ctx, "strip_vendor_prefix", va);
    if (md2_exc_pending(ctx)) { json_free(vend); return NULL; }
    char *base = md2_split_first(md2_jstr(vend), ':');
    json_free(vend);
    bool r = md2_env_set_has(ctx, "ultrafast_models", base);
    free(base);
    return json_bool(r);
}

/* ── provider cache-row cluster ──────────────────────────────────────────── */

/* PoP: md2_cache_entry @ hermes_cli/models.py:_cache_entry */
static json_t *md2_cache_entry(md2_ctx *ctx, const char *fp, json_t *models, json_t *at) {
    json_t *out = json_object();
    json_set(out, "fp", json_string(fp ? fp : ""));
    if (md2_is_none(at)) {
        json_t *now = md2_call(ctx, "time.time", json_object());
        if (md2_exc_pending(ctx)) { json_free(now); json_free(out); return NULL; }
        json_set(out, "at", now ? now : json_null());
    } else {
        json_set(out, "at", json_copy(at));
    }
    json_set(out, "models", md2_list_copy(models));
    return out;
}

/* PoP: md2_live_result_entry @ hermes_cli/models.py:_live_result_entry */
static json_t *md2_live_result_entry(md2_ctx *ctx, json_t *in) {
    const char *fp = json_get_str(in, "fp", "");
    json_t *live = md2_get(in, "live");
    json_t *existing = md2_get(in, "existing");
    json_t *at = md2_get(in, "at");
    if (!md2_is_curated(live))
        return md2_cache_entry(ctx, fp, live, at);
    json_t *va = json_object();
    md2_arg(va, "entry", existing);
    md2_arg_str(va, "fp", fp);
    json_t *valid = md2_call(ctx, "cache_entry_valid", va);
    if (md2_exc_pending(ctx)) { json_free(valid); return NULL; }
    bool v = md2_truthy(valid);
    json_free(valid);
    if (v) {
        if (!md2_is_obj(existing)) {
            md2_raise(ctx, "AttributeError", "'NoneType' object has no attribute 'get'");
            return NULL;
        }
        if (!md2_truthy(md2_get(existing, "fallback"))) return NULL;
    }
    json_t *base = md2_cache_entry(ctx, fp, live, at);
    if (md2_exc_pending(ctx)) { json_free(base); return NULL; }
    json_set(base, "fallback", json_bool(true));
    return base;
}

/* PoP: md2_normalized_cache_slug @ hermes_cli/models.py:_normalized_cache_slug */
static json_t *md2_normalized_cache_slug(md2_ctx *ctx, json_t *in) {
    json_t *provider = md2_get(in, "provider");
    char *s0 = md2_str_or_empty(provider);
    char *st = md2_strip(s0);
    free(s0);
    char *requested = md2_lower(st);
    free(st);
    if (md2_str_eq(requested, "ollama")) {
        json_t *out = json_string(requested);
        free(requested);
        return out;
    }
    free(requested);
    json_t *na = json_object();
    md2_arg(na, "provider", provider);
    json_t *norm = md2_call(ctx, "normalize_provider", na);
    if (md2_exc_pending(ctx)) { json_free(norm); return NULL; }
    if (md2_truthy(norm)) return norm;
    json_free(norm);
    /* (provider or "") */
    char *fb = md2_str_or_empty(provider);
    json_t *out = json_string(fb);
    free(fb);
    return out;
}

/* PoP: md2_model_requires_account_discovery @ hermes_cli/models.py:_model_requires_account_discovery */
static json_t *md2_model_requires_account_discovery(md2_ctx *ctx, json_t *in) {
    const char *model = json_get_str(in, "model", "");
    json_t *slug = md2_normalized_cache_slug(ctx, in);
    if (md2_exc_pending(ctx)) { json_free(slug); return NULL; }
    const char *s = md2_jstr(slug);
    json_free(slug);
    if (md2_str_eq(s, "openai-codex")) {
        json_t *ca = json_object();
        md2_arg_str(ca, "model", model);
        return md2_call(ctx, "codex_model_requires_account_discovery", ca);
    }
    if (md2_str_eq(s, "openai") || md2_str_eq(s, "openai-api")) {
        json_t *aa = json_object();
        md2_arg_str(aa, "model", model);
        return md2_call(ctx, "is_astra_model", aa);
    }
    return json_bool(false);
}

/* PoP: md2_ollama_native_probe_reachable @ hermes_cli/models.py:_ollama_native_probe_reachable */
static json_t *md2_ollama_native_probe_reachable(md2_ctx *ctx, json_t *in) {
    (void)in;
    json_t *base = md2_call(ctx, "get_ollama_base_url", json_object());
    if (md2_exc_pending(ctx)) { json_free(base); return NULL; }
    json_t *ha = json_object();
    md2_arg(ha, "base_url", base);
    json_t *headers0 = md2_call(ctx, "get_ollama_native_headers", ha);
    if (md2_exc_pending(ctx)) { json_free(base); json_free(headers0); return NULL; }
    json_t *headers = md2_truthy(headers0) ? headers0 : NULL;  /* or None */
    json_t *ra = json_object();
    md2_arg(ra, "base_url", base);
    json_t *root = md2_call(ctx, "root_for_ollama_native_api", ra);
    if (md2_exc_pending(ctx)) { json_free(base); json_free(headers0); json_free(root); return NULL; }
    json_t *ka = json_object();
    md2_arg(ka, "root", root);
    md2_arg(ka, "headers", headers);
    json_t *key = md2_call(ctx, "ollama_probe_cache_key", ka);
    if (md2_exc_pending(ctx)) { json_free(base); json_free(headers0); json_free(root); json_free(key); return NULL; }
    json_t *table = md2_env_obj(ctx, "probe_reachable");
    json_t *v = md2_get(table, md2_jstr(key));
    bool r = md2_is_bool(v) && v->bool_val;   /* is True identity */
    json_free(base); json_free(headers0); json_free(root); json_free(key);
    return json_bool(r);
}

/* PoP: md2_resolve_anthropic_pool_catalog_credentials @ hermes_cli/models.py:_resolve_anthropic_pool_catalog_credentials */
static json_t *md2_resolve_anthropic_pool_catalog_credentials(md2_ctx *ctx, json_t *in) {
    (void)in;
    const char *auth_type = md2_env_str(ctx, "auth_type_api_key", "api_key");
    json_t *pa = json_object();
    md2_arg_str(pa, "provider", "anthropic");
    json_t *pool = md2_call(ctx, "read_credential_pool", pa);
    if (md2_exc_pending(ctx)) {
        if (!md2_exc_is_exception(ctx)) { json_free(pool); return NULL; }
        md2_exc_clear(ctx); json_free(pool);
        return md2_pair_str("", "");
    }
    if (!md2_is_arr(pool)) {
        /* for entry in None -> TypeError (caught) */
        json_free(pool);
        return md2_pair_str("", "");
    }
    for (size_t i = 0; i < pool->c.count; i++) {
        json_t *entry = pool->c.items[i];
        if (!md2_is_obj(entry) || !md2_str_eq(json_get_str(entry, "auth_type", NULL), auth_type))
            continue;
        char *t0 = md2_str_or_empty(md2_get(entry, "access_token"));
        char *token = md2_strip(t0);
        free(t0);
        if (token[0]) {
            json_t *b = md2_get(entry, "base_url");
            if (!md2_truthy(b)) b = md2_get(entry, "inference_base_url");
            char *b0 = md2_str_or_empty(b);
            char *base = md2_strip(b0);
            free(b0);
            json_t *out = md2_pair_str(token, base);
            free(token); free(base); json_free(pool);
            return out;
        }
        free(token);
    }
    json_free(pool);
    return md2_pair_str("", "");
}

/* ── probe / deepinfra / chat-catalog leaves ─────────────────────────────── */

/* PoP: md2_probe_neg_key @ hermes_cli/models.py:_probe_neg_key */
static json_t *md2_probe_neg_key(md2_ctx *ctx, json_t *in) {
    json_t *ba = json_object();
    md2_arg_str(ba, "base_url", json_get_str(in, "base_url", ""));
    json_t *r = md2_call(ctx, "base_url_origin", ba);
    if (md2_exc_pending(ctx)) { json_free(r); return NULL; }
    if (!md2_is_arr(r) || r->c.count != 3) {
        json_free(r);
        md2_raise(ctx, "ValueError", "unpack requires a 3-element sequence");
        return NULL;
    }
    json_t *host = json_get(r, 1);
    json_t *port = json_get(r, 2);
    if (!md2_truthy(host)) { json_free(r); return NULL; }
    char *hs = md2_pystr(host);
    char *ps = md2_pystr(port);
    size_t n = strlen(hs) + strlen(ps) + 2;
    char *buf = (char *)malloc(n);
    if (buf) snprintf(buf, n, "%s:%s", hs, ps);
    json_t *out = json_string(buf ? buf : "");
    free(buf); free(hs); free(ps); json_free(r);
    return out;
}

/* PoP: md2_probe_result @ hermes_cli/models.py:_probe_result */
static json_t *md2_probe_result(md2_ctx *ctx, json_t *in) {
    (void)ctx;
    json_t *out = json_object();
    json_set(out, "models", md2_copy_or_null(md2_get(in, "models")));
    json_set(out, "probed_url", md2_copy_or_null(md2_get(in, "probed_url")));
    json_set(out, "resolved_base_url", md2_copy_or_null(md2_get(in, "resolved_base_url")));
    json_set(out, "suggested_base_url", md2_copy_or_null(md2_get(in, "suggested_base_url")));
    json_set(out, "used_fallback", json_bool(json_get_bool(in, "used_fallback", false)));
    return out;
}

/* PoP: md2_deepinfra_env @ hermes_cli/models.py:_deepinfra_env */
static json_t *md2_deepinfra_env(md2_ctx *ctx, json_t *in) {
    json_t *ka = json_object();
    md2_arg_str(ka, "key", json_get_str(in, "key", ""));
    json_t *r = md2_call(ctx, "get_env_value_prefer_dotenv", ka);
    if (md2_exc_pending(ctx)) { json_free(r); return NULL; }
    char *s0 = md2_str_or_empty(r);
    json_free(r);
    char *s = md2_strip(s0);
    free(s0);
    json_t *out = json_string(s);
    free(s);
    return out;
}

/* PoP: md2_chat_catalog_rows @ hermes_cli/models.py:_chat_catalog_rows */
static json_t *md2_chat_catalog_rows(md2_ctx *ctx, json_t *models) {
    json_t *a = json_object();
    md2_arg(a, "models", models);
    return md2_call(ctx, "without_generation_models", a);
}

/* ── per-provider catalog fetchers ───────────────────────────────────────── */

/* PoP: md2_codex_catalog @ hermes_cli/models.py:_codex_catalog */
static json_t *md2_codex_catalog(md2_ctx *ctx, json_t *in) {
    (void)in;
    json_t *access_token = NULL;   /* owned copies */
    json_t *base_url = NULL;
    json_t *ca = json_object();
    md2_arg_bool(ca, "read_only", true);
    json_t *creds = md2_call(ctx, "resolve_codex_runtime_credentials", ca);
    if (md2_exc_pending(ctx)) {
        if (!md2_exc_is_exception(ctx)) { json_free(creds); return NULL; }
        md2_exc_clear(ctx); json_free(creds);
        goto call;   /* access_token = None; base_url stays None */
    }
    if (!md2_is_obj(creds)) {
        /* creds.get(...) -> AttributeError (caught) */
        json_free(creds);
        goto call;
    }
    access_token = md2_copy_or_null(md2_get(creds, "api_key"));
    base_url = md2_copy_or_null(md2_get(creds, "base_url"));
    json_free(creds);
    json_t *ea = json_object();
    md2_arg(ea, "access_token", access_token);
    md2_arg_num(ea, "skew_seconds", 0);
    json_t *exp = md2_call(ctx, "codex_access_token_is_expiring", ea);
    if (md2_exc_pending(ctx)) {
        if (!md2_exc_is_exception(ctx)) { json_free(exp); json_free(access_token); json_free(base_url); return NULL; }
        md2_exc_clear(ctx); json_free(exp);
        json_free(access_token); access_token = NULL;  /* base_url KEEPS its value */
        goto call;
    }
    if (md2_truthy(exp)) { json_free(access_token); access_token = NULL; }
    json_free(exp);
call:;
    json_t *ga = json_object();
    md2_arg(ga, "access_token", access_token);
    md2_arg(ga, "base_url", base_url);
    json_free(access_token); json_free(base_url);
    return md2_call(ctx, "get_codex_model_ids", ga);
}

/* PoP: md2_nous_catalog @ hermes_cli/models.py:_nous_catalog */
static json_t *md2_nous_catalog(md2_ctx *ctx, json_t *in) {
    (void)in;
    /* with background_caller(): creds = resolve_nous_runtime_credentials() */
    json_t *cm = md2_call(ctx, "background_caller", json_object());
    json_free(cm);   /* CM enter/exit inert (no suppression) */
    if (md2_exc_pending(ctx)) {
        if (!md2_exc_is_exception(ctx)) return NULL;
        md2_exc_clear(ctx);
        goto fallback;
    }
    json_t *creds = md2_call(ctx, "resolve_nous_runtime_credentials", json_object());
    if (md2_exc_pending(ctx)) {
        if (!md2_exc_is_exception(ctx)) { json_free(creds); return NULL; }
        md2_exc_clear(ctx); json_free(creds);
        goto fallback;
    }
    if (md2_truthy(creds)) {
        if (!md2_is_obj(creds)) {   /* creds.get -> AttributeError (caught) */
            json_free(creds);
            goto fallback;
        }
        json_t *empty = json_string("");
        json_t *fa = json_object();
        md2_arg(fa, "api_key", md2_get_def(creds, "api_key", empty));
        md2_arg(fa, "inference_base_url", md2_get_def(creds, "base_url", empty));
        json_free(empty);
        json_t *live = md2_call(ctx, "fetch_nous_models", fa);
        json_free(creds);
        if (md2_exc_pending(ctx)) {
            if (!md2_exc_is_exception(ctx)) { json_free(live); return NULL; }
            md2_exc_clear(ctx); json_free(live);
            goto fallback;
        }
        if (md2_truthy(live)) return live;
        json_free(live);
    } else {
        json_free(creds);
    }
fallback:;
    json_t *r = md2_call(ctx, "get_curated_nous_model_ids", json_object());
    if (md2_exc_pending(ctx)) { json_free(r); return NULL; }
    if (md2_truthy(r)) return r;
    json_free(r);
    return NULL;
}

/* PoP: md2_anthropic_catalog @ hermes_cli/models.py:_anthropic_catalog */
static json_t *md2_anthropic_catalog(md2_ctx *ctx, json_t *in) {
    (void)in;
    json_t *model_cfg = md2_call(ctx, "get_model_config_dict", json_object());
    if (md2_exc_pending(ctx)) { json_free(model_cfg); return NULL; }
    if (!md2_is_obj(model_cfg)) {
        json_free(model_cfg);
        md2_raise(ctx, "AttributeError", "'NoneType' object has no attribute 'get'");
        return NULL;
    }
    json_t *empty = json_string("");
    char *prov0 = md2_str_or_empty(md2_get_def(model_cfg, "provider", empty));
    json_t *na = json_object();
    md2_arg_str(na, "provider", prov0);
    free(prov0);
    json_t *norm = md2_call(ctx, "normalize_provider", na);
    if (md2_exc_pending(ctx)) { json_free(model_cfg); json_free(empty); json_free(norm); return NULL; }
    bool is_anth = md2_str_eq(md2_jstr(norm), "anthropic");
    json_free(norm);
    char *cfg_base_url = strdup("");
    char *cfg_api_key = strdup("");
    if (is_anth) {
        char *b0 = md2_str_or_empty(md2_get_def(model_cfg, "base_url", empty));
        char *b = md2_strip(b0); free(b0);
        free(cfg_base_url); cfg_base_url = b;
        char *k0 = md2_str_or_empty(md2_get_def(model_cfg, "api_key", empty));
        char *k = md2_strip(k0); free(k0);
        free(cfg_api_key); cfg_api_key = k;
    }
    json_free(model_cfg); json_free(empty);
    json_t *fa = json_object();
    md2_arg_str(fa, "base_url", cfg_base_url[0] ? cfg_base_url : NULL);
    md2_arg_str(fa, "api_key", cfg_api_key[0] ? cfg_api_key : NULL);
    json_t *live = md2_call(ctx, "fetch_anthropic_models", fa);
    if (md2_exc_pending(ctx)) { free(cfg_base_url); free(cfg_api_key); json_free(live); return NULL; }
    json_t *pm = md2_env_obj(ctx, "provider_models");
    json_t *curated = md2_list_copy(md2_get(pm, "anthropic"));
    if (!md2_truthy(live)) {
        json_free(live); free(cfg_base_url); free(cfg_api_key);
        return md2_curated_wrap(curated);
    }
    if (cfg_base_url[0]) {
        free(cfg_base_url); free(cfg_api_key); json_free(curated);
        return live;
    }
    free(cfg_base_url); free(cfg_api_key);
    json_t *ma = json_object();
    md2_arg(ma, "primary", curated);
    md2_arg(ma, "secondary", live);
    json_free(curated); json_free(live);
    return md2_call(ctx, "merge_unique", ma);
}

/* PoP: md2_static_catalog @ hermes_cli/models.py:_static_catalog */
static json_t *md2_static_catalog(md2_ctx *ctx, json_t *in) {
    const char *normalized = json_get_str(in, "normalized", "");
    json_t *fetcher = md2_get(in, "fetcher");
    bool fetcher_present = !md2_is_none(fetcher);   /* fetcher is not None */
    json_t *sa = json_object();
    md2_arg_str(sa, "provider", normalized);
    json_t *cat = md2_call(ctx, "static_catalog", sa);
    if (md2_exc_pending(ctx)) { json_free(cat); return NULL; }
    json_t *curated_static = fetcher_present ? md2_curated_wrap(cat) : md2_list_copy(cat);
    json_free(cat);
    if (!md2_env_set_has(ctx, "models_dev_preferred", normalized)) {
        json_t *da = json_object();
        md2_arg_str(da, "normalized", normalized);
        md2_arg(da, "rows", curated_static);
        json_t *dropped = md2_call(ctx, "drop_delisted_opencode_models", da);
        json_free(curated_static);
        if (md2_exc_pending(ctx)) { json_free(dropped); return NULL; }
        json_t *rows = md2_chat_catalog_rows(ctx, dropped);
        json_free(dropped);
        return rows;
    }
    json_t *mwa = json_object();
    md2_arg_str(mwa, "provider", normalized);
    md2_arg(mwa, "curated", curated_static);
    json_t *merged0 = md2_call(ctx, "merge_with_models_dev", mwa);
    json_free(curated_static);
    if (md2_exc_pending(ctx)) { json_free(merged0); return NULL; }
    json_t *da2 = json_object();
    md2_arg_str(da2, "normalized", normalized);
    md2_arg(da2, "rows", merged0);
    json_t *merged = md2_call(ctx, "drop_delisted_opencode_models", da2);
    json_free(merged0);
    if (md2_exc_pending(ctx)) { json_free(merged); return NULL; }
    if (md2_str_eq(normalized, "xai") || md2_str_eq(normalized, "xai-oauth")) {
        json_t *xa = json_object();
        md2_arg(xa, "ids", merged);
        json_t *fin = md2_call(ctx, "xai_finalize_catalog", xa);
        json_free(merged);
        if (md2_exc_pending(ctx)) { json_free(fin); return NULL; }
        merged = fin;
    }
    json_t *rows = md2_chat_catalog_rows(ctx, merged);
    json_free(merged);
    return rows;
}

/* ── locked cache-row writer ─────────────────────────────────────────────── */

/* PoP: md2_store_cache_entry @ hermes_cli/models.py:_store_cache_entry */
static json_t *md2_store_cache_entry(md2_ctx *ctx, const char *cache_key, json_t *entry) {
    json_t *path = md2_call(ctx, "provider_models_cache_path", json_object());
    if (md2_exc_pending(ctx)) { json_free(path); return NULL; }
    json_t *la = json_object();
    md2_arg(la, "path", path);
    json_free(path);
    json_t *lock = md2_call(ctx, "write_under_cache_lock", la);
    json_free(lock);
    if (md2_exc_pending(ctx)) return NULL;   /* callback never runs */
    /* write_row(): cache = _load_provider_models_cache(); cache[key] = entry;
     * _save_provider_models_cache(cache) */
    json_t *cache = md2_call(ctx, "load_provider_models_cache", json_object());
    if (md2_exc_pending(ctx)) { json_free(cache); return NULL; }
    if (!md2_is_obj(cache)) {
        json_free(cache);
        md2_raise(ctx, "TypeError", "'NoneType' object does not support item assignment");
        return NULL;
    }
    json_set(cache, cache_key, json_copy(entry));
    json_t *sv = json_object();
    md2_arg(sv, "data", cache);
    json_free(cache);
    json_t *sr = md2_call(ctx, "save_provider_models_cache", sv);
    json_free(sr);
    return NULL;
}

/* ── cached_fetch_api_models (disk-cached custom-endpoint fetch) ─────────── */

/* _catalog(entry): rows = (Native if entry.native_catalog else list)(entry["models"]);
 * return _chat_catalog_rows(rows) */
static json_t *md2_cfa_catalog(md2_ctx *ctx, json_t *entry) {
    json_t *models = md2_get(entry, "models");
    if (!models) {
        md2_raise(ctx, "KeyError", "models");
        return NULL;
    }
    json_t *rows = md2_truthy(md2_get(entry, "native_catalog"))
                 ? md2_native_wrap(models) : md2_list_copy(models);
    json_t *r = md2_chat_catalog_rows(ctx, rows);
    json_free(rows);
    return r;
}
/* _entry(live, at): {**_cache_entry(fp, live, at), "native_catalog": isinstance(...)} */
static json_t *md2_cfa_entry(md2_ctx *ctx, const char *fp, json_t *live, json_t *at) {
    json_t *base = md2_cache_entry(ctx, fp, live, at);
    if (md2_exc_pending(ctx)) { json_free(base); return NULL; }
    json_set(base, "native_catalog", json_bool(md2_is_native(live)));
    return base;
}
/* _live(): fetch_models() when supplied, else fetch_api_models(materialize(...), ...) */
static json_t *md2_cfa_live(md2_ctx *ctx, json_t *in) {
    json_t *fm = md2_get(in, "fetch_models");
    if (!md2_is_none(fm))
        return md2_call(ctx, "fetch_models", json_object());
    json_t *ma = json_object();
    md2_arg(ma, "api_key", md2_get(in, "api_key"));
    json_t *key = md2_call(ctx, "materialize_probe_api_key", ma);
    if (md2_exc_pending(ctx)) { json_free(key); return NULL; }
    json_t *fa = json_object();
    md2_arg(fa, "api_key", key);
    json_free(key);
    md2_arg(fa, "base_url", md2_get(in, "base_url"));
    md2_arg_num(fa, "timeout", json_get_num(in, "timeout", 5.0));
    md2_arg(fa, "api_mode", md2_get(in, "api_mode"));
    md2_arg(fa, "headers", md2_get(in, "headers"));
    return md2_call(ctx, "fetch_api_models", fa);
}

/* PoP: md2_cached_fetch_api_models @ hermes_cli/models.py:cached_fetch_api_models */
static json_t *md2_cached_fetch_api_models(md2_ctx *ctx, json_t *in) {
    bool cache_only = json_get_bool(in, "cache_only", false);
    bool force_refresh = json_get_bool(in, "force_refresh", false);
    double ttl = json_get_num(in, "ttl_seconds", md2_env_num(ctx, "cache_ttl", 3600));
    double stale_max = md2_env_num(ctx, "stale_max", 604800);
    json_t *result = NULL;
    char *cache_key = NULL;
    char *fp = NULL;
    json_t *cache = NULL;
    json_t *now = NULL;

    /* normalized_url = str(base_url or "").strip().rstrip("/").lower() */
    char *b0 = md2_str_or_empty(md2_get(in, "base_url"));
    char *b1 = md2_strip(b0); free(b0);
    char *b2 = md2_rstrip(b1, "/"); free(b1);
    char *normalized_url = md2_lower(b2); free(b2);

    if (!normalized_url[0]) {
        free(normalized_url);
        if (cache_only) return NULL;
        json_t *live = md2_cfa_live(ctx, in);
        if (md2_exc_pending(ctx)) { json_free(live); return NULL; }
        json_t *r = md2_chat_catalog_rows(ctx, live);
        json_free(live);
        return r;
    }
    /* fp = _custom_endpoint_fingerprint(api_key, api_mode, headers) */
    json_t *fpa = json_object();
    md2_arg(fpa, "api_key", md2_get(in, "api_key"));
    md2_arg(fpa, "api_mode", md2_get(in, "api_mode"));
    md2_arg(fpa, "headers", md2_get(in, "headers"));
    json_t *fpn = md2_call(ctx, "custom_endpoint_fingerprint", fpa);
    if (md2_exc_pending(ctx)) { free(normalized_url); json_free(fpn); return NULL; }
    fp = md2_pystr(fpn);
    json_free(fpn);
    size_t ckn = strlen(normalized_url) + strlen(fp) + 16;
    cache_key = (char *)malloc(ckn);
    if (cache_key) snprintf(cache_key, ckn, "custom:%s#%s", normalized_url, fp);
    free(normalized_url);

    cache = md2_call(ctx, "load_provider_models_cache", json_object());
    if (md2_exc_pending(ctx)) goto cleanup;
    json_t *entry = md2_get(cache, cache_key ? cache_key : "");
    now = md2_call(ctx, "time.time", json_object());
    if (md2_exc_pending(ctx)) goto cleanup;
    json_t *nc = md2_get(entry, "native_catalog");
    bool native_row = md2_is_obj(entry) && md2_is_bool(nc) && nc->bool_val;  /* is True */
    bool valid = false;
    if (!force_refresh) {
        json_t *va = json_object();
        md2_arg(va, "entry", entry);
        md2_arg_str(va, "fp", fp);
        md2_arg_bool(va, "allow_empty", native_row);
        json_t *v = md2_call(ctx, "cache_entry_valid", va);
        if (md2_exc_pending(ctx)) { json_free(v); goto cleanup; }
        valid = md2_truthy(v);
        json_free(v);
    }
    if (valid) {
        json_t *at = md2_get(entry, "at");
        if (!at) { md2_raise(ctx, "KeyError", "at"); goto cleanup; }
        double age = md2_num(now) - md2_num(at);
        if (age < ttl) { result = md2_cfa_catalog(ctx, entry); goto cleanup; }
        json_t *models = md2_get(entry, "models");
        if (md2_truthy(models) && age < stale_max) {
            json_t *spa = json_object();
            md2_arg_str(spa, "cache_key", cache_key);
            md2_arg_bool(spa, "refresh_fn", true);
            json_t *sp = md2_call(ctx, "spawn_swr_refresh", spa);
            json_free(sp);
            if (md2_exc_pending(ctx)) goto cleanup;
            result = md2_cfa_catalog(ctx, entry);
            goto cleanup;
        }
    }
    if (cache_only) goto cleanup;   /* result stays NULL (None) */

    json_t *live = md2_cfa_live(ctx, in);
    if (md2_exc_pending(ctx)) { json_free(live); goto cleanup; }
    if (md2_truthy(live) || md2_is_native(live)) {
        json_t *stored = md2_cfa_entry(ctx, fp, live, now);
        json_free(live);
        if (md2_exc_pending(ctx)) { json_free(stored); goto cleanup; }
        json_t *sr = md2_store_cache_entry(ctx, cache_key, stored);
        json_free(sr);
        if (md2_exc_pending(ctx)) { json_free(stored); goto cleanup; }
        result = md2_cfa_catalog(ctx, stored);
        json_free(stored);
        goto cleanup;
    }
    json_free(live);
    json_t *va2 = json_object();
    md2_arg(va2, "entry", entry);
    md2_arg_str(va2, "fp", fp);
    json_t *v2 = md2_call(ctx, "cache_entry_valid", va2);
    if (md2_exc_pending(ctx)) { json_free(v2); goto cleanup; }
    bool v2ok = md2_truthy(v2);
    json_free(v2);
    if (v2ok) { result = md2_cfa_catalog(ctx, entry); goto cleanup; }
    result = md2_chat_catalog_rows(ctx, NULL);   /* _chat_catalog_rows(live=None) */
cleanup:
    free(cache_key);
    free(fp);
    json_free(cache);
    json_free(now);
    return result;
}