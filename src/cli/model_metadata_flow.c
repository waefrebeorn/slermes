/* model_metadata_flow.c — FLOW + leaves port of the UNCLAIMED remainder of
 * agent/model_metadata.py (the task brief names "hermes_cli/model_metadata.py",
 * but the DEFINING module in the quarry is agent/model_metadata.py; PoP lines
 * credit the defining module). The claimed siblings live in
 * src/agent/model_metadata*.c / port_model_metadata.c / provider_metadata.c and
 * are SEAMED here.
 *
 * Decision-sequence port: TTL memo file read/write, local-probe memo, Bedrock
 * failure cooldown, Codex catalog negative-memo / max-cache, the cache-entry
 * validator, Ollama /api/show context parser, LM Studio loaded-context reader,
 * /models payload + cache-entry builders, Codex catalog URL/probe/fetch helpers,
 * the connect-timeout / save-unless-skipped / provider-save orchestrators and the
 * native-Anthropic token estimators. ALL collaborator I/O (claimed siblings,
 * foreign-module imports, clock reads, logger, file open/json.load, callback
 * probes) rides the injected universal seam mm_call; the harness scripts results
 * and records the ordered trace the oracle diffs against LIVE
 * agent/model_metadata.py under the same scripts. Ported functions call each
 * other REAL (internal composition: _ttl_memo_get/_ttl_memo_put ->
 * _load_json_dict, _parse_models_payload -> _endpoint_model_entry,
 * fetch_codex_catalog_entries -> _codex_catalog_urls).
 *
 * Representation contract (shared with tests/oracle_model_metadata_flow.py):
 *   - tuple/set -> array (sets sorted by code point); None -> JSON null;
 *     integral floats collapse to ints on both sides; Path -> POSIX str.
 *   - uncaught exception -> {"__raise__": cls}; an exception passed as a seam /
 *     logger arg projects to {"__raise__": cls, "msg": str(e)}.
 *   - scripts: absent key -> None; {"$seq":[...]} per-call queue (exhaustion
 *     raises RuntimeError("script exhausted: <name>")); {"__raise__":cls,"msg":m}
 *     raises (trace ret is the WHOLE marker); any other value reused.
 *     None-valued seam arg keys dropped on BOTH sides ("" kept).
 *   - except Exception never catches the BaseException trio
 *     (SystemExit/KeyboardInterrupt/GeneratorExit) — those propagate.
 *   - fixture domain: str()/float()/int() operands are ASCII strings, numbers,
 *     bools or null; int() tokens allow underscores BETWEEN digits only; URLs /
 *     paths are POSIX ASCII; memo/cache keys are opaque strings (production uses
 *     tuples — the memo logic is key-agnostic); isinstance(int) positions carry
 *     integral ints or bools (integral floats excluded); file content is scripted
 *     at the json.load level (BOM/decode errors ride as open/json.load raises).
 *   - object doubles (resp/path/file) are attribute dicts read via json_obj_get;
 *     the MEMO_LOCK / httpx-free file `with` context managers are modeled inertly.
 *
 * libjson-only (default oracle deps suffice). Self-contained: the harness
 * #includes "cli/model_metadata_flow.c". Prefix mm_.
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>

/* ── context + seam plumbing ─────────────────────────────────────────────── */

typedef struct mm_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *env;       /* borrowed: scripted constants/flags */
    json_t *state;     /* owned: per-case mutable state window */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} mm_ctx;

static bool mm_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool mm_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool mm_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool mm_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool mm_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool mm_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool mm_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static bool mm_truthy(const json_t *v) {
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

static const char *mm_jstr(const json_t *v) {
    return (mm_is_str(v) && v->str_val) ? v->str_val : "";
}
static double mm_num(const json_t *v) {
    return mm_is_num(v) ? v->num_val : 0.0;
}

/* borrowed object field or NULL */
static json_t *mm_get(const json_t *obj, const char *key) {
    return mm_is_obj(obj) ? json_obj_get(obj, key) : NULL;
}
/* dict.get(key, default): present-null keeps the null node; absent -> default.
 * Returns a BORROWED node (the default node is borrowed too). */
static json_t *mm_get_def(const json_t *obj, const char *key, json_t *def) {
    if (mm_is_obj(obj)) {
        json_t *v = json_obj_get(obj, key);
        if (v) return v;
    }
    return def;
}
static json_t *mm_copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}
static json_t *mm_obj_copy(const json_t *v) {
    json_t *out = json_object();
    if (mm_is_obj(v))
        for (size_t i = 0; i < v->c.count; i++)
            json_set(out, v->c.keys[i], json_copy(v->c.items[i]));
    return out;
}
static json_t *mm_list_copy(const json_t *v) {
    json_t *out = json_array();
    if (mm_is_arr(v))
        for (size_t i = 0; i < v->c.count; i++)
            json_append(out, json_copy(v->c.items[i]));
    return out;
}
static bool mm_arr_has_str(const json_t *arr, const char *s) {
    if (!mm_is_arr(arr)) return false;
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *it = arr->c.items[i];
        if (mm_is_str(it) && mm_str_eq(it->str_val, s)) return true;
    }
    return false;
}
/* state window: return the object at key, creating an empty one if absent. */
static json_t *mm_state_obj(mm_ctx *ctx, const char *key) {
    json_t *v = mm_get(ctx->state, key);
    if (mm_is_obj(v)) return v;
    v = json_object();
    json_set(ctx->state, key, v);
    return v;
}

/* ── string helpers (caller frees) ───────────────────────────────────────── */

static bool mm_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static char *mm_strip(const char *s) {
    if (!s) return strdup("");
    const char *b = s;
    while (*b && mm_is_space(*b)) b++;
    const char *e = b + strlen(b);
    while (e > b && mm_is_space(e[-1])) e--;
    size_t n = (size_t)(e - b);
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, b, n); r[n] = '\0';
    return r;
}
static char *mm_lower(const char *s) {
    char *r = strdup(s ? s : "");
    if (!r) return NULL;
    for (char *p = r; *p; p++) *p = (char)tolower((unsigned char)*p);
    return r;
}
static char *mm_rstrip(const char *s, const char *chars) {
    if (!s) return strdup("");
    size_t n = strlen(s);
    while (n > 0 && strchr(chars, s[n - 1])) n--;
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, s, n); r[n] = '\0';
    return r;
}
/* str(v) for a scalar (null -> "None"). Caller frees. */
static char *mm_pystr(const json_t *v) {
    if (mm_is_none(v)) return strdup("None");
    if (mm_is_bool(v)) return strdup(v->bool_val ? "True" : "False");
    if (mm_is_num(v)) {
        char buf[64]; double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
        else snprintf(buf, sizeof buf, "%g", d);
        return strdup(buf);
    }
    if (mm_is_str(v)) return strdup(v->str_val ? v->str_val : "");
    return strdup("");
}
/* str(v or "") for a scalar (falsy -> ""). Caller frees. */
static char *mm_str_or_empty(const json_t *v) {
    if (!mm_truthy(v)) return strdup("");
    return mm_pystr(v);
}
static bool mm_starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}
static bool mm_ends_with(const char *s, const char *suffix) {
    size_t n = strlen(s), m = strlen(suffix);
    return m <= n && strcmp(s + n - m, suffix) == 0;
}
/* first segment of s.split(sep, 1)[0] (caller frees) */
static char *mm_split_first(const char *s, char sep) {
    const char *p = strchr(s, sep);
    size_t n = p ? (size_t)(p - s) : strlen(s);
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, s, n); r[n] = '\0';
    return r;
}
/* f"{n:,}" comma-grouped integer for a non-negative integral value. Caller frees. */
static char *mm_comma_int(double d) {
    char digits[64];
    snprintf(digits, sizeof digits, "%.0f", d);
    size_t n = strlen(digits);
    /* sign handling (values here are non-negative, but be safe) */
    const char *body = digits;
    char sign[2] = "";
    if (digits[0] == '-') { sign[0] = '-'; sign[1] = '\0'; body = digits + 1; n--; }
    size_t groups = (n + 2) / 3;
    size_t outn = n + (groups ? groups - 1 : 0) + strlen(sign) + 1;
    char *r = (char *)malloc(outn);
    if (!r) return NULL;
    size_t o = 0;
    if (sign[0]) r[o++] = sign[0];
    size_t lead = n % 3; if (lead == 0) lead = 3;
    for (size_t i = 0; i < n; i++) {
        /* i >= lead guard: (i - lead) underflows size_t for i < lead, and
         * SIZE_MAX-k % 3 == 0 planted extra commas AND overflowed the buffer
         * sized for groups-1 commas (1-byte heap overflow -> 0xC0000374). */
        if (i >= lead && (i - lead) % 3 == 0) r[o++] = ',';
        r[o++] = body[i];
    }
    r[o] = '\0';
    return r;
}
/* PurePosixPath(base) / "cache" / name join, projected to a POSIX string.
 * Domain: base is a POSIX path (no "." normalization needed beyond trailing
 * slashes); matches PurePosixPath for the fixture corpus. Caller frees. */
static char *mm_path_join3(const char *base, const char *mid, const char *name) {
    /* strip trailing slashes from base, but keep a root "/" */
    size_t bn = strlen(base);
    while (bn > 1 && base[bn - 1] == '/') bn--;
    bool base_root = (bn == 1 && base[0] == '/');
    bool base_empty = (bn == 0);
    size_t mn = strlen(mid), nn = strlen(name);
    size_t cap = bn + mn + nn + 4;
    char *r = (char *)malloc(cap);
    if (!r) return NULL;
    size_t o = 0;
    if (base_empty) {
        /* PurePosixPath("") == "."; "." / mid / name -> mid/name (no leading .) */
        memcpy(r + o, mid, mn); o += mn;
    } else {
        memcpy(r + o, base, bn); o += bn;
        if (!base_root && base[bn - 1] != '/') r[o++] = '/';
        else if (base_root) { /* base already "/" */ }
        memcpy(r + o, mid, mn); o += mn;
    }
    if (o == 0 || r[o - 1] != '/') r[o++] = '/';
    memcpy(r + o, name, nn); o += nn;
    r[o] = '\0';
    return r;
}

/* float(v) coercion. err: 0 ok, 1 TypeError, 2 ValueError. ASCII token domain
 * (no underscores; inf/infinity/nan accepted case-insensitively). */
static double mm_float(const json_t *v, int *err) {
    *err = 0;
    if (mm_is_none(v)) { *err = 1; return 0.0; }
    if (mm_is_bool(v)) return v->bool_val ? 1.0 : 0.0;
    if (mm_is_num(v)) return v->num_val;
    if (!mm_is_str(v)) { *err = 1; return 0.0; }
    char *t = mm_strip(v->str_val ? v->str_val : "");
    if (!t) { *err = 2; return 0.0; }
    if (!t[0]) { free(t); *err = 2; return 0.0; }
    const char *body = t;
    double sign = 1.0;
    if (*body == '+' || *body == '-') { if (*body == '-') sign = -1.0; body++; }
    char *lb = mm_lower(body);
    if (mm_str_eq(lb, "inf") || mm_str_eq(lb, "infinity")) {
        free(lb); free(t); return sign * INFINITY;
    }
    if (mm_str_eq(lb, "nan")) { free(lb); free(t); return NAN; }
    free(lb);
    /* numeric grammar: [+-]?(\d+(\.\d*)?|\.\d+)([eE][+-]?\d+)? */
    const char *p = t;
    if (*p == '+' || *p == '-') p++;
    bool any = false;
    while (isdigit((unsigned char)*p)) { any = true; p++; }
    if (*p == '.') { p++; while (isdigit((unsigned char)*p)) { any = true; p++; } }
    if (!any) { free(t); *err = 2; return 0.0; }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        if (!isdigit((unsigned char)*p)) { free(t); *err = 2; return 0.0; }
        while (isdigit((unsigned char)*p)) p++;
    }
    if (*p != '\0') { free(t); *err = 2; return 0.0; }
    double d = strtod(t, NULL);
    free(t);
    return d;
}
/* int(token) for an ASCII string token (from str.split, so whitespace-free).
 * Returns 0 ok / 1 ValueError; underscores must sit BETWEEN digits. */
static long long mm_int_token(const char *tok, int *err) {
    *err = 0;
    const char *p = tok;
    if (*p == '+' || *p == '-') p++;
    const char *digits_start = p;
    bool any = false;
    /* validate digit/underscore run with neighbor-digit rule */
    const char *q = p;
    while (*q) {
        if (isdigit((unsigned char)*q)) { any = true; q++; }
        else if (*q == '_') {
            if (q == digits_start || !isdigit((unsigned char)q[-1]) || !isdigit((unsigned char)q[1])) {
                *err = 1; return 0;
            }
            q++;
        } else { *err = 1; return 0; }
    }
    if (!any) { *err = 1; return 0; }
    /* strip underscores into a clean buffer */
    size_t n = strlen(tok);
    char *clean = (char *)malloc(n + 1);
    if (!clean) { *err = 1; return 0; }
    size_t o = 0;
    for (size_t i = 0; i < n; i++) if (tok[i] != '_') clean[o++] = tok[i];
    clean[o] = '\0';
    long long v = strtoll(clean, NULL, 10);
    free(clean);
    return v;
}
/* ── exception plumbing ──────────────────────────────────────────────────── */

static bool mm_exc_pending(const mm_ctx *ctx) { return ctx && ctx->exc != NULL; }
static void mm_exc_clear(mm_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}
static void mm_raise(mm_ctx *ctx, const char *cls, const char *msg) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
    json_set(ctx->exc, "msg", json_string(msg ? msg : ""));
}
static const char *mm_exc_class(const mm_ctx *ctx) {
    return json_get_str(ctx->exc, "__raise__", "");
}
/* FULL builtin exception hierarchy (parent links). The BaseException trio
 * (SystemExit/KeyboardInterrupt/GeneratorExit) links ONLY to BaseException so
 * `except Exception` never catches them. */
static const char *mm_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        {"BaseException", NULL},
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"},
        {"Exception", "BaseException"},
        {"TypeError", "Exception"}, {"StopIteration", "Exception"},
        {"StopAsyncIteration", "Exception"}, {"ArithmeticError", "Exception"},
        {"FloatingPointError", "ArithmeticError"}, {"OverflowError", "ArithmeticError"},
        {"ZeroDivisionError", "ArithmeticError"},
        {"AssertionError", "Exception"}, {"AttributeError", "Exception"},
        {"BufferError", "Exception"}, {"EOFError", "Exception"},
        {"ImportError", "Exception"}, {"ModuleNotFoundError", "ImportError"},
        {"LookupError", "Exception"}, {"IndexError", "LookupError"},
        {"KeyError", "LookupError"}, {"MemoryError", "Exception"},
        {"NameError", "Exception"}, {"UnboundLocalError", "NameError"},
        {"OSError", "Exception"}, {"BlockingIOError", "OSError"},
        {"ChildProcessError", "OSError"}, {"ConnectionError", "OSError"},
        {"BrokenPipeError", "ConnectionError"}, {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
        {"FileExistsError", "OSError"}, {"FileNotFoundError", "OSError"},
        {"InterruptedError", "OSError"}, {"IsADirectoryError", "OSError"},
        {"NotADirectoryError", "OSError"}, {"PermissionError", "OSError"},
        {"ProcessLookupError", "OSError"}, {"TimeoutError", "OSError"},
        {"ReferenceError", "Exception"}, {"RuntimeError", "Exception"},
        {"NotImplementedError", "RuntimeError"}, {"RecursionError", "RuntimeError"},
        {"SyntaxError", "Exception"}, {"IndentationError", "SyntaxError"},
        {"TabError", "IndentationError"}, {"SystemError", "Exception"},
        {"TypeError", "Exception"}, {"ValueError", "Exception"},
        {"UnicodeError", "ValueError"}, {"UnicodeDecodeError", "UnicodeError"},
        {"UnicodeEncodeError", "UnicodeError"}, {"UnicodeTranslateError", "UnicodeError"},
        {"JSONDecodeError", "ValueError"},
        {"Warning", "Exception"}, {"UserWarning", "Warning"},
        {"DeprecationWarning", "Warning"}, {"PendingDeprecationWarning", "Warning"},
        {"SyntaxWarning", "Warning"}, {"RuntimeWarning", "Warning"},
        {"FutureWarning", "Warning"}, {"ImportWarning", "Warning"},
        {"UnicodeWarning", "Warning"}, {"BytesWarning", "Warning"},
        {"ResourceWarning", "Warning"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (mm_str_eq(T[i].c, c)) return T[i].p;
    return NULL;
}
static bool mm_exc_matches(const mm_ctx *ctx, const char *const *classes, size_t n) {
    if (!mm_exc_pending(ctx)) return false;
    const char *cls = mm_exc_class(ctx);
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (mm_str_eq(cls, classes[i])) return true;
        const char *p = mm_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}
/* except Exception (NOT the BaseException trio). */
static bool mm_exc_is_exception(const mm_ctx *ctx) {
    static const char *E[] = {"Exception"};
    return mm_exc_matches(ctx, E, 1);
}

/* ── universal seam ──────────────────────────────────────────────────────── */

static json_t *mm_script_result(mm_ctx *ctx, const json_t *script) {
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
static json_t *mm_call(mm_ctx *ctx, const char *name, json_t *args) {
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
    if (chosen != ctx->exc) ret = mm_script_result(ctx, chosen);
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
static void mm_arg(json_t *args, const char *key, const json_t *val) {
    if (!val || val->type == JSON_NULL) return;
    json_set(args, key, json_copy(val));
}
static void mm_arg_str(json_t *args, const char *key, const char *val) {
    if (!val) return;
    json_set(args, key, json_string(val));
}
static void mm_arg_bool(json_t *args, const char *key, bool val) {
    json_set(args, key, json_bool(val));
}
static void mm_arg_num(json_t *args, const char *key, double val) {
    json_set(args, key, json_number(val));
}
/* logger seam: {"msg","args"}. args_arr ownership transferred. */
static void mm_log(mm_ctx *ctx, const char *level, const char *msg, json_t *args_arr) {
    json_t *a = json_object();
    json_set(a, "msg", json_string(msg));
    json_set(a, "args", args_arr ? args_arr : json_array());
    char name[32];
    snprintf(name, sizeof name, "logger.%s", level);
    json_t *r = mm_call(ctx, name, a);
    json_free(r);
}
/* project the pending exception as a logger/seam arg node {"__raise__","msg"}. */
static json_t *mm_exc_arg(const mm_ctx *ctx) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(mm_exc_class(ctx)));
    json_set(e, "msg", json_string(json_get_str(ctx->exc, "msg", "")));
    return e;
}
/* tuple result helpers */
static json_t *mm_pair(json_t *a, json_t *b) {
    json_t *r = json_array();
    json_append(r, a ? a : json_null());
    json_append(r, b ? b : json_null());
    return r;
}

/* ── env table accessors ─────────────────────────────────────────────────── */

static double mm_env_num(const mm_ctx *ctx, const char *key, double def) {
    return ctx->env ? json_get_num(ctx->env, key, def) : def;
}
static const char *mm_env_str(const mm_ctx *ctx, const char *key, const char *def) {
    return ctx->env ? json_get_str(ctx->env, key, def) : def;
}

/* Python type name for a JSON node (for AttributeError messages). */
static const char *mm_py_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return "int";   /* integral-int fixture domain */
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}
/* raise AttributeError: '<T>' object has no attribute '<attr>' */
static void mm_raise_attr(mm_ctx *ctx, const json_t *recv, const char *attr) {
    char msg[256];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", mm_py_type_name(recv), attr);
    mm_raise(ctx, "AttributeError", msg);
}
/* ── split helpers ───────────────────────────────────────────────────────── */

/* str.split("\n"): split on literal newline, KEEPING empty segments.
 * Returns a json array of strings (owned). */
static json_t *mm_split_newline(const char *s) {
    json_t *out = json_array();
    const char *start = s;
    for (const char *p = s; ; p++) {
        if (*p == '\n' || *p == '\0') {
            size_t n = (size_t)(p - start);
            char *seg = (char *)malloc(n + 1);
            if (seg) { memcpy(seg, start, n); seg[n] = '\0'; json_append(out, json_string(seg)); free(seg); }
            if (*p == '\0') break;
            start = p + 1;
        }
    }
    return out;
}
/* str.split() (no arg): split on whitespace runs, drop empties. Array of str. */
static json_t *mm_split_ws(const char *s) {
    json_t *out = json_array();
    const char *p = s;
    while (*p) {
        while (*p && mm_is_space(*p)) p++;
        if (!*p) break;
        const char *b = p;
        while (*p && !mm_is_space(*p)) p++;
        size_t n = (size_t)(p - b);
        char *seg = (char *)malloc(n + 1);
        if (seg) { memcpy(seg, b, n); seg[n] = '\0'; json_append(out, json_string(seg)); free(seg); }
    }
    return out;
}

/* ── pure leaves ─────────────────────────────────────────────────────────── */

/* PoP: mm_count_parts @ agent/model_metadata.py:_count_parts */
static json_t *mm_count_parts(mm_ctx *ctx, json_t *in) {
    (void)ctx;
    json_t *parts = mm_get(in, "parts");
    json_t *types = mm_get(in, "types");
    if (!mm_is_arr(parts)) return json_number(0);
    double count = 0;
    for (size_t i = 0; i < parts->c.count; i++) {
        json_t *part = parts->c.items[i];
        if (!mm_is_obj(part)) continue;
        json_t *t = mm_get(part, "type");
        if (mm_is_str(t) && mm_arr_has_str(types, t->str_val)) count += 1;
    }
    return json_number(count);
}

/* PoP: mm_lmstudio_loaded_context @ agent/model_metadata.py:_lmstudio_loaded_context */
static json_t *mm_lmstudio_loaded_context(mm_ctx *ctx, json_t *in) {
    (void)ctx;
    json_t *model = mm_get(in, "model");
    if (!mm_is_obj(model)) { mm_raise_attr(ctx, model, "get"); return NULL; }
    json_t *li = mm_get(model, "loaded_instances");
    /* `model.get("loaded_instances", []) or []` — falsy/absent -> no iterations */
    if (!mm_is_arr(li)) return json_null();
    for (size_t i = 0; i < li->c.count; i++) {
        json_t *inst = li->c.items[i];
        json_t *cfg = mm_is_obj(inst) ? mm_get_def(inst, "config", NULL) : NULL;
        json_t *cval = mm_is_obj(cfg) ? mm_get(cfg, "context_length") : NULL;
        /* isinstance(ctx, int) admits bool; ctx > 0 */
        if (mm_is_num(cval) || mm_is_bool(cval)) {
            double v = mm_is_bool(cval) ? (cval->bool_val ? 1.0 : 0.0) : cval->num_val;
            if (v > 0) return json_copy(cval);
        }
    }
    return json_null();
}

/* PoP: mm_codex_catalog_urls @ agent/model_metadata.py:_codex_catalog_urls */
static json_t *mm_codex_catalog_urls(mm_ctx *ctx, json_t *in) {
    const char *endpoint_default = mm_env_str(ctx, "codex_catalog_endpoint",
        "https://chatgpt.com/backend-api/codex/models");
    const char *newest = mm_env_str(ctx, "codex_newest_version", "99.0.0");
    const char *ungated = mm_env_str(ctx, "codex_ungated_version", "0.0.0");
    char *b0 = mm_str_or_empty(mm_get(in, "base_url"));
    char *b1 = mm_strip(b0); free(b0);
    char *base = mm_rstrip(b1, "/"); free(b1);
    char *endpoint;
    if (base[0]) {
        size_t n = strlen(base) + 8;
        endpoint = (char *)malloc(n);
        if (endpoint) snprintf(endpoint, n, "%s/models", base);
    } else {
        endpoint = strdup(endpoint_default);
    }
    free(base);
    json_t *out = json_array();
    const char *vers[2] = {newest, ungated};
    for (int i = 0; i < 2; i++) {
        size_t n = strlen(endpoint) + strlen(vers[i]) + 24;
        char *url = (char *)malloc(n);
        if (url) { snprintf(url, n, "%s?client_version=%s", endpoint, vers[i]); json_append(out, json_string(url)); free(url); }
    }
    free(endpoint);
    return out;
}

/* _ollama_show_context readers. Each collects its yielded ints in order; the
 * only raise is the source .split()/.items() AttributeError at reader start. */
static json_t *mm_osc_gguf(mm_ctx *ctx, json_t *data) {
    json_t *empty = json_object();
    json_t *mi = mm_get_def(data, "model_info", empty);
    json_t *out = json_array();
    if (!mm_is_obj(mi)) {
        mm_raise_attr(ctx, mi, "items");
        json_free(empty);
        return out;
    }
    for (size_t i = 0; i < mi->c.count; i++) {
        const char *key = mi->c.keys[i];
        json_t *value = mi->c.items[i];
        if (strstr(key, "context_length") && (mm_is_num(value) || mm_is_bool(value))) {
            double v = mm_is_bool(value) ? (value->bool_val ? 1.0 : 0.0) : (double)(long long)value->num_val;
            json_append(out, json_number(v));
        }
    }
    json_free(empty);
    return out;
}
static json_t *mm_osc_num_ctx(mm_ctx *ctx, json_t *data) {
    json_t *empty = json_string("");
    json_t *params = mm_get_def(data, "parameters", empty);
    json_t *out = json_array();
    if (!mm_is_str(params)) {
        mm_raise_attr(ctx, params, "split");
        json_free(empty);
        return out;
    }
    json_t *lines = mm_split_newline(params->str_val ? params->str_val : "");
    for (size_t i = 0; i < lines->c.count; i++) {
        const char *line = mm_jstr(lines->c.items[i]);
        char *stripped = mm_strip(line);
        json_t *parts = mm_split_ws(stripped ? stripped : "");
        free(stripped);
        if (strstr(line, "num_ctx") && parts->c.count >= 2) {
            const char *tok = mm_jstr(parts->c.items[parts->c.count - 1]);
            int err = 0;
            long long v = mm_int_token(tok, &err);
            if (!err) json_append(out, json_number((double)v));
        }
        json_free(parts);
    }
    json_free(lines);
    json_free(empty);
    return out;
}

/* PoP: mm_ollama_show_context @ agent/model_metadata.py:_ollama_show_context */
static json_t *mm_ollama_show_context(mm_ctx *ctx, json_t *in) {
    json_t *data = mm_get(in, "data");
    bool gguf_first = json_get_bool(in, "gguf_first", false);
    json_t *minimum = mm_get(in, "minimum");
    if (!mm_is_obj(data)) { mm_raise_attr(ctx, data, "get"); return NULL; }
    /* reader order: gguf_first ? (_gguf, _num_ctx) : (_num_ctx, _gguf) */
    for (int r = 0; r < 2; r++) {
        bool use_gguf = gguf_first ? (r == 0) : (r == 1);
        json_t *vals = use_gguf ? mm_osc_gguf(ctx, data) : mm_osc_num_ctx(ctx, data);
        if (mm_exc_pending(ctx)) { json_free(vals); return NULL; }
        for (size_t i = 0; i < vals->c.count; i++) {
            double c = mm_num(vals->c.items[i]);
            if (mm_is_none(minimum) || c >= mm_num(minimum)) {
                json_free(vals);
                return json_number(c);
            }
        }
        json_free(vals);
    }
    return json_null();
}

/* PoP: mm_endpoint_model_entry @ agent/model_metadata.py:_endpoint_model_entry */
static json_t *mm_endpoint_model_entry(mm_ctx *ctx, json_t *in) {
    json_t *model = mm_get(in, "model");
    const char *model_id = json_get_str(in, "model_id", "");
    json_t *context_length = mm_get(in, "context_length");
    /* optional tuple built left-to-right: extract_first_int then extract_pricing
     * (these fire BEFORE model.get("name"), so a non-dict model raises after them) */
    json_t *ka = json_object();
    mm_arg(ka, "payload", model);
    json_t *keys = json_array();
    json_append(keys, json_string("max_completion_tokens"));
    json_append(keys, json_string("max_output_tokens"));
    json_append(keys, json_string("max_tokens"));
    json_set(ka, "keys", keys);
    json_t *mct = mm_call(ctx, "extract_first_int", ka);
    if (mm_exc_pending(ctx)) { json_free(mct); return NULL; }
    json_t *pa = json_object();
    mm_arg(pa, "payload", model);
    json_t *pr = mm_call(ctx, "extract_pricing", pa);
    if (mm_exc_pending(ctx)) { json_free(mct); json_free(pr); return NULL; }
    /* model.get("name", model_id) — reached only after the optional-tuple seams */
    if (!mm_is_obj(model)) { mm_raise_attr(ctx, model, "get"); json_free(mct); json_free(pr); return NULL; }
    json_t *out = json_object();
    json_t *id_node = json_string(model_id);
    json_t *name = mm_get_def(model, "name", id_node);
    json_set(out, "name", json_copy(name));
    json_free(id_node);
    if (!mm_is_none(context_length)) json_set(out, "context_length", json_copy(context_length));
    if (!mm_is_none(mct)) json_set(out, "max_completion_tokens", json_copy(mct));
    if (mm_truthy(pr)) json_set(out, "pricing", json_copy(pr));   /* _extract_pricing(model) or None */
    json_free(mct);
    json_free(pr);
    return out;
}
/* ── claimed-sibling orchestrators ───────────────────────────────────────── */

/* PoP: mm_note_if_connect_timeout @ agent/model_metadata.py:_note_if_connect_timeout */
static json_t *mm_note_if_connect_timeout(mm_ctx *ctx, json_t *in) {
    json_t *a = json_object();
    mm_arg(a, "exc", mm_get(in, "exc"));
    json_t *r = mm_call(ctx, "is_connect_timeout", a);
    if (mm_exc_pending(ctx)) { json_free(r); return NULL; }
    if (mm_truthy(r)) {
        json_t *b = json_object();
        mm_arg(b, "base_url", mm_get(in, "base_url"));
        json_t *s = mm_call(ctx, "note_endpoint_blackholed", b);
        json_free(s);
        if (mm_exc_pending(ctx)) { json_free(r); return NULL; }
    }
    json_free(r);
    return json_null();
}

/* PoP: mm_save_unless_skipped @ agent/model_metadata.py:_save_unless_skipped */
static json_t *mm_save_unless_skipped(mm_ctx *ctx, json_t *in) {
    json_t *sa = json_object();
    mm_arg(sa, "base_url", mm_get(in, "base_url"));
    mm_arg(sa, "provider", mm_get(in, "provider"));
    json_t *skip = mm_call(ctx, "skip_persistent_context_cache", sa);
    if (mm_exc_pending(ctx)) { json_free(skip); return NULL; }
    if (!mm_truthy(skip)) {
        json_t *va = json_object();
        mm_arg(va, "model", mm_get(in, "model"));
        mm_arg(va, "base_url", mm_get(in, "base_url"));
        mm_arg(va, "length", mm_get(in, "ctx"));
        json_t *s = mm_call(ctx, "save_context_length", va);
        json_free(s);
        if (mm_exc_pending(ctx)) { json_free(skip); return NULL; }
    }
    json_free(skip);
    return json_null();
}

/* PoP: mm_probe_local_context_length @ agent/model_metadata.py:_probe_local_context_length */
static json_t *mm_probe_local_context_length(mm_ctx *ctx, json_t *in) {
    json_t *qa = json_object();
    mm_arg(qa, "model", mm_get(in, "model"));
    mm_arg(qa, "base_url", mm_get(in, "base_url"));
    mm_arg(qa, "api_key", mm_get(in, "api_key"));
    json_t *lc = mm_call(ctx, "query_local_context_length", qa);
    if (mm_exc_pending(ctx)) { json_free(lc); return NULL; }
    if (!mm_truthy(lc)) { json_free(lc); return json_null(); }
    /* local_ctx > 0 (numeric domain; a truthy non-number raises TypeError) */
    if (mm_is_num(lc) || mm_is_bool(lc)) {
        double v = mm_is_bool(lc) ? (lc->bool_val ? 1.0 : 0.0) : lc->num_val;
        if (!(v > 0)) { json_free(lc); return json_null(); }
    } else {
        char msg[160];
        snprintf(msg, sizeof msg, "'>' not supported between instances of '%s' and 'int'", mm_py_type_name(lc));
        mm_raise(ctx, "TypeError", msg);
        json_free(lc);
        return NULL;
    }
    json_t *sa = json_object();
    mm_arg(sa, "base_url", mm_get(in, "base_url"));
    mm_arg(sa, "provider", mm_get(in, "provider"));
    json_t *skip = mm_call(ctx, "skip_persistent_context_cache", sa);
    if (mm_exc_pending(ctx)) { json_free(lc); json_free(skip); return NULL; }
    if (!mm_truthy(skip)) {
        json_t *ma = json_object();
        mm_arg(ma, "model", mm_get(in, "model"));
        mm_arg(ma, "base_url", mm_get(in, "base_url"));
        mm_arg(ma, "length", lc);
        json_t *m = mm_call(ctx, "maybe_cache_local_context_length", ma);
        json_free(m);
        if (mm_exc_pending(ctx)) { json_free(lc); json_free(skip); return NULL; }
    }
    json_free(skip);
    return lc;   /* ownership transferred to the return */
}

/* PoP: mm_save_provider_context_length @ agent/model_metadata.py:save_provider_context_length */
static json_t *mm_save_provider_context_length(mm_ctx *ctx, json_t *in) {
    json_t *base_url = mm_get(in, "base_url");
    json_t *ba = json_object();
    mm_arg(ba, "base_url", base_url);
    mm_arg(ba, "provider", mm_get(in, "provider"));
    json_t *bed = mm_call(ctx, "is_bedrock_context", ba);
    if (mm_exc_pending(ctx)) { json_free(bed); return NULL; }
    json_t *va = json_object();
    mm_arg(va, "model", mm_get(in, "model"));
    mm_arg(va, "length", mm_get(in, "length"));
    if (mm_truthy(bed)) {
        const char *bu = mm_truthy(base_url) ? mm_jstr(base_url) : "bedrock://";
        mm_arg_str(va, "base_url", bu);
        mm_arg_str(va, "source", "bedrock-confirmed-v1");
    } else {
        mm_arg(va, "base_url", base_url);
    }
    json_t *s = mm_call(ctx, "save_context_length", va);
    json_free(s);
    json_free(bed);
    if (mm_exc_pending(ctx)) return NULL;
    return json_null();
}

/* PoP: mm_is_ollama_server @ agent/model_metadata.py:_is_ollama_server */
static json_t *mm_is_ollama_server(mm_ctx *ctx, json_t *in) {
    json_t *da = json_object();
    mm_arg(da, "base_url", mm_get(in, "base_url"));
    mm_arg(da, "api_key", mm_get(in, "api_key"));
    json_t *r = mm_call(ctx, "detect_local_server_type", da);
    if (mm_exc_pending(ctx)) {
        if (mm_exc_is_exception(ctx)) { mm_exc_clear(ctx); json_free(r); return json_bool(false); }
        json_free(r);
        return NULL;   /* BaseException propagates */
    }
    bool is_oll = mm_is_str(r) && mm_str_eq(r->str_val, "ollama");
    json_free(r);
    return json_bool(is_oll);
}

/* PoP: mm_cache_file @ agent/model_metadata.py:_cache_file */
static json_t *mm_cache_file(mm_ctx *ctx, json_t *in) {
    const char *name = json_get_str(in, "name", "");
    json_t *h = mm_call(ctx, "get_hermes_home", json_object());
    if (mm_exc_pending(ctx)) { json_free(h); return NULL; }
    if (mm_is_none(h)) {
        mm_raise(ctx, "TypeError", "unsupported operand type(s) for /: 'NoneType' and 'str'");
        json_free(h);
        return NULL;
    }
    const char *home = mm_jstr(h);
    char *path = mm_path_join3(home, "cache", name);
    json_free(h);
    json_t *out = json_string(path ? path : "");
    free(path);
    return out;
}
/* ── file / TTL-memo flows ───────────────────────────────────────────────── */

/* set the exception a failed float() coercion would raise (for except handlers
 * that log str(e)). err: 1 TypeError, 2 ValueError. */
static void mm_float_raise(mm_ctx *ctx, const json_t *node, int err) {
    char msg[256];
    if (err == 1) {
        snprintf(msg, sizeof msg, "float() argument must be a string or a real number, not '%s'", mm_py_type_name(node));
        mm_raise(ctx, "TypeError", msg);
    } else {
        snprintf(msg, sizeof msg, "could not convert string to float: '%s'", mm_jstr(node));
        mm_raise(ctx, "ValueError", msg);
    }
}

/* PoP: mm_load_json_dict @ agent/model_metadata.py:_load_json_dict */
static json_t *mm_load_json_dict(mm_ctx *ctx, json_t *in) {
    json_t *oa = json_object();
    mm_arg_str(oa, "path", json_get_str(in, "path", ""));
    mm_arg_str(oa, "mode", "r");
    mm_arg_str(oa, "encoding", "utf-8-sig");
    json_t *o = mm_call(ctx, "open", oa);
    if (mm_exc_pending(ctx)) {
        if (mm_exc_is_exception(ctx)) { mm_exc_clear(ctx); json_free(o); return json_object(); }
        json_free(o);
        return NULL;
    }
    json_free(o);   /* file handle (inert CM) */
    json_t *d = mm_call(ctx, "json.load", json_object());
    if (mm_exc_pending(ctx)) {
        if (mm_exc_is_exception(ctx)) { mm_exc_clear(ctx); json_free(d); return json_object(); }
        json_free(d);
        return NULL;
    }
    if (mm_is_obj(d)) return d;
    json_free(d);
    return json_object();
}

/* PoP: mm_ttl_memo_get @ agent/model_metadata.py:_ttl_memo_get */
static json_t *mm_ttl_memo_get(mm_ctx *ctx, json_t *in) {
    const char *key = json_get_str(in, "key", "");
    double ttl = json_get_num(in, "ttl", 0);
    const char *ts_key = json_get_str(in, "ts_key", "ts");
    const char *value_key = json_get_str(in, "value_key", "value");
    json_t *doc = mm_load_json_dict(ctx, in);   /* REAL composition */
    if (mm_exc_pending(ctx)) { json_free(doc); return NULL; }
    json_t *entry = mm_get(doc, key);
    if (!mm_is_obj(entry)) { json_free(doc); return json_null(); }
    /* try: time.time() fires BEFORE the entry[ts_key] subscript */
    json_t *now = mm_call(ctx, "time.time", json_object());
    if (mm_exc_pending(ctx)) {
        if (mm_exc_is_exception(ctx)) { mm_exc_clear(ctx); json_free(now); json_free(doc); return json_null(); }
        json_free(now); json_free(doc);
        return NULL;
    }
    json_t *ts_node = mm_get(entry, ts_key);
    if (!ts_node) { json_free(now); json_free(doc); return json_null(); }   /* KeyError -> None */
    int ferr = 0;
    double tsval = mm_float(ts_node, &ferr);
    if (ferr) { json_free(now); json_free(doc); return json_null(); }   /* TypeError/ValueError -> None */
    if ((mm_num(now) - tsval) >= ttl) { json_free(now); json_free(doc); return json_null(); }
    json_t *vk = mm_get(entry, value_key);
    if (!vk) { json_free(now); json_free(doc); return json_null(); }   /* KeyError -> None */
    json_t *result = json_copy(vk);
    json_free(now);
    json_free(doc);
    return result;
}

/* PoP: mm_ttl_memo_put @ agent/model_metadata.py:_ttl_memo_put */
static json_t *mm_ttl_memo_put(mm_ctx *ctx, json_t *in) {
    const char *key = json_get_str(in, "key", "");
    json_t *value = mm_get(in, "value");
    double ttl = json_get_num(in, "ttl", 0);
    const char *ts_key = json_get_str(in, "ts_key", "ts");
    const char *value_key = json_get_str(in, "value_key", "value");
    const char *what = json_get_str(in, "what", "");
    bool ts_first = json_get_bool(in, "ts_first", false);
    json_t *now = NULL, *doc = NULL, *data = NULL;

    now = mm_call(ctx, "time.time", json_object());
    if (mm_exc_pending(ctx)) goto handle;
    doc = mm_load_json_dict(ctx, in);   /* REAL composition (swallows Exception -> {}) */
    if (mm_exc_pending(ctx)) goto handle;
    data = json_object();
    /* {k:v for k,v in doc.items() if isinstance(v,dict) and (now-float(v.get(ts_key,0)))<ttl} */
    {
        json_t *zero = json_number(0);
        for (size_t i = 0; i < doc->c.count; i++) {
            const char *k = doc->c.keys[i];
            json_t *v = doc->c.items[i];
            if (!mm_is_obj(v)) continue;
            json_t *tsnode = mm_get_def(v, ts_key, zero);
            int ferr = 0;
            double tsval = mm_float(tsnode, &ferr);
            if (ferr) { mm_float_raise(ctx, tsnode, ferr); json_free(zero); goto handle; }
            if ((mm_num(now) - tsval) < ttl) json_set(data, k, json_copy(v));
        }
        json_free(zero);
    }
    /* data[key] = {ts_key:now, value_key:value} if ts_first else {value_key:value, ts_key:now} */
    {
        json_t *entry = json_object();
        if (ts_first) {
            json_set(entry, ts_key, json_copy(now));
            json_set(entry, value_key, mm_copy_or_null(value));
        } else {
            json_set(entry, value_key, mm_copy_or_null(value));
            json_set(entry, ts_key, json_copy(now));
        }
        json_set(data, key, entry);
    }
    {
        json_t *wa = json_object();
        mm_arg_str(wa, "path", json_get_str(in, "path", ""));
        mm_arg(wa, "data", data);
        mm_arg_num(wa, "indent", 0);
        json_t *seps = json_array();
        json_append(seps, json_string(","));
        json_append(seps, json_string(":"));
        json_set(wa, "separators", seps);
        json_t *w = mm_call(ctx, "atomic_json_write", wa);
        json_free(w);
        if (mm_exc_pending(ctx)) goto handle;
    }
    json_free(now); json_free(doc); json_free(data);
    return json_null();
handle:
    if (mm_exc_is_exception(ctx)) {
        json_t *la = json_array();
        json_append(la, json_string(what));
        json_append(la, json_string(json_get_str(ctx->exc, "msg", "")));
        mm_exc_clear(ctx);
        mm_log(ctx, "debug", "Failed to save %s: %s", la);
        json_free(now); json_free(doc); json_free(data);
        return json_null();
    }
    /* BaseException propagates */
    json_free(now); json_free(doc); json_free(data);
    return NULL;
}
/* ── stateful memo / cache flows ─────────────────────────────────────────── */

/* PoP: mm_memo_local_probe @ agent/model_metadata.py:_memo_local_probe */
static json_t *mm_memo_local_probe(mm_ctx *ctx, json_t *in) {
    const char *cache_key = json_get_str(in, "cache_key", "");
    double ttl = mm_env_num(ctx, "local_ctx_probe_ttl", 30.0);
    json_t *cache = mm_state_obj(ctx, "_LOCAL_CTX_PROBE_CACHE");
    json_t *now = mm_call(ctx, "time.monotonic", json_object());
    if (mm_exc_pending(ctx)) { json_free(now); return NULL; }
    json_t *cached = mm_get(cache, cache_key);
    if (!mm_is_none(cached) && mm_is_arr(cached) && cached->c.count >= 2) {
        double ts = mm_num(cached->c.items[1]);
        if ((mm_num(now) - ts) < ttl) {
            json_t *r = json_copy(cached->c.items[0]);
            json_free(now);
            return r;
        }
    }
    json_t *result = mm_call(ctx, "probe", json_object());
    if (mm_exc_pending(ctx)) { json_free(now); json_free(result); return NULL; }
    if (mm_truthy(result)) {
        json_t *entry = json_array();
        json_append(entry, json_copy(result));
        json_append(entry, json_copy(now));
        json_set(cache, cache_key, entry);
    }
    json_free(now);
    return result;
}

/* PoP: mm_bedrock_probe_failed_recently @ agent/model_metadata.py:_bedrock_probe_failed_recently */
static json_t *mm_bedrock_probe_failed_recently(mm_ctx *ctx, json_t *in) {
    const char *key = json_get_str(in, "key", "");
    double ttl = mm_env_num(ctx, "bedrock_probe_failure_ttl", 300.0);
    json_t *cache = mm_state_obj(ctx, "_BEDROCK_PROBE_FAILURE_CACHE");
    json_t *now = mm_call(ctx, "time.monotonic", json_object());
    if (mm_exc_pending(ctx)) { json_free(now); return NULL; }
    /* snapshot expired keys in insertion order, then prune */
    json_t *expired = json_array();
    for (size_t i = 0; i < cache->c.count; i++) {
        double failed_at = mm_num(cache->c.items[i]);
        if ((mm_num(now) - failed_at) >= ttl)
            json_append(expired, json_string(cache->c.keys[i]));
    }
    for (size_t i = 0; i < expired->c.count; i++)
        json_obj_del(cache, mm_jstr(expired->c.items[i]));
    json_free(expired);
    bool member = json_has(cache, key);
    json_free(now);
    return json_bool(member);
}

/* PoP: mm_remember_no_codex_catalog @ agent/model_metadata.py:_remember_no_codex_catalog */
static json_t *mm_remember_no_codex_catalog(mm_ctx *ctx, json_t *in) {
    const char *cache_key = json_get_str(in, "cache_key", "");
    double ttl = mm_env_num(ctx, "codex_ctx_cache_ttl", 3600);
    double neg_ttl = mm_env_num(ctx, "codex_ctx_negative_ttl", 300);
    /* MEMO_LOCK modeled inertly (no seam) */
    json_t *cache = mm_state_obj(ctx, "_codex_oauth_context_cache");
    json_t *now = mm_call(ctx, "time.time", json_object());
    if (mm_exc_pending(ctx)) { json_free(now); return NULL; }
    json_t *current = mm_get(cache, cache_key);
    if (!mm_is_none(current) && mm_is_arr(current) && current->c.count >= 2) {
        json_t *c0 = current->c.items[0];
        double c1 = mm_num(current->c.items[1]);
        if (mm_truthy(c0) && (mm_num(now) - c1) < ttl) { json_free(now); return json_null(); }
    }
    json_t *ba = json_object();
    mm_arg_str(ba, "cache_key", cache_key);
    json_t *b = mm_call(ctx, "bound_codex_caches", ba);
    json_free(b);
    if (mm_exc_pending(ctx)) { json_free(now); return NULL; }
    json_t *entry = json_array();
    json_append(entry, json_object());
    json_append(entry, json_number(mm_num(now) - ttl + neg_ttl));
    json_set(cache, cache_key, entry);
    json_free(now);
    return json_null();
}

/* PoP: mm_cached_codex_catalog_max @ agent/model_metadata.py:_cached_codex_catalog_max */
static json_t *mm_cached_codex_catalog_max(mm_ctx *ctx, json_t *in) {
    json_t *fa = json_object();
    mm_arg(fa, "access_token", mm_get(in, "access_token"));
    mm_arg(fa, "base_url", mm_get(in, "base_url"));
    json_t *fp = mm_call(ctx, "codex_oauth_token_fingerprint", fa);
    if (mm_exc_pending(ctx)) { json_free(fp); return NULL; }
    char *fp_str = mm_pystr(fp);
    json_free(fp);
    json_t *cache = mm_get(ctx->state, "_codex_oauth_max_context_cache");   /* read-only */
    json_t *empty = json_object();
    json_t *published = mm_get_def(cache, fp_str ? fp_str : "", empty);
    free(fp_str);
    json_t *ka = json_object();
    mm_arg(ka, "keys", published);
    mm_arg(ka, "slug", mm_get(in, "slug"));
    json_t *key = mm_call(ctx, "codex_catalog_key", ka);
    if (mm_exc_pending(ctx)) { json_free(key); json_free(empty); return NULL; }
    json_t *result = NULL;
    if (!mm_is_none(key)) result = mm_get(published, mm_jstr(key));
    json_t *ret = mm_copy_or_null(result);
    json_free(key);
    json_free(empty);
    return ret;
}
/* ── cache-entry validator + Codex catalog flows ─────────────────────────── */

/* drop branch: log(msg, model, base_url, shown) then _invalidate_cached_context_length.
 * Returns NULL (None); a seam raise propagates via ctx->exc. */
static json_t *mm_vcl_drop(mm_ctx *ctx, json_t *in, const char *level, const char *msg, json_t *shown) {
    /* log(msg, model, base_url, shown): POSITIONAL args — an array built with
     * json_append (mm_arg/json_set is a silent no-op on arrays). None stays
     * null in the positional list (the oracle only drops None KWARGS keys). */
    json_t *la = json_array();
    json_t *m = mm_get(in, "model");
    json_t *b = mm_get(in, "base_url");
    json_append(la, m ? json_copy(m) : json_null());
    json_append(la, b ? json_copy(b) : json_null());
    json_append(la, shown ? shown : json_null());
    mm_log(ctx, level, msg, la);
    if (mm_exc_pending(ctx)) return NULL;
    json_t *ia = json_object();
    mm_arg(ia, "model", mm_get(in, "model"));
    mm_arg(ia, "base_url", mm_get(in, "base_url"));
    json_t *iv = mm_call(ctx, "invalidate_cached_context_length", ia);
    json_free(iv);
    if (mm_exc_pending(ctx)) return NULL;
    return json_null();
}

/* PoP: mm_validate_cached_context_length @ agent/model_metadata.py:_validate_cached_context_length */
static json_t *mm_validate_cached_context_length(mm_ctx *ctx, json_t *in) {
    json_t *cached_node = mm_get(in, "cached");
    double cached = mm_num(cached_node);
    /* drop_rules tuple is built EAGERLY: rule2 seam fires when cached<=32768,
     * rule3 seam ALWAYS fires, before the loop selects a hit. */
    bool hit1 = cached <= 0;
    bool hit2 = false;
    if (cached <= 32768) {
        json_t *sa = json_object();
        mm_arg(sa, "model", mm_get(in, "model"));
        json_t *s2 = mm_call(ctx, "model_name_suggests_stale_32k_underreport", sa);
        if (mm_exc_pending(ctx)) { json_free(s2); return NULL; }
        hit2 = mm_truthy(s2);
        json_free(s2);
    }
    json_t *pa = json_object();
    mm_arg(pa, "model", mm_get(in, "model"));
    mm_arg(pa, "cached", cached_node);
    json_t *s3 = mm_call(ctx, "stale_pre_catalog_cache_entry", pa);
    if (mm_exc_pending(ctx)) { json_free(s3); return NULL; }
    bool hit3 = mm_truthy(s3);
    json_free(s3);

    if (hit1)
        return mm_vcl_drop(ctx, in, "warning",
            "Dropping non-positive cache entry %s@%s -> %s; re-resolving", json_copy(cached_node));
    if (hit2) {
        char *cs = mm_comma_int(cached);
        json_t *r = mm_vcl_drop(ctx, in, "info",
            "Dropping stale cached context entry %s@%s -> %s (known 32K underreport); re-resolving via hardcoded defaults",
            json_string(cs ? cs : ""));
        free(cs);
        return r;
    }
    if (hit3) {
        char *cs = mm_comma_int(cached);
        json_t *r = mm_vcl_drop(ctx, in, "info",
            "Dropping stale pre-catalog cache entry %s@%s -> %s; re-resolving via hardcoded defaults",
            json_string(cs ? cs : ""));
        free(cs);
        return r;
    }
    json_t *ia = json_object();
    mm_arg(ia, "base_url", mm_get(in, "base_url"));
    json_t *inf = mm_call(ctx, "infer_provider_from_url", ia);
    if (mm_exc_pending(ctx)) { json_free(inf); return NULL; }
    if (mm_is_str(inf) && mm_str_eq(inf->str_val, "nous")) {
        json_free(inf);
        /* logger.debug(msg, model, base_url): positional array, not mm_arg. */
        json_t *la = json_array();
        json_t *m = mm_get(in, "model");
        json_t *b = mm_get(in, "base_url");
        json_append(la, m ? json_copy(m) : json_null());
        json_append(la, b ? json_copy(b) : json_null());
        mm_log(ctx, "debug", "Bypassing persistent cache for %s@%s (Nous portal authoritative)", la);
        if (mm_exc_pending(ctx)) return NULL;
        return json_null();
    }
    json_free(inf);
    json_t *la = json_object();
    mm_arg(la, "base_url", mm_get(in, "base_url"));
    json_t *loc = mm_call(ctx, "is_local_endpoint", la);
    if (mm_exc_pending(ctx)) { json_free(loc); return NULL; }
    if (mm_truthy(loc)) {
        json_free(loc);
        json_t *ra = json_object();
        mm_arg(ra, "model", mm_get(in, "model"));
        mm_arg(ra, "base_url", mm_get(in, "base_url"));
        mm_arg(ra, "cached", cached_node);
        mm_arg(ra, "api_key", mm_get(in, "api_key"));
        json_t *rec = mm_call(ctx, "reconcile_local_cached_context_length", ra);
        if (mm_exc_pending(ctx)) { json_free(rec); return NULL; }
        return rec;
    }
    json_free(loc);
    return mm_copy_or_null(cached_node);
}

/* PoP: mm_fetch_codex_catalog_entries @ agent/model_metadata.py:fetch_codex_catalog_entries */
static json_t *mm_fetch_codex_catalog_entries(mm_ctx *ctx, json_t *in) {
    json_t *urls = mm_codex_catalog_urls(ctx, in);   /* REAL composition */
    json_t *status = NULL;
    json_t *result = NULL;
    for (size_t i = 0; i < urls->c.count; i++) {
        json_t *ga = json_object();
        mm_arg(ga, "url", urls->c.items[i]);
        json_t *resp = mm_call(ctx, "get", ga);
        if (mm_exc_pending(ctx)) { json_free(resp); goto done; }
        json_t *sc = mm_get(resp, "status_code");
        json_free(status);
        status = mm_copy_or_null(sc);
        if (mm_num(sc) != 200) { json_free(resp); continue; }
        json_t *data = mm_get(resp, "json");
        json_t *entries = mm_is_obj(data) ? mm_get(data, "models") : NULL;
        if (mm_is_arr(entries) && entries->c.count > 0) {
            result = mm_pair(mm_list_copy(entries), json_copy(status));
            json_free(resp);
            goto done;
        }
        json_free(resp);
    }
    result = mm_pair(json_array(), mm_copy_or_null(status));
done:
    json_free(urls);
    json_free(status);
    return result;
}

/* PoP: mm_codex_catalog_probe_allowed @ agent/model_metadata.py:_codex_catalog_probe_allowed */
static json_t *mm_codex_catalog_probe_allowed(mm_ctx *ctx, json_t *in) {
    json_t *at = mm_get(in, "access_token");
    if (!mm_truthy(at)) return json_bool(false);
    const char *endpoint_default = mm_env_str(ctx, "codex_catalog_endpoint",
        "https://chatgpt.com/backend-api/codex/models");
    char *b0 = mm_str_or_empty(mm_get(in, "base_url"));
    char *b1 = mm_strip(b0); free(b0);
    const char *base = b1[0] ? b1 : endpoint_default;
    json_t *ha = json_object();
    mm_arg_str(ha, "base_url", base);
    mm_arg_str(ha, "domain", "chatgpt.com");
    json_t *hm = mm_call(ctx, "base_url_host_matches", ha);
    if (mm_exc_pending(ctx)) { free(b1); json_free(hm); return NULL; }
    if (!mm_truthy(hm)) { free(b1); json_free(hm); return json_bool(true); }
    json_free(hm);
    json_t *da = json_object();
    mm_arg(da, "access_token", at);
    json_t *dj = mm_call(ctx, "decode_jwt_claims", da);
    if (mm_exc_pending(ctx)) { free(b1); json_free(dj); return NULL; }
    bool r = mm_truthy(dj);
    free(b1);
    json_free(dj);
    return json_bool(r);
}
/* ── /models payload builder + native-Anthropic estimators ───────────────── */

static const char *MM_CONTEXT_LENGTH_KEYS[] = {
    "context_length", "context_window", "context_size", "max_context_length",
    "max_position_embeddings", "max_model_len", "max_input_tokens",
    "max_sequence_length", "max_seq_len", "n_ctx_train", "n_ctx", "ctx_size",
};

/* _add_model_aliases(cache, model_id, entry): seam + the IDENTICAL state effect
 * (cache[model_id]=entry; if "/" in model_id:
 *  cache.setdefault(model_id.split("/", 1)[1], entry) — the part AFTER the
 *  first slash, e.g. "org/m1" -> "m1"). */
static bool mm_add_model_aliases(mm_ctx *ctx, json_t *cache, const char *model_id, json_t *entry) {
    json_t *aa = json_object();
    mm_arg(aa, "cache", cache);
    mm_arg_str(aa, "model_id", model_id);
    mm_arg(aa, "entry", entry);
    json_t *ar = mm_call(ctx, "add_model_aliases", aa);
    json_free(ar);
    if (mm_exc_pending(ctx)) return false;
    json_set(cache, model_id, json_copy(entry));
    const char *slash = strchr(model_id, '/');
    if (slash && !json_has(cache, slash + 1)) json_set(cache, slash + 1, json_copy(entry));
    return true;
}

/* PoP: mm_parse_models_payload @ agent/model_metadata.py:_parse_models_payload */
static json_t *mm_parse_models_payload(mm_ctx *ctx, json_t *in) {
    json_t *payload = mm_get(in, "payload");
    if (!mm_is_obj(payload)) { mm_raise_attr(ctx, payload, "get"); return NULL; }
    json_t *empty = json_array();
    json_t *data = mm_get_def(payload, "data", empty);
    if (!mm_is_arr(data)) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", mm_py_type_name(data));
        mm_raise(ctx, "TypeError", msg);
        json_free(empty);
        return NULL;
    }
    json_t *cache = json_object();
    for (size_t i = 0; i < data->c.count; i++) {
        json_t *model = data->c.items[i];
        json_t *id_node = mm_is_obj(model) ? mm_get(model, "id") : NULL;
        if (!mm_truthy(id_node)) continue;
        const char *model_id = mm_jstr(id_node);
        /* arg eval: _extract_first_int(model, _CONTEXT_LENGTH_KEYS) fires first */
        json_t *ka = json_object();
        mm_arg(ka, "payload", model);
        json_t *keys = json_array();
        for (size_t k = 0; k < sizeof(MM_CONTEXT_LENGTH_KEYS) / sizeof(MM_CONTEXT_LENGTH_KEYS[0]); k++)
            json_append(keys, json_string(MM_CONTEXT_LENGTH_KEYS[k]));
        json_set(ka, "keys", keys);
        json_t *ctxval = mm_call(ctx, "extract_first_int", ka);
        if (mm_exc_pending(ctx)) { json_free(ctxval); json_free(cache); json_free(empty); return NULL; }
        /* _endpoint_model_entry REAL (fires extract_first_int + extract_pricing) */
        json_t *ein = json_object();
        mm_arg(ein, "model", model);
        mm_arg_str(ein, "model_id", model_id);
        mm_arg(ein, "context_length", ctxval);
        json_free(ctxval);
        json_t *entry = mm_endpoint_model_entry(ctx, ein);
        json_free(ein);
        if (mm_exc_pending(ctx)) { json_free(entry); json_free(cache); json_free(empty); return NULL; }
        if (!mm_add_model_aliases(ctx, cache, model_id, entry)) {
            json_free(entry); json_free(cache); json_free(empty); return NULL;
        }
        json_free(entry);
    }
    json_free(empty);
    return cache;
}

/* unpack a 2-element projection result; raise the live unpack message on drift. */
static bool mm_unpack2(mm_ctx *ctx, json_t *proj, json_t **a, json_t **b) {
    if (!mm_is_arr(proj)) {
        char msg[160];
        snprintf(msg, sizeof msg, "cannot unpack non-iterable %s object", mm_py_type_name(proj));
        mm_raise(ctx, "TypeError", msg);
        return false;
    }
    if (proj->c.count < 2) {
        char msg[160];
        snprintf(msg, sizeof msg, "not enough values to unpack (expected 2, got %zu)", proj->c.count);
        mm_raise(ctx, "ValueError", msg);
        return false;
    }
    if (proj->c.count > 2) {
        char msg[160];
        snprintf(msg, sizeof msg, "too many values to unpack (expected 2, got %zu)", proj->c.count);
        mm_raise(ctx, "ValueError", msg);
        return false;
    }
    *a = proj->c.items[0];
    *b = proj->c.items[1];
    return true;
}
/* sum() over a numeric list (fixture domain). */
static double mm_sum_nums(const json_t *arr) {
    double s = 0;
    if (mm_is_arr(arr))
        for (size_t i = 0; i < arr->c.count; i++) s += mm_num(arr->c.items[i]);
    return s;
}

/* PoP: mm_estimate_native_anthropic_messages_tokens_rough @ agent/model_metadata.py:estimate_native_anthropic_messages_tokens_rough */
static json_t *mm_estimate_native_anthropic_messages_tokens_rough(mm_ctx *ctx, json_t *in) {
    json_t *pa = json_object();
    mm_arg(pa, "messages", mm_get(in, "messages"));
    json_t *proj = mm_call(ctx, "native_anthropic_accounting_projection", pa);
    if (mm_exc_pending(ctx)) { json_free(proj); return NULL; }
    json_t *projected = NULL, *replayed = NULL;
    if (!mm_unpack2(ctx, proj, &projected, &replayed)) { json_free(proj); return NULL; }
    json_t *ea = json_object();
    mm_arg(ea, "messages", projected);
    json_t *est = mm_call(ctx, "estimate_messages_tokens_rough", ea);
    if (mm_exc_pending(ctx)) { json_free(proj); json_free(est); return NULL; }
    double total = mm_num(est) + mm_sum_nums(replayed);
    json_free(proj);
    json_free(est);
    return json_number(total);
}

/* PoP: mm_estimate_native_anthropic_request_tokens_rough @ agent/model_metadata.py:estimate_native_anthropic_request_tokens_rough */
static json_t *mm_estimate_native_anthropic_request_tokens_rough(mm_ctx *ctx, json_t *in) {
    json_t *pa = json_object();
    mm_arg(pa, "messages", mm_get(in, "messages"));
    json_t *proj = mm_call(ctx, "native_anthropic_accounting_projection", pa);
    if (mm_exc_pending(ctx)) { json_free(proj); return NULL; }
    json_t *projected = NULL, *replayed = NULL;
    if (!mm_unpack2(ctx, proj, &projected, &replayed)) { json_free(proj); return NULL; }
    json_t *ea = json_object();
    mm_arg(ea, "messages", projected);
    /* system_prompt defaults to "" (absent -> ""); present-null stays dropped */
    json_t *sp = mm_get(in, "system_prompt");
    if (!sp) mm_arg_str(ea, "system_prompt", "");
    else mm_arg(ea, "system_prompt", sp);
    mm_arg(ea, "tools", mm_get(in, "tools"));
    json_t *est = mm_call(ctx, "estimate_request_tokens_rough", ea);
    if (mm_exc_pending(ctx)) { json_free(proj); json_free(est); return NULL; }
    double total = mm_num(est) + mm_sum_nums(replayed);
    json_free(proj);
    json_free(est);
    return json_number(total);
}