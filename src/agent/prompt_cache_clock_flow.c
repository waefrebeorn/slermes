/* prompt_cache_clock_flow.c — agent/prompt_cache_clock.py FLOW + leaves.
 *
 * Decision-sequence port of the prompt-cache clock: cache-eligibility decision
 * chains (_invalid_reason, _confidence, classify_route), TTL/clock ordering
 * (_fold_coverage, _ends, _estimated_end, the refresh rules), breakpoint
 * selection / Anthropic cache-control projection (_block_marker_ttls,
 * _marker_ttl, _wire_field, attempt_facts), the route-rule verdicts
 * (_rule_anthropic_spec, _rule_openai_spec, _written_tiers), persistence error
 * paths (event_from_json, current_generation) and the consumer boundary
 * predicates (cache_certainly_warm/cold, _duration).
 *
 * ALL clock/storage/endpoint I/O rides the injected universal seam (pccf_call);
 * the harness scripts results and records the ordered trace the oracle diffs
 * against LIVE agent/prompt_cache_clock.py under the same scripts. Internal
 * support leaves (_list_field, _refresh_restart, _refresh_openai,
 * _managed_unload_end, _field, _storage_errors) are transcribed real, matching
 * the oracle's AST-extracted live defs.
 *
 * Representation contract (shared with tests/oracle_prompt_cache_clock_flow.py):
 *   - CacheEvent / _Ctx / _Verdict / CacheWindow doubles are attribute dicts;
 *     dataclass results project via asdict field order; tuples -> arrays;
 *     sets -> sorted arrays; None -> JSON null (C: NULL or JSON_NULL node).
 *   - integral floats collapse to ints on both sides (oracle norm); libjson
 *     emits %.0f for integral doubles and %.17g otherwise (round-trips).
 *   - uncaught exception -> {"__raise__": "<ClassName>"} (class only).
 *   - scripts: absent key -> None; {"$seq":[...]} per-call queue (exhaustion
 *     raises RuntimeError("script exhausted: <name>")); {"__raise__":cls,...}
 *     raises (trace ret is the WHOLE marker); any other value reused.
 *     None-valued seam arg keys are dropped on BOTH sides.
 *   - fixture domain: str()/float()/int() operands are ASCII strings, numbers,
 *     bools or null; _marker_ttl fixtures keep a UNIQUE minimum TTL seconds so
 *     the set-iteration tie never decides the result.
 *
 * libjson-only (default oracle deps suffice). Self-contained: the harness
 * #includes "agent/prompt_cache_clock_flow.c".
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

/* ── context + seam plumbing ─────────────────────────────────────────────── */

typedef struct pccf_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *env;       /* borrowed: scripted tables/constants */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} pccf_ctx;

static bool pccf_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}

static bool pccf_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }

static bool pccf_truthy(const json_t *v) {
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

static const char *pccf_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

static bool pccf_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool pccf_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool pccf_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool pccf_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }

static double pccf_num(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}

/* Deep Python-style equality; NULL pointer and JSON_NULL both read None.
 * Object key order is irrelevant (dict ==); array order matters (tuple ==). */
static bool pccf_json_eq(const json_t *a, const json_t *b) {
    if (pccf_is_none(a) && pccf_is_none(b)) return true;
    if (pccf_is_none(a) || pccf_is_none(b)) return false;
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return pccf_str_eq(a->str_val, b->str_val);
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!pccf_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !pccf_json_eq(a->c.items[i], bv)) return false;
            }
            return true;
        default: return true;
    }
}

/* ── exception plumbing ──────────────────────────────────────────────────── */

static bool pccf_exc_pending(const pccf_ctx *ctx) { return ctx && ctx->exc != NULL; }

static void pccf_exc_clear(pccf_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}

static void pccf_raise(pccf_ctx *ctx, const char *cls) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
}

static const char *pccf_exc_class(const pccf_ctx *ctx) {
    return json_get_str(ctx->exc, "__raise__", "");
}

/* Builtin exception subclass chain (parent lookup). Custom scripted classes
 * (PersistenceError, WorkerPersistenceError, ...) have no entry and read as
 * Exception subclasses (make_exc fallback). */
static const char *pccf_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        {"FileNotFoundError", "OSError"}, {"PermissionError", "OSError"},
        {"IsADirectoryError", "OSError"}, {"NotADirectoryError", "OSError"},
        {"FileExistsError", "OSError"}, {"InterruptedError", "OSError"},
        {"BlockingIOError", "OSError"}, {"ChildProcessError", "OSError"},
        {"ProcessLookupError", "OSError"}, {"ConnectionError", "OSError"},
        {"BrokenPipeError", "ConnectionError"}, {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
        {"TimeoutError", "OSError"},
        {"sqlite3.Error", "Exception"}, {"sqlite3.DatabaseError", "sqlite3.Error"},
        {"sqlite3.OperationalError", "sqlite3.DatabaseError"},
        {"sqlite3.IntegrityError", "sqlite3.DatabaseError"},
        {"sqlite3.ProgrammingError", "sqlite3.DatabaseError"},
        {"sqlite3.InterfaceError", "sqlite3.Error"},
        {"sqlite3.InternalError", "sqlite3.DatabaseError"},
        {"sqlite3.NotSupportedError", "sqlite3.DatabaseError"},
        {"sqlite3.DataError", "sqlite3.DatabaseError"},
        {"JSONDecodeError", "ValueError"}, {"UnicodeDecodeError", "UnicodeError"},
        {"UnicodeError", "ValueError"},
        {"ZeroDivisionError", "ArithmeticError"}, {"OverflowError", "ArithmeticError"},
        {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (pccf_str_eq(T[i].c, c)) return T[i].p;
    return NULL;
}

/* True when the pending exception matches one of `classes` (subclass-aware). */
static bool pccf_exc_matches(const pccf_ctx *ctx, const char *const *classes, size_t n) {
    if (!pccf_exc_pending(ctx)) return false;
    const char *cls = pccf_exc_class(ctx);
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (pccf_str_eq(cls, classes[i])) return true;
        const char *p = pccf_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}

/* ── universal seam ──────────────────────────────────────────────────────── */

static json_t *pccf_script_result(pccf_ctx *ctx, const json_t *script) {
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
static json_t *pccf_call(pccf_ctx *ctx, const char *name, json_t *args) {
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
    if (chosen != ctx->exc) ret = pccf_script_result(ctx, chosen);
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

/* Arg builders: None-valued keys are dropped on BOTH sides ("" is kept). */
static void pccf_arg_str(json_t *args, const char *key, const char *val) {
    if (!val) return;
    json_set(args, key, json_string(val));
}

/* ── TTL / confidence / retention tables (mirror live constants) ─────────── */

/* agent.prompt_caching.TTL_SECONDS */
static double pccf_ttl_get(const char *key, bool *found) {
    static const struct { const char *k; double s; } T[] = {
        {"5m", 300}, {"30m", 1800}, {"1h", 3600}, {"12h", 43200},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (pccf_str_eq(T[i].k, key)) { if (found) *found = true; return T[i].s; }
    if (found) *found = false;
    return 0;
}

static int pccf_conf_rank(const char *c, bool *found) {
    static const struct { const char *k; int r; } T[] = {
        {"unknown", 0}, {"assumed", 1}, {"declared", 2}, {"proven", 3},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (pccf_str_eq(T[i].k, c)) { if (found) *found = true; return T[i].r; }
    if (found) *found = false;
    return 0;
}

#define PCCF_STORAGE_UNAVAILABLE (-1.0)
#define PCCF_OPENAI_READ_FLOOR "30m"
#define PCCF_RETENTION_24H 86400.0

/* ── string-set helpers (out set / found set / clamped dedup) ────────────── */

static void pccf_set_add(json_t *set, const char *s) {
    json_set(set, s, json_bool(true));
}

static int pccf_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* Sorted array of a string-set's members (sets project as sorted arrays). */
static json_t *pccf_set_sorted(const json_t *set) {
    json_t *arr = json_array();
    if (!pccf_is_obj(set) || set->c.count == 0) return arr;
    const char **keys = (const char **)malloc(set->c.count * sizeof(char *));
    if (!keys) return arr;
    for (size_t i = 0; i < set->c.count; i++) keys[i] = set->c.keys[i];
    qsort(keys, set->c.count, sizeof(char *), pccf_cmp_str);
    for (size_t i = 0; i < set->c.count; i++) json_append(arr, json_string(keys[i]));
    free(keys);
    return arr;
}

/* ── numeric coercion (Python int()/float() fixture domain) ──────────────── */

/* float(v): number/bool -> double; ASCII string -> strtod (ValueError on a
 * non-numeric or partially-consumed string); anything else -> TypeError.
 * Returns false with ctx->exc set on error. */
static bool pccf_to_float(pccf_ctx *ctx, const json_t *v, double *out) {
    if (pccf_is_num(v)) { *out = v->num_val; return true; }
    if (v && v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return true; }
    if (pccf_is_str(v)) {
        const char *s = v->str_val ? v->str_val : "";
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\v' || *s == '\f') s++;
        if (*s == '\0') { pccf_raise(ctx, "ValueError"); return false; }
        char *end = NULL;
        double d = strtod(s, &end);
        if (end == s) { pccf_raise(ctx, "ValueError"); return false; }
        while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r' || *end == '\v' || *end == '\f') end++;
        if (*end != '\0') { pccf_raise(ctx, "ValueError"); return false; }
        *out = d;
        return true;
    }
    pccf_raise(ctx, "TypeError");
    return false;
}

/* int(v): number -> trunc toward zero; bool -> 0/1; ASCII string -> integer
 * parse (ValueError on failure); anything else -> TypeError. */
static bool pccf_to_int(pccf_ctx *ctx, const json_t *v, long long *out) {
    if (pccf_is_num(v)) { *out = (long long)v->num_val; return true; }
    if (v && v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return true; }
    if (pccf_is_str(v)) {
        const char *s = v->str_val ? v->str_val : "";
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\v' || *s == '\f') s++;
        if (*s == '\0') { pccf_raise(ctx, "ValueError"); return false; }
        char *end = NULL;
        long long n = strtoll(s, &end, 10);
        if (end == s) { pccf_raise(ctx, "ValueError"); return false; }
        while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r' || *end == '\v' || *end == '\f') end++;
        if (*end != '\0') { pccf_raise(ctx, "ValueError"); return false; }
        *out = n;
        return true;
    }
    pccf_raise(ctx, "TypeError");
    return false;
}

/* str(v) for string/int fixture fields (session_id). */
static char *pccf_str_of(const json_t *v) {
    char buf[64];
    if (pccf_is_str(v)) {
        const char *s = v->str_val ? v->str_val : "";
        size_t n = strlen(s) + 1;
        char *p = (char *)malloc(n);
        if (p) memcpy(p, s, n);
        return p;
    }
    if (pccf_is_num(v)) {
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
    } else if (v && v->type == JSON_BOOL) {
        snprintf(buf, sizeof(buf), "%s", v->bool_val ? "True" : "False");
    } else {
        snprintf(buf, sizeof(buf), "%s", "None");
    }
    char *p = (char *)malloc(strlen(buf) + 1);
    if (p) strcpy(p, buf);
    return p;
}

/* ── support leaves (transcribed real; oracle keeps the live defs) ───────── */

/* _list_field(api_kwargs, key) -> list */
static json_t *pccf_list_field(const json_t *api_kwargs, const char *key) {
    json_t *v = json_obj_get(api_kwargs, key);
    if (pccf_is_arr(v)) return json_copy(v);
    return json_array();
}

/* _refresh_restart(deadline, tier, at) -> at + TTL_SECONDS[tier] */
static double pccf_refresh_restart(double at, const char *tier) {
    return at + pccf_ttl_get(tier, NULL);
}

/* _refresh_openai(deadline, tier, at) -> max(deadline, at + TTL["30m"]) */
static double pccf_refresh_openai(double deadline, double at) {
    double floor = at + pccf_ttl_get(PCCF_OPENAI_READ_FLOOR, NULL);
    return deadline > floor ? deadline : floor;
}

/* _field(window, name): dict.get / getattr — same read for our doubles. */
static json_t *pccf_field(const json_t *window, const char *name) {
    if (!pccf_is_obj(window)) return NULL;
    return json_obj_get(window, name);
}

/* tuple(v): array -> copy; string -> chars; object -> keys; else TypeError.
 * Returns NULL with ctx->exc set when v is not iterable. */
static json_t *pccf_tupleify(pccf_ctx *ctx, const json_t *v) {
    if (pccf_is_arr(v)) return json_copy(v);
    if (pccf_is_str(v)) {
        json_t *arr = json_array();
        const char *s = v->str_val ? v->str_val : "";
        for (const char *p = s; *p; p++) {
            char ch[2] = { *p, '\0' };
            json_append(arr, json_string(ch));
        }
        return arr;
    }
    if (pccf_is_obj(v)) {
        json_t *arr = json_array();
        for (size_t i = 0; i < v->c.count; i++) json_append(arr, json_string(v->c.keys[i]));
        return arr;
    }
    pccf_raise(ctx, "TypeError");
    return NULL;
}

/* _managed_unload_end(base_url, idle_from): is_managed_endpoint seam + the
 * scripted IDLE_UNLOAD_S / _SWEEP_EVERY_S constants. */
static json_t *pccf_managed_unload_end(pccf_ctx *ctx, const char *base_url, double idle_from) {
    json_t *args = json_object();
    pccf_arg_str(args, "base_url", base_url);
    json_t *managed = pccf_call(ctx, "is_managed_endpoint", args);
    bool is_managed = pccf_truthy(managed);
    json_free(managed);
    if (pccf_exc_pending(ctx)) return NULL;
    if (!is_managed) return NULL;
    double unload = json_get_num(ctx->env, "idle_unload_s", 0);
    double sweep = json_get_num(ctx->env, "sweep_every_s", 0);
    return json_number(idle_from + unload + 2.0 * sweep);
}

/* ── request side ────────────────────────────────────────────────────────── */

/* @ agent/prompt_cache_clock.py:_block_marker_ttls */
static void pccf_block_marker_ttls(pccf_ctx *ctx, const json_t *block, json_t *out) {
    (void)ctx;
    if (!pccf_is_obj(block)) return;
    json_t *content = json_obj_get(block, "content");
    /* parts = (block, *content if isinstance(content, list) else ()) */
    size_t nextra = pccf_is_arr(content) ? content->c.count : 0;
    for (size_t i = 0; i <= nextra; i++) {
        const json_t *part = (i == 0) ? block : content->c.items[i - 1];
        if (!pccf_is_obj(part)) continue;
        json_t *marker = json_obj_get(part, "cache_control");
        if (!pccf_is_obj(marker)) continue;
        json_t *ttl = json_obj_get(marker, "ttl");
        /* marker.get("ttl") or "5m" — falsy ttl defaults; fixture ttl is a
         * string or falsy (null/""/0/false). */
        const char *t = (pccf_truthy(ttl) && pccf_is_str(ttl)) ? ttl->str_val : "5m";
        pccf_set_add(out, t);
    }
}

/* @ agent/prompt_cache_clock.py:_marker_ttl */
static json_t *pccf_marker_ttl(pccf_ctx *ctx, const json_t *api_kwargs) {
    json_t *found = json_object();
    json_t *system = pccf_list_field(api_kwargs, "system");
    json_t *messages = pccf_list_field(api_kwargs, "messages");
    json_t *tools = pccf_list_field(api_kwargs, "tools");
    /* blocks = (*system, *messages[:2], *messages[-4:], *tools[-1:]) */
    json_t *blocks = json_array();
    for (size_t i = 0; i < system->c.count; i++) json_append(blocks, json_copy(system->c.items[i]));
    size_t mn = messages->c.count;
    for (size_t i = 0; i < mn && i < 2; i++) json_append(blocks, json_copy(messages->c.items[i]));
    size_t start = mn > 4 ? mn - 4 : 0;
    for (size_t i = start; i < mn; i++) json_append(blocks, json_copy(messages->c.items[i]));
    if (tools->c.count > 0) json_append(blocks, json_copy(tools->c.items[tools->c.count - 1]));
    for (size_t i = 0; i < blocks->c.count; i++)
        pccf_block_marker_ttls(ctx, blocks->c.items[i], found);
    json_free(system); json_free(messages); json_free(tools); json_free(blocks);
    json_t *ret = NULL;
    if (found->c.count > 0) {
        double best = 0; const char *best_key = NULL; bool first = true;
        for (size_t i = 0; i < found->c.count; i++) {
            const char *k = found->c.keys[i];
            double s = pccf_ttl_get(k, NULL);
            if (first || s < best) { best = s; best_key = k; first = false; }
        }
        if (best_key) ret = json_string(best_key);
    }
    json_free(found);
    return ret;
}

/* @ agent/prompt_cache_clock.py:_wire_field */
static json_t *pccf_wire_field(const json_t *api_kwargs, const char *key) {
    if (json_has(api_kwargs, key)) return json_copy(json_obj_get(api_kwargs, key));
    json_t *extra = json_obj_get(api_kwargs, "extra_body");
    if (pccf_is_obj(extra)) return json_copy(json_obj_get(extra, key));
    return NULL;
}

/* @ agent/prompt_cache_clock.py:attempt_facts */
static json_t *pccf_attempt_facts(pccf_ctx *ctx, const json_t *api_kwargs) {
    if (!pccf_is_obj(api_kwargs)) return NULL;
    json_t *options = pccf_wire_field(api_kwargs, "prompt_cache_options");
    /* dict literal evaluation order: sent_at (time seam) first. */
    json_t *sent = pccf_call(ctx, "time", NULL);
    if (pccf_exc_pending(ctx)) { json_free(options); json_free(sent); return NULL; }
    json_t *out = json_object();
    json_set(out, "sent_at", sent ? sent : json_null());
    json_t *mt = pccf_marker_ttl(ctx, api_kwargs);
    json_set(out, "marker_ttl", mt ? mt : json_null());
    json_t *openai_ttl = NULL;
    if (pccf_is_obj(options)) openai_ttl = json_copy(json_obj_get(options, "ttl"));
    json_set(out, "openai_ttl", openai_ttl ? openai_ttl : json_null());
    json_t *retention = pccf_wire_field(api_kwargs, "prompt_cache_retention");
    json_set(out, "retention", retention ? retention : json_null());
    json_free(options);
    return out;
}

/* ── route classes and rules ─────────────────────────────────────────────── */

/* @ agent/prompt_cache_clock.py:classify_route */
static json_t *pccf_classify_route(bool has_markers) {
    return json_string(has_markers ? "anthropic_spec" : "openai_spec");
}

/* @ agent/prompt_cache_clock.py:_written_tiers */
static json_t *pccf_written_tiers(const json_t *tiers) {
    json_t *arr = json_array();
    if (!pccf_is_obj(tiers)) return arr;
    /* collect (key, seconds) for n > 0 and key in TTL_SECONDS, stable sort */
    size_t n = tiers->c.count;
    const char **keys = (const char **)malloc((n ? n : 1) * sizeof(char *));
    double *secs = (double *)malloc((n ? n : 1) * sizeof(double));
    size_t cnt = 0;
    if (keys && secs) {
        for (size_t i = 0; i < n; i++) {
            json_t *cv = tiers->c.items[i];
            bool gt0 = (pccf_is_num(cv) && cv->num_val > 0) ||
                       (cv && cv->type == JSON_BOOL && cv->bool_val);
            bool found = false;
            double s = pccf_ttl_get(tiers->c.keys[i], &found);
            if (gt0 && found) { keys[cnt] = tiers->c.keys[i]; secs[cnt] = s; cnt++; }
        }
        /* stable insertion sort by seconds ascending */
        for (size_t i = 1; i < cnt; i++) {
            const char *k = keys[i]; double s = secs[i];
            size_t j = i;
            while (j > 0 && secs[j - 1] > s) { keys[j] = keys[j - 1]; secs[j] = secs[j - 1]; j--; }
            keys[j] = k; secs[j] = s;
        }
        for (size_t i = 0; i < cnt; i++) json_append(arr, json_string(keys[i]));
    }
    free(keys); free(secs);
    return arr;
}

static json_t *pccf_verdict(json_t *tiers, const char *confidence, const char *reason) {
    json_t *v = json_object();
    json_set(v, "tiers", tiers);
    json_set(v, "confidence", json_string(confidence));
    json_set(v, "reason", json_string(reason));
    return v;
}

/* @ agent/prompt_cache_clock.py:_rule_anthropic_spec */
static json_t *pccf_rule_anthropic_spec(pccf_ctx *ctx, const json_t *c) {
    json_t *write_tiers = json_obj_get(c, "write_tiers");
    json_t *proven = pccf_written_tiers(write_tiers);
    json_t *tiers; const char *confidence; const char *reason;
    bool is_proven = proven->c.count > 0;
    if (is_proven) {
        tiers = proven; confidence = "proven"; reason = "anthropic_usage_split";
    } else {
        json_free(proven);
        json_t *req = json_obj_get(c, "requested_ttl");
        const char *r = (pccf_truthy(req) && pccf_is_str(req)) ? req->str_val : "5m";
        tiers = json_array();
        json_append(tiers, json_string(r));
        confidence = "declared"; reason = "anthropic_requested";
    }
    json_t *ceiling = json_obj_get(c, "ceiling");
    if (pccf_is_none(ceiling))
        return pccf_verdict(tiers, confidence, reason);
    const char *ceil = pccf_jstr(ceiling);
    bool cfound = false;
    double ceil_s = pccf_ttl_get(ceil, &cfound);
    if (!cfound) { pccf_raise(ctx, "KeyError"); json_free(tiers); return NULL; }
    bool all_within = true;
    for (size_t i = 0; i < tiers->c.count; i++) {
        double s = pccf_ttl_get(pccf_jstr(tiers->c.items[i]), NULL);
        if (s > ceil_s) { all_within = false; break; }
    }
    if (all_within) {
        char rbuf[128];
        const char *rout = reason;
        if (!is_proven) { snprintf(rbuf, sizeof(rbuf), "route_ceiling_%s", ceil); rout = rbuf; }
        return pccf_verdict(tiers, confidence, rout);
    }
    /* clamped = sorted({min((t, ceiling), key=TTL.get) for t in tiers}, key=TTL[]) */
    json_t *set = json_object();
    for (size_t i = 0; i < tiers->c.count; i++) {
        const char *t = pccf_jstr(tiers->c.items[i]);
        double ts = pccf_ttl_get(t, NULL);
        const char *pick = (ts <= ceil_s) ? t : ceil;  /* tie -> t (first) */
        pccf_set_add(set, pick);
    }
    json_free(tiers);
    /* sort set members by TTL seconds ascending (all valid TTL keys here) */
    json_t *clamped = json_array();
    size_t n = set->c.count;
    const char **keys = (const char **)malloc((n ? n : 1) * sizeof(char *));
    double *secs = (double *)malloc((n ? n : 1) * sizeof(double));
    if (keys && secs) {
        for (size_t i = 0; i < n; i++) { keys[i] = set->c.keys[i]; secs[i] = pccf_ttl_get(set->c.keys[i], NULL); }
        for (size_t i = 1; i < n; i++) {
            const char *k = keys[i]; double s = secs[i]; size_t j = i;
            while (j > 0 && secs[j - 1] > s) { keys[j] = keys[j - 1]; secs[j] = secs[j - 1]; j--; }
            keys[j] = k; secs[j] = s;
        }
        for (size_t i = 0; i < n; i++) json_append(clamped, json_string(keys[i]));
    }
    free(keys); free(secs); json_free(set);
    char rbuf[128];
    snprintf(rbuf, sizeof(rbuf), "route_ceiling_%s", ceil);
    return pccf_verdict(clamped, "declared", rbuf);
}

/* @ agent/prompt_cache_clock.py:_rule_openai_spec */
static json_t *pccf_rule_openai_spec(const json_t *c) {
    json_t *proven = pccf_written_tiers(json_obj_get(c, "write_tiers"));
    if (proven->c.count > 0)
        return pccf_verdict(proven, "proven", "openai_write_tier");
    json_free(proven);
    json_t *tiers = json_array();
    json_append(tiers, json_string(PCCF_OPENAI_READ_FLOOR));
    return pccf_verdict(tiers, "declared", "openai_spec_30m");
}

/* @ agent/prompt_cache_clock.py:_fold_coverage */
static json_t *pccf_fold_coverage(pccf_ctx *ctx, const json_t *previous, const json_t *verdict,
                                  const char *route_class, double at, long long read, long long write) {
    bool anthropic = pccf_str_eq(route_class, "anthropic_spec");
    bool openai = pccf_str_eq(route_class, "openai_spec");
    json_t *coverage = json_object();
    if (!anthropic && !openai) {
        json_t *r = json_array();
        json_append(r, coverage);
        json_append(r, json_bool(false));
        return r;
    }
    if (read > 0 && !pccf_is_none(previous)) {
        json_t *pcov = json_obj_get(previous, "coverage");
        if (pccf_is_obj(pcov)) {
            for (size_t i = 0; i < pcov->c.count; i++) {
                const char *tier = pcov->c.keys[i];
                bool found = false;
                pccf_ttl_get(tier, &found);
                double deadline = pccf_num(pcov->c.items[i]);
                if (!found || !(deadline > at)) continue;
                double refreshed = anthropic ? pccf_refresh_restart(at, tier)
                                             : pccf_refresh_openai(deadline, at);
                json_set(coverage, tier, json_number(refreshed));
            }
        }
    }
    bool carried = coverage->c.count > 0;
    if (write > 0 || !carried) {
        json_t *vt = json_obj_get(verdict, "tiers");
        if (pccf_is_arr(vt)) {
            for (size_t i = 0; i < vt->c.count; i++) {
                const char *tier = pccf_jstr(vt->c.items[i]);
                bool found = false;
                double ts = pccf_ttl_get(tier, &found);
                if (!found) { pccf_raise(ctx, "KeyError"); json_free(coverage); return NULL; }
                double deadline = at + ts;
                json_t *ex = json_obj_get(coverage, tier);
                double val = (ex && pccf_is_num(ex) && ex->num_val < deadline) ? ex->num_val : deadline;
                json_set(coverage, tier, json_number(val));
            }
        }
    }
    json_t *r = json_array();
    json_append(r, coverage);
    json_append(r, json_bool(carried));
    return r;
}

/* @ agent/prompt_cache_clock.py:_ends */
static json_t *pccf_ends(const json_t *coverage) {
    json_t *r = json_array();
    if (!pccf_is_obj(coverage) || coverage->c.count == 0) {
        json_append(r, json_null());
        json_append(r, json_null());
        return r;
    }
    double mn = 0, mx = 0; bool first = true;
    for (size_t i = 0; i < coverage->c.count; i++) {
        double v = pccf_num(coverage->c.items[i]);
        if (first) { mn = mx = v; first = false; }
        else { if (v < mn) mn = v; if (v > mx) mx = v; }
    }
    json_append(r, json_number(mn));
    json_append(r, json_number(mx));
    return r;
}

/* @ agent/prompt_cache_clock.py:_estimated_end */
static json_t *pccf_estimated_end(pccf_ctx *ctx, const json_t *coverage, const char *base_url,
                                  const json_t *retention, double at, const json_t *previous,
                                  bool carried, double idle_from) {
    if (!pccf_is_obj(coverage) || coverage->c.count == 0) return NULL;
    json_t *args = json_object();
    pccf_arg_str(args, "base_url", base_url);
    json_t *local = pccf_call(ctx, "is_local_endpoint", args);
    bool is_local = pccf_truthy(local);
    json_free(local);
    if (pccf_exc_pending(ctx)) return NULL;
    if (is_local) {
        double idle = at > idle_from ? at : idle_from;
        return pccf_managed_unload_end(ctx, base_url, idle);
    }
    double estimated = 0; bool first = true;
    for (size_t i = 0; i < coverage->c.count; i++) {
        double v = pccf_num(coverage->c.items[i]);
        if (first || v > estimated) { estimated = v; first = false; }
    }
    if (pccf_is_str(retention) && pccf_str_eq(retention->str_val, "24h")) {
        double stretch = at + PCCF_RETENTION_24H;
        if (stretch > estimated) estimated = stretch;
    }
    if (carried && !pccf_is_none(previous)) {
        json_t *pe = json_obj_get(previous, "estimated_until");
        if (!pccf_is_none(pe)) {
            double pv = pccf_num(pe);
            if (pv > estimated) estimated = pv;
        }
    }
    return json_number(estimated);
}

/* @ agent/prompt_cache_clock.py:_confidence */
static json_t *pccf_confidence(pccf_ctx *ctx, const json_t *verdict, const json_t *previous,
                               bool carried, long long write) {
    const char *vconf = pccf_jstr(json_obj_get(verdict, "confidence"));
    const char *vreason = pccf_jstr(json_obj_get(verdict, "reason"));
    json_t *r = json_array();
    if (!carried || pccf_is_none(previous)) {
        json_append(r, json_string(vconf));
        json_append(r, json_string(vreason));
        return r;
    }
    const char *pconf = pccf_jstr(json_obj_get(previous, "confidence"));
    const char *preason = pccf_jstr(json_obj_get(previous, "reason"));
    if (write == 0) {
        json_append(r, json_string(pconf));
        json_append(r, json_string(preason));
        return r;
    }
    bool vf = false, pf = false;
    int vr = pccf_conf_rank(vconf, &vf);
    int pr = pccf_conf_rank(pconf, &pf);
    if (!vf || !pf) { pccf_raise(ctx, "KeyError"); json_free(r); return NULL; }
    const char *conf = (vr <= pr) ? vconf : pconf;  /* min ties -> first (verdict) */
    json_append(r, json_string(conf));
    json_append(r, json_string(vreason));
    return r;
}

/* ── identity / eligibility ──────────────────────────────────────────────── */

/* @ agent/prompt_cache_clock.py:_invalid_reason */
static json_t *pccf_invalid_reason(const json_t *event, const char *session_id,
                                   const json_t *generation, const json_t *route) {
    const char *ev_sid = pccf_jstr(json_obj_get(event, "session_id"));
    if (ev_sid[0] != '\0' && !pccf_str_eq(ev_sid, session_id))
        return json_string("session_rotated");
    json_t *ev_gen = json_obj_get(event, "generation");
    bool gen_unavail = pccf_is_num(generation) && generation->num_val == PCCF_STORAGE_UNAVAILABLE;
    bool ev_unavail = pccf_is_num(ev_gen) && ev_gen->num_val == PCCF_STORAGE_UNAVAILABLE;
    if (gen_unavail || ev_unavail)
        return json_string("storage_unavailable");
    if (!pccf_is_none(generation) && !pccf_json_eq(ev_gen, generation))
        return json_string("compacted");
    if (!pccf_is_none(route)) {
        json_t *ev_route = json_obj_get(event, "route");
        if (!pccf_json_eq(ev_route, route))
            return json_string("route_changed");
    }
    return NULL;
}

/* @ agent/prompt_cache_clock.py:current_generation */
static json_t *pccf_current_generation(pccf_ctx *ctx, bool has_reader, const json_t *session_id) {
    if (!pccf_truthy(session_id) || !has_reader) return NULL;
    char *sid = pccf_str_of(session_id);
    json_t *args = json_object();
    pccf_arg_str(args, "session_id", sid ? sid : "");
    free(sid);
    json_t *value = pccf_call(ctx, "prompt_cache_generation", args);
    if (pccf_exc_pending(ctx)) {
        static const char *const STORAGE_ERRORS[] = {
            "sqlite3.Error", "OSError", "PersistenceError", "WorkerPersistenceError",
        };
        if (pccf_exc_matches(ctx, STORAGE_ERRORS, 4)) {
            pccf_exc_clear(ctx);
            json_free(value);
            return json_number(PCCF_STORAGE_UNAVAILABLE);
        }
        json_free(value);
        return NULL;  /* propagate */
    }
    /* value if isinstance(value, int) else None — bool is an int subclass */
    if (pccf_is_num(value) || (value && value->type == JSON_BOOL)) return value;
    json_free(value);
    return NULL;
}

/* ── persistence ─────────────────────────────────────────────────────────── */

/* @ agent/prompt_cache_clock.py:event_from_json */
static json_t *pccf_event_from_json(pccf_ctx *ctx, const json_t *raw) {
    if (!pccf_is_str(raw)) return NULL;
    json_t *data = json_parse(raw->str_val ? raw->str_val : "", NULL);
    if (!data) return NULL;                 /* json.loads ValueError -> None */
    if (!pccf_is_obj(data)) { json_free(data); return NULL; }
    /* legacy route_class mapping (inserts the key even when absent -> None) */
    json_t *rc = json_obj_get(data, "route_class");
    json_t *mapped = NULL;
    if (pccf_is_str(rc)) {
        const char *m = NULL;
        static const struct { const char *k, *v; } L[] = {
            {"openai_ttl", "openai_spec"}, {"openai_retention", "openai_spec"},
            {"implicit", "openai_spec"}, {"route_ceiling", "anthropic_spec"},
            {"anthropic_native", "anthropic_spec"}, {"openrouter_claude", "anthropic_spec"},
            {"anthropic_gateway", "anthropic_spec"},
        };
        for (size_t i = 0; i < sizeof(L) / sizeof(L[0]); i++)
            if (pccf_str_eq(L[i].k, rc->str_val)) { m = L[i].v; break; }
        mapped = m ? json_string(m) : json_copy(rc);
    } else {
        mapped = rc ? json_copy(rc) : json_null();
    }
    json_set(data, "route_class", mapped);
    /* route / destination must exist (KeyError) and be iterable (TypeError) */
    if (!json_has(data, "route") || !json_has(data, "destination")) { json_free(data); return NULL; }
    json_t *route = pccf_tupleify(ctx, json_obj_get(data, "route"));
    if (!route) { json_free(data); return NULL; }
    json_t *dest = pccf_tupleify(ctx, json_obj_get(data, "destination"));
    if (!dest) { json_free(route); json_free(data); return NULL; }
    /* required fields (route_class always present after the legacy line) */
    static const char *const REQ[] = {
        "requested_ttl", "sent_at", "read_tokens", "write_tokens", "tier", "confidence", "reason",
    };
    for (size_t i = 0; i < sizeof(REQ) / sizeof(REQ[0]); i++)
        if (!json_has(data, REQ[i])) { json_free(route); json_free(dest); json_free(data); return NULL; }
    /* extra keys -> TypeError */
    static const char *const KNOWN[] = {
        "route", "destination", "route_class", "requested_ttl", "sent_at", "read_tokens",
        "write_tokens", "tier", "confidence", "reason", "coverage", "guaranteed_until",
        "estimated_until", "session_id", "generation", "upstream",
    };
    for (size_t i = 0; i < data->c.count; i++) {
        bool ok = false;
        for (size_t j = 0; j < sizeof(KNOWN) / sizeof(KNOWN[0]); j++)
            if (pccf_str_eq(data->c.keys[i], KNOWN[j])) { ok = true; break; }
        if (!ok) { json_free(route); json_free(dest); json_free(data); return NULL; }
    }
    /* build the event in asdict field order, filling defaults for absent keys */
    json_t *ev = json_object();
    json_set(ev, "route", route);
    json_set(ev, "destination", dest);
    json_set(ev, "route_class", json_copy(json_obj_get(data, "route_class")));
    json_set(ev, "requested_ttl", json_copy(json_obj_get(data, "requested_ttl")));
    json_set(ev, "sent_at", json_copy(json_obj_get(data, "sent_at")));
    json_set(ev, "read_tokens", json_copy(json_obj_get(data, "read_tokens")));
    json_set(ev, "write_tokens", json_copy(json_obj_get(data, "write_tokens")));
    json_set(ev, "tier", json_copy(json_obj_get(data, "tier")));
    json_set(ev, "confidence", json_copy(json_obj_get(data, "confidence")));
    json_set(ev, "reason", json_copy(json_obj_get(data, "reason")));
    json_set(ev, "coverage", json_has(data, "coverage") ? json_copy(json_obj_get(data, "coverage")) : json_object());
    json_set(ev, "guaranteed_until", json_has(data, "guaranteed_until") ? json_copy(json_obj_get(data, "guaranteed_until")) : json_null());
    json_set(ev, "estimated_until", json_has(data, "estimated_until") ? json_copy(json_obj_get(data, "estimated_until")) : json_null());
    json_set(ev, "session_id", json_has(data, "session_id") ? json_copy(json_obj_get(data, "session_id")) : json_string(""));
    json_set(ev, "generation", json_has(data, "generation") ? json_copy(json_obj_get(data, "generation")) : json_null());
    json_set(ev, "upstream", json_has(data, "upstream") ? json_copy(json_obj_get(data, "upstream")) : json_null());
    json_free(data);
    return ev;
}

/* ── consumer boundary predicates ────────────────────────────────────────── */

/* @ agent/prompt_cache_clock.py:cache_certainly_warm */
static json_t *pccf_cache_certainly_warm(pccf_ctx *ctx, const json_t *window, const json_t *now) {
    json_t *guaranteed = pccf_is_none(window) ? NULL : pccf_field(window, "guaranteed_until");
    if (pccf_is_none(guaranteed)) return json_bool(false);
    double now_val;
    if (pccf_is_none(now)) {
        json_t *t = pccf_call(ctx, "time", NULL);
        if (pccf_exc_pending(ctx)) { json_free(t); return NULL; }
        now_val = pccf_num(t);
        json_free(t);
    } else {
        now_val = pccf_num(now);
    }
    double g;
    if (!pccf_to_float(ctx, guaranteed, &g)) return NULL;
    return json_bool(now_val < g);
}

/* @ agent/prompt_cache_clock.py:cache_certainly_cold */
static json_t *pccf_cache_certainly_cold(pccf_ctx *ctx, const json_t *window, const json_t *now) {
    if (pccf_is_none(window)) return json_bool(false);
    json_t *conf = pccf_field(window, "confidence");
    if (pccf_is_str(conf) && pccf_str_eq(conf->str_val, "unknown")) return json_bool(false);
    json_t *estimated = pccf_field(window, "estimated_until");
    if (pccf_is_none(estimated)) return json_bool(false);
    double now_val;
    if (pccf_is_none(now)) {
        json_t *t = pccf_call(ctx, "time", NULL);
        if (pccf_exc_pending(ctx)) { json_free(t); return NULL; }
        now_val = pccf_num(t);
        json_free(t);
    } else {
        now_val = pccf_num(now);
    }
    double e;
    if (!pccf_to_float(ctx, estimated, &e)) return NULL;
    return json_bool(now_val >= e);
}

/* @ agent/prompt_cache_clock.py:_duration */
static json_t *pccf_duration(pccf_ctx *ctx, const json_t *seconds) {
    long long s;
    if (!pccf_to_int(ctx, seconds, &s)) return NULL;
    if (s < 0) s = 0;
    char buf[64];
    if (s < 60) {
        snprintf(buf, sizeof(buf), "%llds", s);
    } else {
        long long minutes = s / 60, secs = s % 60;
        if (minutes < 60) {
            if (secs) snprintf(buf, sizeof(buf), "%lldm %llds", minutes, secs);
            else snprintf(buf, sizeof(buf), "%lldm", minutes);
        } else {
            long long hours = minutes / 60; minutes = minutes % 60;
            if (minutes) snprintf(buf, sizeof(buf), "%lldh %lldm", hours, minutes);
            else snprintf(buf, sizeof(buf), "%lldh", hours);
        }
    }
    return json_string(buf);
}