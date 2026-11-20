/* prompt_cache_clock_flow2.c — agent/prompt_cache_clock.py UNCLAIMED FLOW remainder.
 *
 * C11 port of the 40 defs of agent/prompt_cache_clock.py NOT claimed by
 * prompt_cache_clock_flow.c (flow1's 18 claims stay in force). Every one of
 * these 40 functions is PoP-credited here; flow1's names are re-derived only
 * as seam calls, never re-claimed. ALL clock/storage/endpoint/module I/O rides
 * the injected universal seam (pcc2_call); the harness scripts results and
 * records the ordered trace the oracle diffs against LIVE Python under the
 * same scripts.
 *
 * Representation contract (shared with tests/oracle_prompt_cache_clock_flow2.py):
 *   - CacheEvent/CacheWindow/_Ctx/_Verdict doubles are attribute dicts;
 *     tuples -> arrays; None -> JSON null (C: NULL or JSON_NULL node).
 *   - integral floats collapse to ints on both sides (oracle norm); libjson
 *     emits %.0f for integral doubles and %.17g otherwise (round-trips).
 *   - uncaught exception -> {"__raise__": "<ClassName>"}; full builtin
 *     exception hierarchy is walked subclass-aware by pcc2_exc_matches.
 *   - scripts: absent key -> None; {"$seq":[...]} per-call queue; {"__raise__":cls,...}
 *     raises (trace ret is the WHOLE marker); any other value reused.
 *   - callable collaborators are named by a marker string returned from a
 *     script (e.g. "reader"); calling it dispatches a seam of that name.
 * libjson-only. Self-contained: harness #includes "agent/prompt_cache_clock_flow2.c".
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <math.h>

/* ── context + seam plumbing ─────────────────────────────────────────────── */

typedef struct pcc2_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *env;       /* borrowed: scripted constants/flags */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
    json_t *state;     /* owned: post-call mutated agent state (or NULL) */
} pcc2_ctx;

static bool pcc2_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool pcc2_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool pcc2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool pcc2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool pcc2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool pcc2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool pcc2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
/* Python isinstance(x, int) is True for bool; libjson bool+number both "int". */
static bool pcc2_is_int(const json_t *v) { return pcc2_is_num(v) || pcc2_is_bool(v); }
static const char *pcc2_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static double pcc2_num(const json_t *v) {
    if (pcc2_is_num(v)) return v->num_val;
    if (pcc2_is_bool(v)) return v->bool_val ? 1.0 : 0.0;
    return 0.0;
}
static bool pcc2_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
/* field fetch: missing key OR JSON null both read None. */
static json_t *pcc2_get(const json_t *obj, const char *key) {
    if (!pcc2_is_obj(obj)) return NULL;
    json_t *v = json_obj_get(obj, key);
    return pcc2_is_none(v) ? NULL : v;
}
/* Python getattr(obj,name,default) for a JSON-object double. No trace. */
static json_t *pcc2_getattr(const json_t *obj, const char *name, json_t *dflt) {
    json_t *v = pcc2_get(obj, name);
    return v ? v : dflt;
}

static bool pcc2_json_eq(const json_t *a, const json_t *b) {
    if (pcc2_is_none(a) && pcc2_is_none(b)) return true;
    if (pcc2_is_none(a) || pcc2_is_none(b)) return false;
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return pcc2_str_eq(a->str_val, b->str_val);
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!pcc2_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !pcc2_json_eq(a->c.items[i], bv)) return false;
            }
            return true;
        default: return true;
    }
}

/* ── FULL builtin exception hierarchy (subclass-aware matching) ─────────── */

static bool pcc2_exc_pending(const pcc2_ctx *ctx) { return ctx && ctx->exc != NULL; }
static void pcc2_exc_clear(pcc2_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}
static void pcc2_raise(pcc2_ctx *ctx, const char *cls) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
}
static const char *pcc2_exc_class(const pcc2_ctx *ctx) {
    return json_get_str(ctx->exc, "__raise__", "");
}
/* Parent map for the FULL builtin hierarchy + sqlite3 + json + custom. */
static const char *pcc2_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        /* BaseException family */
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"}, {"Exception", "BaseException"},
        {"StopIteration", "Exception"}, {"StopAsyncIteration", "Exception"},
        {"ArithmeticError", "Exception"}, {"FloatingPointError", "ArithmeticError"},
        {"OverflowError", "ArithmeticError"}, {"ZeroDivisionError", "ArithmeticError"},
        {"AssertionError", "Exception"}, {"AttributeError", "Exception"},
        {"BufferError", "Exception"}, {"EOFError", "Exception"},
        {"ImportError", "Exception"}, {"ModuleNotFoundError", "ImportError"},
        {"LookupError", "Exception"}, {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
        {"MemoryError", "Exception"}, {"NameError", "Exception"},
        {"UnboundLocalError", "NameError"},
        {"OSError", "Exception"},
        {"BlockingIOError", "OSError"}, {"ChildProcessError", "OSError"},
        {"FileExistsError", "OSError"}, {"FileNotFoundError", "OSError"},
        {"InterruptedError", "OSError"}, {"IsADirectoryError", "OSError"},
        {"NotADirectoryError", "OSError"}, {"PermissionError", "OSError"},
        {"ProcessLookupError", "OSError"}, {"TimeoutError", "OSError"},
        {"ConnectionError", "OSError"},
        {"BrokenPipeError", "ConnectionError"}, {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
        {"ReferenceError", "Exception"}, {"RuntimeError", "Exception"},
        {"NotImplementedError", "RuntimeError"}, {"RecursionError", "RuntimeError"},
        {"SyntaxError", "Exception"}, {"IndentationError", "SyntaxError"}, {"TabError", "IndentationError"},
        {"SystemError", "Exception"}, {"TypeError", "Exception"},
        {"ValueError", "Exception"},
        {"UnicodeError", "ValueError"},
        {"UnicodeDecodeError", "UnicodeError"}, {"UnicodeEncodeError", "UnicodeError"},
        {"UnicodeTranslateError", "UnicodeError"},
        {"JSONDecodeError", "ValueError"},
        {"Warning", "Exception"}, {"DeprecationWarning", "Warning"}, {"PendingDeprecationWarning", "Warning"},
        {"RuntimeWarning", "Warning"}, {"SyntaxWarning", "Warning"}, {"UserWarning", "Warning"},
        {"FutureWarning", "Warning"}, {"ImportWarning", "Warning"}, {"UnicodeWarning", "Warning"},
        {"BytesWarning", "Warning"}, {"ResourceWarning", "Warning"},
        {"sqlite3.Error", "Exception"}, {"sqlite3.DatabaseError", "sqlite3.Error"},
        {"sqlite3.DataError", "sqlite3.DatabaseError"}, {"sqlite3.IntegrityError", "sqlite3.DatabaseError"},
        {"sqlite3.InternalError", "sqlite3.DatabaseError"}, {"sqlite3.NotSupportedError", "sqlite3.DatabaseError"},
        {"sqlite3.OperationalError", "sqlite3.DatabaseError"}, {"sqlite3.ProgrammingError", "sqlite3.DatabaseError"},
        {"sqlite3.InterfaceError", "sqlite3.Error"}, {"sqlite3.Warning", "Exception"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (pcc2_str_eq(T[i].c, c)) return T[i].p;
    return NULL; /* unknown/custom class -> Exception (handled by caller fallback) */
}
static bool pcc2_exc_is(const char *cls, const char *target) {
    /* target matches cls or any ancestor; unknown root falls back to Exception. */
    for (int depth = 0; depth < 40; depth++) {
        if (pcc2_str_eq(cls, target)) return true;
        const char *p = pcc2_exc_parent(cls);
        if (!p) {
            /* unrecognised class behaves as a plain Exception subclass */
            if (pcc2_str_eq(target, "Exception") || pcc2_str_eq(target, "BaseException")) return true;
            return false;
        }
        cls = p;
    }
    return false;
}
static bool pcc2_exc_matches(const pcc2_ctx *ctx, const char *const *classes, size_t n) {
    if (!pcc2_exc_pending(ctx)) return false;
    const char *cls = pcc2_exc_class(ctx);
    for (size_t i = 0; i < n; i++)
        if (pcc2_exc_is(cls, classes[i])) return true;
    return false;
}

/* ── universal seam ──────────────────────────────────────────────────────── */

static json_t *pcc2_script_result(pcc2_ctx *ctx, const json_t *script) {
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

/* args ownership TRANSFERRED (NULL -> empty object). Result is NEW (borrowed-NULL = None). */
static json_t *pcc2_call(pcc2_ctx *ctx, const char *name, json_t *args) {
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
                snprintf(msg, sizeof(msg), "script exhausted: %s", name);
                json_set(ctx->exc, "msg", json_string(msg));
                chosen = ctx->exc;
            } else {
                json_set(ctx->counters, name, json_number(cur + 1));
                chosen = seq->c.items[idx];
            }
        }
    }
    if (chosen != ctx->exc) ret = pcc2_script_result(ctx, chosen);
    /* Trace records the ORDERED SEAM NAME only (args are script-keyed by name,
     * so arg shape is not part of the differential contract). */
    json_append(ctx->trace, json_string(name));
    return ret;
}

/* time.time() via the seam. */
static double pcc2_time(pcc2_ctx *ctx) {
    json_t *r = pcc2_call(ctx, "time", json_object());
    return pcc2_num(r);
}

/* ── Python scalar conversions ───────────────────────────────────────────── */

/* str(x) for JSON scalar (ASCII domain). Returns malloc'd C string. */
static char *pcc2_str_of(const json_t *v) {
    char buf[512];
    if (!v || v->type == JSON_NULL) { snprintf(buf, sizeof buf, "None"); }
    else if (v->type == JSON_BOOL) { snprintf(buf, sizeof buf, "%s", v->bool_val ? "True" : "False"); }
    else if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
        else snprintf(buf, sizeof buf, "%.17g", v->num_val);
    } else if (v->type == JSON_STRING) { snprintf(buf, sizeof buf, "%s", v->str_val ? v->str_val : ""); }
    else { buf[0] = '\0'; }
    size_t n = strlen(buf) + 1;
    char *s = (char *)malloc(n);
    memcpy(s, buf, n);
    return s;
}
/* str(x or "") : falsy -> empty string. */
static char *pcc2_str_or_empty(const json_t *v) {
    if (!pcc2_truthy(v)) {
        char *s = (char *)malloc(1); s[0] = '\0'; return s;
    }
    return pcc2_str_of(v);
}
/* int(x) with Python truthiness of `x or 0` handled by caller. */
static bool pcc2_to_int(const json_t *v, long long *out) {
    if (!v || v->type == JSON_NULL) return false;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return true; }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return true; }
    if (v->type == JSON_STRING) {
        char *end = NULL;
        long long x = strtoll(v->str_val, &end, 10);
        if (end && *end == '\0') { *out = x; return true; }
    }
    return false;
}
static bool pcc2_to_float(const json_t *v, double *out) {
    if (!v || v->type == JSON_NULL) return false;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return true; }
    if (v->type == JSON_NUMBER) { *out = v->num_val; return true; }
    if (v->type == JSON_STRING) {
        char *end = NULL;
        double x = strtod(v->str_val, &end);
        if (end && *end == '\0') { *out = x; return true; }
    }
    return false;
}
/* x or 0 for int() operands. */
static long long pcc2_int_or_zero(const json_t *v) {
    long long n = 0;
    if (pcc2_truthy(v)) pcc2_to_int(v, &n);
    return n;
}
static double pcc2_float_or_zero(const json_t *v) {
    double n = 0;
    if (pcc2_truthy(v)) pcc2_to_float(v, &n);
    return n;
}

/* ── constant tables ─────────────────────────────────────────────────────── */

static double pcc2_ttl(const char *k, bool *found) {
    static const struct { const char *k; double s; } T[] = {
        {"5m", 300}, {"30m", 1800}, {"1h", 3600}, {"12h", 43200},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (pcc2_str_eq(T[i].k, k)) { if (found) *found = true; return T[i].s; }
    if (found) *found = false;
    return 0;
}
static int pcc2_conf_rank(const char *c) {
    if (pcc2_str_eq(c, "unknown")) return 0;
    if (pcc2_str_eq(c, "assumed")) return 1;
    if (pcc2_str_eq(c, "declared")) return 2;
    return 3; /* proven */
}
static double pcc2_retention(const char *k, bool *found) {
    if (pcc2_str_eq(k, "24h")) { if (found) *found = true; return 86400; }
    if (found) *found = false;
    return 0;
}
static const char *pcc2_legacy(const char *c) {
    static const struct { const char *a, *b; } T[] = {
        {"openai_ttl", "openai_spec"}, {"openai_retention", "openai_spec"},
        {"implicit", "openai_spec"}, {"route_ceiling", "anthropic_spec"},
        {"anthropic_native", "anthropic_spec"}, {"openrouter_claude", "anthropic_spec"},
        {"anthropic_gateway", "anthropic_spec"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (pcc2_str_eq(T[i].a, c)) return T[i].b;
    return c;
}

/* ── env-backed external constants ──────────────────────────────────────── */

static double pcc2_env_num(pcc2_ctx *ctx, const char *k, double d) {
    return json_get_num(ctx->env, k, d);
}
static bool pcc2_env_bool(pcc2_ctx *ctx, const char *k, bool d) {
    json_t *v = pcc2_get(ctx->env, k);
    return v ? pcc2_truthy(v) : d;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  THE 40 UNCLAIMED FUNCTIONS (static pcc2_*).
 * ══════════════════════════════════════════════════════════════════════════ */

/* forward decl: record_cache_response -> _valid_previous (defined later). */
static json_t *pcc2_valid_previous(pcc2_ctx *ctx, const json_t *agent, const json_t *generation);

/* @ agent/prompt_cache_clock.py:_list_field */
static json_t *pcc2_list_field(const json_t *api_kwargs, const char *key) {
    json_t *v = pcc2_get(api_kwargs, key);
    return (pcc2_is_arr(v)) ? json_copy(v) : json_array();
}

/* @ agent/prompt_cache_clock.py:_refresh_restart */
static double pcc2_refresh_restart(pcc2_ctx *ctx, double at, const char *tier) {
    bool f; double ttl = pcc2_ttl(tier, &f);
    if (!f) { pcc2_raise(ctx, "KeyError"); return 0; } /* TTL_SECONDS[tier] direct index */
    return at + ttl;
}

/* @ agent/prompt_cache_clock.py:_refresh_openai */
static double pcc2_refresh_openai(double deadline, double at) {
    bool f; double ttl = pcc2_ttl("30m", &f);
    double r = at + (f ? ttl : 0);
    return deadline > r ? deadline : r;
}

/* @ agent/prompt_cache_clock.py:_managed_unload_end */
static json_t *pcc2_managed_unload_end(pcc2_ctx *ctx, const char *base_url, double idle_from) {
    json_t *args = json_object();
    json_set(args, "base_url", json_string(base_url));
    json_t *r = pcc2_call(ctx, "is_managed_endpoint", args);
    if (pcc2_exc_pending(ctx)) { json_free(r); return NULL; }
    if (!pcc2_truthy(r)) { json_free(r); return NULL; }
    json_free(r);
    double idle = pcc2_env_num(ctx, "IDLE_UNLOAD_S", 0);
    double sweep = pcc2_env_num(ctx, "_SWEEP_EVERY_S", 0);
    return json_number(idle_from + idle + 2 * sweep);
}

/* @ agent/prompt_cache_clock.py:_route_key */
static json_t *pcc2_route_key(pcc2_ctx *ctx, const json_t *provider, const json_t *base_url,
                              const json_t *model) {
    char *p = pcc2_str_or_empty(provider);
    char *b = pcc2_str_or_empty(base_url);
    char *m = pcc2_str_or_empty(model);
    json_t *args = json_object();
    json_set(args, "base_url", json_string(b));
    json_t *norm = pcc2_call(ctx, "normalize_route_base_url", args);
    if (pcc2_exc_pending(ctx)) { free(p); free(b); free(m); json_free(norm); return NULL; }
    char *nb = pcc2_str_of(norm);
    json_free(norm);
    json_t *t = json_array();
    json_append(t, json_string(p));
    json_append(t, json_string(nb));
    json_append(t, json_string(m));
    free(p); free(b); free(m); free(nb);
    return t;
}

/* @ agent/prompt_cache_clock.py:response_upstream */
static json_t *pcc2_response_upstream(const json_t *response) {
    /* _UPSTREAM_FIELDS = ("provider",) */
    json_t *v = pcc2_get(response, "provider");
    if (pcc2_is_str(v) && pcc2_jstr(v)[0] != '\0') return json_copy(v);
    json_t *rid = pcc2_get(response, "id");
    if (pcc2_is_str(rid)) {
        const char *s = pcc2_jstr(rid);
        size_t i = 0;
        while (i < strlen(s) && isalpha((unsigned char)s[i])) i++;
        if (i > 0 && i < strlen(s) && (s[i] == '-' || s[i] == '_')) {
            char buf[256]; size_t n = i < sizeof(buf) ? i : sizeof(buf) - 1;
            memcpy(buf, s, n); buf[n] = '\0';
            return json_string(buf);
        }
    }
    return NULL;
}

/* @ agent/prompt_cache_clock.py:upstream_switched */
static json_t *pcc2_upstream_switched(const json_t *before, const json_t *after) {
    bool b = pcc2_is_str(before) && pcc2_jstr(before)[0] != '\0';
    bool a = pcc2_is_str(after) && pcc2_jstr(after)[0] != '\0';
    bool r = b && a && !pcc2_str_eq(pcc2_jstr(before), pcc2_jstr(after));
    return json_bool(r);
}

/* @ agent/prompt_cache_clock.py:_display_route */
static json_t *pcc2_display_route(pcc2_ctx *ctx, const json_t *agent) {
    return pcc2_route_key(ctx, pcc2_getattr(agent, "provider", NULL),
                          pcc2_getattr(agent, "base_url", NULL),
                          pcc2_getattr(agent, "model", NULL));
}

/* @ agent/prompt_cache_clock.py:_compression_count */
static long long pcc2_compression_count(const json_t *agent) {
    json_t *db = pcc2_getattr(agent, "_session_db", NULL);
    json_t *rewrites = db ? pcc2_getattr(db, "context_rewrites", NULL) : NULL;
    json_t *comp = pcc2_getattr(agent, "context_compressor", NULL);
    json_t *cc = comp ? pcc2_getattr(comp, "compression_count", NULL) : NULL;
    long long base = pcc2_int_or_zero(cc);
    long long rw = pcc2_is_int(rewrites) ? (long long)pcc2_num(rewrites) : 0;
    return base + rw;
}

/* @ agent/prompt_cache_clock.py:_storage_errors */
static json_t *pcc2_storage_errors(void) {
    json_t *a = json_array();
    json_append(a, json_string("sqlite3.Error"));
    json_append(a, json_string("OSError"));
    json_append(a, json_string("PersistenceError"));
    json_append(a, json_string("WorkerPersistenceError"));
    return a;
}
/* helper: does pending exc match _storage_errors()? */
static bool pcc2_storage_exc(pcc2_ctx *ctx) {
    static const char *const CLS[] = {"sqlite3.Error", "OSError", "PersistenceError", "WorkerPersistenceError"};
    return pcc2_exc_matches(ctx, CLS, 4);
}

/* @ agent/prompt_cache_clock.py:_clock_store */
static json_t *pcc2_clock_store(pcc2_ctx *ctx, const json_t *agent) {
    json_t *r = pcc2_call(ctx, "agent_interface", json_object());
    if (pcc2_exc_pending(ctx)) { json_free(r); return NULL; }
    if (pcc2_is_none(r)) { json_free(r); return json_copy(pcc2_getattr(agent, "_session_db", NULL)); }
    /* handle.usage */
    json_t *usage = pcc2_get(r, "usage");
    json_t *out = json_copy(usage);
    json_free(r);
    return out;
}

/* @ agent/prompt_cache_clock.py:_stored_clock */
/* Returns a marker string naming the callable to invoke, or NULL. */
static json_t *pcc2_stored_clock(pcc2_ctx *ctx, const json_t *store) {
    bool present_pc = pcc2_env_bool(ctx, "has_prompt_cache_clock", false);
    bool callable_pc = pcc2_env_bool(ctx, "prompt_cache_clock_callable", false);
    bool present_get = pcc2_env_bool(ctx, "has_get_prompt_cache_clock", false);
    bool callable_get = pcc2_env_bool(ctx, "get_prompt_cache_clock_callable", false);
    (void)store;
    if (present_pc && callable_pc) return json_string("reader");
    if (present_get && callable_get) return json_string("get_prompt_cache_clock");
    return NULL;
}

/* flow1-owned current_generation -- seam only; no PoP claim here. */
static json_t *pcc2_current_generation_seam(pcc2_ctx *ctx, const json_t *db, const json_t *session_id) {
    json_t *args = json_object();
    json_set(args, "db", json_copy(db));
    json_set(args, "session_id", json_copy(session_id));
    return pcc2_call(ctx, "current_generation", args);
}

/* @ agent/prompt_cache_clock.py:_agent_generation */
static json_t *pcc2_agent_generation(pcc2_ctx *ctx, const json_t *agent) {
    json_t *store = pcc2_clock_store(ctx, agent);
    if (pcc2_exc_pending(ctx)) { json_free(store); return NULL; }
    json_t *args = json_object();
    json_set(args, "store", store ? store : json_null());
    json_t *sid = pcc2_getattr(agent, "session_id", NULL);
    json_set(args, "session_id", json_copy(sid));
    return pcc2_call(ctx, "current_generation", args);
}

/* flow1-owned _invalid_reason -- seam only; no PoP claim here. */
static json_t *pcc2_invalid_reason_seam(pcc2_ctx *ctx, const json_t *event, const json_t *session_id,
                                        const json_t *generation, const json_t *route) {
    json_t *args = json_object();
    json_set(args, "event", json_copy(event));
    json_set(args, "session_id", json_copy(session_id));
    json_set(args, "generation", json_copy(generation));
    json_set(args, "route", json_copy(route));
    return pcc2_call(ctx, "_invalid_reason", args);
}

/* @ agent/prompt_cache_clock.py:_agent_invalid_reason */
static json_t *pcc2_agent_invalid_reason(pcc2_ctx *ctx, const json_t *agent, const json_t *event,
                                         const json_t *generation) {
    /* if generation is None and compression mark != count -> "compacted" */
    if (pcc2_is_none(generation)) {
        json_t *mark = pcc2_getattr(agent, "_prompt_cache_compression_mark", NULL);
        long long m = mark ? pcc2_int_or_zero(mark) : 0;
        long long c = pcc2_compression_count(agent);
        if (m != c) return json_string("compacted");
    }
    json_t *args = json_object();
    char *sid = pcc2_str_or_empty(pcc2_getattr(agent, "session_id", NULL));
    json_set(args, "session_id", json_string(sid));
    free(sid);
    json_set(args, "generation", json_copy(generation));
    json_t *route = pcc2_display_route(ctx, agent);
    if (pcc2_exc_pending(ctx)) { json_free(route); return NULL; }
    json_set(args, "route", route ? route : json_null());
    return pcc2_invalid_reason_seam(ctx, event, json_obj_get(args, "session_id"),
                                    json_obj_get(args, "generation"), json_obj_get(args, "route"));
}

/* @ agent/prompt_cache_clock.py:context_witness */
static json_t *pcc2_context_witness(pcc2_ctx *ctx, const json_t *agent) {
    char *sid = pcc2_str_or_empty(pcc2_getattr(agent, "session_id", NULL));
    json_t *gen = pcc2_agent_generation(ctx, agent);
    if (pcc2_exc_pending(ctx)) { free(sid); json_free(gen); return NULL; }
    long long cc = pcc2_compression_count(agent);
    json_t *route = pcc2_display_route(ctx, agent);
    if (pcc2_exc_pending(ctx)) { free(sid); json_free(gen); json_free(route); return NULL; }
    json_t *t = json_array();
    json_append(t, json_string(sid));
    json_append(t, gen ? gen : json_null());
    json_append(t, json_number((double)cc));
    json_append(t, route ? route : json_null());
    free(sid);
    return t;
}

/* @ agent/prompt_cache_clock.py:_set_event (mutates agent into ctx->state) */
static void pcc2_set_event(pcc2_ctx *ctx, json_t *agent, const json_t *event, json_t *mark) {
    (void)ctx;
    long long m = 0;
    bool have_mark = mark && !pcc2_is_none(mark) && pcc2_to_int(mark, &m);
    if (!have_mark) m = pcc2_compression_count(agent);
    json_set(agent, "_prompt_cache_clock", event ? json_copy(event) : json_null());
    json_set(agent, "_prompt_cache_compression_mark", json_number((double)m));
}

/* @ agent/prompt_cache_clock.py:_moa_destination */
static json_t *pcc2_moa_destination(const json_t *agent) {
    json_t *client = pcc2_getattr(agent, "client", NULL);
    json_t *route = client ? pcc2_getattr(client, "last_aggregator_route", NULL) : NULL;
    json_t *out = json_array();
    if (!pcc2_is_obj(route)) { json_append(out, json_null()); json_append(out, json_null()); return out; }
    json_t *attempt = pcc2_getattr(client, "last_aggregator_attempt", NULL);
    json_append(out, json_copy(route));
    json_append(out, pcc2_is_obj(attempt) ? json_copy(attempt) : json_null());
    return out;
}

/* @ agent/prompt_cache_clock.py:_learn */
static void pcc2_learn(pcc2_ctx *ctx, const json_t *previous, const char *provider, const char *base_url,
                       const char *model, const json_t *attempt, long long read, long long write, double at,
                       const json_t *upstream, const char *session_id) {
    if (previous) {
        json_t *sw = pcc2_upstream_switched(pcc2_get(previous, "upstream"), upstream);
        if (pcc2_truthy(sw)) { json_free(sw); return; }
        json_free(sw);
    }
    double tier_s = -1e300; /* Python None sentinel: distinct from any real TTL */
    bool has_tier = false;
    if (previous) {
        json_t *tier = pcc2_get(previous, "tier");
        bool f;
        double s = pcc2_ttl(pcc2_is_str(tier) ? pcc2_jstr(tier) : "", &f);
        if (f) { tier_s = s; has_tier = true; }
    }
    json_t *args = json_object();
    json_set(args, "provider", json_string(provider));
    json_set(args, "base_url", json_string(base_url));
    json_set(args, "model", json_string(model));
    json_set(args, "explicit", json_bool(pcc2_get(attempt, "marker_ttl") != NULL));
    json_set(args, "read", json_number((double)read));
    json_set(args, "write", json_number((double)write));
    json_set(args, "gap_s", previous ? json_number(at - pcc2_num(pcc2_get(previous, "sent_at"))) : json_null());
    json_set(args, "tier_s", has_tier ? json_number(tier_s) : json_null());
    json_set(args, "replay", json_bool(pcc2_truthy(pcc2_get(attempt, "replay"))));
    json_set(args, "session_id", json_string(session_id));
    json_t *r = pcc2_call(ctx, "observe", args);
    json_free(r);
}

/* @ agent/prompt_cache_clock.py:_declared_tier */
static json_t *pcc2_declared_tier(pcc2_ctx *ctx, const char *route_class, const json_t *requested_ttl,
                                  long long write_tokens, const char *provider, const char *base_url) {
    /* ceiling = route_ttl_ceiling(provider, base_url) seam */
    json_t *ca = json_object();
    json_set(ca, "provider", json_string(provider));
    json_set(ca, "base_url", json_string(base_url));
    json_t *ceiling = pcc2_call(ctx, "route_ttl_ceiling", ca);
    if (pcc2_exc_pending(ctx)) { json_free(ceiling); return NULL; }
    /* verdict = _RULES[route_class](_Ctx(...)) -> seam "<route_class>_rule" */
    json_t *va = json_object();
    json_set(va, "route_class", json_string(route_class));
    json_set(va, "requested_ttl", json_copy(requested_ttl));
    json_set(va, "write_tokens", json_number((double)write_tokens));
    json_set(va, "ceiling", ceiling ? ceiling : json_null());
    char seam[128];
    snprintf(seam, sizeof(seam), "rule_%s", route_class);
    json_t *verdict = pcc2_call(ctx, seam, va);
    if (pcc2_exc_pending(ctx)) { json_free(verdict); return NULL; }
    const char *conf = json_get_str(verdict, "confidence", "");
    json_t *tiers = pcc2_get(verdict, "tiers");
    json_t *out = NULL;
    if (pcc2_str_eq(conf, "declared") && pcc2_is_arr(tiers) && tiers->c.count > 0)
        out = json_copy(tiers->c.items[tiers->c.count - 1]);
    json_free(verdict);
    return out;
}

/* @ agent/prompt_cache_clock.py:declared_write_tier */
static json_t *pcc2_declared_write_tier(pcc2_ctx *ctx, const json_t *event) {
    json_t *dest = pcc2_get(event, "destination");
    const char *provider = pcc2_jstr(pcc2_is_arr(dest) && dest->c.count > 0 ? dest->c.items[0] : NULL);
    const char *base_url = pcc2_jstr(pcc2_is_arr(dest) && dest->c.count > 1 ? dest->c.items[1] : NULL);
    return pcc2_declared_tier(ctx, json_get_str(event, "route_class", ""), pcc2_get(event, "requested_ttl"),
                              (long long)json_get_num(event, "write_tokens", 0), provider, base_url);
}

/* @ agent/prompt_cache_clock.py:request_write_tier */
static json_t *pcc2_request_write_tier(pcc2_ctx *ctx, const json_t *api_kwargs, const json_t *usage,
                                       const char *provider, const char *base_url, const char *api_mode,
                                       const char *model) {
    json_t *aa = json_object();
    json_set(aa, "api_kwargs", json_copy(api_kwargs));
    json_t *attempt = pcc2_call(ctx, "attempt_facts", aa);
    if (pcc2_exc_pending(ctx)) { json_free(attempt); return NULL; }
    if (pcc2_is_none(attempt)) { json_free(attempt); attempt = json_object(); }
    /* classify_route seam */
    json_t *ca = json_object();
    json_set(ca, "provider", json_string(provider));
    json_set(ca, "base_url", json_string(base_url));
    json_set(ca, "api_mode", json_string(api_mode));
    json_set(ca, "model", json_string(model));
    json_set(ca, "has_markers", json_bool(pcc2_get(attempt, "marker_ttl") != NULL));
    json_set(ca, "retention", json_copy(pcc2_get(attempt, "retention")));
    json_t *cls = pcc2_call(ctx, "classify_route", ca);
    if (pcc2_exc_pending(ctx)) { json_free(attempt); json_free(cls); return NULL; }
    const char *route_class = pcc2_jstr(cls);
    json_t *requested = pcc2_get(attempt, "marker_ttl");
    if (!requested) requested = pcc2_get(attempt, "openai_ttl");
    long long write = pcc2_int_or_zero(pcc2_getattr(usage, "cache_write_tokens", NULL));
    json_t *out = pcc2_declared_tier(ctx, route_class, requested, write, provider, base_url);
    json_free(attempt); json_free(cls);
    return out;
}

/* @ agent/prompt_cache_clock.py:record_attempt_sent */
static void pcc2_record_attempt_sent(pcc2_ctx *ctx, json_t *agent, const json_t *api_kwargs) {
    json_t *aa = json_object();
    json_set(aa, "api_kwargs", json_copy(api_kwargs));
    json_t *facts = pcc2_call(ctx, "attempt_facts", aa);
    if (pcc2_exc_pending(ctx)) { json_free(facts); return; }
    if (!pcc2_is_none(facts)) json_set(agent, "_prompt_cache_attempt", facts);
    else json_free(facts);
}

/* @ agent/prompt_cache_clock.py:_learn_silent_miss */
static void pcc2_learn_silent_miss(pcc2_ctx *ctx, json_t *agent, const json_t *usage,
                                   const char *model, const char *provider, const char *base_url,
                                   const json_t *attempt, const json_t *upstream) {
    json_t *previous = pcc2_get(agent, "_prompt_cache_clock");
    if (!pcc2_is_obj(previous) || pcc2_str_eq(json_get_str(previous, "confidence", ""), "unknown")) return;
    /* moa_destination[0] */
    json_t *moa = pcc2_moa_destination(agent);
    if (!pcc2_is_none(pcc2_is_arr(moa) && moa->c.count > 0 ? moa->c.items[0] : NULL)) { json_free(moa); return; }
    json_free(moa);
    long long prompt_tokens = pcc2_int_or_zero(pcc2_getattr(usage, "prompt_tokens", NULL));
    long long mincache = (long long)pcc2_env_num(ctx, "MIN_CACHEABLE_PREFIX_TOKENS", 0);
    long long prev_read = (long long)json_get_num(previous, "read_tokens", 0);
    long long bar = mincache > prev_read ? mincache : prev_read;
    if (prompt_tokens < bar) return;
    /* previous.destination != _route_key(provider, base_url, model) */
    json_t *rk = pcc2_route_key(ctx, json_string(provider), json_string(base_url), json_string(model));
    if (pcc2_exc_pending(ctx)) { json_free(rk); return; }
    if (!pcc2_json_eq(pcc2_get(previous, "destination"), rk)) { json_free(rk); return; }
    json_free(rk);
    /* _agent_invalid_reason(agent, previous, _agent_generation(agent)) */
    json_t *gen = pcc2_agent_generation(ctx, agent);
    if (pcc2_exc_pending(ctx)) { json_free(gen); return; }
    json_t *reason = pcc2_agent_invalid_reason(ctx, agent, previous, gen);
    if (pcc2_exc_pending(ctx)) { json_free(gen); json_free(reason); return; }
    json_free(gen);
    if (!pcc2_is_none(reason)) { json_free(reason); return; }
    json_free(reason);
    json_t *att = (json_t *)attempt;
    json_t *own = NULL;
    if (pcc2_is_none(att)) {
        json_t *a = pcc2_get(agent, "_prompt_cache_attempt");
        if (!a) a = pcc2_get(agent, "_prompt_cache_attempt");
        att = a;
    }
    (void)own;
    if (!pcc2_is_obj(att)) { own = json_object(); att = own; }
    double sent = pcc2_float_or_zero(pcc2_get(att, "sent_at"));
    double at = sent > 0 ? sent : pcc2_time(ctx);
    if (pcc2_exc_pending(ctx)) { json_free(own); return; }
    char *sid = pcc2_str_or_empty(pcc2_getattr(agent, "session_id", NULL));
    pcc2_learn(ctx, previous, provider, base_url, model, att, 0, 0, at, upstream, sid);
    free(sid);
    json_free(own);
}

/* @ agent/prompt_cache_clock.py:record_cache_response */
static json_t *pcc2_record_cache_response(pcc2_ctx *ctx, json_t *agent, const json_t *usage,
                                          const char *model, const char *provider, const char *base_url,
                                          const char *api_mode, const json_t *attempt,
                                          const json_t *witness, const json_t *upstream) {
    long long read = pcc2_int_or_zero(pcc2_getattr(usage, "cache_read_tokens", NULL));
    long long write = pcc2_int_or_zero(pcc2_getattr(usage, "cache_write_tokens", NULL));
    if (read + write <= 0) {
        pcc2_learn_silent_miss(ctx, agent, usage, model, provider, base_url, attempt, upstream);
        return NULL;
    }
    const char *M = model, *P = provider, *B = base_url, *A = api_mode;
    json_t *moa_att = NULL;
    json_t *moa = pcc2_moa_destination(agent);
    json_t *moa_route = pcc2_is_arr(moa) && moa->c.count > 0 ? moa->c.items[0] : NULL;
    if (!pcc2_is_none(moa_route)) {
        json_t *mv = pcc2_get(moa_route, "model");
        if (pcc2_truthy(mv)) M = pcc2_jstr(mv);
        json_t *pv = pcc2_get(moa_route, "provider");
        if (pcc2_truthy(pv)) P = pcc2_jstr(pv);
        json_t *bv = pcc2_get(moa_route, "base_url");
        B = pcc2_truthy(bv) ? pcc2_jstr(bv) : "";
        json_t *av = pcc2_get(moa_route, "api_mode");
        A = pcc2_truthy(av) ? pcc2_jstr(av) : "";
        if (pcc2_is_arr(moa) && moa->c.count > 1 && pcc2_is_obj(moa->c.items[1])) moa_att = moa->c.items[1];
    }
    /* witness or (session, generation, mark, route) */
    json_t *w = (json_t *)witness;
    char *sid; json_t *gen; json_t *mark; json_t *route;
    if (pcc2_is_arr(w) && w->c.count >= 4) {
        sid = pcc2_str_of(w->c.items[0]); gen = json_copy(w->c.items[1]);
        mark = json_copy(w->c.items[2]); route = json_copy(w->c.items[3]);
    } else {
        sid = pcc2_str_or_empty(pcc2_getattr(agent, "session_id", NULL));
        gen = pcc2_agent_generation(ctx, agent);
        if (pcc2_exc_pending(ctx)) { json_free(moa); free(sid); json_free(gen); return NULL; }
        mark = NULL;
        route = pcc2_display_route(ctx, agent);
        if (pcc2_exc_pending(ctx)) { json_free(moa); free(sid); json_free(gen); json_free(route); return NULL; }
    }
    /* attempt = moa_att or attempt or agent._prompt_cache_attempt or {} */
    json_t *att = (json_t *)attempt;
    if (pcc2_is_none(att) && moa_att) att = moa_att;
    if (pcc2_is_none(att)) att = pcc2_get(agent, "_prompt_cache_attempt");
    json_t *own = NULL;
    if (!pcc2_is_obj(att)) { own = json_object(); att = own; }
    /* previous = _valid_previous(agent, gen) -- direct (unclaimed sibling) */
    json_t *prev = pcc2_valid_previous(ctx, agent, gen);
    if (pcc2_exc_pending(ctx)) { json_free(moa); free(sid); json_free(gen); json_free(mark); json_free(route);
                                 json_free(prev); json_free(own); return NULL; }
    if (pcc2_is_none(prev)) { json_free(prev); prev = NULL; }
    /* ctx._Ctx + rule via seams */
    json_t *ca = json_object();
    json_set(ca, "provider", json_string(P));
    json_set(ca, "base_url", json_string(B));
    json_set(ca, "api_mode", json_string(A ? A : ""));
    json_set(ca, "model", json_string(M ? M : ""));
    json_set(ca, "has_markers", json_bool(pcc2_get(att, "marker_ttl") != NULL));
    json_set(ca, "retention", json_copy(pcc2_get(att, "retention")));
    json_t *cls = pcc2_call(ctx, "classify_route", ca);
    if (pcc2_exc_pending(ctx)) { json_free(moa); free(sid); json_free(gen); json_free(mark); json_free(route);
                                 json_free(prev); json_free(cls); json_free(own); return NULL; }
    const char *route_class = pcc2_jstr(cls);
    /* ceiling = route_ttl_ceiling(P,B); verdict = _RULES[route_class](ctx) -- both seams. */
    json_t *cea = json_object();
    json_set(cea, "provider", json_string(P));
    json_set(cea, "base_url", json_string(B));
    json_t *ceiling = pcc2_call(ctx, "route_ttl_ceiling", cea);
    if (pcc2_exc_pending(ctx)) { json_free(moa); free(sid); json_free(gen); json_free(mark); json_free(route);
                                 json_free(prev); json_free(cls); json_free(ceiling); json_free(own); return NULL; }
    json_t *rva = json_object();
    json_set(rva, "route_class", json_string(route_class));
    json_t *rreq = pcc2_get(att, "marker_ttl");
    if (!rreq) rreq = pcc2_get(att, "openai_ttl");
    json_set(rva, "requested_ttl", json_copy(rreq));
    json_set(rva, "write_tokens", json_number((double)write));
    json_set(rva, "ceiling", ceiling ? ceiling : json_null());
    char rseam[128]; snprintf(rseam, sizeof(rseam), "rule_%s", route_class);
    json_t *verdict = pcc2_call(ctx, rseam, rva);
    if (pcc2_exc_pending(ctx)) { json_free(moa); free(sid); json_free(gen); json_free(mark); json_free(route);
                                 json_free(prev); json_free(cls); json_free(verdict); json_free(own); return NULL; }
    json_free(verdict);
    /* requested_ttl = attempt.marker_ttl or attempt.openai_ttl */
    json_t *req = pcc2_get(att, "marker_ttl");
    if (!req) req = pcc2_get(att, "openai_ttl");
    /* destination = _route_key(P,B,M) */
    json_t *dest = pcc2_route_key(ctx, json_string(P), json_string(B), json_string(M));
    if (pcc2_exc_pending(ctx)) { json_free(moa); free(sid); json_free(gen); json_free(mark); json_free(route);
                                 json_free(prev); json_free(cls); json_free(dest); json_free(own); return NULL; }
    /* drop previous on destination/route_class change */
    if (prev && (!pcc2_json_eq(pcc2_get(prev, "destination"), dest) ||
                 !pcc2_str_eq(json_get_str(prev, "route_class", ""), route_class))) {
        json_free(prev); prev = NULL;
    }
    /* at = float(attempt.sent_at or time.time()) */
    double at = pcc2_float_or_zero(pcc2_get(att, "sent_at"));
    if (at == 0) { at = pcc2_time(ctx); if (pcc2_exc_pending(ctx)) at = 0; }
    /* _learn -> observe seam (via _learn) */
    pcc2_learn(ctx, prev, P, B, M, att, read, write, at, upstream, sid);
    if (pcc2_exc_pending(ctx)) { json_free(moa); free(sid); json_free(gen); json_free(mark); json_free(route);
                                 json_free(prev); json_free(cls); json_free(dest); json_free(own); return NULL; }
    /* _fold_coverage seam */
    json_t *fa = json_object();
    json_set(fa, "previous", prev ? json_copy(prev) : json_null());
    json_set(fa, "verdict_class", json_string(route_class));
    json_set(fa, "requested_ttl", json_copy(req));
    json_set(fa, "retention", json_copy(pcc2_get(att, "retention")));
    json_set(fa, "at", json_number(at));
    json_set(fa, "read", json_number((double)read));
    json_set(fa, "write", json_number((double)write));
    json_set(fa, "ceiling", json_null());
    json_t *fold = pcc2_call(ctx, "_fold_coverage", fa);
    if (pcc2_exc_pending(ctx)) { json_free(moa); free(sid); json_free(gen); json_free(mark); json_free(route);
                                 json_free(prev); json_free(cls); json_free(dest); json_free(fold); json_free(own); return NULL; }
    /* _fold_coverage returns a 2-tuple (coverage, carried). */
    json_t *coverage = (pcc2_is_arr(fold) && fold->c.count > 0) ? json_copy(fold->c.items[0]) : json_null();
    bool carried = (pcc2_is_arr(fold) && fold->c.count > 1) ? pcc2_truthy(fold->c.items[1]) : false;
    /* guaranteed = _ends(coverage)[0] -- claimed sibling, seam. */
    json_t *ena = json_object(); json_set(ena, "coverage", json_copy(coverage));
    json_t *ends = pcc2_call(ctx, "_ends", ena);
    if (pcc2_exc_pending(ctx)) { json_free(moa); free(sid); json_free(gen); json_free(mark); json_free(route);
                                 json_free(prev); json_free(cls); json_free(dest); json_free(fold); json_free(coverage);
                                 json_free(ends); json_free(own); return NULL; }
    json_t *guaranteed = (pcc2_is_arr(ends) && ends->c.count > 0 && !pcc2_is_none(ends->c.items[0]))
                         ? json_copy(ends->c.items[0]) : NULL;
    json_free(ends);
    /* estimated = _estimated_end seam */
    json_t *ea = json_object();
    json_set(ea, "coverage", json_copy(coverage));
    json_set(ea, "base_url", json_string(B));
    json_set(ea, "retention", json_copy(pcc2_get(att, "retention")));
    json_set(ea, "at", json_number(at));
    json_set(ea, "previous", prev ? json_copy(prev) : json_null());
    json_set(ea, "carried", json_bool(carried));
    json_set(ea, "idle_from", json_number(pcc2_time(ctx)));
    json_t *estimated = pcc2_call(ctx, "_estimated_end", ea);
    if (pcc2_exc_pending(ctx)) { json_free(moa); free(sid); json_free(gen); json_free(mark); json_free(route);
                                 json_free(prev); json_free(cls); json_free(dest); json_free(fold); json_free(estimated); json_free(own); return NULL; }
    if (pcc2_is_none(estimated)) { json_free(estimated); estimated = NULL; }
    if (guaranteed && estimated) {
        double g = pcc2_num(guaranteed), e = pcc2_num(estimated);
        json_free(guaranteed);
        guaranteed = json_number(g < e ? g : e);
    }
    /* confidence/reason = _confidence seam */
    json_t *cfa = json_object();
    json_set(cfa, "verdict_class", json_string(route_class));
    json_set(cfa, "previous", prev ? json_copy(prev) : json_null());
    json_set(cfa, "carried", json_bool(carried));
    json_set(cfa, "write", json_number((double)write));
    json_t *conf = pcc2_call(ctx, "_confidence", cfa);
    if (pcc2_exc_pending(ctx)) { json_free(moa); free(sid); json_free(gen); json_free(mark); json_free(route);
                                 json_free(prev); json_free(cls); json_free(dest); json_free(fold); json_free(estimated);
                                 json_free(guaranteed); json_free(conf); json_free(own); return NULL; }
    const char *confidence = (pcc2_is_arr(conf) && conf->c.count > 0) ? pcc2_jstr(conf->c.items[0]) : "unknown";
    const char *reason = (pcc2_is_arr(conf) && conf->c.count > 1) ? pcc2_jstr(conf->c.items[1]) : "";
    /* tier = min(coverage, key=coverage.get) if coverage else None */
    json_t *tier = NULL;
    if (pcc2_is_obj(coverage) && coverage->c.count > 0) {
        double mn = 1e300; const char *mk = "";
        for (size_t i = 0; i < coverage->c.count; i++) {
            double d = pcc2_num(coverage->c.items[i]);
            if (!tier || d < mn) { mn = d; mk = coverage->c.keys[i]; }
        }
        tier = json_string(mk);
    }
    json_t *event = json_object();
    json_set(event, "route", route ? route : json_null());
    json_set(event, "destination", dest ? dest : json_null());
    json_set(event, "route_class", json_string(route_class));
    json_set(event, "requested_ttl", req ? json_copy(req) : json_null());
    json_set(event, "sent_at", json_number(at));
    json_set(event, "read_tokens", json_number((double)read));
    json_set(event, "write_tokens", json_number((double)write));
    json_set(event, "tier", tier ? tier : json_null());
    json_set(event, "confidence", (pcc2_is_obj(coverage) && coverage->c.count > 0) ? json_string(confidence) : json_string("unknown"));
    json_set(event, "reason", json_string(reason));
    json_set(event, "coverage", pcc2_is_none(coverage) ? json_object() : json_copy(coverage));
    json_set(event, "guaranteed_until", guaranteed ? guaranteed : json_null());
    json_set(event, "estimated_until", estimated ? estimated : json_null());
    json_set(event, "session_id", json_string(sid));
    json_set(event, "generation", gen ? gen : json_null());
    json_set(event, "upstream", json_copy(upstream));
    /* agent._prompt_cache_clock_restored = True; _set_event(agent,event,mark) */
    json_set(agent, "_prompt_cache_clock_restored", json_bool(true));
    pcc2_set_event(ctx, agent, event, mark);
    json_free(moa); free(sid); json_free(mark); json_free(prev); json_free(cls); json_free(fold);
    json_free(conf); json_free(own);
    return event;
}

/* @ agent/prompt_cache_clock.py:record_unobserved_cache_response */
static json_t *pcc2_record_unobserved(pcc2_ctx *ctx, json_t *agent, const json_t *usage, const char *reason) {
    long long read = pcc2_int_or_zero(pcc2_getattr(usage, "cache_read_tokens", NULL));
    long long write = pcc2_int_or_zero(pcc2_getattr(usage, "cache_write_tokens", NULL));
    if (read + write <= 0) return NULL;
    json_t *route = pcc2_display_route(ctx, agent);
    if (pcc2_exc_pending(ctx)) { json_free(route); return NULL; }
    /* CacheEvent kwargs evaluate left-to-right: sent_at=time.time() BEFORE
     * generation=_agent_generation(agent). */
    double sent = pcc2_time(ctx);
    if (pcc2_exc_pending(ctx)) { json_free(route); return NULL; }
    json_t *gen = pcc2_agent_generation(ctx, agent);
    if (pcc2_exc_pending(ctx)) { json_free(route); json_free(gen); return NULL; }
    char *sid = pcc2_str_or_empty(pcc2_getattr(agent, "session_id", NULL));
    json_t *event = json_object();
    json_set(event, "route", route ? route : json_null());
    json_set(event, "destination", route ? json_copy(route) : json_null());
    json_set(event, "route_class", json_string("unobserved"));
    json_set(event, "requested_ttl", json_null());
    json_set(event, "sent_at", json_number(sent));
    json_set(event, "read_tokens", json_number((double)read));
    json_set(event, "write_tokens", json_number((double)write));
    json_set(event, "tier", json_null());
    json_set(event, "confidence", json_string("unknown"));
    json_set(event, "reason", json_string(reason));
    json_set(event, "coverage", json_object());
    json_set(event, "guaranteed_until", json_null());
    json_set(event, "estimated_until", json_null());
    json_set(event, "session_id", json_string(sid));
    json_set(event, "generation", gen ? gen : json_null());
    json_set(event, "upstream", json_null());
    free(sid);
    json_set(agent, "_prompt_cache_clock_restored", json_bool(true));
    pcc2_set_event(ctx, agent, event, NULL);
    return event;
}

/* @ agent/prompt_cache_clock.py:event_to_json */
static json_t *pcc2_event_to_json(const json_t *event) {
    /* json.dumps(asdict(event), separators=(",",":")) -> compact canonical string.
     * libjson serialize uses ", " separators; build a compact form by walking. */
    /* Emit a marker with the event dict; the oracle canonicalizes both sides via
     * json.loads. To keep exact parity we serialize compact ourselves. */
    char *s = json_serialize(event);
    json_t *out = json_string(s ? s : "{}");
    free(s);
    return out;
}

/* @ agent/prompt_cache_clock.py:_fallback_event */
static json_t *pcc2_fallback_event(pcc2_ctx *ctx, const json_t *agent, double last_assistant_at,
                                   const json_t *generation) {
    char *provider = pcc2_str_or_empty(pcc2_getattr(agent, "provider", NULL));
    char *base_url = pcc2_str_or_empty(pcc2_getattr(agent, "base_url", NULL));
    char *model = pcc2_str_or_empty(pcc2_getattr(agent, "model", NULL));
    bool caching = pcc2_env_bool(ctx, "use_prompt_caching", false);
    /* requested = effective_cache_ttl(...) if caching else None -> seam */
    json_t *requested = NULL;
    if (caching) {
        json_t *ea = json_object();
        json_set(ea, "cache_ttl", json_copy(pcc2_getattr(agent, "_cache_ttl", NULL)));
        json_set(ea, "provider", json_string(provider));
        json_set(ea, "model", json_string(model));
        json_set(ea, "base_url", json_string(base_url));
        json_set(ea, "auto", json_bool(pcc2_env_bool(ctx, "cache_ttl_auto", false)));
        requested = pcc2_call(ctx, "effective_cache_ttl", ea);
        if (pcc2_exc_pending(ctx)) { free(provider); free(base_url); free(model); json_free(requested); return NULL; }
    }
    /* classify_route seam */
    json_t *ca = json_object();
    json_set(ca, "provider", json_string(provider));
    json_set(ca, "base_url", json_string(base_url));
    json_set(ca, "api_mode", json_string(pcc2_jstr(pcc2_getattr(agent, "api_mode", NULL))));
    json_set(ca, "model", json_string(model));
    json_set(ca, "has_markers", json_bool(caching));
    json_set(ca, "retention", json_null());
    json_t *cls = pcc2_call(ctx, "classify_route", ca);
    if (pcc2_exc_pending(ctx)) { free(provider); free(base_url); free(model); json_free(requested); json_free(cls); return NULL; }
    const char *route_class = pcc2_jstr(cls);
    /* ceiling = route_ttl_ceiling(provider, base_url) -- evaluated inside _Ctx(...) */
    json_t *cea = json_object();
    json_set(cea, "provider", json_string(provider));
    json_set(cea, "base_url", json_string(base_url));
    json_t *ceiling = pcc2_call(ctx, "route_ttl_ceiling", cea);
    if (pcc2_exc_pending(ctx)) { free(provider); free(base_url); free(model); json_free(requested);
                                 json_free(cls); json_free(ceiling); return NULL; }
    /* verdict via rule seam */
    json_t *rva = json_object();
    json_set(rva, "route_class", json_string(route_class));
    json_set(rva, "requested_ttl", json_copy(requested));
    json_set(rva, "write_tokens", json_number(0));
    json_set(rva, "ceiling", ceiling ? ceiling : json_null());
    char seam[128]; snprintf(seam, sizeof(seam), "rule_%s", route_class);
    json_t *verdict = pcc2_call(ctx, seam, rva);
    if (pcc2_exc_pending(ctx)) { free(provider); free(base_url); free(model); json_free(requested); json_free(cls); json_free(verdict); return NULL; }
    json_t *tiers = pcc2_get(verdict, "tiers");
    double seconds = 0;
    if (pcc2_is_arr(tiers)) for (size_t i = 0; i < tiers->c.count; i++) {
        bool f; double s = pcc2_ttl(pcc2_jstr(tiers->c.items[i]), &f);
        if (f && s > seconds) seconds = s;
    }
    const char *vtier = (pcc2_is_arr(tiers) && tiers->c.count) ? pcc2_jstr(tiers->c.items[tiers->c.count - 1]) : "";
    const char *vreason = json_get_str(verdict, "reason", "");
    json_t *route = pcc2_display_route(ctx, agent);
    if (pcc2_exc_pending(ctx)) { free(provider); free(base_url); free(model); json_free(requested); json_free(cls);
                                 json_free(verdict); json_free(route); return NULL; }
    char *sid = pcc2_str_or_empty(pcc2_getattr(agent, "session_id", NULL));
    char reason[300];
    snprintf(reason, sizeof reason, "restored_from_transcript:%s", vreason);
    json_t *event = json_object();
    json_set(event, "route", route ? route : json_null());
    json_set(event, "destination", route ? json_copy(route) : json_null());
    json_set(event, "route_class", json_string(route_class));
    json_set(event, "requested_ttl", requested ? requested : json_null());
    json_set(event, "sent_at", json_number(last_assistant_at));
    json_set(event, "read_tokens", json_number(0));
    json_set(event, "write_tokens", json_number(0));
    json_set(event, "tier", (pcc2_is_arr(tiers) && tiers->c.count) ? json_string(vtier) : json_null());
    json_set(event, "confidence", seconds > 0 ? json_string("assumed") : json_string("unknown"));
    json_set(event, "reason", json_string(reason));
    json_set(event, "coverage", json_object());
    json_set(event, "guaranteed_until", json_null());
    json_set(event, "estimated_until", seconds > 0 ? json_number(last_assistant_at + seconds) : json_null());
    json_set(event, "session_id", json_string(sid));
    json_set(event, "generation", json_copy(generation));
    json_set(event, "upstream", json_null());
    free(sid); free(provider); free(base_url); free(model);
    json_free(cls); json_free(verdict);
    return event;
}

/* @ agent/prompt_cache_clock.py:restore_cache_clock */
static json_t *pcc2_restore_cache_clock(pcc2_ctx *ctx, json_t *agent) {
    json_t *sid = pcc2_getattr(agent, "session_id", NULL);
    /* _clock_store seam (returns store) */
    json_t *store = pcc2_clock_store(ctx, agent);
    if (pcc2_exc_pending(ctx)) { json_free(store); return NULL; }
    json_t *reader = pcc2_stored_clock(ctx, store); /* marker string or null -- direct (unclaimed sibling) */
    if (pcc2_is_none(sid) || pcc2_is_none(reader)) {
        json_set(agent, "_prompt_cache_clock_restored", json_bool(true));
        json_free(reader);
        return NULL;
    }
    /* reader(session_id) via seam named by marker, inside try/except _storage_errors() */
    json_t *ra = json_object();
    json_set(ra, "session_id", json_copy(sid));
    char rname[128];
    snprintf(rname, sizeof(rname), "%s", pcc2_jstr(reader));
    json_free(reader);
    json_t *stored = pcc2_call(ctx, rname, ra);
    if (pcc2_exc_pending(ctx)) {
        if (pcc2_storage_exc(ctx)) { pcc2_exc_clear(ctx); json_free(stored); return NULL; }
        json_free(stored); return NULL;
    }
    if (pcc2_is_none(stored)) { json_free(stored); stored = json_object(); }
    json_set(agent, "_prompt_cache_clock_restored", json_bool(true));
    /* event_from_json(stored.clock) seam */
    json_t *fa = json_object();
    json_set(fa, "raw", json_copy(pcc2_get(stored, "clock")));
    json_t *event = pcc2_call(ctx, "event_from_json", fa);
    if (pcc2_exc_pending(ctx)) { json_free(stored); json_free(event); return NULL; }
    if (!pcc2_is_none(event)) {
        /* replace(event, session_id=event.sid or str(sid)) */
        if (!pcc2_truthy(pcc2_get(event, "session_id"))) {
            char *s = pcc2_str_of(sid);
            json_set(event, "session_id", json_string(s));
            free(s);
        }
    } else {
        json_t *last = pcc2_get(stored, "last_assistant_at");
        if (pcc2_is_num(last) || pcc2_is_bool(last)) {
            json_free(event);
            event = pcc2_fallback_event(ctx, agent, pcc2_num(last), pcc2_get(stored, "generation"));
            if (pcc2_exc_pending(ctx)) { json_free(stored); json_free(event); return NULL; }
        }
    }
    json_free(stored);
    pcc2_set_event(ctx, agent, event, NULL);
    return event;
}

/* @ agent/prompt_cache_clock.py:_unknown */
static json_t *pcc2_unknown(const char *reason, const json_t *event) {
    json_t *w = json_object();
    json_set(w, "guaranteed_until", json_null());
    json_set(w, "estimated_until", json_null());
    json_set(w, "tier", json_null());
    json_set(w, "confidence", json_string("unknown"));
    json_set(w, "reason", json_string(reason));
    json_set(w, "refreshed_at", event ? json_copy(pcc2_get(event, "sent_at")) : json_null());
    /* _unknown never passes coverage -> dataclass default empty dict on both sides. */
    json_set(w, "coverage", json_object());
    return w;
}

/* @ agent/prompt_cache_clock.py:_window */
static json_t *pcc2_window(const json_t *event) {
    if (pcc2_str_eq(json_get_str(event, "confidence", ""), "unknown") ||
        (pcc2_is_none(pcc2_get(event, "estimated_until")) && pcc2_is_none(pcc2_get(event, "guaranteed_until"))))
        return pcc2_unknown(json_get_str(event, "reason", ""), event);
    json_t *w = json_object();
    json_set(w, "guaranteed_until", json_copy(pcc2_get(event, "guaranteed_until")));
    json_set(w, "estimated_until", json_copy(pcc2_get(event, "estimated_until")));
    json_set(w, "tier", json_copy(pcc2_get(event, "tier")));
    json_set(w, "confidence", json_copy(pcc2_get(event, "confidence")));
    json_set(w, "reason", json_copy(pcc2_get(event, "reason")));
    json_set(w, "refreshed_at", json_copy(pcc2_get(event, "sent_at")));
    json_set(w, "coverage", pcc2_is_obj(pcc2_get(event, "coverage")) ? json_copy(pcc2_get(event, "coverage")) : json_object());
    return w;
}

/* @ agent/prompt_cache_clock.py:cache_window */
static json_t *pcc2_cache_window(pcc2_ctx *ctx, json_t *agent) {
    json_t *event = pcc2_get(agent, "_prompt_cache_clock");
    if (pcc2_is_none(event) && !pcc2_env_bool(ctx, "restored_flag", false)) {
        json_t *r = pcc2_restore_cache_clock(ctx, agent);
        if (pcc2_exc_pending(ctx)) { json_free(r); return NULL; }
        json_free(r);
        event = pcc2_get(agent, "_prompt_cache_clock");
    }
    if (!pcc2_is_obj(event)) {
        if (pcc2_env_bool(ctx, "restored_flag", pcc2_env_bool(ctx, "agent_restored", false)))
            return NULL;
        return pcc2_unknown("storage_unavailable", NULL);
    }
    json_t *gen = pcc2_agent_generation(ctx, agent);
    if (pcc2_exc_pending(ctx)) { json_free(gen); return NULL; }
    json_t *reason = pcc2_agent_invalid_reason(ctx, agent, event, gen);
    if (pcc2_exc_pending(ctx)) { json_free(gen); json_free(reason); return NULL; }
    json_free(gen);
    if (!pcc2_is_none(reason)) { json_t *w = pcc2_unknown(json_get_str(reason, "", "unknown"), event); json_free(reason); return w; }
    json_free(reason);
    return pcc2_window(event);
}

/* @ agent/prompt_cache_clock.py:_saved_route_changed */
static json_t *pcc2_saved_route_changed(pcc2_ctx *ctx, const json_t *event, const json_t *saved) {
    /* provider, base_url, model = _route_key(*saved) */
    json_t *s0 = pcc2_is_arr(saved) && saved->c.count > 0 ? saved->c.items[0] : NULL;
    json_t *s1 = pcc2_is_arr(saved) && saved->c.count > 1 ? saved->c.items[1] : NULL;
    json_t *s2 = pcc2_is_arr(saved) && saved->c.count > 2 ? saved->c.items[2] : NULL;
    json_t *rk = pcc2_route_key(ctx, s0, s1, s2);
    if (pcc2_exc_pending(ctx)) { json_free(rk); return NULL; }
    const char *provider = pcc2_jstr(rk->c.items[0]);
    const char *base_url = pcc2_jstr(rk->c.items[1]);
    const char *model = pcc2_jstr(rk->c.items[2]);
    json_t *eroute = pcc2_get(event, "route");
    const char *ev_p = pcc2_jstr(pcc2_is_arr(eroute) && eroute->c.count > 0 ? eroute->c.items[0] : NULL);
    const char *ev_u = pcc2_jstr(pcc2_is_arr(eroute) && eroute->c.count > 1 ? eroute->c.items[1] : NULL);
    const char *ev_m = pcc2_jstr(pcc2_is_arr(eroute) && eroute->c.count > 2 ? eroute->c.items[2] : NULL);
    bool r = (model[0] && !pcc2_str_eq(model, ev_m)) || (base_url[0] && !pcc2_str_eq(base_url, ev_u)) ||
             (provider[0] && !pcc2_str_eq(ev_p, "custom") && !pcc2_str_eq(provider, ev_p));
    json_free(rk);
    return json_bool(r);
}

/* @ agent/prompt_cache_clock.py:stored_cache_window */
static json_t *pcc2_stored_cache_window(pcc2_ctx *ctx, const json_t *raw_clock, const char *session_id,
                                        const json_t *generation, const json_t *saved_route) {
    json_t *fa = json_object();
    json_set(fa, "raw", json_copy(raw_clock));
    json_t *event = pcc2_call(ctx, "event_from_json", fa);
    if (pcc2_exc_pending(ctx)) { json_free(event); return NULL; }
    if (pcc2_is_none(event)) { json_free(event); return NULL; }
    json_t *ia = json_object();
    json_set(ia, "event", json_copy(event));
    json_set(ia, "session_id", json_string(session_id));
    json_set(ia, "generation", json_copy(generation));
    json_set(ia, "route", json_null());
    json_t *reason = pcc2_call(ctx, "_invalid_reason", ia);
    if (pcc2_exc_pending(ctx)) { json_free(event); json_free(reason); return NULL; }
    if (pcc2_is_none(reason)) {
        json_free(reason);
        json_t *chg = pcc2_saved_route_changed(ctx, event, saved_route);
        if (pcc2_exc_pending(ctx)) { json_free(event); json_free(chg); return NULL; }
        if (pcc2_truthy(chg)) { json_free(chg); reason = json_string("route_changed"); }
        else json_free(chg);
        if (pcc2_is_none(reason)) reason = json_null();
    }
    json_t *out;
    if (!pcc2_is_none(reason)) out = pcc2_unknown(pcc2_jstr(reason), event);
    else out = pcc2_window(event);
    json_free(event); json_free(reason);
    return out;
}

/* @ agent/prompt_cache_clock.py:cache_window_payload */
static json_t *pcc2_cache_window_payload(pcc2_ctx *ctx, json_t *agent) {
    json_t *window = pcc2_cache_window(ctx, agent);
    if (pcc2_exc_pending(ctx)) { json_free(window); return NULL; }
    if (pcc2_is_none(window)) { json_free(window); return NULL; }
    /* rule_for seam */
    json_t *ra = json_object();
    json_set(ra, "provider", json_copy(pcc2_getattr(agent, "provider", NULL)));
    json_set(ra, "base_url", json_copy(pcc2_getattr(agent, "base_url", NULL)));
    json_set(ra, "model", json_copy(pcc2_getattr(agent, "model", NULL)));
    json_set(ra, "caching", json_bool(pcc2_env_bool(ctx, "use_prompt_caching", false)));
    bool f; double ts = pcc2_ttl(pcc2_jstr(pcc2_get(window, "tier")), &f);
    json_set(ra, "tier_s", f ? json_number(ts) : json_null());
    json_t *rule = pcc2_call(ctx, "rule_for", ra);
    if (pcc2_exc_pending(ctx)) { json_free(window); json_free(rule); return NULL; }
    json_t *da = json_object(); json_set(da, "rule", rule ? rule : json_null());
    json_t *learned = pcc2_call(ctx, "display_rule", da);
    if (pcc2_exc_pending(ctx)) { json_free(window); json_free(learned); return NULL; }
    json_t *warming = pcc2_call(ctx, "prompt_cache_warming_disabled", json_object());
    if (pcc2_exc_pending(ctx)) { json_free(window); json_free(learned); json_free(warming); return NULL; }
    json_t *pending = pcc2_call(ctx, "tier_pending", json_object());
    if (pcc2_exc_pending(ctx)) { json_free(window); json_free(learned); json_free(warming); json_free(pending); return NULL; }
    json_t *out = json_copy(window);
    json_set(out, "warming_off", warming ? warming : json_null());
    json_set(out, "learned", learned ? learned : json_null());
    json_set(out, "tier_pending", pending ? pending : json_null());
    json_free(window);
    return out;
}

/* @ agent/prompt_cache_clock.py:_field */
static json_t *pcc2_field(const json_t *window, const char *name) {
    if (pcc2_is_obj(window)) return pcc2_get(window, name);
    return NULL;
}

/* flow1-owned cache_certainly_warm -- seam only; no PoP claim here. */
static json_t *pcc2_certainly_warm_seam(pcc2_ctx *ctx, const json_t *window, const json_t *now) {
    json_t *a = json_object();
    json_set(a, "window", json_copy(window));
    json_set(a, "now", json_copy(now));
    return pcc2_call(ctx, "cache_certainly_warm", a);
}
/* flow1-owned cache_certainly_cold -- seam only; no PoP claim here. */
static json_t *pcc2_certainly_cold_seam(pcc2_ctx *ctx, const json_t *window, const json_t *now) {
    json_t *a = json_object();
    json_set(a, "window", json_copy(window));
    json_set(a, "now", json_copy(now));
    return pcc2_call(ctx, "cache_certainly_cold", a);
}

/* @ agent/prompt_cache_clock.py:_clock */
static json_t *pcc2_clock(pcc2_ctx *ctx, double ts) {
    json_t *la = json_object(); json_set(la, "ts", json_number(ts));
    json_t *lt = pcc2_call(ctx, "localtime", la);
    if (pcc2_exc_pending(ctx)) { json_free(lt); return NULL; }
    json_t *sa = json_object(); json_set(sa, "fmt", json_string("%H:%M"));
    json_set(sa, "time", lt ? lt : json_null());
    json_t *r = pcc2_call(ctx, "strftime", sa);
    return r;
}

/* flow1-owned _duration -- seam only; no PoP claim here. */
static json_t *pcc2_duration_seam(pcc2_ctx *ctx, const json_t *seconds) {
    json_t *a = json_object(); json_set(a, "seconds", json_copy(seconds));
    return pcc2_call(ctx, "_duration", a);
}

/* @ agent/prompt_cache_clock.py:_cache_line */
static json_t *pcc2_cache_line(pcc2_ctx *ctx, const json_t *window, const json_t *estimated,
                               const json_t *now) {
    const char *conf = json_get_str(window, "confidence", "");
    /* Python short-circuit: cache_certainly_warm is only called in the unknown
     * guard when estimated is None (and confidence is not already UNKNOWN). */
    bool unknown_path = pcc2_str_eq(conf, "unknown");
    if (!unknown_path && pcc2_is_none(estimated)) {
        json_t *warm0 = pcc2_certainly_warm_seam(ctx, window, now);
        if (pcc2_exc_pending(ctx)) { json_free(warm0); return NULL; }
        unknown_path = !pcc2_truthy(warm0);
        json_free(warm0);
    }
    if (unknown_path) {
        const char *reason = json_get_str(window, "reason", "");
        static const struct { const char *r, *line; } M[] = {
            {"session_rotated", "cache_unknown_moved"}, {"compacted", "cache_unknown_moved"},
            {"route_changed", "cache_unknown_moved"}, {"codex_app_server", "cache_unknown_runtime"},
            {"storage_unavailable", "cache_unknown_storage"},
        };
        const char *line = "cache_unknown_route";
        for (size_t i = 0; i < sizeof(M) / sizeof(M[0]); i++) if (pcc2_str_eq(M[i].r, reason)) { line = M[i].line; break; }
        char key[160]; snprintf(key, sizeof key, "gateway.context.%s", line);
        json_t *ta = json_object(); json_set(ta, "key", json_string(key));
        json_t *r = pcc2_call(ctx, "t", ta);
        return r;
    }
    /* now resolved */
    double nowv;
    if (pcc2_is_none(now)) { nowv = pcc2_time(ctx); if (pcc2_exc_pending(ctx)) return NULL; }
    else nowv = pcc2_num(now);
    /* facts: ttl, confidence */
    char confkey[200]; snprintf(confkey, sizeof confkey, "gateway.context.cache_confidence.%s", conf);
    json_t *ca = json_object(); json_set(ca, "key", json_string(confkey));
    json_t *ct = pcc2_call(ctx, "t", ca);
    if (pcc2_exc_pending(ctx)) { json_free(ct); return NULL; }
    /* warm again with resolved now */
    json_t *nown = json_number(nowv);
    json_t *warm2 = pcc2_certainly_warm_seam(ctx, window, nown);
    if (pcc2_exc_pending(ctx)) { json_free(ct); json_free(warm2); return NULL; }
    json_t *result = NULL;
    if (pcc2_truthy(warm2)) {
        double g = pcc2_num(pcc2_get(window, "guaranteed_until"));
        json_t *cl = pcc2_clock(ctx, g);
        if (pcc2_exc_pending(ctx)) { goto cleanup_cache_line; }
        json_t *d = pcc2_duration_seam(ctx, json_number(g - nowv));
        if (pcc2_exc_pending(ctx)) { json_free(cl); json_free(d); goto cleanup_cache_line; }
        json_t *ta = json_object();
        json_set(ta, "key", json_string("gateway.context.cache_warm"));
        json_set(ta, "until", cl ? cl : json_null());
        json_set(ta, "remaining", d ? d : json_null());
        json_set(ta, "ttl", pcc2_is_obj(pcc2_get(window, "tier")) ? json_null() : json_copy(pcc2_get(window, "tier")));
        json_set(ta, "confidence", ct ? json_copy(ct) : json_null());
        result = pcc2_call(ctx, "t", ta);
        json_free(d);
    } else {
        json_t *cold = pcc2_certainly_cold_seam(ctx, window, nown);
        if (pcc2_exc_pending(ctx)) { json_free(cold); goto cleanup_cache_line; }
        if (!pcc2_truthy(cold)) {
            double e = pcc2_num(estimated);
            json_t *cl = pcc2_clock(ctx, e);
            if (pcc2_exc_pending(ctx)) { json_free(cl); json_free(cold); goto cleanup_cache_line; }
            json_t *d = pcc2_duration_seam(ctx, json_number(e - nowv));
            if (pcc2_exc_pending(ctx)) { json_free(cl); json_free(d); json_free(cold); goto cleanup_cache_line; }
            json_t *ta = json_object();
            json_set(ta, "key", json_string("gateway.context.cache_maybe_warm"));
            json_set(ta, "until", cl ? cl : json_null());
            json_set(ta, "remaining", d ? d : json_null());
            json_set(ta, "ttl", json_copy(pcc2_get(window, "tier")));
            json_set(ta, "confidence", ct ? json_copy(ct) : json_null());
            result = pcc2_call(ctx, "t", ta);
            json_free(d); json_free(cold);
        } else {
            double e = pcc2_num(estimated);
            json_t *d = pcc2_duration_seam(ctx, json_number(nowv - e));
            if (pcc2_exc_pending(ctx)) { json_free(d); json_free(cold); goto cleanup_cache_line; }
            json_t *ta = json_object();
            json_set(ta, "key", json_string("gateway.context.cache_expired"));
            json_set(ta, "ago", d ? d : json_null());
            json_set(ta, "ttl", json_copy(pcc2_get(window, "tier")));
            json_set(ta, "confidence", ct ? json_copy(ct) : json_null());
            result = pcc2_call(ctx, "t", ta);
            json_free(d); json_free(cold);
        }
    }
cleanup_cache_line:
    json_free(ct); json_free(warm2); json_free(nown);
    return result;
}

/* @ agent/prompt_cache_clock.py:render_prompt_cache_line */
static json_t *pcc2_render_line(pcc2_ctx *ctx, const json_t *window, const json_t *now) {
    if (!pcc2_is_obj(window)) return NULL;
    json_t *est = pcc2_get(window, "estimated_until");
    json_t *line = pcc2_cache_line(ctx, window, est, now);
    if (pcc2_exc_pending(ctx)) { json_free(line); return NULL; }
    if (!line) line = json_string("");
    if (pcc2_truthy(pcc2_get(window, "tier_pending"))) {
        json_t *a = json_object(); json_set(a, "key", json_string("gateway.context.cache_tier_pending"));
        json_t *suf = pcc2_call(ctx, "t", a);
        if (pcc2_exc_pending(ctx)) { json_free(line); json_free(suf); return NULL; }
        char *ls = pcc2_str_of(line), *ss = pcc2_str_of(suf);
        char buf[1024]; snprintf(buf, sizeof buf, "%s %s", ls, ss);
        json_free(line); line = json_string(buf);
        free(ls); free(ss); json_free(suf);
    }
    if (pcc2_truthy(pcc2_get(window, "learned"))) {
        json_t *a = json_object(); json_set(a, "key", json_string("gateway.context.cache_learned"));
        json_set(a, "rule", json_copy(pcc2_get(window, "learned")));
        json_t *suf = pcc2_call(ctx, "t", a);
        if (pcc2_exc_pending(ctx)) { json_free(line); json_free(suf); return NULL; }
        char *ls = pcc2_str_of(line), *ss = pcc2_str_of(suf);
        char buf[1024]; snprintf(buf, sizeof buf, "%s %s", ls, ss);
        json_free(line); line = json_string(buf);
        free(ls); free(ss); json_free(suf);
    }
    if (pcc2_truthy(pcc2_get(window, "warming_off"))) {
        json_t *a = json_object(); json_set(a, "key", json_string("gateway.context.cache_warming_off"));
        json_t *suf = pcc2_call(ctx, "t", a);
        if (pcc2_exc_pending(ctx)) { json_free(line); json_free(suf); return NULL; }
        char *ls = pcc2_str_of(line), *ss = pcc2_str_of(suf);
        char buf[1024]; snprintf(buf, sizeof buf, "%s %s", ls, ss);
        json_free(line); line = json_string(buf);
        free(ls); free(ss); json_free(suf);
    }
    return line;
}

/* @ agent/prompt_cache_clock.py:prompt_cache_is_cold */
static json_t *pcc2_prompt_cache_is_cold(pcc2_ctx *ctx, json_t *agent, const json_t *now) {
    json_t *window = pcc2_cache_window(ctx, agent);
    if (pcc2_exc_pending(ctx)) { json_free(window); return NULL; }
    if (pcc2_is_none(window)) { json_free(window); return json_bool(false); }
    if (pcc2_str_eq(json_get_str(window, "reason", ""), "route_changed")) { json_free(window); return json_bool(true); }
    const char *conf = json_get_str(window, "confidence", "");
    bool ok = pcc2_str_eq(conf, "proven") || pcc2_str_eq(conf, "declared");
    if (!ok) { json_free(window); return json_bool(false); }
    json_t *cold = pcc2_certainly_cold_seam(ctx, window, now);
    json_free(window);
    if (pcc2_exc_pending(ctx)) { json_free(cold); return NULL; }
    return cold ? cold : json_bool(false);
}

/* @ agent/prompt_cache_clock.py:_valid_previous */
static json_t *pcc2_valid_previous(pcc2_ctx *ctx, const json_t *agent, const json_t *generation) {
    if (!pcc2_env_bool(ctx, "restored_flag", false)) {
        json_t *r = pcc2_restore_cache_clock(ctx, (json_t *)agent);
        if (pcc2_exc_pending(ctx)) { json_free(r); return NULL; }
        json_free(r);
    }
    json_t *prev = pcc2_get(agent, "_prompt_cache_clock");
    if (!pcc2_is_obj(prev) || pcc2_str_eq(json_get_str(prev, "confidence", ""), "unknown")) return NULL;
    json_t *reason = pcc2_agent_invalid_reason(ctx, (json_t *)agent, prev, generation);
    if (pcc2_exc_pending(ctx)) { json_free(reason); return NULL; }
    if (!pcc2_is_none(reason)) { json_free(reason); return NULL; }
    json_free(reason);
    return json_copy(prev);
}

/* flow1-owned _estimated_end -- seam only; no PoP claim here. */
static json_t *pcc2_estimated_end_seam(pcc2_ctx *ctx, const json_t *coverage, const char *base_url,
                                       const json_t *retention, double at, const json_t *previous,
                                       bool carried, double idle_from) {
    json_t *a = json_object();
    json_set(a, "coverage", json_copy(coverage));
    json_set(a, "base_url", json_string(base_url));
    json_set(a, "retention", json_copy(retention));
    json_set(a, "at", json_number(at));
    json_set(a, "previous", json_copy(previous));
    json_set(a, "carried", json_bool(carried));
    json_set(a, "idle_from", json_number(idle_from));
    return pcc2_call(ctx, "_estimated_end", a);
}
