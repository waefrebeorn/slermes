/* agent_runtime_helpers_flow2.c — FLOW + leaves port of the UNCLAIMED remainder
 * of agent/agent_runtime_helpers.py (the claimed siblings live in
 * port_agent_runtime_helpers_flow.c / agent_runtime_helpers*.c / runtime_helpers_ports.c
 * and are SEAMED here).
 *
 * Decision-sequence port: the pre-API message-sanitizer passes (drop invalid
 * roles / empty tool_calls arrays / results without ids, repair invalid
 * function.name, positional pairing, id dedup, result-name realign), the
 * corruption-marker / tool-result find leaves, tool_results_this_turn, the
 * API-error debug projection, credential-pool identity / sync / revert flows,
 * primary-runtime field apply + capability restore, the model-switch snapshot
 * pair, the pre-tool plugin hook gate, and the steer-requeue / auth-recovery
 * delegations. ALL collaborator I/O (claimed siblings, foreign-module imports,
 * run_agent logger / AIAgent statics, pool methods, plugin hooks, logger) rides
 * the injected universal seam arh2_call; the harness scripts results and records
 * the ordered trace the oracle diffs against LIVE agent/agent_runtime_helpers.py
 * under the same scripts. Ported functions call each other REAL where composed.
 *
 * Representation contract (shared with tests/oracle_agent_runtime_helpers_flow2.py):
 *   - object doubles (agent/pool/entry/error.response) are attribute dicts read
 *     via json_obj_get; tuple/set -> array (sets sorted); None -> JSON null;
 *     integral floats collapse to ints on both sides.
 *   - uncaught exception -> {"__raise__": cls} (class only); an exception passed
 *     as a logger arg projects to {"__raise__": cls, "msg": str(e)}.
 *   - scripts: absent key -> None; {"$seq":[...]} per-call queue (exhaustion
 *     raises RuntimeError("script exhausted: <name>")); {"__raise__":cls,"msg":m}
 *     raises (trace ret is the WHOLE marker); any other value reused. None-valued
 *     seam arg keys dropped on BOTH sides.
 *   - the _MISSING sentinel projects to {"__missing__": true}.
 *   - object-valued seam results consumed by `if obj:` truthiness (pool.current)
 *     are null or a NON-EMPTY record; empty {} is outside that live domain.
 *   - fixture domain: str operands ASCII; .strip() ASCII whitespace; tool_call
 *     entries are dicts with dict-or-null function; function.name / tool_call_id
 *     are str-or-null; json.dumps content is ASCII scalars/containers and
 *     float-free (the C dumps renderer formats integral doubles as ints, so a
 *     1.0 literal would diverge from Python json.dumps' "1.0"); agent doubles are
 *     plain attribute namespaces (no property setters, so base_url does not
 *     derive _base_url_lower).
 *
 * libjson-only (default oracle deps suffice). Self-contained: the harness
 * #includes "agent/agent_runtime_helpers_flow2.c". arh_/arhf_ prefixes are taken
 * by the claimed ports; this port uses arh2_.
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>

/* ── context + seam plumbing ─────────────────────────────────────────────── */

typedef struct arh2_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *env;       /* borrowed: scripted constants/flags */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} arh2_ctx;

static bool arh2_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool arh2_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool arh2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool arh2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool arh2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool arh2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool arh2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static bool arh2_truthy(const json_t *v) {
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

static const char *arh2_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static double arh2_num(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}

/* borrowed object field or NULL */
static json_t *arh2_get(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}
static bool arh2_has(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT && json_obj_get(obj, key) != NULL;
}
/* dict.get(key, default): present-null keeps the null node; absent -> default. */
static json_t *arh2_get_def(const json_t *obj, const char *key, json_t *def) {
    if (obj && obj->type == JSON_OBJECT) {
        json_t *v = json_obj_get(obj, key);
        if (v) return v;
    }
    return def;
}
static json_t *arh2_copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}

/* Deep Python-style equality; NULL pointer and JSON_NULL both read None. */
static bool arh2_json_eq(const json_t *a, const json_t *b) {
    if (arh2_is_none(a) && arh2_is_none(b)) return true;
    if (arh2_is_none(a) || arh2_is_none(b)) return false;
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return arh2_str_eq(a->str_val, b->str_val);
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!arh2_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !arh2_json_eq(a->c.items[i], bv)) return false;
            }
            return true;
        default: return true;
    }
}

/* ── string helpers (caller frees) ───────────────────────────────────────── */

static bool arh2_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static char *arh2_strip(const char *s) {
    if (!s) return strdup("");
    const char *b = s;
    while (*b && arh2_is_space(*b)) b++;
    const char *e = b + strlen(b);
    while (e > b && arh2_is_space(e[-1])) e--;
    size_t n = (size_t)(e - b);
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, b, n); r[n] = '\0';
    return r;
}

/* growable string buffer */
typedef struct { char *p; size_t len, cap; } arh2_sb;
static void arh2_sb_init(arh2_sb *s) { s->cap = 64; s->len = 0; s->p = (char *)malloc(s->cap); if (s->p) s->p[0] = '\0'; }
static void arh2_sb_put(arh2_sb *s, const char *t) {
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
static void arh2_sb_putc(arh2_sb *s, char c) { char t[2] = {c, '\0'}; arh2_sb_put(s, t); }

/* Python json.dumps (ensure_ascii=True, separators=(", ", ": ")) for the ASCII
 * scalar/container domain. Caller frees. */
static void arh2_dumps_into(arh2_sb *s, const json_t *v) {
    if (arh2_is_none(v)) { arh2_sb_put(s, "null"); return; }
    if (arh2_is_bool(v)) { arh2_sb_put(s, v->bool_val ? "true" : "false"); return; }
    if (arh2_is_num(v)) {
        char buf[64]; double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
        else snprintf(buf, sizeof buf, "%g", d);
        arh2_sb_put(s, buf); return;
    }
    if (arh2_is_str(v)) {
        arh2_sb_putc(s, '"');
        for (const char *p = arh2_jstr(v); *p; p++) {
            unsigned char c = (unsigned char)*p;
            switch (c) {
                case '"': arh2_sb_put(s, "\\\""); break;
                case '\\': arh2_sb_put(s, "\\\\"); break;
                case '\n': arh2_sb_put(s, "\\n"); break;
                case '\r': arh2_sb_put(s, "\\r"); break;
                case '\t': arh2_sb_put(s, "\\t"); break;
                case '\b': arh2_sb_put(s, "\\b"); break;
                case '\f': arh2_sb_put(s, "\\f"); break;
                default:
                    if (c < 0x20) { char e[8]; snprintf(e, sizeof e, "\\u%04x", c); arh2_sb_put(s, e); }
                    else arh2_sb_putc(s, (char)c);
            }
        }
        arh2_sb_putc(s, '"'); return;
    }
    if (arh2_is_arr(v)) {
        arh2_sb_putc(s, '[');
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) arh2_sb_put(s, ", ");
            arh2_dumps_into(s, v->c.items[i]);
        }
        arh2_sb_putc(s, ']'); return;
    }
    if (arh2_is_obj(v)) {
        arh2_sb_putc(s, '{');
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) arh2_sb_put(s, ", ");
            json_t *k = json_string(v->c.keys[i]);
            arh2_dumps_into(s, k);
            json_free(k);
            arh2_sb_put(s, ": ");
            arh2_dumps_into(s, v->c.items[i]);
        }
        arh2_sb_putc(s, '}'); return;
    }
}
static char *arh2_py_json_dumps(const json_t *v) {
    arh2_sb s; arh2_sb_init(&s);
    arh2_dumps_into(&s, v);
    return s.p ? s.p : strdup("");
}

/* ── exception plumbing ──────────────────────────────────────────────────── */

static bool arh2_exc_pending(const arh2_ctx *ctx) { return ctx && ctx->exc != NULL; }
static void arh2_exc_clear(arh2_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}
static void arh2_raise(arh2_ctx *ctx, const char *cls, const char *msg) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
    json_set(ctx->exc, "msg", json_string(msg ? msg : ""));
}
static const char *arh2_exc_class(const arh2_ctx *ctx) {
    return json_get_str(ctx->exc, "__raise__", "");
}
static const char *arh2_exc_parent(const char *c) {
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
        {"FloatingPointError", "ArithmeticError"}, {"ArithmeticError", "Exception"},
        {"IndexError", "LookupError"}, {"KeyError", "LookupError"}, {"LookupError", "Exception"},
        {"ValueError", "Exception"}, {"TypeError", "Exception"}, {"AttributeError", "Exception"},
        {"RuntimeError", "Exception"}, {"NotImplementedError", "RuntimeError"},
        {"RecursionError", "RuntimeError"}, {"OSError", "Exception"}, {"NameError", "Exception"},
        {"UnboundLocalError", "NameError"}, {"AssertionError", "Exception"}, {"ImportError", "Exception"},
        {"ModuleNotFoundError", "ImportError"}, {"MemoryError", "Exception"}, {"ReferenceError", "Exception"},
        {"BufferError", "Exception"}, {"EOFError", "Exception"}, {"StopIteration", "Exception"},
        {"StopAsyncIteration", "Exception"}, {"SyntaxError", "Exception"}, {"IndentationError", "SyntaxError"},
        {"TabError", "IndentationError"}, {"SystemError", "Exception"}, {"Warning", "Exception"},
        {"Exception", "BaseException"},
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (arh2_str_eq(T[i].c, c)) return T[i].p;
    return NULL;
}
static bool arh2_exc_matches(const arh2_ctx *ctx, const char *const *classes, size_t n) {
    if (!arh2_exc_pending(ctx)) return false;
    const char *cls = arh2_exc_class(ctx);
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (arh2_str_eq(cls, classes[i])) return true;
        const char *p = arh2_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}
/* except Exception (NOT the BaseException trio). */
static bool arh2_exc_is_exception(const arh2_ctx *ctx) {
    static const char *E[] = {"Exception"};
    return arh2_exc_matches(ctx, E, 1);
}
/* exception marker node {"__raise__","msg"} (for logger args). */
static json_t *arh2_exc_marker(const char *cls, const char *msg) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(cls));
    json_set(e, "msg", json_string(msg ? msg : ""));
    return e;
}
static json_t *arh2_exc_arg(const arh2_ctx *ctx) {
    return arh2_exc_marker(arh2_exc_class(ctx), json_get_str(ctx->exc, "msg", ""));
}

/* ── universal seam ──────────────────────────────────────────────────────── */

static json_t *arh2_script_result(arh2_ctx *ctx, const json_t *script) {
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
static json_t *arh2_call(arh2_ctx *ctx, const char *name, json_t *args) {
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
    if (chosen != ctx->exc) ret = arh2_script_result(ctx, chosen);
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
static void arh2_arg(json_t *args, const char *key, const json_t *val) {
    if (!val || val->type == JSON_NULL) return;
    json_set(args, key, json_copy(val));
}
static void arh2_arg_str(json_t *args, const char *key, const char *val) {
    if (!val) return;
    json_set(args, key, json_string(val));
}
/* logger seam: {msg,args[,exc_info]}. prefix "" (module) or "ra." (run_agent).
 * args_arr ownership transferred. */
static json_t *arh2_log(arh2_ctx *ctx, const char *prefix, const char *level,
                        const char *msg, json_t *args_arr, bool exc_info) {
    json_t *a = json_object();
    json_set(a, "msg", json_string(msg));
    json_set(a, "args", args_arr ? args_arr : json_array());
    if (exc_info) json_set(a, "exc_info", json_bool(true));
    char name[48];
    snprintf(name, sizeof name, "%slogger.%s", prefix, level);
    json_t *r = arh2_call(ctx, name, a);
    json_free(r);
    return NULL;
}

/* ── set / variant helpers (variants project as arrays of strings) ───────── */

static bool arh2_arr_has_str(const json_t *arr, const char *s) {
    if (!arh2_is_arr(arr)) return false;
    for (size_t i = 0; i < arr->c.count; i++)
        if (arh2_is_str(arr->c.items[i]) && arh2_str_eq(arr->c.items[i]->str_val, s)) return true;
    return false;
}
/* set intersection non-empty (both string arrays). */
static bool arh2_sets_intersect(const json_t *a, const json_t *b) {
    if (!arh2_is_arr(a) || !arh2_is_arr(b)) return false;
    for (size_t i = 0; i < a->c.count; i++)
        if (arh2_is_str(a->c.items[i]) && arh2_arr_has_str(b, a->c.items[i]->str_val)) return true;
    return false;
}
/* any variant present as an object key. */
static bool arh2_arr_intersects_keys(const json_t *arr, const json_t *obj) {
    if (!arh2_is_arr(arr) || !arh2_is_obj(obj)) return false;
    for (size_t i = 0; i < arr->c.count; i++)
        if (arh2_is_str(arr->c.items[i]) && json_obj_get(obj, arr->c.items[i]->str_val)) return true;
    return false;
}
/* min string of a variant array (borrowed pointer into the array, or NULL). */
static const char *arh2_set_min(const json_t *arr) {
    const char *m = NULL;
    if (!arh2_is_arr(arr)) return NULL;
    for (size_t i = 0; i < arr->c.count; i++) {
        if (!arh2_is_str(arr->c.items[i])) continue;
        const char *s = arr->c.items[i]->str_val;
        if (!m || strcmp(s, m) < 0) m = s;
    }
    return m;
}
static int arh2_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
/* sorted array of an object's keys (new). */
static json_t *arh2_obj_keys_sorted(const json_t *obj) {
    json_t *arr = json_array();
    if (!arh2_is_obj(obj) || obj->c.count == 0) return arr;
    const char **keys = (const char **)malloc(obj->c.count * sizeof(char *));
    if (!keys) return arr;
    for (size_t i = 0; i < obj->c.count; i++) keys[i] = obj->c.keys[i];
    qsort(keys, obj->c.count, sizeof(char *), arh2_cmp_str);
    for (size_t i = 0; i < obj->c.count; i++) json_append(arr, json_string(keys[i]));
    free(keys);
    return arr;
}

/* run_agent AIAgent._VALID_API_ROLES (env-backed; defaults to the live frozenset
 * when env omits it). _ra() is the trivial lazy import of the run_agent stub
 * module, not a recorded seam. */
static bool arh2_role_valid(arh2_ctx *ctx, const json_t *role) {
    if (!arh2_is_str(role)) return false;
    json_t *valid = arh2_get(ctx->env, "valid_api_roles");
    if (arh2_is_arr(valid)) return arh2_arr_has_str(valid, role->str_val);
    static const char *DEF[] = {"system", "user", "assistant", "tool", "function", "developer"};
    for (size_t i = 0; i < sizeof(DEF) / sizeof(DEF[0]); i++)
        if (arh2_str_eq(DEF[i], role->str_val)) return true;
    return false;
}
/* _ra().AIAgent._get_tool_call_id_static(tc) / _get_tool_call_name_static(tc). */
static json_t *arh2_ra_static(arh2_ctx *ctx, const char *which, const json_t *tc) {
    json_t *a = json_object();
    arh2_arg(a, "tc", tc);
    char name[64];
    snprintf(name, sizeof name, "ra.%s", which);
    return arh2_call(ctx, name, a);
}

/* ── message-sanitizer leaves ────────────────────────────────────────────── */

/* PoP: arh2_prepend_corruption_marker @ agent/agent_runtime_helpers.py:_prepend_corruption_marker */
static json_t *arh2_prepend_corruption_marker(arh2_ctx *ctx, json_t *tool_msg, const char *marker) {
    (void)ctx;
    json_t *existing = arh2_get(tool_msg, "content");
    if (arh2_is_str(existing) && strncmp(arh2_jstr(existing), marker, strlen(marker)) == 0)
        return NULL;
    char *existing_str = NULL;
    bool free_existing = false;
    if (arh2_is_str(existing)) {
        existing_str = (char *)arh2_jstr(existing);
    } else if (arh2_is_none(existing)) {
        existing_str = NULL;
    } else {
        existing_str = arh2_py_json_dumps(existing);
        free_existing = true;
    }
    json_t *content;
    if (existing_str && existing_str[0] != '\0') {
        arh2_sb s; arh2_sb_init(&s);
        arh2_sb_put(&s, marker); arh2_sb_put(&s, "\n"); arh2_sb_put(&s, existing_str);
        content = json_string(s.p ? s.p : "");
        free(s.p);
    } else {
        content = json_string(marker);
    }
    if (free_existing) free(existing_str);
    json_set(tool_msg, "content", content);
    const char *dbm = json_get_str(ctx->env, "db_persisted_marker", "_db_persisted");
    json_obj_del(tool_msg, dbm);
    return NULL;
}

/* PoP: arh2_find_tool_result @ agent/agent_runtime_helpers.py:_find_tool_result */
static json_t *arh2_find_tool_result(arh2_ctx *ctx, json_t *messages, double start, json_t *tool_call) {
    if (!arh2_is_arr(messages)) return json_null();
    size_t n = messages->c.count;
    size_t s = start < 0 ? 0 : (size_t)start;
    for (size_t i = s; i < n; i++) {
        json_t *cand = messages->c.items[i];
        if (!arh2_is_obj(cand) || !arh2_str_eq(json_get_str(cand, "role", ""), "tool"))
            return json_null();
        json_t *a1 = json_object();
        arh2_arg(a1, "tool_call_id", arh2_get(cand, "tool_call_id"));
        json_t *rv = arh2_call(ctx, "tool_result_id_variants", a1);
        if (arh2_exc_pending(ctx)) { json_free(rv); return NULL; }
        json_t *a2 = json_object();
        arh2_arg(a2, "tc", tool_call);
        json_t *cv = arh2_call(ctx, "tool_call_id_variants", a2);
        if (arh2_exc_pending(ctx)) { json_free(rv); json_free(cv); return NULL; }
        bool hit = arh2_sets_intersect(rv, cv);
        json_free(rv); json_free(cv);
        if (hit) return json_copy(cand);
    }
    return json_null();
}

/* PoP: arh2_drop_invalid_roles @ agent/agent_runtime_helpers.py:_drop_invalid_roles */
static json_t *arh2_drop_invalid_roles(arh2_ctx *ctx, json_t *messages) {
    if (!arh2_is_arr(messages)) return json_array();
    for (size_t i = 0; i < messages->c.count; i++) {
        json_t *msg = messages->c.items[i];
        json_t *role = arh2_get(msg, "role");
        if (!arh2_role_valid(ctx, role)) {
            json_t *a = json_array();
            json_append(a, arh2_copy_or_null(role));
            arh2_log(ctx, "ra.", "debug",
                     "Pre-call sanitizer: dropping message with invalid role %r", a, false);
            if (arh2_exc_pending(ctx)) return NULL;
        }
    }
    json_t *out = json_array();
    for (size_t i = 0; i < messages->c.count; i++) {
        json_t *msg = messages->c.items[i];
        if (arh2_role_valid(ctx, arh2_get(msg, "role")))
            json_append(out, json_copy(msg));
    }
    return out;
}

/* PoP: arh2_drop_empty_tool_calls_arrays @ agent/agent_runtime_helpers.py:_drop_empty_tool_calls_arrays */
static json_t *arh2_drop_empty_tool_calls_arrays(arh2_ctx *ctx, json_t *messages) {
    if (!arh2_is_arr(messages)) return json_array();
    json_t *normalized = json_array();
    int dropped = 0;
    for (size_t i = 0; i < messages->c.count; i++) {
        json_t *msg = messages->c.items[i];
        bool strip = arh2_is_obj(msg)
            && arh2_str_eq(json_get_str(msg, "role", ""), "assistant")
            && arh2_has(msg, "tool_calls")
            && !(arh2_is_arr(arh2_get(msg, "tool_calls")) && arh2_truthy(arh2_get(msg, "tool_calls")));
        if (strip) {
            json_t *cp = json_object();
            for (size_t k = 0; k < msg->c.count; k++)
                if (!arh2_str_eq(msg->c.keys[k], "tool_calls"))
                    json_set(cp, msg->c.keys[k], json_copy(msg->c.items[k]));
            json_append(normalized, cp);
            dropped++;
        } else {
            json_append(normalized, json_copy(msg));
        }
    }
    if (!dropped) { json_free(normalized); return json_copy(messages); }
    json_t *a = json_array();
    json_append(a, json_number(dropped));
    arh2_log(ctx, "ra.", "debug",
             "Pre-call sanitizer: dropped empty/invalid tool_calls on %d assistant message(s)", a, false);
    if (arh2_exc_pending(ctx)) { json_free(normalized); return NULL; }
    return normalized;
}

/* PoP: arh2_repair_invalid_tool_call_names @ agent/agent_runtime_helpers.py:_repair_invalid_tool_call_names */
static json_t *arh2_repair_invalid_tool_call_names(arh2_ctx *ctx, json_t *messages) {
    if (!arh2_is_arr(messages)) return NULL;
    for (size_t mi = 0; mi < messages->c.count; mi++) {
        json_t *msg = messages->c.items[mi];
        if (!arh2_str_eq(json_get_str(msg, "role", ""), "assistant")) continue;
        json_t *tcs = arh2_get(msg, "tool_calls");
        if (!arh2_truthy(tcs) || !arh2_is_arr(tcs)) continue;
        json_t *new_tcs = NULL; /* copy-on-write result list */
        for (size_t idx = 0; idx < tcs->c.count; idx++) {
            json_t *tc = tcs->c.items[idx];
            if (!arh2_is_obj(tc)) continue; /* domain: dict tool_calls */
            json_t *fn = arh2_get(tc, "function");
            json_t *name = arh2_is_obj(fn) ? arh2_get(fn, "name") : NULL;
            json_t *a = json_object();
            arh2_arg(a, "name", name);
            json_t *coerced = arh2_call(ctx, "coerce_tool_name", a);
            if (arh2_exc_pending(ctx)) { json_free(coerced); json_free(new_tcs); return NULL; }
            bool same = arh2_json_eq(coerced, name);
            if (same) { json_free(coerced); continue; }
            /* arg evaluation order: (name or "")[:80], coerced, then id_static seam */
            char *name80 = NULL;
            if (arh2_truthy(name) && arh2_is_str(name)) {
                name80 = strdup(arh2_jstr(name));
                if (name80 && strlen(name80) > 80) name80[80] = '\0';
            } else {
                name80 = strdup("");
            }
            json_t *idv = arh2_ra_static(ctx, "_get_tool_call_id_static", tc);
            if (arh2_exc_pending(ctx)) { free(name80); json_free(coerced); json_free(idv); json_free(new_tcs); return NULL; }
            json_t *la = json_array();
            json_append(la, json_string(name80 ? name80 : ""));
            json_append(la, arh2_copy_or_null(coerced));
            json_append(la, arh2_copy_or_null(idv));
            free(name80);
            arh2_log(ctx, "ra.", "warning",
                     "Pre-call sanitizer: repairing tool_call with invalid function.name %r -> %r (id=%s)",
                     la, false);
            json_free(idv);
            if (arh2_exc_pending(ctx)) { json_free(coerced); json_free(new_tcs); return NULL; }
            if (!new_tcs) new_tcs = json_copy(tcs);
            json_t *new_fn;
            if (arh2_is_obj(fn)) {
                new_fn = json_copy(fn);
                json_set(new_fn, "name", json_copy(coerced));
            } else {
                new_fn = json_object();
                json_set(new_fn, "name", json_copy(coerced));
                json_set(new_fn, "arguments", json_string("{}"));
            }
            json_t *new_tc = json_copy(tc);
            json_set(new_tc, "function", new_fn);
            json_array_set(new_tcs, idx, new_tc);
            json_free(coerced);
        }
        if (new_tcs) json_set(msg, "tool_calls", new_tcs);
    }
    return NULL;
}

/* PoP: arh2_drop_results_without_ids @ agent/agent_runtime_helpers.py:_drop_results_without_ids */
static json_t *arh2_drop_results_without_ids(arh2_ctx *ctx, json_t *messages) {
    if (!arh2_is_arr(messages)) return json_array();
    json_t *kept = json_array();
    for (size_t i = 0; i < messages->c.count; i++) {
        json_t *m = messages->c.items[i];
        bool drop = false;
        if (arh2_str_eq(json_get_str(m, "role", ""), "tool")) {
            json_t *tid = arh2_get(m, "tool_call_id");
            const char *s = arh2_truthy(tid) ? arh2_jstr(tid) : "";
            char *st = arh2_strip(s);
            drop = !st || st[0] == '\0';
            free(st);
        }
        if (!drop) json_append(kept, json_copy(m));
    }
    if (kept->c.count != messages->c.count) {
        json_t *a = json_array();
        json_append(a, json_number((double)(messages->c.count - kept->c.count)));
        arh2_log(ctx, "ra.", "debug",
                 "Pre-call sanitizer: dropped %d tool result(s) with missing/empty tool_call_id", a, false);
        if (arh2_exc_pending(ctx)) { json_free(kept); return NULL; }
    }
    return kept;
}

/* flush helper for _pair_tool_calls_positionally (sorted-key stub injection). */
static json_t *arh2_pair_flush(arh2_ctx *ctx, json_t *paired, json_t *declared, int *stubs) {
    json_t *keys = arh2_obj_keys_sorted(declared);
    for (size_t i = 0; i < keys->c.count; i++) {
        const char *key = keys->c.items[i]->str_val;
        json_t *entry = arh2_get(declared, key);
        json_t *tc = arh2_get(entry, "tc");
        json_t *nm = arh2_ra_static(ctx, "_get_tool_call_name_static", tc);
        if (arh2_exc_pending(ctx)) { json_free(nm); json_free(keys); return NULL; }
        json_t *ca = json_object(); arh2_arg(ca, "tc", tc);
        json_t *co = arh2_call(ctx, "coalesce_tool_call_id", ca);
        if (arh2_exc_pending(ctx)) { json_free(nm); json_free(co); json_free(keys); return NULL; }
        const char *cid = arh2_truthy(co) ? arh2_jstr(co) : key;
        json_t *stub = json_object();
        json_set(stub, "role", json_string("tool"));
        json_set(stub, "name", arh2_copy_or_null(nm));
        json_set(stub, "content", json_string("[Result unavailable \xe2\x80\x94 see context summary above]"));
        json_set(stub, "tool_call_id", json_string(cid));
        json_append(paired, stub);
        (*stubs)++;
        json_free(nm); json_free(co);
    }
    json_free(keys);
    /* declared_calls.clear() */
    while (declared->c.count > 0) json_obj_del(declared, declared->c.keys[0]);
    return NULL;
}
/* PoP: arh2_pair_tool_calls_positionally @ agent/agent_runtime_helpers.py:_pair_tool_calls_positionally */
static json_t *arh2_pair_tool_calls_positionally(arh2_ctx *ctx, json_t *messages) {
    if (!arh2_is_arr(messages)) return json_array();
    json_t *paired = json_array();
    json_t *declared = json_object(); /* key -> {tc, variants} */
    int dropped = 0, stubs = 0;
    for (size_t i = 0; i < messages->c.count; i++) {
        json_t *msg = messages->c.items[i];
        const char *role = json_get_str(msg, "role", "");
        if (arh2_str_eq(role, "assistant")) {
            if (arh2_pair_flush(ctx, paired, declared, &stubs)) goto fail;
            json_t *tcs = arh2_get(msg, "tool_calls");
            if (arh2_is_arr(tcs)) {
                for (size_t j = 0; j < tcs->c.count; j++) {
                    json_t *tc = tcs->c.items[j];
                    json_t *a = json_object(); arh2_arg(a, "tc", tc);
                    json_t *variants = arh2_call(ctx, "tool_call_id_variants", a);
                    if (arh2_exc_pending(ctx)) { json_free(variants); goto fail; }
                    if (arh2_truthy(variants)) {
                        const char *key = arh2_set_min(variants);
                        if (key) {
                            json_t *e = json_object();
                            json_set(e, "tc", json_copy(tc));
                            json_set(e, "variants", json_copy(variants));
                            json_set(declared, key, e);
                        }
                    }
                    json_free(variants);
                }
            }
        } else if (arh2_str_eq(role, "tool")) {
            json_t *a = json_object();
            arh2_arg(a, "tool_call_id", arh2_get(msg, "tool_call_id"));
            json_t *rv = arh2_call(ctx, "tool_result_id_variants", a);
            if (arh2_exc_pending(ctx)) { json_free(rv); goto fail; }
            const char *matched = NULL;
            for (size_t k = 0; k < declared->c.count; k++) {
                json_t *e = declared->c.items[k];
                if (arh2_sets_intersect(arh2_get(e, "variants"), rv)) { matched = declared->c.keys[k]; break; }
            }
            json_free(rv);
            if (!matched) { dropped++; continue; }
            json_obj_del(declared, matched);
        } else if (arh2_str_eq(role, "user")) {
            if (arh2_pair_flush(ctx, paired, declared, &stubs)) goto fail;
        }
        json_append(paired, json_copy(msg));
    }
    if (arh2_pair_flush(ctx, paired, declared, &stubs)) goto fail;
    json_free(declared);
    if (dropped) {
        json_t *a = json_array(); json_append(a, json_number(dropped));
        arh2_log(ctx, "ra.", "debug",
                 "Pre-call sanitizer: removed %d positionally orphaned tool result(s)", a, false);
        if (arh2_exc_pending(ctx)) { json_free(paired); return NULL; }
    }
    if (stubs) {
        json_t *a = json_array(); json_append(a, json_number(stubs));
        arh2_log(ctx, "ra.", "debug",
                 "Pre-call sanitizer: added %d stub tool result(s) for positionally unanswered tool call(s)", a, false);
        if (arh2_exc_pending(ctx)) { json_free(paired); return NULL; }
    }
    if (dropped || stubs) return paired;
    json_free(paired);
    return json_copy(messages);
fail:
    json_free(paired); json_free(declared);
    return NULL;
}

/* PoP: arh2_dedupe_tool_call_ids @ agent/agent_runtime_helpers.py:_dedupe_tool_call_ids */
static json_t *arh2_dedupe_tool_call_ids(arh2_ctx *ctx, json_t *messages) {
    if (!arh2_is_arr(messages)) return json_array();
    json_t *outstanding = json_object();        /* variant -> group_id (number) */
    json_t *groups = json_array();              /* group_id -> variants array */
    int next_group = 0, removed = 0;
    json_t *deduped = json_array();
    for (size_t i = 0; i < messages->c.count; i++) {
        json_t *msg = messages->c.items[i];
        const char *role = json_get_str(msg, "role", "");
        json_t *tcs = arh2_get(msg, "tool_calls");
        if (arh2_str_eq(role, "assistant") && arh2_truthy(tcs)) {
            json_t *kept = json_array();
            json_t *src = arh2_is_arr(tcs) ? tcs : NULL;
            size_t tn = src ? src->c.count : 0;
            for (size_t j = 0; j < tn; j++) {
                json_t *tc = src->c.items[j];
                json_t *a = json_object(); arh2_arg(a, "tc", tc);
                json_t *variants = arh2_call(ctx, "tool_call_id_variants", a);
                if (arh2_exc_pending(ctx)) { json_free(variants); json_free(kept); goto fail; }
                if (arh2_truthy(variants) && arh2_arr_intersects_keys(variants, outstanding)) {
                    removed++; json_free(variants); continue;
                }
                if (arh2_truthy(variants)) {
                    int gid = next_group++;
                    json_append(groups, json_copy(variants));
                    for (size_t k = 0; k < variants->c.count; k++) {
                        if (!arh2_is_str(variants->c.items[k])) continue;
                        const char *v = variants->c.items[k]->str_val;
                        if (!json_obj_get(outstanding, v)) json_set(outstanding, v, json_number(gid));
                    }
                }
                json_append(kept, json_copy(tc));
                json_free(variants);
            }
            if (kept->c.count > 0) {
                json_t *nm = json_copy(msg);
                json_set(nm, "tool_calls", kept);
                json_append(deduped, nm);
            } else if (tn > 0) {
                json_t *nm = json_object();
                for (size_t k = 0; k < msg->c.count; k++)
                    if (!arh2_str_eq(msg->c.keys[k], "tool_calls"))
                        json_set(nm, msg->c.keys[k], json_copy(msg->c.items[k]));
                json_append(deduped, nm);
                json_free(kept);
            } else {
                json_append(deduped, json_copy(msg));
                json_free(kept);
            }
        } else if (arh2_str_eq(role, "tool")) {
            json_t *a = json_object();
            arh2_arg(a, "tool_call_id", arh2_get(msg, "tool_call_id"));
            json_t *rv = arh2_call(ctx, "tool_result_id_variants", a);
            if (arh2_exc_pending(ctx)) { json_free(rv); goto fail; }
            /* candidate_groups = {outstanding[v] for v in rv if v in outstanding} */
            bool any_cand = false; double min_gid = 0;
            if (arh2_is_arr(rv)) {
                for (size_t k = 0; k < rv->c.count; k++) {
                    if (!arh2_is_str(rv->c.items[k])) continue;
                    json_t *g = json_obj_get(outstanding, rv->c.items[k]->str_val);
                    if (g && arh2_is_num(g)) {
                        if (!any_cand || g->num_val < min_gid) min_gid = g->num_val;
                        any_cand = true;
                    }
                }
            }
            bool rv_truthy = arh2_truthy(rv);
            json_free(rv);
            if (rv_truthy && !any_cand) { removed++; continue; }
            if (any_cand) {
                size_t gidx = (size_t)min_gid;
                json_t *gvars = gidx < groups->c.count ? groups->c.items[gidx] : NULL;
                if (arh2_is_arr(gvars)) {
                    for (size_t k = 0; k < gvars->c.count; k++) {
                        if (!arh2_is_str(gvars->c.items[k])) continue;
                        const char *v = gvars->c.items[k]->str_val;
                        json_t *cur = json_obj_get(outstanding, v);
                        if (cur && arh2_is_num(cur) && cur->num_val == min_gid)
                            json_obj_del(outstanding, v);
                    }
                }
                if (gidx < groups->c.count) json_array_set(groups, gidx, json_null());
            }
            json_append(deduped, json_copy(msg));
        } else {
            json_append(deduped, json_copy(msg));
        }
    }
    json_free(outstanding); json_free(groups);
    if (!removed) { json_free(deduped); return json_copy(messages); }
    json_t *a = json_array(); json_append(a, json_number(removed));
    arh2_log(ctx, "ra.", "debug",
             "Pre-call sanitizer: removed %d duplicate tool_call_id reference(s)", a, false);
    if (arh2_exc_pending(ctx)) { json_free(deduped); return NULL; }
    return deduped;
fail:
    json_free(outstanding); json_free(groups); json_free(deduped);
    return NULL;
}

/* PoP: arh2_realign_tool_result_names @ agent/agent_runtime_helpers.py:_realign_tool_result_names */
static json_t *arh2_realign_tool_result_names(arh2_ctx *ctx, json_t *messages) {
    if (!arh2_is_arr(messages)) return json_array();
    json_t *call_names = json_object();
    for (size_t i = 0; i < messages->c.count; i++) {
        json_t *msg = messages->c.items[i];
        if (!arh2_str_eq(json_get_str(msg, "role", ""), "assistant")) continue;
        json_t *tcs = arh2_get(msg, "tool_calls");
        if (!arh2_is_arr(tcs)) continue;
        for (size_t j = 0; j < tcs->c.count; j++) {
            json_t *tc = tcs->c.items[j];
            json_t *idv = arh2_ra_static(ctx, "_get_tool_call_id_static", tc);
            if (arh2_exc_pending(ctx)) { json_free(idv); json_free(call_names); return NULL; }
            json_t *nm = arh2_ra_static(ctx, "_get_tool_call_name_static", tc);
            if (arh2_exc_pending(ctx)) { json_free(idv); json_free(nm); json_free(call_names); return NULL; }
            const char *ids = arh2_truthy(idv) ? arh2_jstr(idv) : "";
            char *cid = arh2_strip(ids);
            if (cid && cid[0] != '\0' && arh2_truthy(nm))
                json_set(call_names, cid, json_copy(nm));
            free(cid); json_free(idv); json_free(nm);
        }
    }
    json_t *aligned = json_array();
    arh2_sb realigned; arh2_sb_init(&realigned);
    int nreal = 0;
    for (size_t i = 0; i < messages->c.count; i++) {
        json_t *msg = messages->c.items[i];
        if (arh2_str_eq(json_get_str(msg, "role", ""), "tool")) {
            json_t *tid = arh2_get(msg, "tool_call_id");
            const char *tids = arh2_truthy(tid) ? arh2_jstr(tid) : "";
            char *key = arh2_strip(tids);
            json_t *expected = key ? json_obj_get(call_names, key) : NULL;
            json_t *current = arh2_get(msg, "name");
            if (arh2_truthy(expected) && arh2_truthy(current) && !arh2_json_eq(current, expected)) {
                json_t *nm = json_copy(msg);
                json_set(nm, "name", json_copy(expected));
                json_append(aligned, nm);
                if (nreal) arh2_sb_put(&realigned, ", ");
                arh2_sb_put(&realigned, arh2_jstr(current));
                arh2_sb_put(&realigned, " -> ");
                arh2_sb_put(&realigned, arh2_jstr(expected));
                nreal++;
                free(key);
                continue;
            }
            free(key);
        }
        json_append(aligned, json_copy(msg));
    }
    json_free(call_names);
    if (!nreal) { free(realigned.p); json_free(aligned); return json_copy(messages); }
    json_t *a = json_array();
    json_append(a, json_number(nreal));
    json_append(a, json_string(realigned.p ? realigned.p : ""));
    free(realigned.p);
    arh2_log(ctx, "ra.", "debug",
             "Pre-call sanitizer: realigned %d tool result name(s) with their tool_call function name (%s)",
             a, false);
    if (arh2_exc_pending(ctx)) { json_free(aligned); return NULL; }
    return aligned;
}

/* PoP: arh2_tool_results_this_turn @ agent/agent_runtime_helpers.py:tool_results_this_turn */
static json_t *arh2_tool_results_this_turn(arh2_ctx *ctx, json_t *messages) {
    (void)ctx;
    int count = 0;
    if (arh2_is_arr(messages)) {
        for (size_t i = messages->c.count; i > 0; i--) {
            json_t *msg = messages->c.items[i - 1];
            if (!arh2_is_obj(msg)) continue;
            const char *role = json_get_str(msg, "role", "");
            if (arh2_str_eq(role, "user")) break;
            if (arh2_str_eq(role, "tool")) count++;
        }
    }
    return json_number(count);
}

/* ── API-error debug projection ──────────────────────────────────────────── */

/* PoP: arh2_api_error_debug_info @ agent/agent_runtime_helpers.py:_api_error_debug_info */
static json_t *arh2_api_error_debug_info(arh2_ctx *ctx, json_t *error) {
    json_t *info = json_object();
    json_set(info, "type", json_string(json_get_str(error, "cls", "Exception")));
    json_set(info, "message", json_string(json_get_str(error, "msg", "")));
    json_t *attrs = arh2_get(error, "attrs");
    static const char *KEYS[] = {"status_code", "request_id", "code", "param", "type", "body"};
    for (size_t i = 0; i < sizeof(KEYS) / sizeof(KEYS[0]); i++) {
        json_t *v = arh2_get(attrs, KEYS[i]);
        if (v && v->type != JSON_NULL) json_set(info, KEYS[i], json_copy(v));
    }
    json_t *resp = arh2_get(error, "response");
    if (resp && resp->type != JSON_NULL) {
        /* try: info["response_status"] = getattr(resp, "status_code", None) */
        json_set(info, "response_status", arh2_copy_or_null(arh2_get(resp, "status_code")));
        json_t *text = arh2_get(resp, "text");
        if (text) {
            json_set(info, "response_text", json_copy(text));
        } else {
            /* response_obj.text raises AttributeError; caught + logged */
            json_t *e = arh2_exc_marker("AttributeError",
                                        "'types.SimpleNamespace' object has no attribute 'text'");
            json_t *a = json_array(); json_append(a, e);
            arh2_log(ctx, "ra.", "debug", "Could not extract error response details: %s", a, false);
            if (arh2_exc_pending(ctx)) { json_free(info); return NULL; }
        }
    }
    return info;
}

/* ── credential-pool / agent-state flows ─────────────────────────────────── */

/* PoP: arh2_sync_credential_pool_entry_id @ agent/agent_runtime_helpers.py:sync_credential_pool_entry_id */
static json_t *arh2_sync_credential_pool_entry_id(arh2_ctx *ctx, json_t *agent) {
    json_t *pool = arh2_get(agent, "_credential_pool");
    json_t *result = NULL;
    if (!arh2_is_none(pool)) {
        json_t *a = json_object();
        arh2_arg(a, "api_key", arh2_get(agent, "api_key"));
        result = arh2_call(ctx, "pool.entry_id_for_api_key", a);
        if (arh2_exc_pending(ctx)) {
            if (arh2_exc_is_exception(ctx)) {
                arh2_exc_clear(ctx);
                json_set(agent, "_credential_pool_entry_id", json_null());
                return NULL;
            }
            json_free(result);
            return NULL; /* BaseException propagates */
        }
    }
    json_set(agent, "_credential_pool_entry_id", arh2_copy_or_null(result));
    json_free(result);
    return NULL;
}

/* PoP: arh2_failed_credential_identity @ agent/agent_runtime_helpers.py:_failed_credential_identity */
static json_t *arh2_failed_credential_identity(arh2_ctx *ctx, json_t *agent, json_t *pool) {
    json_t *hint = arh2_get(agent, "api_key");
    if (!arh2_truthy(hint)) hint = NULL;
    json_t *raw_id = arh2_get(agent, "_credential_pool_entry_id");
    json_t *cred = (arh2_is_str(raw_id) && arh2_truthy(raw_id)) ? raw_id : NULL;
    /* Values taken from the pool.current() result must be OWNED snapshots:
     * cur is freed below, and a borrowed pointer into it dangles (UAF). */
    json_t *hint_owned = NULL, *cred_owned = NULL;
    if (!hint) {
        if (arh2_is_none(pool)) {
            arh2_raise(ctx, "AttributeError", "'NoneType' object has no attribute 'current'");
            return NULL;
        }
        json_t *cur = arh2_call(ctx, "pool.current", json_object());
        if (arh2_exc_pending(ctx)) { json_free(cur); return NULL; }
        if (arh2_truthy(cur)) {
            json_t *h = arh2_get(cur, "runtime_api_key");
            if (h) { hint_owned = json_copy(h); hint = hint_owned; }
            if (!cred) {
                json_t *cid = arh2_get(cur, "id");
                if (arh2_is_str(cid) && arh2_truthy(cid)) {
                    cred_owned = json_copy(cid);
                    cred = cred_owned;
                }
            }
        }
        json_free(cur);
    }
    json_t *ret = json_array();
    json_append(ret, arh2_copy_or_null(hint));
    json_append(ret, arh2_copy_or_null(cred));
    json_free(hint_owned);
    json_free(cred_owned);
    return ret;
}

/* PoP: arh2_restore_runtime_capabilities @ agent/agent_runtime_helpers.py:_restore_runtime_capabilities */
static json_t *arh2_restore_runtime_capabilities(arh2_ctx *ctx, json_t *agent, json_t *rt) {
    json_t *raw;
    bool has_rc = arh2_has(rt, "runtime_capabilities");
    if (has_rc) raw = arh2_get(rt, "runtime_capabilities");
    else raw = arh2_get(rt, "capabilities");
    if (arh2_is_obj(raw)) {
        json_set(agent, "runtime_capabilities", json_copy(raw));
    } else if (has_rc) {
        arh2_log(ctx, "", "warning", "Ignoring malformed runtime capabilities snapshot", json_array(), false);
        if (arh2_exc_pending(ctx)) return NULL;
    }
    return NULL;
}

/* PoP: arh2_apply_primary_runtime_fields @ agent/agent_runtime_helpers.py:_apply_primary_runtime_fields */
static json_t *arh2_apply_primary_runtime_fields(arh2_ctx *ctx, json_t *agent, json_t *rt) {
    static const char *REQ[] = {"model", "provider", "base_url", "api_key", "client_kwargs"};
    for (size_t i = 0; i < sizeof(REQ) / sizeof(REQ[0]); i++)
        if (!arh2_has(rt, REQ[i])) { arh2_raise(ctx, "KeyError", REQ[i]); return NULL; }
    json_set(agent, "model", json_copy(arh2_get(rt, "model")));
    json_set(agent, "provider", json_copy(arh2_get(rt, "provider")));
    json_t *rp = arh2_get_def(rt, "requested_provider", arh2_get(agent, "provider"));
    json_set(agent, "requested_provider", arh2_copy_or_null(rp));
    json_set(agent, "base_url", json_copy(arh2_get(rt, "base_url")));
    json_t *a = json_object();
    arh2_arg(a, "provider", arh2_get(agent, "provider"));
    arh2_arg(a, "base_url", arh2_get(agent, "base_url"));
    json_t *iar = arh2_call(ctx, "is_actual_route", a);
    if (arh2_exc_pending(ctx)) { json_free(iar); return NULL; }
    if (arh2_truthy(iar)) {
        json_set(agent, "api_mode", json_string("chat_completions"));
    } else {
        if (!arh2_has(rt, "api_mode")) { json_free(iar); arh2_raise(ctx, "KeyError", "api_mode"); return NULL; }
        json_set(agent, "api_mode", json_copy(arh2_get(rt, "api_mode")));
    }
    json_free(iar);
    if (arh2_has(agent, "_transport_cache")) {
        json_t *tc = arh2_get(agent, "_transport_cache");
        if (arh2_is_none(tc)) { arh2_raise(ctx, "AttributeError", "'NoneType' object has no attribute 'clear'"); return NULL; }
        if (arh2_is_obj(tc)) while (tc->c.count > 0) json_obj_del(tc, tc->c.keys[0]);
    }
    json_set(agent, "api_key", json_copy(arh2_get(rt, "api_key")));
    json_t *ref = arh2_get_def(rt, "reasoning_echo_flag", NULL);
    json_set(agent, "_reasoning_echo_flag", ref ? json_copy(ref) : json_bool(false));
    json_t *ro = arh2_get(rt, "request_overrides");
    json_set(agent, "request_overrides", arh2_truthy(ro) ? json_copy(ro) : json_object());
    json_set(agent, "_client_kwargs", json_copy(arh2_get(rt, "client_kwargs")));
    return NULL;
}

/* PoP: arh2_revert_credential_rotation @ agent/agent_runtime_helpers.py:_revert_credential_rotation */
static json_t *arh2_revert_credential_rotation(arh2_ctx *ctx, json_t *agent) {
    json_t *revert_id = arh2_get(agent, "_credential_pool_revert_id");
    if (!arh2_truthy(revert_id)) return NULL;
    json_t *pool = arh2_get(agent, "_credential_pool");
    json_t *entry_id = arh2_get(agent, "_credential_pool_entry_id");
    if (arh2_is_none(pool) || arh2_json_eq(entry_id, revert_id)) {
        json_set(agent, "_credential_pool_revert_id", json_null());
        return NULL;
    }
    json_t *a = json_object();
    arh2_arg(a, "revert_id", revert_id);
    arh2_arg(a, "model", arh2_get(agent, "model"));
    json_t *entry = arh2_call(ctx, "pool.reclaim", a);
    if (arh2_exc_pending(ctx)) {
        if (arh2_exc_is_exception(ctx)) {
            json_t *ea = arh2_exc_arg(ctx);
            arh2_exc_clear(ctx);
            json_t *la = json_array(); json_append(la, ea);
            arh2_log(ctx, "", "warning", "Credential revert check failed: %s", la, false);
            json_free(entry);
            return NULL;
        }
        json_free(entry);
        return NULL;
    }
    if (arh2_is_none(entry)) { json_free(entry); return NULL; }
    /* agent._swap_credential(entry) */
    json_t *sw = arh2_get(agent, "_swap_credential");
    if (!sw) {
        arh2_raise(ctx, "AttributeError", "'types.SimpleNamespace' object has no attribute '_swap_credential'");
        json_free(entry); return NULL;
    }
    if (arh2_is_none(sw)) {
        arh2_raise(ctx, "TypeError", "'NoneType' object is not callable");
        json_free(entry); return NULL;
    }
    json_t *sa = json_object();
    arh2_arg(sa, "entry", entry);
    json_t *sr = arh2_call(ctx, "agent._swap_credential", sa);
    if (arh2_exc_pending(ctx)) { json_free(sr); json_free(entry); return NULL; }
    bool is_false = arh2_is_bool(sr) && !sr->bool_val;
    json_free(sr);
    if (!is_false) {
        json_t *la = json_array();
        json_t *eid = arh2_get(entry, "id");
        json_t *lbl = arh2_get(entry, "label");
        json_append(la, eid ? json_copy(eid) : json_string("?"));
        json_append(la, lbl ? json_copy(lbl) : json_string("?"));
        arh2_log(ctx, "", "info",
                 "Credential %s (%s) available again \xe2\x80\x94 reverted pool rotation", la, false);
        if (arh2_exc_pending(ctx)) { json_free(entry); return NULL; }
    }
    json_free(entry);
    json_set(agent, "_credential_pool_revert_id", json_null());
    return NULL;
}

/* PoP: arh2_requeue_pending_steer @ agent/agent_runtime_helpers.py:_requeue_pending_steer */
static json_t *arh2_requeue_pending_steer(arh2_ctx *ctx, json_t *agent, const char *steer_text) {
    json_t *a = json_object();
    arh2_arg(a, "agent", agent);
    arh2_arg_str(a, "steer_text", steer_text);
    json_t *r = arh2_call(ctx, "requeue_drained_steer", a);
    json_free(r);
    return NULL;
}

/* PoP: arh2_pre_tool_block_message @ agent/agent_runtime_helpers.py:_pre_tool_block_message */
static json_t *arh2_pre_tool_block_message(arh2_ctx *ctx, json_t *agent, json_t *in) {
    json_t *function_args = arh2_get(in, "function_args");
    json_t *mt = arh2_get(in, "middleware_trace");
    /* list(middleware_trace): null -> TypeError caught -> (None, function_args) */
    if (!arh2_is_arr(mt)) {
        json_t *ret = json_array();
        json_append(ret, json_null());
        json_append(ret, arh2_copy_or_null(function_args));
        return ret;
    }
    json_t *a = json_object();
    arh2_arg(a, "function_name", arh2_get(in, "function_name"));
    arh2_arg(a, "function_args", function_args);
    json_t *etid = arh2_get(in, "effective_task_id");
    arh2_arg_str(a, "task_id", arh2_truthy(etid) ? arh2_jstr(etid) : "");
    json_t *sid = arh2_get(agent, "session_id");
    arh2_arg_str(a, "session_id", arh2_truthy(sid) ? arh2_jstr(sid) : "");
    json_t *tcid = arh2_get(in, "tool_call_id");
    arh2_arg_str(a, "tool_call_id", arh2_truthy(tcid) ? arh2_jstr(tcid) : "");
    json_t *tid = arh2_get(agent, "_current_turn_id");
    arh2_arg_str(a, "turn_id", arh2_truthy(tid) ? arh2_jstr(tid) : "");
    json_t *arid = arh2_get(agent, "_current_api_request_id");
    arh2_arg_str(a, "api_request_id", arh2_truthy(arid) ? arh2_jstr(arid) : "");
    arh2_arg(a, "middleware_trace", mt);
    json_t *r = arh2_call(ctx, "_dispatch_pre_tool_call_hooks", a);
    if (arh2_exc_pending(ctx)) {
        if (arh2_exc_is_exception(ctx)) {
            arh2_exc_clear(ctx);
            json_free(r);
            json_t *ret = json_array();
            json_append(ret, json_null());
            json_append(ret, arh2_copy_or_null(function_args));
            return ret;
        }
        json_free(r);
        return NULL;
    }
    /* unpack (block_message, modified_args) */
    json_t *block = arh2_is_arr(r) ? json_get(r, 0) : NULL;
    json_t *modified = arh2_is_arr(r) ? json_get(r, 1) : NULL;
    json_t *ret = json_array();
    json_append(ret, arh2_copy_or_null(block));
    /* modified_args if modified_args is not None else function_args —
     * a PRESENT JSON null is None and must fall back, not pass through. */
    json_append(ret, !arh2_is_none(modified) ? json_copy(modified)
                                             : arh2_copy_or_null(function_args));
    json_free(r);
    return ret;
}

/* ── model-switch snapshot pair ──────────────────────────────────────────── */

static const char *ARH2_SWITCH_FIELDS[] = {
    "model", "provider", "requested_provider", "base_url", "api_mode", "api_key", "client",
    "_anthropic_client", "_anthropic_api_key", "_anthropic_base_url", "_is_anthropic_oauth",
    "_config_context_length", "_reasoning_echo_flag", "runtime_capabilities",
    "_credential_pool", "_credential_pool_entry_id",
    "_reasoning_disable_rejected", "_reasoning_floor_required", "_reasoning_effort_rejected",
    "_stream_options_unsupported", "_disable_streaming", "_codex_reasoning_replay_enabled",
    "_codex_reasoning_replay_rejected",
};
#define ARH2_N_SWITCH_FIELDS (sizeof(ARH2_SWITCH_FIELDS) / sizeof(ARH2_SWITCH_FIELDS[0]))

/* PoP: arh2_snapshot_switch_state @ agent/agent_runtime_helpers.py:_snapshot_switch_state */
static json_t *arh2_snapshot_switch_state(arh2_ctx *ctx, json_t *agent) {
    (void)ctx;
    json_t *snap = json_object();
    for (size_t i = 0; i < ARH2_N_SWITCH_FIELDS; i++) {
        json_t *v = arh2_get(agent, ARH2_SWITCH_FIELDS[i]);
        if (v) json_set(snap, ARH2_SWITCH_FIELDS[i], json_copy(v));
        else {
            json_t *m = json_object();
            json_set(m, "__missing__", json_bool(true));
            json_set(snap, ARH2_SWITCH_FIELDS[i], m);
        }
    }
    json_t *ck = arh2_get(agent, "_client_kwargs");
    json_set(snap, "_client_kwargs", arh2_truthy(ck) ? json_copy(ck) : json_object());
    return snap;
}

/* PoP: arh2_restore_switch_snapshot @ agent/agent_runtime_helpers.py:_restore_switch_snapshot */
static json_t *arh2_restore_switch_snapshot(arh2_ctx *ctx, json_t *agent, json_t *snapshot) {
    (void)ctx;
    if (!arh2_is_obj(snapshot)) return NULL;
    for (size_t i = 0; i < snapshot->c.count; i++) {
        json_t *v = snapshot->c.items[i];
        if (arh2_is_obj(v) && json_is_true(json_obj_get(v, "__missing__"))) continue;
        json_set(agent, snapshot->c.keys[i], json_copy(v));
    }
    return NULL;
}

/* PoP: arh2_recover_auth_failure @ agent/agent_runtime_helpers.py:_recover_auth_failure */
static json_t *arh2_recover_auth_failure(arh2_ctx *ctx, json_t *agent, json_t *pool, json_t *kwargs) {
    json_t *a = json_object();
    arh2_arg(a, "agent", agent);
    arh2_arg(a, "pool", pool);
    if (arh2_is_obj(kwargs))
        for (size_t i = 0; i < kwargs->c.count; i++)
            arh2_arg(a, kwargs->c.keys[i], kwargs->c.items[i]);
    return arh2_call(ctx, "recover_auth_failure", a);
}