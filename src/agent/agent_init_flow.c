/* agent_init_flow.c — FLOW + leaves port of the UNCLAIMED remainder of
 * agent/agent_init.py (the claimed siblings live in agent_init.c / init_ports.c /
 * core_helpers.c / config_py_pure.c and are SEAMED here).
 *
 * Decision-sequence port of the agent-init phase helpers: the strict config
 * coercion leaves (_parse_config_int / _cfg_flag / _cfg_dict / _positive_int),
 * the default-table apply (_set_defaults) + turn/usage state init, logging setup,
 * prompt-cache config, the ordered api-mode ladder (_resolve_api_mode), the
 * wire-mode client dispatch (_build_client), the compression config cluster
 * (_compression_threshold / _compression_codex_settings / _parse_compression_config),
 * the custom-provider / route / context-length scoping cluster
 * (_custom_provider_configured_base_url / _configured_default_base_url /
 * _active_route_url / _scope_context_length_to_default_runtime /
 * set_config_context_length / config_context_length_for_runtime), the context
 * engine selector + compressor max-tokens + minimum-context enforcement, the
 * non-agentic Hermes warning, and the usage-state init. ALL collaborator I/O
 * (claimed siblings, foreign-module imports, run_agent / module loggers, self
 * methods, ctor/factory seams) rides the injected universal seam ai_call; the
 * harness scripts results and records the ordered trace the oracle diffs against
 * LIVE agent/agent_init.py under the same scripts. Ported functions call each
 * other REAL where composed.
 *
 * Representation contract (shared with tests/oracle_agent_init_flow.py):
 *   - object doubles (agent / compressor / engine / candidate) are attribute
 *     dicts read via json_obj_get; tuple/set -> array (sets sorted); None -> JSON
 *     null; integral floats collapse to ints on both sides.
 *   - uncaught exception -> {"__raise__": cls} (class only); an exception passed
 *     as a logger arg projects to {"__raise__": cls, "msg": str(e)}.
 *   - scripts: MISSING key -> fail-closed RuntimeError("no script for <name>");
 *     {"$seq":[...]} per-call queue (exhaustion raises RuntimeError("script
 *     exhausted: <name>")); {"__raise__":cls,"msg":m} raises (trace ret is the
 *     WHOLE marker); a present JSON null is a real None result; any other value
 *     reused. None-valued seam arg keys dropped on BOTH sides.
 *   - self-method presence is declared by the fixture "methods" array (ctx.methods);
 *     a missing method raises AttributeError BEFORE any seam event (LOAD_ATTR).
 *   - direct agent attribute reads raise AttributeError when the key is ABSENT
 *     (present-null is a real None); getattr-with-default reads return the default.
 *   - factory table entries (_TURN_STATE / _USAGE_STATE callables) fire a
 *     "factory.<name>" seam; their scripted JSON result is stored verbatim.
 *
 * Fixture domain (ASCII scope): str operands ASCII; .strip()/.lower() ASCII;
 * int()/float() string operands use the 6 ASCII whitespace chars (space \t \n \r
 * \v \f), optional sign, underscores BETWEEN digits; no NaN/Infinity literals;
 * URL operands are bracket-free ASCII (no IPv6 / NFKC netloc surface); str()-
 * projected fields are str/bool/int/null (no non-integral floats); context_length
 * / max_tokens / num_ctx fields are integral ints (libjson has no int/float
 * identity); set-valued seam scripts are SORTED string arrays. The four SDK
 * client initializers (_init_anthropic_client / _init_moa_client /
 * _init_bedrock_client / _init_openai_client) are SEAMED collaborators in
 * _build_client (their bodies are SDK construction; they remain unclaimed).
 *
 * libjson-only (default oracle deps suffice). Self-contained: the harness
 * #includes "agent/agent_init_flow.c". ai_ prefix; static helpers.
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>

/* ── context + seam plumbing ─────────────────────────────────────────────── */

typedef struct ai_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *env;       /* borrowed: scripted constants/flags */
    json_t *methods;   /* borrowed: array of available self-method names */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} ai_ctx;

static bool ai_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool ai_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool ai_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool ai_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool ai_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool ai_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool ai_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static bool ai_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static const char *ai_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static double ai_num(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}

/* borrowed object field or NULL */
static json_t *ai_get(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}
static bool ai_has(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT && json_obj_get(obj, key) != NULL;
}
/* dict.get(key, default): present-null keeps the null node; absent -> default. */
static json_t *ai_get_def(const json_t *obj, const char *key, const json_t *def) {
    if (obj && obj->type == JSON_OBJECT) {
        json_t *v = json_obj_get(obj, key);
        if (v) return v;
    }
    return (json_t *)def;
}
static json_t *ai_copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}

/* Deep Python-style equality; NULL pointer and JSON_NULL both read None. */
static bool ai_json_eq(const json_t *a, const json_t *b) {
    if (ai_is_none(a) && ai_is_none(b)) return true;
    if (ai_is_none(a) || ai_is_none(b)) return false;
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return ai_str_eq(a->str_val, b->str_val);
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!ai_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !ai_json_eq(a->c.items[i], bv)) return false;
            }
            return true;
        default: return true;
    }
}

/* ── string helpers (caller frees) ───────────────────────────────────────── */

static bool ai_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static char *ai_strip(const char *s) {
    if (!s) return strdup("");
    const char *b = s;
    while (*b && ai_is_space(*b)) b++;
    const char *e = b + strlen(b);
    while (e > b && ai_is_space(e[-1])) e--;
    size_t n = (size_t)(e - b);
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, b, n); r[n] = '\0';
    return r;
}
static char *ai_lower(const char *s) {
    char *r = strdup(s ? s : "");
    if (!r) return NULL;
    for (char *p = r; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    return r;
}
/* rstrip of a single char set (ASCII). */
static char *ai_rstrip_chars(const char *s, const char *chars) {
    if (!s) return strdup("");
    size_t n = strlen(s);
    while (n > 0 && strchr(chars, s[n - 1])) n--;
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, s, n); r[n] = '\0';
    return r;
}
static bool ai_endswith(const char *s, const char *suf) {
    size_t n = strlen(s), m = strlen(suf);
    return n >= m && strcmp(s + n - m, suf) == 0;
}
/* portable strndup (C11; no POSIX dependency). Caller frees. */
static char *ai_strndup(const char *s, size_t n) {
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    if (n) memcpy(r, s, n);
    r[n] = '\0';
    return r;
}
static bool ai_startswith(const char *s, const char *pre) {
    return strncmp(s, pre, strlen(pre)) == 0;
}

/* growable string buffer */
typedef struct { char *p; size_t len, cap; } ai_sb;
static void ai_sb_init(ai_sb *s) { s->cap = 64; s->len = 0; s->p = (char *)malloc(s->cap); if (s->p) s->p[0] = '\0'; }
static void ai_sb_put(ai_sb *s, const char *t) {
    if (!s->p || !t) return;
    size_t n = strlen(t);
    if (s->len + n + 1 > s->cap) {
        while (s->len + n + 1 > s->cap) s->cap *= 2;
        char *np = (char *)realloc(s->p, s->cap);
        if (!np) return;
        s->p = np;
    }
    memcpy(s->p + s->len, t, n); s->len += n; s->p[s->len] = '\0';
}
static void ai_sb_putc(ai_sb *s, char c) { char t[2] = {c, '\0'}; ai_sb_put(s, t); }

/* ── exception plumbing (FULL builtin hierarchy) ─────────────────────────── */

static bool ai_exc_pending(const ai_ctx *ctx) { return ctx && ctx->exc != NULL; }
static void ai_exc_clear(ai_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}
static void ai_raise(ai_ctx *ctx, const char *cls, const char *msg) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
    json_set(ctx->exc, "msg", json_string(msg ? msg : ""));
}
static const char *ai_exc_class(const ai_ctx *ctx) {
    return json_get_str(ctx->exc, "__raise__", "");
}
/* Full CPython builtin exception parent table (child -> parent). */
static const char *ai_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        {"Exception", "BaseException"},
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"}, {"BaseExceptionGroup", "BaseException"},
        {"StopIteration", "Exception"}, {"StopAsyncIteration", "Exception"},
        {"ArithmeticError", "Exception"}, {"FloatingPointError", "ArithmeticError"},
        {"OverflowError", "ArithmeticError"}, {"ZeroDivisionError", "ArithmeticError"},
        {"AssertionError", "Exception"}, {"AttributeError", "Exception"},
        {"BufferError", "Exception"}, {"EOFError", "Exception"},
        {"ImportError", "Exception"}, {"ModuleNotFoundError", "ImportError"},
        {"LookupError", "Exception"}, {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
        {"MemoryError", "Exception"}, {"NameError", "Exception"}, {"UnboundLocalError", "NameError"},
        {"OSError", "Exception"}, {"BlockingIOError", "OSError"}, {"ChildProcessError", "OSError"},
        {"ConnectionError", "OSError"}, {"BrokenPipeError", "ConnectionError"},
        {"ConnectionAbortedError", "ConnectionError"}, {"ConnectionRefusedError", "ConnectionError"},
        {"ConnectionResetError", "ConnectionError"}, {"FileExistsError", "OSError"},
        {"FileNotFoundError", "OSError"}, {"InterruptedError", "OSError"},
        {"IsADirectoryError", "OSError"}, {"NotADirectoryError", "OSError"},
        {"PermissionError", "OSError"}, {"ProcessLookupError", "OSError"}, {"TimeoutError", "OSError"},
        {"ReferenceError", "Exception"}, {"RuntimeError", "Exception"},
        {"NotImplementedError", "RuntimeError"}, {"RecursionError", "RuntimeError"},
        {"SyntaxError", "Exception"}, {"IndentationError", "SyntaxError"}, {"TabError", "IndentationError"},
        {"SystemError", "Exception"}, {"TypeError", "Exception"}, {"ValueError", "Exception"},
        {"UnicodeError", "ValueError"}, {"UnicodeDecodeError", "UnicodeError"},
        {"UnicodeEncodeError", "UnicodeError"}, {"UnicodeTranslateError", "UnicodeError"},
        {"ExceptionGroup", "Exception"},
        {"Warning", "Exception"}, {"DeprecationWarning", "Warning"},
        {"PendingDeprecationWarning", "Warning"}, {"RuntimeWarning", "Warning"},
        {"SyntaxWarning", "Warning"}, {"UserWarning", "Warning"}, {"FutureWarning", "Warning"},
        {"ImportWarning", "Warning"}, {"UnicodeWarning", "Warning"}, {"BytesWarning", "Warning"},
        {"EncodingWarning", "Warning"}, {"ResourceWarning", "Warning"},
        /* OSError aliases */
        {"EnvironmentError", "OSError"}, {"IOError", "OSError"}, {"WindowsError", "OSError"},
        {"JSONDecodeError", "ValueError"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (ai_str_eq(T[i].c, c)) return T[i].p;
    return NULL;
}
static bool ai_exc_matches(const ai_ctx *ctx, const char *const *classes, size_t n) {
    if (!ai_exc_pending(ctx)) return false;
    const char *cls = ai_exc_class(ctx);
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (ai_str_eq(cls, classes[i])) return true;
        const char *p = ai_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}
/* except Exception (NOT the BaseException trio / BaseExceptionGroup). */
static bool ai_exc_is_exception(const ai_ctx *ctx) {
    static const char *E[] = {"Exception"};
    return ai_exc_matches(ctx, E, 1);
}
static bool ai_exc_is_typeval(const ai_ctx *ctx) {
    static const char *E[] = {"TypeError", "ValueError"};
    return ai_exc_matches(ctx, E, 2);
}
/* exception marker node {"__raise__","msg"} (for logger args). */
static json_t *ai_exc_marker(const char *cls, const char *msg) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(cls));
    json_set(e, "msg", json_string(msg ? msg : ""));
    return e;
}
static json_t *ai_exc_arg(const ai_ctx *ctx) {
    return ai_exc_marker(ai_exc_class(ctx), json_get_str(ctx->exc, "msg", ""));
}

/* ── universal seam (fail-closed on missing script) ──────────────────────── */

static json_t *ai_script_result(ai_ctx *ctx, const json_t *script) {
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
static json_t *ai_call(ai_ctx *ctx, const char *name, json_t *args) {
    if (!args) args = json_object();
    json_t *ret = NULL;
    json_t *script = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *chosen = script;
    bool fail_closed = false;
    if (!script) {
        /* MISSING key -> fail-closed RuntimeError (a present null is a real None). */
        if (ctx->exc) json_free(ctx->exc);
        ctx->exc = json_object();
        json_set(ctx->exc, "__raise__", json_string("RuntimeError"));
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        json_set(ctx->exc, "msg", json_string(msg));
        chosen = ctx->exc;
        fail_closed = true;
    } else if (script->type == JSON_OBJECT) {
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
                fail_closed = true;
            } else {
                json_set(ctx->counters, name, json_number(cur + 1));
                chosen = seq->c.items[idx];
            }
        }
    }
    if (!fail_closed) ret = ai_script_result(ctx, chosen);
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args);
    if (fail_closed) json_set(ev, "ret", json_copy(ctx->exc));
    else if (ctx->exc && chosen && chosen->type == JSON_OBJECT && json_obj_get(chosen, "__raise__"))
        json_set(ev, "ret", json_copy(ctx->exc));
    else json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(ctx->trace, ev);
    return ret;
}

/* Arg builders: None-valued keys dropped on BOTH sides ("" kept). Borrowed val
 * is copied; the caller keeps ownership of the source node. */
static void ai_arg(json_t *args, const char *key, const json_t *val) {
    if (!val || val->type == JSON_NULL) return;
    json_set(args, key, json_copy(val));
}
static void ai_arg_str(json_t *args, const char *key, const char *val) {
    if (!val) return;
    json_set(args, key, json_string(val));
}
/* logger seam: {msg,args[,exc_info]}. prefix "" (module) or "ra." (run_agent).
 * args_arr ownership transferred. */
static void ai_log(ai_ctx *ctx, const char *prefix, const char *level,
                   const char *msg, json_t *args_arr, bool exc_info) {
    json_t *a = json_object();
    json_set(a, "msg", json_string(msg));
    json_set(a, "args", args_arr ? args_arr : json_array());
    if (exc_info) json_set(a, "exc_info", json_bool(true));
    char name[48];
    snprintf(name, sizeof name, "%slogger.%s", prefix, level);
    json_t *r = ai_call(ctx, name, a);
    json_free(r);
}

/* self-method availability (fixture "methods" array). */
static bool ai_has_method(const ai_ctx *ctx, const char *name) {
    if (!ai_is_arr(ctx->methods)) return false;
    for (size_t i = 0; i < ctx->methods->c.count; i++)
        if (ai_is_str(ctx->methods->c.items[i]) && ai_str_eq(ctx->methods->c.items[i]->str_val, name))
            return true;
    return false;
}
/* direct agent attribute read: absent -> AttributeError (present-null is None). */
static json_t *ai_agent_attr(ai_ctx *ctx, const json_t *agent, const char *key) {
    json_t *v = ai_get(agent, key);
    if (!v) {
        char msg[160];
        snprintf(msg, sizeof msg, "'types.SimpleNamespace' object has no attribute '%s'", key);
        ai_raise(ctx, "AttributeError", msg);
        return NULL;
    }
    return v;
}

/* ── set helpers (sets project as SORTED string arrays) ──────────────────── */

static bool ai_arr_has_str(const json_t *arr, const char *s) {
    if (!ai_is_arr(arr)) return false;
    for (size_t i = 0; i < arr->c.count; i++)
        if (ai_is_str(arr->c.items[i]) && ai_str_eq(arr->c.items[i]->str_val, s)) return true;
    return false;
}
static bool ai_sets_intersect(const json_t *a, const json_t *b) {
    if (!ai_is_arr(a) || !ai_is_arr(b)) return false;
    for (size_t i = 0; i < a->c.count; i++)
        if (ai_is_str(a->c.items[i]) && ai_arr_has_str(b, a->c.items[i]->str_val)) return true;
    return false;
}
/* union of two string sets -> new sorted deduped array. */
static int ai_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
static json_t *ai_set_union(const json_t *a, const json_t *b) {
    json_t *out = json_array();
    const char **tmp = NULL; size_t n = 0, cap = 0;
    for (int pass = 0; pass < 2; pass++) {
        const json_t *src = pass == 0 ? a : b;
        if (!ai_is_arr(src)) continue;
        for (size_t i = 0; i < src->c.count; i++) {
            if (!ai_is_str(src->c.items[i])) continue;
            const char *s = src->c.items[i]->str_val;
            bool dup = false;
            for (size_t k = 0; k < n; k++) if (ai_str_eq(tmp[k], s)) { dup = true; break; }
            if (dup) continue;
            if (n == cap) { cap = cap ? cap * 2 : 8; tmp = (const char **)realloc(tmp, cap * sizeof(char *)); }
            if (tmp) tmp[n++] = s;
        }
    }
    if (tmp && n > 1) qsort(tmp, n, sizeof(char *), ai_cmp_str);
    for (size_t i = 0; i < n; i++) json_append(out, json_string(tmp[i]));
    free(tmp);
    return out;
}

/* ── Python scalar str() projection (str/bool/int/null domain) ───────────── */

static char *ai_py_str(const json_t *v) {
    if (ai_is_none(v)) return strdup("None");
    if (ai_is_bool(v)) return strdup(v->bool_val ? "True" : "False");
    if (ai_is_num(v)) {
        char buf[64]; double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
        else snprintf(buf, sizeof buf, "%.17g", d);
        return strdup(buf);
    }
    if (ai_is_str(v)) return strdup(ai_jstr(v));
    /* containers outside the str()-projection domain; approximate repr. */
    char *s = json_serialize(v);
    return s ? s : strdup("");
}
/* str(x or "") — falsy collapses to the EMPTY STRING (not str(None)). */
static char *ai_py_str_or_empty(const json_t *x) {
    if (!ai_truthy(x)) return strdup("");
    return ai_py_str(x);
}

/* ── int()/float() string grammar (ASCII whitespace, sign, underscores) ──── */

/* Validate a digit run with underscores BETWEEN digits; append cleaned digits. */
static bool ai_digit_run(const char **pp, ai_sb *clean) {
    const char *p = *pp;
    bool any = false, last_digit = false;
    while (*p) {
        if (*p >= '0' && *p <= '9') {
            if (clean) ai_sb_putc(clean, *p);
            any = true; last_digit = true; p++;
        } else if (*p == '_') {
            if (!last_digit) return false;          /* underscore needs a digit before */
            if (!(p[1] >= '0' && p[1] <= '9')) return false; /* and after */
            last_digit = false; p++;
        } else break;
    }
    *pp = p;
    return any;
}
/* Parse a Python float() string operand (already stripped). ok=0 -> ValueError. */
static bool ai_float_grammar(const char *s, double *out) {
    const char *p = s;
    int sign = 1;
    if (*p == '+' || *p == '-') { if (*p == '-') sign = -1; p++; }
    /* inf / infinity / nan (case-insensitive) */
    char low[16]; size_t li = 0;
    for (const char *q = p; *q && li < 15; q++) low[li++] = (char)tolower((unsigned char)*q);
    low[li] = '\0';
    if (ai_str_eq(low, "inf") || ai_str_eq(low, "infinity")) { *out = sign * INFINITY; return true; }
    if (ai_str_eq(low, "nan")) { *out = NAN; return true; }
    ai_sb clean; ai_sb_init(&clean);
    bool ok = true;
    const char *start = p;
    bool int_digits = ai_digit_run(&p, &clean);
    bool frac_digits = false;
    if (*p == '.') {
        ai_sb_putc(&clean, '.'); p++;
        frac_digits = ai_digit_run(&p, &clean);
    }
    if (!int_digits && !frac_digits) { ok = false; goto done; }
    if (*p == 'e' || *p == 'E') {
        ai_sb_putc(&clean, 'e'); p++;
        if (*p == '+' || *p == '-') { ai_sb_putc(&clean, *p); p++; }
        if (!ai_digit_run(&p, &clean)) { ok = false; goto done; }
    }
    if (*p != '\0') { ok = false; goto done; }
    (void)start;
    *out = sign * strtod(clean.p ? clean.p : "", NULL);
done:
    free(clean.p);
    return ok;
}
/* Parse a Python int() base-10 string operand (already stripped). */
static bool ai_int_grammar(const char *s, double *out) {
    const char *p = s;
    int sign = 1;
    if (*p == '+' || *p == '-') { if (*p == '-') sign = -1; p++; }
    ai_sb clean; ai_sb_init(&clean);
    bool ok = ai_digit_run(&p, &clean) && *p == '\0';
    if (ok) *out = sign * strtod(clean.p ? clean.p : "", NULL);
    free(clean.p);
    return ok;
}
static char *ai_strip_narrow(const char *s) {
    /* int()/float() strip the 6 ASCII whitespace chars (NOT \x1c-\x1f). */
    return ai_strip(s);
}
/* float(v) -> new number node, or NULL with ctx->exc set. */
static json_t *ai_py_float(ai_ctx *ctx, const json_t *v) {
    if (ai_is_none(v)) {
        ai_raise(ctx, "TypeError", "float() argument must be a string or a real number, not 'NoneType'");
        return NULL;
    }
    if (ai_is_bool(v)) return json_number(v->bool_val ? 1.0 : 0.0);
    if (ai_is_num(v)) return json_copy(v);
    if (ai_is_str(v)) {
        char *st = ai_strip_narrow(ai_jstr(v));
        double d = 0;
        bool ok = ai_float_grammar(st ? st : "", &d);
        if (!ok) {
            char msg[256];
            snprintf(msg, sizeof msg, "could not convert string to float: '%s'", ai_jstr(v));
            ai_raise(ctx, "ValueError", msg);
            free(st);
            return NULL;
        }
        free(st);
        return json_number(d);
    }
    const char *tn = ai_is_arr(v) ? "list" : ai_is_obj(v) ? "dict" : "object";
    char msg[160];
    snprintf(msg, sizeof msg, "float() argument must be a string or a real number, not '%s'", tn);
    ai_raise(ctx, "TypeError", msg);
    return NULL;
}
/* int(v) -> new number node (integral), or NULL with ctx->exc set. */
static json_t *ai_py_int(ai_ctx *ctx, const json_t *v) {
    if (ai_is_none(v)) {
        ai_raise(ctx, "TypeError", "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
        return NULL;
    }
    if (ai_is_bool(v)) return json_number(v->bool_val ? 1.0 : 0.0);
    if (ai_is_num(v)) {
        double d = v->num_val;
        if (isnan(d)) { ai_raise(ctx, "ValueError", "cannot convert float NaN to integer"); return NULL; }
        if (isinf(d)) { ai_raise(ctx, "OverflowError", "cannot convert float infinity to integer"); return NULL; }
        return json_number(trunc(d));
    }
    if (ai_is_str(v)) {
        char *st = ai_strip_narrow(ai_jstr(v));
        double d = 0;
        bool ok = ai_int_grammar(st ? st : "", &d);
        if (!ok) {
            char msg[256];
            snprintf(msg, sizeof msg, "invalid literal for int() with base 10: '%s'", ai_jstr(v));
            ai_raise(ctx, "ValueError", msg);
            free(st);
            return NULL;
        }
        free(st);
        return json_number(d);
    }
    const char *tn = ai_is_arr(v) ? "list" : ai_is_obj(v) ? "dict" : "object";
    char msg[160];
    snprintf(msg, sizeof msg, "int() argument must be a string, a bytes-like object or a real number, not '%s'", tn);
    ai_raise(ctx, "TypeError", msg);
    return NULL;
}

/* ── urllib.parse urlsplit/urlunparse (bracket-free ASCII domain) ────────── */

typedef struct { char *scheme, *netloc, *path, *params, *query, *fragment; } ai_url;
static void ai_url_init(ai_url *u) { memset(u, 0, sizeof(*u)); }
static void ai_url_free(ai_url *u) {
    free(u->scheme); free(u->netloc); free(u->path);
    free(u->params); free(u->query); free(u->fragment);
    ai_url_init(u);
}
static bool ai_scheme_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.';
}
static bool ai_in_list(const char *const *list, size_t n, const char *s) {
    for (size_t i = 0; i < n; i++) if (ai_str_eq(list[i], s)) return true;
    return false;
}
static const char *AI_USES_PARAMS[] = {
    "", "ftp", "hdl", "prospero", "http", "imap", "https", "shttp", "rtsp",
    "rtsps", "rtspu", "sip", "sips", "mms", "sftp", "tel",
};
static const char *AI_USES_NETLOC[] = {
    "", "ftp", "http", "gopher", "nntp", "telnet", "imap", "wais", "file", "mms",
    "https", "shttp", "snews", "prospero", "rtsp", "rtsps", "rtspu", "rsync",
    "svn", "svn+ssh", "sftp", "nfs", "git", "git+ssh", "ws", "wss", "itms-services",
};
/* _splitnetloc: netloc up to first of /?# from start. */
static void ai_splitnetloc(const char *url, size_t start, char **netloc, char **rest) {
    size_t len = strlen(url), delim = len;
    const char *delims = "/?#";
    for (const char *d = delims; *d; d++) {
        const char *w = strchr(url + start, *d);
        if (w) { size_t pos = (size_t)(w - url); if (pos < delim) delim = pos; }
    }
    *netloc = ai_strndup(url + start, delim - start);
    *rest = strdup(url + delim);
}
/* _splitparams: first ';' after the last '/'. */
static void ai_splitparams(const char *url, char **path, char **params) {
    const char *slash = strrchr(url, '/');
    const char *semi = slash ? strchr(slash, ';') : strchr(url, ';');
    if (!semi) { *path = strdup(url); *params = strdup(""); return; }
    size_t i = (size_t)(semi - url);
    *path = ai_strndup(url, i);
    *params = strdup(semi + 1);
}
/* urlsplit + params. Returns false with ctx->exc on the bracket ValueError. */
static bool ai_urlparse(ai_ctx *ctx, const char *url_in, ai_url *out) {
    ai_url_init(out);
    /* lstrip C0 + space; remove \t \r \n everywhere. */
    const char *b = url_in;
    while (*b && (unsigned char)*b <= 0x20) b++;
    ai_sb sb; ai_sb_init(&sb);
    for (const char *p = b; *p; p++)
        if (*p != '\t' && *p != '\r' && *p != '\n') ai_sb_putc(&sb, *p);
    char *url = sb.p ? sb.p : strdup("");
    char *scheme = NULL, *netloc = NULL, *query = NULL, *fragment = NULL;
    /* scheme detection */
    const char *colon = strchr(url, ':');
    if (colon && colon != url && isalpha((unsigned char)url[0])) {
        size_t i = (size_t)(colon - url);
        bool all = true;
        for (size_t k = 0; k < i; k++) if (!ai_scheme_char(url[k])) { all = false; break; }
        if (all) {
            scheme = ai_strndup(url, i);
            for (char *p = scheme; *p; p++) *p = (char)tolower((unsigned char)*p);
            char *rest = strdup(colon + 1);
            free(url); url = rest;
        }
    }
    if (url[0] == '/' && url[1] == '/') {
        char *rest = NULL;
        ai_splitnetloc(url, 2, &netloc, &rest);
        free(url); url = rest;
        bool lb = strchr(netloc, '[') != NULL, rb = strchr(netloc, ']') != NULL;
        if (lb != rb) {
            ai_raise(ctx, "ValueError", "Invalid IPv6 URL");
            free(url); free(scheme); free(netloc);
            return false;
        }
        if (lb && rb) {
            /* balanced bracketed hosts are outside the documented domain. */
            ai_raise(ctx, "ValueError", "Invalid IPv6 URL");
            free(url); free(scheme); free(netloc);
            return false;
        }
    }
    char *hash = strchr(url, '#');
    if (hash) { *hash = '\0'; fragment = strdup(hash + 1); }
    char *qmark = strchr(url, '?');
    if (qmark) { *qmark = '\0'; query = strdup(qmark + 1); }
    /* params */
    char *path = NULL, *params = NULL;
    bool uses_params = ai_in_list(AI_USES_PARAMS, sizeof(AI_USES_PARAMS)/sizeof(AI_USES_PARAMS[0]), scheme ? scheme : "");
    if (uses_params && strchr(url, ';')) ai_splitparams(url, &path, &params);
    else { path = strdup(url); params = strdup(""); }
    free(url);
    out->scheme = scheme ? scheme : strdup("");
    out->netloc = netloc ? netloc : strdup("");
    out->path = path;
    out->params = params;
    out->query = query ? query : strdup("");
    out->fragment = fragment ? fragment : strdup("");
    return true;
}
/* _urlunsplit + urlunparse netloc defaulting. query/fragment NULL = omit. */
static char *ai_urlunparse(const char *scheme, const char *netloc, const char *path,
                           const char *params, const char *query, const char *fragment) {
    char *p = strdup(path ? path : "");
    /* urlunparse: if not netloc -> decide '' vs None */
    bool netloc_present = netloc && netloc[0] != '\0';
    bool netloc_none = false;
    if (!netloc_present) {
        bool uses = ai_in_list(AI_USES_NETLOC, sizeof(AI_USES_NETLOC)/sizeof(AI_USES_NETLOC[0]), scheme ? scheme : "");
        if (scheme && scheme[0] && uses && (!p[0] || p[0] == '/')) netloc_present = true; /* netloc='' */
        else netloc_none = true;
    }
    if (params && params[0]) {
        ai_sb sb; ai_sb_init(&sb); ai_sb_put(&sb, p); ai_sb_putc(&sb, ';'); ai_sb_put(&sb, params);
        free(p); p = sb.p ? sb.p : strdup("");
    }
    ai_sb sb; ai_sb_init(&sb);
    if (!netloc_none) {
        if (p[0] && p[0] != '/') ai_sb_putc(&sb, '/');
        ai_sb_put(&sb, "//"); ai_sb_put(&sb, netloc ? netloc : ""); ai_sb_put(&sb, p);
    } else {
        if (p[0] == '/' && p[1] == '/') ai_sb_put(&sb, "//");
        ai_sb_put(&sb, p);
    }
    free(p);
    p = sb.p ? sb.p : strdup("");
    ai_sb out; ai_sb_init(&out);
    if (scheme && scheme[0]) { ai_sb_put(&out, scheme); ai_sb_putc(&out, ':'); }
    ai_sb_put(&out, p);
    /* urlunparse passes query/fragment through `or None`: empty -> omitted. */
    if (query && query[0]) { ai_sb_putc(&out, '?'); ai_sb_put(&out, query); }
    if (fragment && fragment[0]) { ai_sb_putc(&out, '#'); ai_sb_put(&out, fragment); }
    free(p);
    return out.p ? out.p : strdup("");
}
/* urlunparse(urlparse(s)._replace(query="")) — strip the query, keep the rest. */
static char *ai_url_strip_query(ai_ctx *ctx, const char *s, bool *ok) {
    ai_url u;
    *ok = ai_urlparse(ctx, s, &u);
    if (!*ok) { ai_url_free(&u); return NULL; }
    char *r = ai_urlunparse(u.scheme, u.netloc, u.path, u.params, "", u.fragment);
    ai_url_free(&u);
    return r;
}

/* ── config coercion leaves ──────────────────────────────────────────────── */

/* PoP: ai_parse_config_int @ agent/agent_init.py:_parse_config_int */
static json_t *ai_parse_config_int(ai_ctx *ctx, const json_t *raw, const json_t *def) {
    (void)ctx;
    if (ai_is_bool(raw)) return json_copy(def);
    if (ai_is_num(raw)) {
        double d = raw->num_val;
        if (d == floor(d) && !isinf(d)) return json_copy(raw);
        return json_copy(def);
    }
    /* try: int(str(raw).strip()) except (TypeError, ValueError): default */
    char *s = ai_py_str(raw);
    char *st = ai_strip(s ? s : "");
    free(s);
    double d = 0;
    bool ok = ai_int_grammar(st ? st : "", &d);
    free(st);
    if (!ok) return json_copy(def);
    return json_number(d);
}

/* PoP: ai_cfg_flag @ agent/agent_init.py:_cfg_flag */
static json_t *ai_cfg_flag(ai_ctx *ctx, const json_t *cfg, const char *key, const json_t *def) {
    (void)ctx;
    json_t *v = ai_get_def(cfg, key, def);
    char *s = ai_py_str(v);
    char *low = ai_lower(s ? s : "");
    free(s);
    bool hit = ai_str_eq(low, "true") || ai_str_eq(low, "1") || ai_str_eq(low, "yes");
    free(low);
    return json_bool(hit);
}

/* PoP: ai_cfg_dict @ agent/agent_init.py:_cfg_dict */
static json_t *ai_cfg_dict(ai_ctx *ctx, const json_t *cfg, const char *key) {
    (void)ctx;
    json_t *empty = json_object();
    json_t *section = ai_get_def(cfg, key, empty);
    /* section may alias empty (absent key): read it BEFORE freeing. */
    json_t *ret = ai_is_obj(section) ? json_copy(section) : json_object();
    json_free(empty);
    return ret;
}

/* PoP: ai_positive_int @ agent/agent_init.py:_positive_int */
static json_t *ai_positive_int(ai_ctx *ctx, const json_t *raw, const json_t *reject) {
    /* reject: array of type names refused outright ("bool","float"). */
    if (ai_is_arr(reject) && reject->c.count > 0) {
        if (ai_arr_has_str(reject, "bool") && ai_is_bool(raw)) return json_null();
        /* float rejection: domain carries int/str/bool/null here (no floats);
         * a non-integral number models a rejected float. */
        if (ai_arr_has_str(reject, "float") && ai_is_num(raw)) {
            double d = raw->num_val;
            if (!(d == floor(d) && !isinf(d))) return json_null();
        }
        if (ai_arr_has_str(reject, "int") && ai_is_num(raw)) return json_null();
        if (ai_arr_has_str(reject, "str") && ai_is_str(raw)) return json_null();
    }
    json_t *parsed = ai_py_int(ctx, raw);
    if (!parsed) {
        if (ai_exc_is_typeval(ctx)) { ai_exc_clear(ctx); return json_null(); }
        return NULL; /* OverflowError etc. propagates */
    }
    if (parsed->num_val > 0) return parsed;
    json_free(parsed);
    return json_null();
}

/* ── default-table apply + state tables ──────────────────────────────────── */

static void ai_table_put(json_t *t, const char *k, json_t *v) { json_set(t, k, v); }
static json_t *ai_factory_marker(void) {
    json_t *m = json_object();
    json_set(m, "__factory__", json_bool(true));
    return m;
}

/* PoP: ai_set_defaults @ agent/agent_init.py:_set_defaults */
static json_t *ai_set_defaults(ai_ctx *ctx, json_t *agent, const json_t *table) {
    if (!ai_is_obj(table)) return NULL;
    for (size_t i = 0; i < table->c.count; i++) {
        const char *name = table->c.keys[i];
        json_t *value = table->c.items[i];
        json_t *assigned;
        if (ai_is_obj(value) && json_is_true(json_obj_get(value, "__factory__"))) {
            char seam[160];
            snprintf(seam, sizeof seam, "factory.%s", name);
            assigned = ai_call(ctx, seam, json_object());
            if (ai_exc_pending(ctx)) { json_free(assigned); return NULL; }
            if (!assigned) assigned = json_null();
        } else {
            assigned = json_copy(value);
        }
        json_set(agent, name, assigned);
    }
    return NULL;
}

static json_t *ai_turn_state_table(void) {
    json_t *t = json_object();
    ai_table_put(t, "_iteration_budget_warning_injected", json_bool(false));
    ai_table_put(t, "_budget_exhausted_injected", json_bool(false));
    ai_table_put(t, "_budget_grace_call", json_bool(false));
    ai_table_put(t, "_run_budget_started_at", json_null());
    ai_table_put(t, "_run_budget_wrapup_injected", json_bool(false));
    ai_table_put(t, "_last_activity_ts", ai_factory_marker());
    ai_table_put(t, "_last_activity_desc", json_string("initializing"));
    ai_table_put(t, "_last_activity_provenance", json_string("unknown"));
    ai_table_put(t, "_session_activity_last_persist_mono", json_number(0.0));
    ai_table_put(t, "_current_tool", json_null());
    ai_table_put(t, "_api_call_count", json_number(0));
    ai_table_put(t, "_skip_mcp_refresh", json_bool(false));
    ai_table_put(t, "_tool_snapshot_generation", json_number(0));
    ai_table_put(t, "_rate_limit_state", json_null());
    ai_table_put(t, "_credits_state", json_null());
    ai_table_put(t, "_credits_session_start_micros", json_null());
    ai_table_put(t, "_or_cache_hits", json_number(0));
    return t;
}
static json_t *ai_usage_state_table(void) {
    json_t *t = json_object();
    ai_table_put(t, "_user_turn_count", json_number(0));
    ai_table_put(t, "_is_user_initiated_turn", json_bool(false));
    ai_table_put(t, "_usage_anchor", json_null());
    ai_table_put(t, "_turn_base_usage_anchor", json_null());
    ai_table_put(t, "_request_pressure_anchored", json_bool(false));
    ai_table_put(t, "session_prompt_tokens", json_number(0));
    ai_table_put(t, "session_completion_tokens", json_number(0));
    ai_table_put(t, "session_total_tokens", json_number(0));
    ai_table_put(t, "session_api_calls", json_number(0));
    ai_table_put(t, "session_input_tokens", json_number(0));
    ai_table_put(t, "session_output_tokens", json_number(0));
    ai_table_put(t, "session_cache_read_tokens", json_number(0));
    ai_table_put(t, "session_cache_write_tokens", json_number(0));
    ai_table_put(t, "session_reasoning_tokens", json_number(0));
    ai_table_put(t, "session_estimated_cost_usd", json_number(0.0));
    ai_table_put(t, "session_cost_status", json_string("unknown"));
    ai_table_put(t, "session_cost_source", json_string("none"));
    ai_table_put(t, "_api_latency_history", ai_factory_marker());
    ai_table_put(t, "_api_output_history", ai_factory_marker());
    return t;
}

/* PoP: ai_init_turn_state @ agent/agent_init.py:_init_turn_state */
static json_t *ai_init_turn_state(ai_ctx *ctx, json_t *agent, const json_t *run_budget_seconds) {
    json_t *table = ai_turn_state_table();
    json_t *r = ai_set_defaults(ctx, agent, table);
    json_free(table);
    if (ai_exc_pending(ctx)) { json_free(r); return NULL; }
    json_free(r);
    json_t *a = json_object();
    ai_arg(a, "value", run_budget_seconds);
    json_t *nb = ai_call(ctx, "normalize_run_budget_seconds", a);
    if (ai_exc_pending(ctx)) { json_free(nb); return NULL; }
    json_set(agent, "run_budget_seconds", nb ? nb : json_null());
    json_t *cl = ai_call(ctx, "new_credits_latch", json_object());
    if (ai_exc_pending(ctx)) { json_free(cl); return NULL; }
    json_set(agent, "_credits_latch", cl ? cl : json_null());
    return NULL;
}

/* PoP: ai_init_usage_state @ agent/agent_init.py:_init_usage_state */
static json_t *ai_init_usage_state(ai_ctx *ctx, json_t *agent) {
    json_t *session_cwd = ai_get(agent, "session_cwd"); /* getattr default None */
    json_t *working_dir = NULL;
    bool own_wd = false;
    if (ai_truthy(session_cwd)) {
        working_dir = session_cwd; /* borrowed */
    } else {
        json_t *sc = ai_call(ctx, "scope_terminal_cwd", json_object());
        if (ai_exc_pending(ctx)) { json_free(sc); return NULL; }
        if (ai_truthy(sc)) { working_dir = sc; own_wd = true; }
        else { json_free(sc); working_dir = NULL; }
    }
    json_t *skip = ai_agent_attr(ctx, agent, "skip_context_files");
    if (ai_exc_pending(ctx)) { if (own_wd) json_free(working_dir); return NULL; }
    json_t *a = json_object();
    ai_arg(a, "working_dir", working_dir);
    json_set(a, "enabled", json_bool(!ai_truthy(skip)));
    json_t *tracker = ai_call(ctx, "SubdirectoryHintTracker", a);
    if (own_wd) json_free(working_dir);
    if (ai_exc_pending(ctx)) { json_free(tracker); return NULL; }
    json_set(agent, "_subdirectory_hints", tracker ? tracker : json_null());
    json_t *table = ai_usage_state_table();
    json_t *r = ai_set_defaults(ctx, agent, table);
    json_free(table);
    if (ai_exc_pending(ctx)) { json_free(r); return NULL; }
    json_free(r);
    return NULL;
}

/* PoP: ai_setup_logging @ agent/agent_init.py:_setup_logging */
static json_t *ai_setup_logging(ai_ctx *ctx, json_t *agent) {
    json_t *home = ai_call(ctx, "get_hermes_home", json_object());
    if (ai_exc_pending(ctx)) { json_free(home); return NULL; }
    json_t *a = json_object();
    ai_arg(a, "hermes_home", home);
    json_free(home);
    json_t *r = ai_call(ctx, "setup_logging", a);
    if (ai_exc_pending(ctx)) { json_free(r); return NULL; }
    json_free(r);
    json_t *verbose = ai_agent_attr(ctx, agent, "verbose_logging");
    if (ai_exc_pending(ctx)) return NULL;
    if (ai_truthy(verbose)) {
        json_t *rv = ai_call(ctx, "setup_verbose_logging", json_object());
        if (ai_exc_pending(ctx)) { json_free(rv); return NULL; }
        json_free(rv);
        ai_log(ctx, "ra.", "info",
               "Verbose logging enabled (third-party library logs suppressed)", json_array(), false);
        if (ai_exc_pending(ctx)) return NULL;
    }
    return NULL;
}

/* PoP: ai_init_prompt_cache_config @ agent/agent_init.py:_init_prompt_cache_config */
static json_t *ai_init_prompt_cache_config(ai_ctx *ctx, json_t *agent) {
    const char *AUTO = "auto";
    if (!ai_has_method(ctx, "_anthropic_prompt_cache_policy")) {
        ai_raise(ctx, "AttributeError",
                 "'types.SimpleNamespace' object has no attribute '_anthropic_prompt_cache_policy'");
        return NULL;
    }
    json_t *pol = ai_call(ctx, "agent._anthropic_prompt_cache_policy", json_object());
    if (ai_exc_pending(ctx)) { json_free(pol); return NULL; }
    if (!ai_is_arr(pol) || pol->c.count != 2) {
        json_free(pol);
        ai_raise(ctx, "ValueError", "not enough values to unpack (expected 2, got 0)");
        return NULL;
    }
    json_set(agent, "_use_prompt_caching", json_copy(pol->c.items[0]));
    json_set(agent, "_use_native_cache_layout", json_copy(pol->c.items[1]));
    json_free(pol);
    json_set(agent, "_cache_disabled", json_bool(false));

    /* ttl = AUTO; try: ttl = (load_config_readonly().get("prompt_caching", {}) or {}).get("cache_ttl", AUTO) */
    json_t *ttl = json_string(AUTO);
    json_t *cfg = ai_call(ctx, "load_config_readonly", json_object());
    if (ai_exc_pending(ctx)) {
        json_free(cfg);
        if (ai_exc_is_exception(ctx)) {
            json_t *la = json_array();
            json_append(la, json_string(ai_exc_class(ctx)));
            ai_exc_clear(ctx);
            ai_log(ctx, "", "warning",
                   "Agent configuration for prompt_caching.cache_ttl could not be loaded (%s); "
                   "using default prompt caching settings. Check the active Hermes Python "
                   "environment and dependencies; run 'hermes config check' after "
                   "configuration imports work again.", la, false);
            if (ai_exc_pending(ctx)) { json_free(ttl); return NULL; }
        } else { json_free(ttl); return NULL; }
    } else {
        /* .get("prompt_caching", {}) on the result; non-dict -> AttributeError */
        bool raised = false;
        json_t *pc = NULL;
        if (!ai_is_obj(cfg)) {
            raised = true;
        } else {
            json_t *empty = json_object();
            pc = ai_get_def(cfg, "prompt_caching", empty);
            /* pc or {} — falsy -> {}; detach BEFORE freeing the default node
             * (pc may alias empty when the key is absent). */
            if (!ai_truthy(pc)) pc = NULL;
            json_free(empty);
            if (pc && !ai_is_obj(pc)) raised = true;
        }
        if (raised) {
            json_free(cfg);
            ai_raise(ctx, "AttributeError", "'object' object has no attribute 'get'");
            if (ai_exc_is_exception(ctx)) {
                json_t *la = json_array();
                json_append(la, json_string(ai_exc_class(ctx)));
                ai_exc_clear(ctx);
                ai_log(ctx, "", "warning",
                       "Agent configuration for prompt_caching.cache_ttl could not be loaded (%s); "
                       "using default prompt caching settings. Check the active Hermes Python "
                       "environment and dependencies; run 'hermes config check' after "
                       "configuration imports work again.", la, false);
                if (ai_exc_pending(ctx)) { json_free(ttl); return NULL; }
            } else { json_free(ttl); return NULL; }
        } else {
            /* pc is a truthy dict or NULL (== {}) */
            json_t *auto_node = json_string(AUTO);
            json_t *ct = pc ? ai_get_def(pc, "cache_ttl", auto_node) : auto_node;
            json_free(ttl);
            ttl = json_copy(ct); /* ct may alias auto_node: copy before freeing */
            json_free(auto_node);
            json_free(cfg);
        }
    }

    json_set(agent, "_cache_ttl", json_string("5m"));
    bool is_auto = ai_is_str(ttl) && ai_str_eq(ttl->str_val, AUTO);
    json_set(agent, "_cache_ttl_auto", json_bool(is_auto));
    bool in_tuple = ai_is_str(ttl) && (ai_str_eq(ttl->str_val, "5m") || ai_str_eq(ttl->str_val, "1h"));
    if (in_tuple) {
        json_set(agent, "_cache_ttl", json_copy(ttl));
    } else if (is_auto && !ai_truthy(ai_get(agent, "side_agent"))) {
        /* elif ttl == AUTO_CACHE_TTL and not getattr(agent, "side_agent", False) */
        json_t *a = json_object();
        ai_arg(a, "agent", agent);
        json_t *st = ai_call(ctx, "auto_start_cache_ttl", a);
        if (ai_exc_pending(ctx)) { json_free(st); json_free(ttl); return NULL; }
        json_set(agent, "_cache_ttl", st ? st : json_null());
    } else {
        /* elif cache_ttl_means_disabled(ttl) — reached for auto+side_agent too */
        json_t *a = json_object();
        ai_arg(a, "ttl", ttl);
        json_t *dis = ai_call(ctx, "cache_ttl_means_disabled", a);
        if (ai_exc_pending(ctx)) { json_free(dis); json_free(ttl); return NULL; }
        bool disabled = ai_truthy(dis);
        json_free(dis);
        if (disabled) {
            json_set(agent, "_use_prompt_caching", json_bool(false));
            json_set(agent, "_use_native_cache_layout", json_bool(false));
            json_set(agent, "_cache_ttl", json_null());
            json_set(agent, "_cache_disabled", json_bool(true));
        }
    }
    json_free(ttl);
    return NULL;
}


/* ── api-mode ladder + client dispatch ───────────────────────────────────── */

static const char *AI_EXPLICIT_API_MODES[] = {
    "chat_completions", "codex_responses", "anthropic_messages", "bedrock_converse",
    "codex_app_server",
};
static bool ai_in_explicit_modes(const json_t *m) {
    if (!ai_is_str(m)) return false;
    for (size_t i = 0; i < sizeof(AI_EXPLICIT_API_MODES)/sizeof(AI_EXPLICIT_API_MODES[0]); i++)
        if (ai_str_eq(AI_EXPLICIT_API_MODES[i], m->str_val)) return true;
    return false;
}

/* PoP: ai_resolve_api_mode @ agent/agent_init.py:_resolve_api_mode */
static json_t *ai_resolve_api_mode(ai_ctx *ctx, json_t *agent, const json_t *api_mode,
                                   const json_t *provider_name, const json_t *base_url) {
    json_t *host = ai_agent_attr(ctx, agent, "_base_url_hostname");
    if (ai_exc_pending(ctx)) return NULL;
    json_t *url = ai_agent_attr(ctx, agent, "_base_url_lower");
    if (ai_exc_pending(ctx)) return NULL;
    const char *hosts = ai_jstr(host), *urls = ai_jstr(url);
    json_t *provider = ai_agent_attr(ctx, agent, "provider");
    if (ai_exc_pending(ctx)) return NULL;
    const char *prov = ai_jstr(provider);

    /* if is_actual_route(agent.provider, base_url) */
    json_t *a = json_object();
    ai_arg(a, "provider", provider);
    ai_arg(a, "base_url", base_url);
    json_t *iar = ai_call(ctx, "is_actual_route", a);
    if (ai_exc_pending(ctx)) { json_free(iar); return NULL; }
    bool actual = ai_truthy(iar);
    json_free(iar);
    if (actual) {
        json_set(agent, "api_mode", json_string("chat_completions"));
        return NULL;
    }
    /* elif api_mode in _EXPLICIT_API_MODES or (api_mode and api_mode in registered_api_modes()) */
    bool explicit_hit = ai_in_explicit_modes(api_mode);
    bool registered_hit = false;
    if (!explicit_hit && ai_truthy(api_mode)) {
        json_t *rm = ai_call(ctx, "registered_api_modes", json_object());
        if (ai_exc_pending(ctx)) { json_free(rm); return NULL; }
        registered_hit = ai_is_str(api_mode) && ai_arr_has_str(rm, api_mode->str_val);
        json_free(rm);
    }
    if (explicit_hit || registered_hit) {
        json_set(agent, "api_mode", ai_copy_or_null(api_mode));
        return NULL;
    }
    if (ai_str_eq(prov, "openai-codex") || ai_str_eq(prov, "xai") || ai_str_eq(prov, "xai-oauth")) {
        json_set(agent, "api_mode", json_string("codex_responses"));
        return NULL;
    }
    bool pn_none = ai_is_none(provider_name);
    if (pn_none && ai_str_eq(hosts, "chatgpt.com") && strstr(urls, "/backend-api/codex")) {
        json_set(agent, "api_mode", json_string("codex_responses"));
        json_set(agent, "provider", json_string("openai-codex"));
        return NULL;
    }
    if (pn_none && ai_str_eq(hosts, "api.x.ai")) {
        json_set(agent, "api_mode", json_string("codex_responses"));
        json_set(agent, "provider", json_string("xai"));
        return NULL;
    }
    if (ai_str_eq(prov, "anthropic") || (pn_none && ai_str_eq(hosts, "api.anthropic.com"))) {
        json_set(agent, "api_mode", json_string("anthropic_messages"));
        json_set(agent, "provider", json_string("anthropic"));
        return NULL;
    }
    char *url_rstrip = ai_rstrip_chars(urls, "/");
    bool anthropic_suffix = ai_endswith(url_rstrip ? url_rstrip : "", "/anthropic");
    free(url_rstrip);
    if (anthropic_suffix) {
        json_set(agent, "api_mode", json_string("anthropic_messages"));
        return NULL;
    }
    bool bedrock = ai_str_eq(prov, "bedrock");
    if (!bedrock && ai_startswith(hosts, "bedrock-runtime.")) {
        json_t *ba = json_object();
        ai_arg_str(ba, "url", urls);
        ai_arg_str(ba, "host", "amazonaws.com");
        json_t *bm = ai_call(ctx, "base_url_host_matches", ba);
        if (ai_exc_pending(ctx)) { json_free(bm); return NULL; }
        bedrock = ai_truthy(bm);
        json_free(bm);
    }
    if (bedrock) {
        json_set(agent, "api_mode", json_string("bedrock_converse"));
        return NULL;
    }
    if (ai_str_eq(prov, "nous") || ai_str_eq(prov, "nous-portal") || ai_str_eq(prov, "nousresearch")) {
        json_t *model = ai_agent_attr(ctx, agent, "model");
        if (ai_exc_pending(ctx)) return NULL;
        json_t *na = json_object();
        ai_arg(na, "model", model);
        json_t *nm = ai_call(ctx, "nous_api_mode", na);
        if (ai_exc_pending(ctx)) { json_free(nm); return NULL; }
        json_set(agent, "api_mode", nm ? nm : json_null());
        return NULL;
    }
    /* else: host-mandated wire check (LAST) */
    json_t *mandated = NULL;
    {
        json_t *ha = json_object();
        const char *bu = ai_truthy(base_url) ? ai_jstr(base_url) : "";
        ai_arg_str(ha, "base_url", bu);
        mandated = ai_call(ctx, "host_mandated_api_mode", ha);
        if (ai_exc_pending(ctx)) {
            if (ai_exc_is_exception(ctx)) { ai_exc_clear(ctx); json_free(mandated); mandated = NULL; }
            else { json_free(mandated); return NULL; }
        }
    }
    if (!ai_is_none(mandated)) json_set(agent, "api_mode", mandated);
    else { json_free(mandated); json_set(agent, "api_mode", json_string("chat_completions")); }
    return NULL;
}

/* PoP: ai_build_client @ agent/agent_init.py:_build_client */
static json_t *ai_build_client(ai_ctx *ctx, json_t *agent, const json_t *api_key,
                               const json_t *base_url, const json_t *fallback_model) {
    json_set(agent, "_anthropic_client", json_null());
    json_set(agent, "_is_anthropic_oauth", json_bool(false));
    json_t *provider = ai_agent_attr(ctx, agent, "provider");
    if (ai_exc_pending(ctx)) return NULL;
    json_t *model = ai_agent_attr(ctx, agent, "model");
    if (ai_exc_pending(ctx)) return NULL;
    json_t *ta = json_object();
    ai_arg(ta, "provider", provider);
    ai_arg(ta, "model", model);
    json_t *timeout = ai_call(ctx, "get_provider_request_timeout", ta);
    if (ai_exc_pending(ctx)) { json_free(timeout); return NULL; }
    json_t *api_mode = ai_agent_attr(ctx, agent, "api_mode");
    if (ai_exc_pending(ctx)) { json_free(timeout); return NULL; }
    const char *am = ai_jstr(api_mode);
    const char *prov = ai_jstr(provider);
    json_t *r = NULL;
    if (ai_str_eq(am, "anthropic_messages")) {
        json_t *a = json_object();
        ai_arg(a, "agent", agent); ai_arg(a, "api_key", api_key);
        ai_arg(a, "base_url", base_url); ai_arg(a, "provider_timeout", timeout);
        r = ai_call(ctx, "init_anthropic_client", a);
    } else if (ai_str_eq(prov, "moa")) {
        json_t *a = json_object();
        ai_arg(a, "agent", agent); ai_arg(a, "api_key", api_key);
        r = ai_call(ctx, "init_moa_client", a);
    } else if (ai_str_eq(am, "bedrock_converse")) {
        json_t *a = json_object();
        ai_arg(a, "agent", agent); ai_arg(a, "base_url", base_url);
        r = ai_call(ctx, "init_bedrock_client", a);
    } else {
        json_t *a = json_object();
        ai_arg(a, "agent", agent); ai_arg(a, "api_key", api_key);
        ai_arg(a, "base_url", base_url); ai_arg(a, "fallback_model", fallback_model);
        ai_arg(a, "provider_timeout", timeout);
        r = ai_call(ctx, "init_openai_client", a);
    }
    json_free(timeout);
    if (ai_exc_pending(ctx)) { json_free(r); return NULL; }
    json_free(r);
    return NULL;
}

/* ── compression cluster ─────────────────────────────────────────────────── */

/* PoP: ai_compression_threshold @ agent/agent_init.py:_compression_threshold */
static json_t *ai_compression_threshold(ai_ctx *ctx, json_t *agent, const json_t *cfg) {
    json_t *def_th = json_number(0.50);
    json_t *th_raw = ai_get_def(cfg, "threshold", def_th);
    json_t *threshold = ai_py_float(ctx, th_raw);
    json_free(def_th); /* th_raw may alias def_th: free only after the read */
    if (ai_exc_pending(ctx)) { json_free(threshold); return NULL; }
    json_t *true_node = json_bool(true);
    json_t *autoraise = ai_cfg_flag(ctx, cfg, "codex_gpt55_autoraise", true_node);
    json_t *notice_enabled = ai_cfg_flag(ctx, cfg, "codex_gpt55_autoraise_notice", true_node);
    json_free(true_node);
    json_set(agent, "_compression_threshold_autoraised", json_null());

    /* with suppress(Exception): model override merge */
    bool propagate = false;
    json_t *model = ai_agent_attr(ctx, agent, "model");
    if (ai_exc_pending(ctx)) { propagate = !ai_exc_is_exception(ctx); goto suppress; }
    json_t *provider = ai_agent_attr(ctx, agent, "provider");
    if (ai_exc_pending(ctx)) { propagate = !ai_exc_is_exception(ctx); goto suppress; }
    {
        json_t *a = json_object();
        ai_arg(a, "model", model); ai_arg(a, "provider", provider);
        json_set(a, "allow_codex_gpt55_autoraise", json_copy(autoraise));
        json_t *mct = ai_call(ctx, "_compression_threshold_for_model", a);
        if (ai_exc_pending(ctx)) { json_free(mct); propagate = !ai_exc_is_exception(ctx); goto suppress; }
        /* is_codex_autoraise = (_is_codex_gpt54_or_gpt55(...) or _is_codex_spark(...)) */
        json_t *ca = json_object();
        ai_arg(ca, "model", model); ai_arg(ca, "provider", provider);
        json_t *c54 = ai_call(ctx, "_is_codex_gpt54_or_gpt55", ca);
        if (ai_exc_pending(ctx)) { json_free(mct); json_free(c54); propagate = !ai_exc_is_exception(ctx); goto suppress; }
        bool is_codex = ai_truthy(c54);
        json_free(c54);
        if (!is_codex) {
            json_t *sa = json_object();
            ai_arg(sa, "model", model); ai_arg(sa, "provider", provider);
            json_t *spark = ai_call(ctx, "_is_codex_spark", sa);
            if (ai_exc_pending(ctx)) { json_free(mct); json_free(spark); propagate = !ai_exc_is_exception(ctx); goto suppress; }
            is_codex = ai_truthy(spark);
            json_free(spark);
        }
        json_t *ra = json_object();
        ai_arg(ra, "global_threshold", threshold);
        ai_arg(ra, "model_cthresh", mct);
        ai_arg(ra, "model", model);
        json_set(ra, "is_codex_autoraise", json_bool(is_codex));
        json_free(mct);
        json_t *resolved = ai_call(ctx, "resolve_compression_threshold", ra);
        if (ai_exc_pending(ctx)) { json_free(resolved); propagate = !ai_exc_is_exception(ctx); goto suppress; }
        if (!ai_is_arr(resolved) || resolved->c.count != 2) {
            json_free(resolved);
            ai_raise(ctx, "ValueError", "not enough values to unpack (expected 2, got 0)");
            propagate = false; /* ValueError is an Exception -> suppressed */
            goto suppress;
        }
        json_free(threshold);
        threshold = json_copy(resolved->c.items[0]);
        json_set(agent, "_compression_threshold_autoraised", json_copy(resolved->c.items[1]));
        json_free(resolved);
    }
suppress:
    if (ai_exc_pending(ctx)) {
        if (propagate) { json_free(threshold); json_free(autoraise); json_free(notice_enabled); return NULL; }
        ai_exc_clear(ctx);
    }
    json_t *ret = json_array();
    json_append(ret, threshold);
    json_append(ret, notice_enabled);
    json_free(autoraise);
    return ret;
}

/* PoP: ai_compression_codex_settings @ agent/agent_init.py:_compression_codex_settings */
static json_t *ai_compression_codex_settings(ai_ctx *ctx, const json_t *cfg) {
    json_t *native = json_string("native");
    json_t *v = ai_get_def(cfg, "codex_app_server_auto", native);
    if (!ai_truthy(v)) v = native;
    char *s = ai_py_str(v);
    char *app_server_auto = ai_lower(s ? s : "");
    free(s);
    json_free(native);
    bool valid = ai_str_eq(app_server_auto, "native") || ai_str_eq(app_server_auto, "hermes") ||
                 ai_str_eq(app_server_auto, "off");
    if (!valid) {
        json_t *la = json_array();
        json_append(la, json_string(app_server_auto));
        ai_log(ctx, "ra.", "warning",
               "Invalid compression.codex_app_server_auto=%r; using 'native'. "
               "Valid values are: native, hermes, off.", la, false);
        if (ai_exc_pending(ctx)) { free(app_server_auto); return NULL; }
        free(app_server_auto);
        app_server_auto = strdup("native");
    }
    /* responses_native = is_truthy_value(cfg.get("codex_responses_native", False)) */
    json_t *false_node = json_bool(false);
    json_t *rn_raw = ai_get_def(cfg, "codex_responses_native", false_node);
    json_t *rna = json_object();
    ai_arg(rna, "value", rn_raw);
    json_free(false_node); /* rn_raw may alias false_node: free after last read */
    json_t *responses_native = ai_call(ctx, "is_truthy_value", rna);
    if (ai_exc_pending(ctx)) { free(app_server_auto); json_free(responses_native); return NULL; }
    /* compact_threshold */
    json_t *compact_threshold = json_null();
    json_t *raw = ai_get(cfg, "codex_responses_compact_threshold");
    if (!ai_is_none(raw)) {
        json_t *reject = json_array();
        json_append(reject, json_string("bool"));
        json_append(reject, json_string("float"));
        json_free(compact_threshold);
        compact_threshold = ai_positive_int(ctx, raw, reject);
        json_free(reject);
        if (ai_exc_pending(ctx)) { free(app_server_auto); json_free(compact_threshold); json_free(responses_native); return NULL; }
        if (ai_is_none(compact_threshold)) {
            json_t *la = json_array();
            json_append(la, json_copy(raw));
            ai_log(ctx, "ra.", "warning",
                   "Invalid compression.codex_responses_compact_threshold=%r; "
                   "using the automatic threshold derived from local compression.", la, false);
            if (ai_exc_pending(ctx)) { free(app_server_auto); json_free(compact_threshold); json_free(responses_native); return NULL; }
        }
    }
    json_t *ret = json_array();
    json_append(ret, json_string(app_server_auto));
    json_append(ret, responses_native ? responses_native : json_null());
    json_append(ret, compact_threshold);
    free(app_server_auto);
    return ret;
}


static double ai_max_d(double a, double b) { return a > b ? a : b; }
static double ai_min_d(double a, double b) { return a < b ? a : b; }

/* is_truthy_value(cfg.get(key), default=d) seam helper. */
static json_t *ai_truthy_seam(ai_ctx *ctx, const json_t *cfg, const char *key, bool def) {
    json_t *a = json_object();
    ai_arg(a, "value", ai_get(cfg, key));
    json_set(a, "default", json_bool(def));
    return ai_call(ctx, "is_truthy_value", a);
}

/* PoP: ai_parse_compression_config @ agent/agent_init.py:_parse_compression_config */
static json_t *ai_parse_compression_config(ai_ctx *ctx, json_t *agent, const json_t *agent_cfg) {
    json_t *cfg = ai_cfg_dict(ctx, agent_cfg, "compression");
    json_t *settings = json_object();
    json_t *tr = ai_compression_threshold(ctx, agent, cfg);
    if (ai_exc_pending(ctx)) { json_free(tr); goto fail; }
    json_set(settings, "threshold", json_copy(tr->c.items[0]));
    json_set(settings, "autoraise_notice_enabled", json_copy(tr->c.items[1]));
    json_free(tr);

    json_t *def_tr = json_number(0.20);
    json_t *target_ratio = ai_py_float(ctx, ai_get_def(cfg, "target_ratio", def_tr));
    json_free(def_tr);
    if (ai_exc_pending(ctx)) { json_free(target_ratio); goto fail; }
    json_t *def_pl = json_number(20);
    json_t *protect_last = ai_py_int(ctx, ai_get_def(cfg, "protect_last_n", def_pl));
    json_free(def_pl);
    if (ai_exc_pending(ctx)) { json_free(protect_last); goto fail; }
    json_t *def_ma = json_number(3);
    json_t *max_attempts = ai_parse_config_int(ctx, ai_get_def(cfg, "max_attempts", def_ma), def_ma);
    json_free(def_ma);
    if (max_attempts->num_val < 1) { json_free(max_attempts); max_attempts = json_number(3); }

    /* threshold_tokens default = cfg_get(DEFAULT_CONFIG, "compression", "threshold_tokens") == None */
    json_t *tt_raw = ai_get(cfg, "threshold_tokens");
    json_t *threshold_tokens = json_null();
    if (!ai_is_none(tt_raw)) {
        json_t *no_reject = json_array();
        json_free(threshold_tokens);
        threshold_tokens = ai_positive_int(ctx, tt_raw, no_reject);
        json_free(no_reject);
        if (ai_exc_pending(ctx)) { json_free(threshold_tokens); goto fail; }
    }
    json_t *def_pf = json_number(3);
    json_t *protect_first = ai_py_int(ctx, ai_get_def(cfg, "protect_first_n", def_pf));
    json_free(def_pf);
    if (ai_exc_pending(ctx)) { json_free(protect_first); goto fail; }
    double pf = ai_max_d(0, protect_first->num_val);
    json_free(protect_first);
    protect_first = json_number(pf);

    json_t *checkpoint_required = ai_truthy_seam(ctx, cfg, "checkpoint_required", false);
    if (ai_exc_pending(ctx)) { json_free(checkpoint_required); goto fail; }
    {
        json_t *a = json_object();
        ai_arg(a, "checkpoint_required", checkpoint_required);
        ai_arg(a, "api_mode", ai_get(agent, "api_mode"));
        json_t *r = ai_call(ctx, "_refuse_checkpoint_required_on_codex_app_server", a);
        if (ai_exc_pending(ctx)) { json_free(r); goto fail; }
        json_free(r);
    }
    json_t *codex = ai_compression_codex_settings(ctx, cfg);
    if (ai_exc_pending(ctx)) { json_free(codex); goto fail; }
    json_t *def_idle = json_number(0);
    json_t *idle = ai_py_int(ctx, ai_get_def(cfg, "idle_compact_after_seconds", def_idle));
    json_free(def_idle);
    if (ai_exc_pending(ctx)) { json_free(idle); json_free(codex); goto fail; }
    double idle_d = ai_max_d(0, idle->num_val);
    json_free(idle);

    /* CompressionSettings(...) — kwargs evaluated in source order. */
    json_t *true_node = json_bool(true);
    json_set(settings, "enabled", ai_cfg_flag(ctx, cfg, "enabled", true_node));
    json_free(true_node);
    json_set(settings, "target_ratio", target_ratio);
    json_set(settings, "protect_last", protect_last);
    {
        json_t *def_tm = json_string("lean");
        json_t *tm_raw = ai_get_def(cfg, "tail_mode", def_tm);
        char *s = ai_py_str(tm_raw);
        json_free(def_tm); /* tm_raw may alias def_tm: free only after the read */
        char *st = ai_strip(s ? s : "");
        free(s);
        char *low = ai_lower(st ? st : "");
        free(st);
        json_set(settings, "tail_mode", json_string(low ? low : ""));
        free(low);
    }
    {
        json_t *def_mtu = json_number(1);
        json_t *mtu = ai_parse_config_int(ctx, ai_get_def(cfg, "min_tail_user_messages", def_mtu), def_mtu);
        json_free(def_mtu);
        json_set(settings, "min_tail_users", json_number(ai_max_d(1, mtu->num_val)));
        json_free(mtu);
    }
    json_set(settings, "max_attempts", json_number(ai_min_d(max_attempts->num_val, 10)));
    json_free(max_attempts);
    {
        json_t *def_pp = json_number(0);
        json_t *pp = ai_parse_config_int(ctx, ai_get_def(cfg, "proactive_prune_tokens", def_pp), def_pp);
        json_free(def_pp);
        json_set(settings, "proactive_prune_tokens", json_number(ai_max_d(0, pp->num_val)));
        json_free(pp);
    }
    {
        json_t *def_pc = json_number(8000);
        json_t *pc = ai_parse_config_int(ctx, ai_get_def(cfg, "proactive_prune_min_result_chars", def_pc), def_pc);
        json_free(def_pc);
        json_set(settings, "proactive_prune_min_chars", pc);
    }
    {
        json_t *def_pr = json_number(4096);
        json_t *pr = ai_parse_config_int(ctx, ai_get_def(cfg, "proactive_prune_min_reclaim_tokens", def_pr), def_pr);
        json_free(def_pr);
        json_set(settings, "proactive_prune_min_reclaim", json_number(ai_max_d(0, pr->num_val)));
        json_free(pr);
    }
    json_set(settings, "protect_first", protect_first);
    json_t *false_node = json_bool(false);
    json_set(settings, "abort_on_summary_failure", ai_cfg_flag(ctx, cfg, "abort_on_summary_failure", false_node));
    json_free(false_node);
    {
        json_t *mt = ai_cfg_dict(ctx, cfg, "model_thresholds");
        json_t *out = json_object();
        for (size_t i = 0; i < mt->c.count; i++) {
            json_t *v = mt->c.items[i];
            if (ai_is_num(v) && !ai_is_bool(v))
                json_set(out, mt->c.keys[i], json_copy(v));
        }
        json_free(mt);
        json_set(settings, "model_thresholds", out);
    }
    json_set(settings, "threshold_tokens", threshold_tokens);
    json_set(settings, "checkpoint_required", checkpoint_required);
    json_set(settings, "in_place", ai_truthy_seam(ctx, cfg, "in_place", true));
    if (ai_exc_pending(ctx)) goto fail;
    json_set(settings, "micro_compact", ai_truthy_seam(ctx, cfg, "micro_compact", false));
    if (ai_exc_pending(ctx)) goto fail;
    {
        json_t *def_me = json_number(1);
        json_t *me = ai_parse_config_int(ctx, ai_get_def(cfg, "micro_compact_every_n_turns", def_me), def_me);
        json_free(def_me);
        json_set(settings, "micro_compact_every_n_turns", json_number(ai_max_d(1, me->num_val)));
        json_free(me);
    }
    {
        json_t *def_md = json_number(2000);
        json_t *md = ai_parse_config_int(ctx, ai_get_def(cfg, "micro_compact_defrag_threshold_tokens", def_md), def_md);
        json_free(def_md);
        json_set(settings, "micro_compact_defrag_tokens", json_number(ai_max_d(1, md->num_val)));
        json_free(md);
    }
    json_set(settings, "codex_app_server_auto", json_copy(codex->c.items[0]));
    json_set(settings, "codex_responses_native", json_copy(codex->c.items[1]));
    json_set(settings, "codex_responses_compact_threshold", json_copy(codex->c.items[2]));
    json_free(codex);
    json_set(settings, "idle_compact_after_seconds", json_number(idle_d));
    json_set(settings, "prepare_ahead", ai_truthy_seam(ctx, cfg, "prepare_ahead", false));
    if (ai_exc_pending(ctx)) goto fail;
    json_free(cfg);
    return settings;
fail:
    json_free(settings);
    json_free(cfg);
    return NULL;
}


/* ── custom-provider / route / context-length cluster ────────────────────── */

static const char *AI_RUNTIME_FIRST_PROVIDER_IDS[] = {
    "auto", "moa", "vertex", "google-vertex", "vertex-ai", "gcp-vertex", "vertexai",
};

/* normalize_route_base_url(value) seam -> owned str node (or null). */
static json_t *ai_norm_route(ai_ctx *ctx, const json_t *value) {
    json_t *a = json_object();
    ai_arg(a, "value", value);
    return ai_call(ctx, "normalize_route_base_url", a);
}
/* custom_provider_runtime_ids(value) seam -> owned set array. */
static json_t *ai_runtime_ids(ai_ctx *ctx, const json_t *value) {
    json_t *a = json_object();
    ai_arg(a, "value", value);
    return ai_call(ctx, "custom_provider_runtime_ids", a);
}

/* PoP: ai_custom_provider_configured_base_url @ agent/agent_init.py:_custom_provider_configured_base_url */
static json_t *ai_custom_provider_configured_base_url(ai_ctx *ctx, const json_t *configured_provider,
                                                      const json_t *agent_cfg, const json_t *custom_providers) {
    json_t *wa = json_object();
    ai_arg(wa, "value", configured_provider);
    json_t *wanted = ai_call(ctx, "normalize_custom_provider_name", wa);
    if (ai_exc_pending(ctx)) { json_free(wanted); return NULL; }
    const char *want = ai_jstr(wanted);
    json_t *disabled = json_array();
    json_t *result = NULL;
    json_t *user_providers = ai_get(agent_cfg, "providers");
    if (ai_is_obj(user_providers)) {
        for (size_t i = 0; i < user_providers->c.count && !result; i++) {
            const char *key = user_providers->c.keys[i];
            json_t *entry = user_providers->c.items[i];
            if (!ai_is_obj(entry)) continue;
            json_t *ka = json_object(); ai_arg_str(ka, "value", key);
            json_t *ids_key = ai_call(ctx, "custom_provider_runtime_ids", ka);
            if (ai_exc_pending(ctx)) { json_free(ids_key); goto fail; }
            json_t *na = json_object(); ai_arg(na, "value", ai_get(entry, "name"));
            json_t *ids_name = ai_call(ctx, "custom_provider_runtime_ids", na);
            if (ai_exc_pending(ctx)) { json_free(ids_key); json_free(ids_name); goto fail; }
            json_t *ids = ai_set_union(ids_key, ids_name);
            json_free(ids_key); json_free(ids_name);
            json_t *ea = json_object(); ai_arg(ea, "entry", entry);
            json_t *enabled = ai_call(ctx, "is_provider_enabled", ea);
            if (ai_exc_pending(ctx)) { json_free(ids); json_free(enabled); goto fail; }
            bool en = ai_truthy(enabled);
            json_free(enabled);
            if (!en) {
                json_t *nd = ai_set_union(disabled, ids);
                json_free(disabled); disabled = nd;
                json_free(ids);
                continue;
            }
            if (ai_arr_has_str(ids, want)) {
                json_t *url_raw = ai_get(entry, "api");
                if (!ai_truthy(url_raw)) url_raw = ai_get(entry, "url");
                if (!ai_truthy(url_raw)) url_raw = ai_get(entry, "base_url");
                json_t *url = ai_norm_route(ctx, url_raw);
                if (ai_exc_pending(ctx)) { json_free(ids); json_free(url); goto fail; }
                if (ai_truthy(url)) { result = url; json_free(ids); break; }
                json_free(url);
            }
            json_free(ids);
        }
    }
    if (!result && ai_is_arr(custom_providers)) {
        for (size_t i = 0; i < custom_providers->c.count && !result; i++) {
            json_t *entry = custom_providers->c.items[i];
            if (!ai_is_obj(entry)) continue;
            json_t *key_ids = ai_runtime_ids(ctx, ai_get(entry, "provider_key"));
            if (ai_exc_pending(ctx)) { json_free(key_ids); goto fail; }
            if (ai_sets_intersect(key_ids, disabled)) { json_free(key_ids); continue; }
            json_t *name_ids = ai_runtime_ids(ctx, ai_get(entry, "name"));
            if (ai_exc_pending(ctx)) { json_free(key_ids); json_free(name_ids); goto fail; }
            json_t *ids = ai_set_union(key_ids, name_ids);
            json_free(key_ids); json_free(name_ids);
            if (ai_arr_has_str(ids, want)) {
                json_t *url = ai_norm_route(ctx, ai_get(entry, "base_url"));
                if (ai_exc_pending(ctx)) { json_free(ids); json_free(url); goto fail; }
                if (ai_truthy(url)) { result = url; json_free(ids); break; }
                json_free(url);
            }
            json_free(ids);
        }
    }
    json_free(disabled);
    json_free(wanted);
    return result ? result : json_string("");
fail:
    json_free(disabled);
    json_free(wanted);
    json_free(result);
    return NULL;
}

/* PoP: ai_configured_default_base_url @ agent/agent_init.py:_configured_default_base_url */
static json_t *ai_configured_default_base_url(ai_ctx *ctx, const json_t *agent_cfg,
                                              const json_t *model_cfg, const json_t *custom_providers) {
    json_t *configured_base_url = ai_norm_route(ctx, ai_get(model_cfg, "base_url"));
    if (ai_exc_pending(ctx)) { json_free(configured_base_url); return NULL; }
    json_t *prov_raw = ai_get(model_cfg, "provider");
    char *ps = ai_py_str_or_empty(prov_raw);
    char *configured_provider = ai_strip(ps ? ps : "");
    free(ps);
    json_t *nra = json_object(); ai_arg_str(nra, "value", configured_provider);
    json_t *norm = ai_call(ctx, "normalize_custom_provider_name", nra);
    if (ai_exc_pending(ctx)) { free(configured_provider); json_free(configured_base_url); json_free(norm); return NULL; }
    const char *norms = ai_jstr(norm);
    bool candidate = ai_truthy(norm);
    bool runtime_first = false;
    for (size_t i = 0; i < sizeof(AI_RUNTIME_FIRST_PROVIDER_IDS)/sizeof(AI_RUNTIME_FIRST_PROVIDER_IDS[0]); i++)
        if (ai_str_eq(AI_RUNTIME_FIRST_PROVIDER_IDS[i], norms)) { runtime_first = true; break; }
    if (runtime_first) {
        candidate = false;
    } else if (candidate && !ai_str_eq(norms, "custom") && !ai_startswith(norms, "custom:")) {
        json_t *ra = json_object(); ai_arg_str(ra, "provider", norms);
        json_t *resolved = ai_call(ctx, "resolve_provider", ra);
        if (ai_exc_pending(ctx)) {
            if (ai_exc_is_exception(ctx)) { ai_exc_clear(ctx); json_free(resolved); resolved = NULL; }
            else { free(configured_provider); json_free(configured_base_url); json_free(norm); json_free(resolved); return NULL; }
        } else {
            char *rs = ai_py_str_or_empty(resolved);
            char *rst = ai_strip(rs ? rs : "");
            free(rs);
            char *rlow = ai_lower(rst ? rst : "");
            free(rst);
            candidate = !ai_str_eq(rlow, norms);
            free(rlow);
            json_free(resolved);
        }
    }
    if (!ai_truthy(configured_base_url) && candidate) {
        json_t *cp = ai_custom_provider_configured_base_url(ctx, norm, agent_cfg, custom_providers);
        if (ai_exc_pending(ctx)) { free(configured_provider); json_free(configured_base_url); json_free(norm); json_free(cp); return NULL; }
        json_free(configured_base_url);
        configured_base_url = cp;
    }
    free(configured_provider);
    json_free(norm);
    return configured_base_url;
}

/* PoP: ai_active_route_url @ agent/agent_init.py:_active_route_url */
static json_t *ai_active_route_url(ai_ctx *ctx, json_t *agent, const json_t *base_url) {
    json_t *agent_base = ai_agent_attr(ctx, agent, "base_url");
    if (ai_exc_pending(ctx)) return NULL;
    char *as = ai_py_str_or_empty(agent_base);
    char *active = as ? as : strdup("");
    char *rs = ai_py_str_or_empty(base_url);
    const char *requested = rs ? rs : "";
    /* "?" in _requested.split("#", 1)[0] */
    const char *hash = strchr(requested, '#');
    size_t pre_len = hash ? (size_t)(hash - requested) : strlen(requested);
    bool has_q = false;
    for (size_t i = 0; i < pre_len; i++) if (requested[i] == '?') { has_q = true; break; }
    if (has_q) {
        bool ok = false;
        char *without_query = ai_url_strip_query(ctx, requested, &ok);
        if (ai_exc_pending(ctx)) {
            if (ai_exc_is_typeval(ctx)) { ai_exc_clear(ctx); free(without_query); }
            else { free(active); free(rs); free(without_query); return NULL; }
        } else {
            json_t *wq_node = json_string(without_query ? without_query : "");
            json_t *n1 = ai_norm_route(ctx, wq_node);
            json_free(wq_node);
            if (ai_exc_pending(ctx)) {
                if (ai_exc_is_typeval(ctx)) { ai_exc_clear(ctx); json_free(n1); }
                else { free(active); free(rs); free(without_query); json_free(n1); return NULL; }
            } else {
                json_t *act_node = json_string(active);
                json_t *n2 = ai_norm_route(ctx, act_node);
                json_free(act_node);
                if (ai_exc_pending(ctx)) {
                    if (ai_exc_is_typeval(ctx)) { ai_exc_clear(ctx); json_free(n1); json_free(n2); }
                    else { free(active); free(rs); free(without_query); json_free(n1); json_free(n2); return NULL; }
                } else {
                    if (ai_json_eq(n1, n2)) {
                        free(active);
                        active = strdup(requested);
                    }
                    json_free(n1); json_free(n2);
                }
            }
            free(without_query);
        }
    }
    free(rs);
    json_t *act_node = json_string(active);
    json_t *ret = ai_norm_route(ctx, act_node);
    json_free(act_node);
    free(active);
    if (ai_exc_pending(ctx)) { json_free(ret); return NULL; }
    return ret ? ret : json_string("");
}


/* PoP: ai_scope_context_length_to_default_runtime @ agent/agent_init.py:_scope_context_length_to_default_runtime */
static json_t *ai_scope_context_length_to_default_runtime(ai_ctx *ctx, json_t *agent,
        const json_t *agent_cfg, const json_t *model_cfg, const json_t *custom_providers,
        const json_t *config_context_length, const json_t *base_url) {
    json_t *deflt = ai_get(model_cfg, "default");
    json_t *default_owned = NULL;
    if (ai_is_obj(deflt)) {
        json_t *a = json_object(); ai_arg(a, "default", deflt);
        json_t *split = ai_call(ctx, "split_model_config_default", a);
        if (ai_exc_pending(ctx)) { json_free(split); return NULL; }
        if (!ai_is_arr(split) || split->c.count != 2) {
            json_free(split);
            ai_raise(ctx, "ValueError", "not enough values to unpack (expected 2, got 0)");
            return NULL;
        }
        default_owned = json_copy(split->c.items[0]);
        json_free(split);
        deflt = default_owned;
    }
    char *ds = ai_py_str_or_empty(deflt);
    char *configured_default_model = ai_strip(ds ? ds : "");
    free(ds);
    json_free(default_owned);
    char *configured_runtime = strdup(configured_default_model);
    json_t *agent_model = ai_agent_attr(ctx, agent, "model");
    if (ai_exc_pending(ctx)) { free(configured_default_model); free(configured_runtime); return NULL; }
    char *active_runtime = strdup(ai_jstr(agent_model));
    if (configured_default_model[0] != '\0') {
        /* with suppress(Exception): normalize both models */
        json_t *provider = ai_agent_attr(ctx, agent, "provider");
        if (ai_exc_pending(ctx)) {
            if (ai_exc_is_exception(ctx)) ai_exc_clear(ctx);
            else { free(configured_default_model); free(configured_runtime); free(active_runtime); return NULL; }
        } else {
            json_t *a1 = json_object();
            ai_arg_str(a1, "model", configured_default_model); ai_arg(a1, "provider", provider);
            json_t *n1 = ai_call(ctx, "normalize_model_for_provider", a1);
            if (ai_exc_pending(ctx)) {
                json_free(n1);
                if (ai_exc_is_exception(ctx)) ai_exc_clear(ctx);
                else { free(configured_default_model); free(configured_runtime); free(active_runtime); return NULL; }
            } else {
                free(configured_runtime);
                configured_runtime = strdup(ai_jstr(n1));
                json_free(n1);
                json_t *a2 = json_object();
                ai_arg(a2, "model", agent_model); ai_arg(a2, "provider", provider);
                json_t *n2 = ai_call(ctx, "normalize_model_for_provider", a2);
                if (ai_exc_pending(ctx)) {
                    json_free(n2);
                    if (ai_exc_is_exception(ctx)) ai_exc_clear(ctx);
                    else { free(configured_default_model); free(configured_runtime); free(active_runtime); return NULL; }
                } else {
                    free(active_runtime);
                    active_runtime = strdup(ai_jstr(n2));
                    json_free(n2);
                }
            }
        }
    }
    json_t *configured_base_url = ai_configured_default_base_url(ctx, agent_cfg, model_cfg, custom_providers);
    if (ai_exc_pending(ctx)) { free(configured_default_model); free(configured_runtime); free(active_runtime); json_free(configured_base_url); return NULL; }
    json_t *active_base_url = ai_active_route_url(ctx, agent, base_url);
    if (ai_exc_pending(ctx)) { free(configured_default_model); free(configured_runtime); free(active_runtime); json_free(configured_base_url); json_free(active_base_url); return NULL; }
    json_t *prov_raw = ai_get(model_cfg, "provider");
    char *cps = ai_py_str_or_empty(prov_raw);
    char *configured_provider = ai_strip(cps ? cps : "");
    free(cps);
    json_t *provider = ai_agent_attr(ctx, agent, "provider");
    if (ai_exc_pending(ctx)) { free(configured_default_model); free(configured_runtime); free(active_runtime); free(configured_provider); json_free(configured_base_url); json_free(active_base_url); return NULL; }
    json_t *ca = json_object();
    ai_arg(ca, "configured_base_url", configured_base_url);
    ai_arg(ca, "active_base_url", active_base_url);
    ai_arg_str(ca, "configured_provider", configured_provider);
    ai_arg(ca, "active_provider", provider);
    json_set(ca, "already_normalized", json_bool(true));
    json_t *rm = ai_call(ctx, "context_route_mismatch", ca);
    if (ai_exc_pending(ctx)) { free(configured_default_model); free(configured_runtime); free(active_runtime); free(configured_provider); json_free(configured_base_url); json_free(active_base_url); json_free(rm); return NULL; }
    bool route_mismatch = ai_truthy(rm);
    json_free(rm);
    bool model_mismatch = configured_runtime[0] != '\0' && !ai_str_eq(configured_runtime, active_runtime);
    json_t *ret = NULL;
    if (model_mismatch || route_mismatch) {
        json_t *la = json_array();
        json_append(la, ai_copy_or_null(config_context_length));
        json_append(la, json_string(ai_jstr(agent_model)));
        json_append(la, json_string(ai_truthy(active_base_url) ? ai_jstr(active_base_url) : ai_jstr(provider)));
        json_append(la, json_string(configured_default_model));
        json_append(la, ai_truthy(configured_base_url) ? json_copy(configured_base_url) : ai_copy_or_null(prov_raw));
        ai_log(ctx, "ra.", "debug",
               "Ignoring model.context_length=%s for startup runtime %s at %s "
               "(configured default is %s at %s)", la, false);
        if (ai_exc_pending(ctx)) { free(configured_default_model); free(configured_runtime); free(active_runtime); free(configured_provider); json_free(configured_base_url); json_free(active_base_url); return NULL; }
        ret = json_null();
    } else {
        ret = ai_copy_or_null(config_context_length);
    }
    free(configured_default_model); free(configured_runtime); free(active_runtime); free(configured_provider);
    json_free(configured_base_url); json_free(active_base_url);
    return ret;
}

/* PoP: ai_set_config_context_length @ agent/agent_init.py:set_config_context_length */
static json_t *ai_set_config_context_length(ai_ctx *ctx, json_t *agent, const json_t *value) {
    (void)ctx;
    json_set(agent, "_config_context_length", ai_copy_or_null(value));
    json_t *compressor = ai_get(agent, "context_compressor"); /* getattr default None */
    if (!ai_is_none(compressor) && ai_is_obj(compressor))
        json_set(compressor, "_config_context_length", ai_copy_or_null(value));
    return NULL;
}

/* PoP: ai_config_context_length_for_runtime @ agent/agent_init.py:config_context_length_for_runtime */
static json_t *ai_config_context_length_for_runtime(ai_ctx *ctx, json_t *agent, const json_t *config) {
    json_t *agent_cfg = NULL;
    bool own_cfg = false;
    json_t *ret = NULL;
    if (ai_is_obj(config)) {
        agent_cfg = (json_t *)config; /* borrowed */
    } else {
        agent_cfg = ai_call(ctx, "load_config", json_object());
        own_cfg = true;
        if (ai_exc_pending(ctx)) goto caught;
    }
    if (!ai_is_obj(agent_cfg)) { ret = json_null(); goto done; }
    json_t *empty = json_object();
    json_t *model_section = ai_get_def(agent_cfg, "model", empty);
    json_free(empty);
    if (!ai_is_obj(model_section)) { ret = json_null(); goto done; }
    json_t *pin = ai_get(model_section, "context_length");
    if (ai_is_none(pin) || ai_is_bool(pin)) { ret = json_null(); goto done; }
    json_t *pin_int = ai_py_int(ctx, pin);
    if (ai_exc_pending(ctx)) { json_free(pin_int); goto caught; }
    if (pin_int->num_val <= 0) { json_free(pin_int); ret = json_null(); goto done; }
    json_t *cpa = json_object(); ai_arg(cpa, "cfg", agent_cfg);
    json_t *cps = ai_call(ctx, "get_compatible_custom_providers", cpa);
    if (ai_exc_pending(ctx)) { json_free(pin_int); json_free(cps); goto caught; }
    json_t *agent_base = ai_get(agent, "base_url"); /* getattr default "" */
    char *bs = ai_py_str_or_empty(agent_base);
    json_t *base_node = json_string(bs ? bs : "");
    free(bs);
    ret = ai_scope_context_length_to_default_runtime(ctx, agent, agent_cfg, model_section, cps, pin_int, base_node);
    json_free(base_node);
    json_free(pin_int);
    json_free(cps);
    if (ai_exc_pending(ctx)) goto caught;
    goto done;
caught:
    if (ai_exc_pending(ctx)) {
        if (ai_exc_is_exception(ctx)) {
            ai_exc_clear(ctx);
            ai_log(ctx, "", "debug",
                   "Could not re-read model.context_length for the current runtime", json_array(), true);
            if (ai_exc_pending(ctx)) { ret = NULL; goto done; }
            ret = json_null();
        } else {
            ret = NULL; /* BaseException propagates */
        }
    }
done:
    if (own_cfg) json_free(agent_cfg);
    return ret;
}

/* ── context engine selector + compressor helpers ────────────────────────── */

/* PoP: ai_select_context_engine @ agent/agent_init.py:_select_context_engine */
static json_t *ai_select_context_engine(ai_ctx *ctx, const json_t *agent_cfg) {
    json_t *sw = ai_call(ctx, "safe_worker_enabled", json_object());
    if (ai_exc_pending(ctx)) { json_free(sw); return NULL; }
    bool safe = ai_truthy(sw);
    json_free(sw);
    if (safe) return json_null();
    char *engine_name = strdup("compressor");
    /* with suppress(Exception): _ctx_cfg.get("engine", "compressor") or "compressor" */
    {
        json_t *ctx_cfg = NULL;
        json_t *empty = json_object();
        if (ai_is_obj(agent_cfg)) ctx_cfg = ai_get_def(agent_cfg, "context", empty);
        else ctx_cfg = empty;
        bool raised = false;
        if (!ai_is_obj(ctx_cfg)) raised = true;
        if (!raised) {
            json_t *def = json_string("compressor");
            json_t *eng = ai_get_def(ctx_cfg, "engine", def);
            json_free(def);
            if (ai_truthy(eng)) {
                free(engine_name);
                engine_name = strdup(ai_jstr(eng));
            }
        }
        json_free(empty);
        (void)raised; /* AttributeError suppressed -> keep "compressor" */
    }
    if (ai_str_eq(engine_name, "compressor")) { free(engine_name); return json_null(); }
    json_t *selected = NULL;
    bool copy_failed = false;
    {
        json_t *a = json_object(); ai_arg_str(a, "name", engine_name);
        selected = ai_call(ctx, "load_context_engine", a);
        if (ai_exc_pending(ctx)) {
            if (ai_exc_is_exception(ctx)) {
                json_t *ea = ai_exc_arg(ctx);
                ai_exc_clear(ctx);
                json_t *la = json_array(); json_append(la, ea);
                ai_log(ctx, "ra.", "debug", "Context engine load from plugins/context_engine/: %s", la, false);
                if (ai_exc_pending(ctx)) { json_free(selected); free(engine_name); return NULL; }
                json_free(selected); selected = NULL;
            } else { json_free(selected); free(engine_name); return NULL; }
        }
    }
    if (ai_is_none(selected)) {
        json_t *candidate = ai_call(ctx, "get_plugin_context_engine", json_object());
        if (ai_exc_pending(ctx)) {
            if (ai_exc_is_exception(ctx)) { ai_exc_clear(ctx); json_free(candidate); candidate = NULL; }
            else { json_free(candidate); json_free(selected); free(engine_name); return NULL; }
        }
        if (!ai_is_none(candidate)) {
            json_t *cname = ai_get(candidate, "name");
            if (!cname) {
                ai_raise(ctx, "AttributeError", "'types.SimpleNamespace' object has no attribute 'name'");
                json_free(candidate); json_free(selected); free(engine_name);
                return NULL;
            }
            if (ai_is_str(cname) && ai_str_eq(cname->str_val, engine_name)) {
                json_t *cl = ai_call(ctx, "engine.clone_for_agent", json_object());
                if (ai_exc_pending(ctx)) {
                    if (ai_exc_is_exception(ctx)) {
                        copy_failed = true;
                        json_t *ea = ai_exc_arg(ctx);
                        ai_exc_clear(ctx);
                        json_t *la = json_array();
                        json_append(la, json_string(engine_name));
                        json_append(la, ea);
                        ai_log(ctx, "ra.", "warning",
                               "Context engine '%s' could not be safely copied for this "
                               "agent (%s) \xe2\x80\x94 falling back to built-in compressor. Plugin "
                               "engines that hold uncopyable state (locks, DB connections) "
                               "should override clone_for_agent() (or __deepcopy__) to copy "
                               "only mutable budget state.", la, false);
                        if (ai_exc_pending(ctx)) { json_free(cl); json_free(candidate); json_free(selected); free(engine_name); return NULL; }
                        json_free(cl); cl = NULL;
                    } else { json_free(cl); json_free(candidate); json_free(selected); free(engine_name); return NULL; }
                }
                json_free(selected);
                selected = cl;
            }
        }
        json_free(candidate);
    }
    if (ai_is_none(selected) && !copy_failed) {
        json_t *la = json_array(); json_append(la, json_string(engine_name));
        ai_log(ctx, "ra.", "warning",
               "Context engine '%s' not found \xe2\x80\x94 falling back to built-in compressor", la, false);
        if (ai_exc_pending(ctx)) { json_free(selected); free(engine_name); return NULL; }
    }
    free(engine_name);
    return selected ? selected : json_null();
}

/* PoP: ai_compressor_max_tokens @ agent/agent_init.py:_compressor_max_tokens */
static json_t *ai_compressor_max_tokens(ai_ctx *ctx, json_t *agent) {
    json_t *max_tokens = ai_agent_attr(ctx, agent, "max_tokens");
    if (ai_exc_pending(ctx)) return NULL;
    if (!ai_is_none(max_tokens)) return json_copy(max_tokens);
    /* with suppress(Exception): gemini default */
    json_t *provider = ai_agent_attr(ctx, agent, "provider");
    if (ai_exc_pending(ctx)) {
        if (ai_exc_is_exception(ctx)) { ai_exc_clear(ctx); return json_null(); }
        return NULL;
    }
    char *ps = ai_py_str_or_empty(provider);
    char *pst = ai_strip(ps ? ps : ""); free(ps);
    char *plow = ai_lower(pst ? pst : ""); free(pst);
    bool gemini_provider = ai_str_eq(plow, "gemini") || ai_str_eq(plow, "google") ||
                           ai_str_eq(plow, "google-gemini") || ai_str_eq(plow, "google-ai-studio");
    free(plow);
    bool native = false;
    if (!gemini_provider) {
        json_t *base_url = ai_agent_attr(ctx, agent, "base_url");
        if (ai_exc_pending(ctx)) {
            if (ai_exc_is_exception(ctx)) { ai_exc_clear(ctx); return json_null(); }
            return NULL;
        }
        json_t *a = json_object(); ai_arg(a, "base_url", base_url);
        json_t *r = ai_call(ctx, "is_native_gemini_base_url", a);
        if (ai_exc_pending(ctx)) {
            json_free(r);
            if (ai_exc_is_exception(ctx)) { ai_exc_clear(ctx); return json_null(); }
            return NULL;
        }
        native = ai_truthy(r);
        json_free(r);
    }
    if (gemini_provider || native) return json_number(65535);
    return json_null();
}

/* thousands-separated integer formatting ({_ctx:,}). Caller frees. */
static char *ai_format_comma(double d) {
    char raw[64];
    snprintf(raw, sizeof raw, "%.0f", d);
    bool neg = raw[0] == '-';
    const char *digits = neg ? raw + 1 : raw;
    size_t n = strlen(digits);
    ai_sb sb; ai_sb_init(&sb);
    if (neg) ai_sb_putc(&sb, '-');
    for (size_t i = 0; i < n; i++) {
        if (i > 0 && (n - i) % 3 == 0) ai_sb_putc(&sb, ',');
        ai_sb_putc(&sb, digits[i]);
    }
    return sb.p ? sb.p : strdup("");
}

/* PoP: ai_enforce_minimum_context @ agent/agent_init.py:_enforce_minimum_context */
static json_t *ai_enforce_minimum_context(ai_ctx *ctx, json_t *agent) {
    const double MIN_CTX = 64000;
    json_t *compressor = ai_agent_attr(ctx, agent, "context_compressor");
    if (ai_exc_pending(ctx)) return NULL;
    json_t *zero = json_number(0);
    json_t *ctx_node = ai_get_def(compressor, "context_length", zero);
    json_free(zero);
    double ctx_val = ai_num(ctx_node);
    json_t *ollama = ai_agent_attr(ctx, agent, "_ollama_num_ctx");
    if (ai_exc_pending(ctx)) return NULL;
    json_t *base_url = ai_agent_attr(ctx, agent, "base_url");
    if (ai_exc_pending(ctx)) return NULL;
    json_t *provider = ai_agent_attr(ctx, agent, "provider");
    if (ai_exc_pending(ctx)) return NULL;
    if (ai_truthy(ollama)) {
        json_t *a = json_object();
        ai_arg(a, "base_url", base_url); ai_arg(a, "provider", provider);
        json_t *ls = ai_call(ctx, "is_local_server", a);
        if (ai_exc_pending(ctx)) { json_free(ls); return NULL; }
        if (ai_truthy(ls)) {
            double base = ai_truthy(ctx_node) ? ctx_val : 0;
            ctx_val = ai_max_d(base, ai_num(ollama));
        }
        json_free(ls);
    }
    char *ps = ai_py_str_or_empty(provider);
    char *pst = ai_strip(ps ? ps : ""); free(ps);
    char *plow = ai_lower(pst ? pst : ""); free(pst);
    bool is_lmstudio = ai_str_eq(plow, "lmstudio");
    free(plow);
    json_t *ccl = ai_agent_attr(ctx, agent, "_config_context_length");
    if (ai_exc_pending(ctx)) return NULL;
    bool allow_lmstudio = is_lmstudio && ai_is_num(ccl) && !ai_is_bool(ccl) && ccl->num_val > 0;
    if (ctx_val != 0 && ctx_val < MIN_CTX && !allow_lmstudio) {
        json_t *a = json_object();
        ai_arg(a, "base_url", base_url); ai_arg(a, "provider", provider);
        json_t *ls = ai_call(ctx, "is_local_server", a);
        if (ai_exc_pending(ctx)) { json_free(ls); return NULL; }
        bool local = ai_truthy(ls);
        json_free(ls);
        char *ctx_str = ai_format_comma(ctx_val);
        char *min_str = ai_format_comma(MIN_CTX);
        ai_sb msg; ai_sb_init(&msg);
        ai_sb_put(&msg, "Model "); ai_sb_put(&msg, ai_jstr(ai_agent_attr(ctx, agent, "model")));
        if (ai_exc_pending(ctx)) { free(ctx_str); free(min_str); free(msg.p); return NULL; }
        ai_sb_put(&msg, " has a context window of "); ai_sb_put(&msg, ctx_str);
        ai_sb_put(&msg, " tokens, which is below the minimum "); ai_sb_put(&msg, min_str);
        ai_sb_put(&msg, " required by Hermes Agent.  ");
        if (local) {
            ai_sb_put(&msg, "Your local server is serving a "); ai_sb_put(&msg, ctx_str);
            ai_sb_put(&msg, "-token window.  Start it with at least 64K context (llama.cpp: -c 64000; "
                            "vLLM: --max-model-len; Ollama: OLLAMA_CONTEXT_LENGTH=64000 or a Modelfile num_ctx), "
                            "or set model.ollama_num_ctx in config.yaml to the window it really serves "
                            "(at least 64K).");
        } else {
            ai_sb_put(&msg, "Choose a model with at least 64K context.  If your server "
                            "reports a window smaller than the model's true window, set "
                            "model.context_length in config.yaml to the real value "
                            "(this must be at least 64K).");
        }
        ai_raise(ctx, "ValueError", msg.p ? msg.p : "");
        free(ctx_str); free(min_str); free(msg.p);
        return NULL;
    }
    return NULL;
}

/* PoP: ai_warn_nonagentic_hermes_model @ agent/agent_init.py:_warn_nonagentic_hermes_model */
static json_t *ai_warn_nonagentic_hermes_model(ai_ctx *ctx, json_t *agent) {
    json_t *quiet = ai_agent_attr(ctx, agent, "quiet_mode");
    if (ai_exc_pending(ctx)) return NULL;
    json_t *platform = ai_agent_attr(ctx, agent, "platform");
    if (ai_exc_pending(ctx)) return NULL;
    const char *plat = ai_truthy(platform) ? ai_jstr(platform) : "cli";
    if (ai_truthy(quiet) || ai_str_eq(plat, "cli")) return NULL;
    /* with suppress(Exception) */
    json_t *model = ai_agent_attr(ctx, agent, "model");
    if (ai_exc_pending(ctx)) {
        if (ai_exc_is_exception(ctx)) { ai_exc_clear(ctx); return NULL; }
        return NULL;
    }
    json_t *ma = json_object();
    ai_arg_str(ma, "model", ai_truthy(model) ? ai_jstr(model) : "");
    json_t *warn = ai_call(ctx, "_check_hermes_model_warning", ma);
    if (ai_exc_pending(ctx)) {
        json_free(warn);
        if (ai_exc_is_exception(ctx)) { ai_exc_clear(ctx); return NULL; }
        return NULL;
    }
    if (ai_truthy(warn)) {
        if (!ai_has_method(ctx, "_emit_warning")) {
            json_free(warn);
            ai_raise(ctx, "AttributeError", "'types.SimpleNamespace' object has no attribute '_emit_warning'");
            ai_exc_clear(ctx); /* suppressed */
            return NULL;
        }
        static const char *USER_MSG =
            "\xe2\x9a\xa0 Nous Research Hermes 3 & 4 models are NOT agentic \xe2\x80\x94 they "
            "lack reliable tool-calling for agent workflows (delegation, "
            "cron, proactive tools). Consider an agentic model instead "
            "(Claude, GPT, Gemini, Qwen-Coder, etc.).";
        json_t *ea = json_object(); ai_arg_str(ea, "msg", USER_MSG);
        json_t *er = ai_call(ctx, "agent._emit_warning", ea);
        if (ai_exc_pending(ctx)) {
            json_free(er); json_free(warn);
            if (ai_exc_is_exception(ctx)) { ai_exc_clear(ctx); return NULL; }
            return NULL;
        }
        json_free(er);
        /* _ra().logger.warning(_hermes_warn) — the warn text IS the msg, no args. */
        ai_log(ctx, "ra.", "warning", ai_jstr(warn), json_array(), false);
        if (ai_exc_pending(ctx)) { json_free(warn); return NULL; }
    }
    json_free(warn);
    return NULL;
}
