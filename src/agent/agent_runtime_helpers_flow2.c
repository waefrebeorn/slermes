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

/* ══ UNCLAIMED remainder (27 fns) — added by flow2 port ═══════════════════ */

/* forward decls for composed flow2 fns (defined later in this section) */
static json_t *arh2_build_anthropic_client_from_runtime(arh2_ctx *ctx, json_t *agent, json_t *rt);
static json_t *arh2_apply_switched_provider_request_overrides(arh2_ctx *ctx, json_t *agent, const char *new_provider);
static json_t *arh2_build_switched_client(arh2_ctx *ctx, json_t *agent, const char *new_provider,
                                          json_t *api_key, const char *base_url, const char *api_mode,
                                          const char *new_norm);
static json_t *arh2_socket_from_stream(arh2_ctx *ctx, json_t *stream);
static json_t *arh2_socket_from_candidate(arh2_ctx *ctx, json_t *candidate);
static json_t *arh2_connection_candidates(arh2_ctx *ctx, json_t *start);

/* small helpers */
static char *arh2_lower_dup(const char *s) {
    if (!s) return strdup("");
    char *r = strdup(s);
    for (char *p = r; *p; p++) *p = (char)tolower((unsigned char)*p);
    return r;
}
static bool arh2_starts(const char *s, const char *pre) {
    return s && pre && strncmp(s, pre, strlen(pre)) == 0;
}
static bool arh2_ends_any(const char *s, const json_t *arr) {
    if (!s || !arh2_is_arr(arr)) return false;
    for (size_t i = 0; i < arr->c.count; i++) {
        const char *t = arh2_jstr(arr->c.items[i]);
        size_t n = strlen(t), m = strlen(s);
        if (n && m >= n && strcmp(s + m - n, t) == 0) return true;
    }
    return false;
}
static const char *arh2_sdef(const json_t *o, const char *k, const char *d) {
    json_t *v = arh2_get(o, k);
    return (v && v->type == JSON_STRING) ? v->str_val : d;
}
static double arh2_ndef(const json_t *o, const char *k, double d) {
    json_t *v = arh2_get(o, k);
    return arh2_is_num(v) ? v->num_val : d;
}
static bool arh2_bdef(const json_t *o, const char *k, bool d) {
    json_t *v = arh2_get(o, k);
    return arh2_is_bool(v) ? v->bool_val : d;
}
/* bool(agent field or default) */
static bool arh2_truthy_key(const json_t *o, const char *k) {
    return arh2_truthy(arh2_get(o, k));
}
/* str(field or "") stripped+lowered */
static char *arh2_field_lower_strip(const json_t *o, const char *k) {
    return arh2_lower_dup(arh2_strip(arh2_sdef(o, k, "")));
}
/* Python str.removeprefix for ASCII prefix */
static const char *arh2_removeprefix(const char *s, const char *pre) {
    if (s && pre && strncmp(s, pre, strlen(pre)) == 0) return s + strlen(pre);
    return s ? s : "";
}
/* result tuple(bool, bool) as a JSON array */
static json_t *arh2_pair_bool(bool a, bool b) {
    json_t *r = json_array();
    json_append(r, json_bool(a));
    json_append(r, json_bool(b));
    return r;
}
/* arbitrary JSON-array result helper */
#define ARH2_ARR(...) /* unused */
static json_t *arh2_str_array(const char *const *v, size_t n) {
    json_t *r = json_array();
    for (size_t i = 0; i < n; i++) json_append(r, json_string(v[i] ? v[i] : ""));
    return r;
}

/* ── _cursor_skip_prefix ─────────────────────────────────────────────────── */
/* PoP: arh2_cursor_skip_prefix @ agent/agent_runtime_helpers.py:_cursor_skip_prefix */
static json_t *arh2_cursor_skip_prefix(arh2_ctx *ctx, const json_t *messages, const json_t *cursor) {
    (void)ctx;
    size_t start = 0;
    json_t *prev = arh2_is_obj(cursor) ? arh2_get(cursor, "prefix") : NULL;
    if (arh2_is_arr(prev) && arh2_is_arr(messages)) {
        size_t lim = prev->c.count < messages->c.count ? prev->c.count : messages->c.count;
        while (start < lim) {
            json_t *a = messages->c.items[start], *b = prev->c.items[start];
            bool same = false;
            if (arh2_is_obj(a) && arh2_is_obj(b)) {
                json_t *ia = arh2_get(a, "__id"), *ib = arh2_get(b, "__id");
                if (ia && ib) same = arh2_json_eq(ia, ib);
            }
            if (!same) break;
            start++;
        }
    }
    return json_number((double)start);
}

/* ── _recover_rate_limit ─────────────────────────────────────────────────── */
static json_t *arh2_pool_entries(arh2_ctx *ctx) {
    return arh2_call(ctx, "pool.entries", json_object());
}
static json_t *arh2_pool_current(arh2_ctx *ctx) {
    return arh2_call(ctx, "pool.current", json_object());
}
static json_t *arh2_entry_for(const json_t *entries, const char *key, const json_t *val) {
    if (!arh2_is_arr(entries) || !val || arh2_is_none(val)) return NULL;
    for (size_t i = 0; i < entries->c.count; i++)
        if (arh2_json_eq(arh2_get(entries->c.items[i], key), val)) return entries->c.items[i];
    return NULL;
}
static bool arh2_contains_tok(const char *hay, const json_t *toks) {
    if (!hay || !arh2_is_arr(toks)) return false;
    for (size_t i = 0; i < toks->c.count; i++) {
        const char *t = arh2_jstr(toks->c.items[i]);
        if (t[0] && strstr(hay, t)) return true;
    }
    return false;
}
static json_t *arh2_rotate_swap(arh2_ctx *ctx, const char *reason) {
    json_t *a = json_object();
    json_set(a, "code", json_number(429));
    json_set(a, "reason", json_string(reason ? reason : ""));
    json_t *r = arh2_call(ctx, "rotate_and_swap", a);
    bool ok = arh2_truthy(r);
    json_free(r);
    return arh2_pair_bool(ok, !ok);
}
/* PoP: arh2_recover_rate_limit @ agent/agent_runtime_helpers.py:_recover_rate_limit */
static json_t *arh2_recover_rate_limit(arh2_ctx *ctx, json_t *pool,
        bool has_retried_429, json_t *error_context, json_t *api_key_hint,
        json_t *credential_id, bool rotate_unused) {
    (void)pool; (void)rotate_unused;
    json_t *current = NULL;
    if (!arh2_is_none(credential_id)) {
        json_t *e = arh2_pool_entries(ctx);
        current = arh2_entry_for(e, "id", credential_id);
        json_free(e);
    }
    if (!arh2_is_none(api_key_hint) && !arh2_truthy(current)) {
        json_t *e = arh2_pool_entries(ctx);
        json_t *f = arh2_entry_for(e, "runtime_api_key", api_key_hint);
        if (f) current = json_copy(f);
        json_free(e);
    }
    if (!arh2_truthy(current)) {
        json_free(current);
        current = arh2_pool_current(ctx);
    }
    json_t *lst = arh2_get(current, "last_status");
    const char *exhausted = arh2_sdef(ctx->env, "status_exhausted", "exhausted");
    bool pre_exhausted = arh2_is_str(lst) && arh2_str_eq(lst->str_val, exhausted);
    if (pre_exhausted) {
        json_t *a = json_array();
        if (lst) json_append(a, json_copy(lst));
        arh2_log(ctx, "ra.", "info",
                 "Credential already exhausted (last_status=%s) — rotating immediately instead of retrying",
                 a, false);
        return arh2_rotate_swap(ctx, "rate limit, pre-exhausted");
    }
    bool usage = false;
    if (arh2_is_obj(error_context)) {
        char *reason = arh2_lower_dup(arh2_sdef(error_context, "reason", ""));
        char *msg = arh2_lower_dup(arh2_sdef(error_context, "message", ""));
        usage = arh2_contains_tok(reason, arh2_get(ctx->env, "usage_limit_reason_tokens")) ||
                arh2_contains_tok(msg, arh2_get(ctx->env, "usage_limit_message_tokens"));
        free(reason); free(msg);
    }
    if (!has_retried_429 && !usage) return arh2_pair_bool(false, true);
    return arh2_rotate_swap(ctx, "rate limit");
}

/* ── _build_anthropic_client_from_runtime ────────────────────────────────── */
static json_t *arh2_get_timeout(arh2_ctx *ctx, const char *provider, const char *model) {
    json_t *a = json_object();
    arh2_arg_str(a, "provider", provider);
    arh2_arg_str(a, "model", model);
    return arh2_call(ctx, "get_provider_request_timeout", a);
}
/* PoP: arh2_build_anthropic_client_from_runtime @ agent/agent_runtime_helpers.py:_build_anthropic_client_from_runtime */
static json_t *arh2_build_anthropic_client_from_runtime(arh2_ctx *ctx, json_t *agent, json_t *rt) {
    json_t *key = arh2_get(rt, "anthropic_api_key");
    json_t *base = arh2_get(rt, "anthropic_base_url");
    json_set(agent, "_anthropic_api_key", arh2_copy_or_null(key));
    json_set(agent, "_anthropic_base_url", arh2_copy_or_null(base));
    json_t *t = arh2_get_timeout(ctx, arh2_sdef(agent, "provider", ""), arh2_sdef(agent, "model", ""));
    json_t *a = json_object();
    arh2_arg(a, "api_key", key);
    arh2_arg(a, "base_url", base);
    arh2_arg(a, "timeout", t);
    json_t *cli = arh2_call(ctx, "build_anthropic_client", a);
    json_set(agent, "_anthropic_client", cli ? cli : json_null());
    json_set(agent, "_is_anthropic_oauth", arh2_copy_or_null(arh2_get(rt, "is_anthropic_oauth")));
    json_set(agent, "client", json_null());
    json_free(t);
    return NULL;
}

/* ── _rebuild_primary_client ─────────────────────────────────────────────── */
/* PoP: arh2_rebuild_primary_client @ agent/agent_runtime_helpers.py:_rebuild_primary_client */
static json_t *arh2_rebuild_primary_client(arh2_ctx *ctx, json_t *agent, json_t *rt, const char *reason) {
    char *prov = arh2_field_lower_strip(agent, "provider");
    if (arh2_str_eq(prov, "moa")) {
        json_t *a = json_object();
        arh2_arg(a, "agent", agent);
        arh2_arg(a, "model", arh2_get(agent, "model"));
        json_t *cli = arh2_call(ctx, "build_moa_facade", a);
        json_set(agent, "client", cli ? cli : json_null());
        json_set(agent, "_anthropic_client", json_null());
    } else if (arh2_str_eq(arh2_sdef(agent, "provider", ""), "bedrock") &&
               (arh2_str_eq(arh2_sdef(agent, "api_mode", ""), "anthropic_messages") ||
                arh2_str_eq(arh2_sdef(agent, "api_mode", ""), "bedrock_converse"))) {
        json_t *a = json_object();
        arh2_arg(a, "agent", agent);
        arh2_arg(a, "base_url", arh2_get(agent, "base_url"));
        arh2_arg_str(a, "api_mode", arh2_sdef(agent, "api_mode", ""));
        json_t *r = arh2_call(ctx, "bind_bedrock_runtime", a);
        json_free(r);
    } else if (arh2_str_eq(arh2_sdef(agent, "api_mode", ""), "anthropic_messages")) {
        arh2_build_anthropic_client_from_runtime(ctx, agent, rt);
    } else {
        json_t *ck = arh2_get(rt, "client_kwargs");
        json_t *a = json_object();
        arh2_arg(a, "client_kwargs", arh2_is_obj(ck) ? ck : json_object());
        arh2_arg_str(a, "reason", reason);
        json_set(a, "shared", json_bool(true));
        json_t *cli = arh2_call(ctx, "agent._create_openai_client", a);
        json_set(agent, "client", cli ? cli : json_null());
    }
    free(prov);
    return NULL;
}

/* ── _primary_reset_gate_blocks ──────────────────────────────────────────── */
static json_t *arh2_matches_primary(arh2_ctx *ctx, json_t *obj) {
    json_t *a = json_object();
    arh2_arg(a, "obj", obj);
    return arh2_call(ctx, "matches_primary", a);
}
static json_t *arh2_load_primary_pool(arh2_ctx *ctx) {
    return arh2_call(ctx, "load_primary_pool", json_object());
}
static json_t *arh2_pool_next(arh2_ctx *ctx, const char *model) {
    json_t *a = json_object();
    if (model && model[0]) json_set(a, "model", json_string(model));
    return arh2_call(ctx, "pool.next_available_at", a);
}
/* PoP: arh2_primary_reset_gate_blocks @ agent/agent_runtime_helpers.py:_primary_reset_gate_blocks */
static json_t *arh2_primary_reset_gate_blocks(arh2_ctx *ctx, json_t *agent, json_t *rt,
        const char *primary_provider, const char *primary_runtime_base_url) {
    (void)primary_runtime_base_url;
    json_t *prefetched_pool = NULL;
    bool prefetched = false;
    json_t *pool = arh2_get(agent, "_credential_pool");
    json_t *m = arh2_matches_primary(ctx, pool);
    bool matches = arh2_truthy(m); json_free(m);
    if (!matches) {
        json_free(prefetched_pool);
        prefetched_pool = pool = arh2_load_primary_pool(ctx);
        prefetched = true;
    }
    char *primary_model = arh2_strip(arh2_sdef(rt, "model", ""));
    json_t *next_at = arh2_truthy(pool) ? arh2_pool_next(ctx, primary_model) : NULL;
    free(primary_model);
    double now = arh2_ndef(ctx->env, "now", 0.0);
    if (arh2_is_num(next_at) && next_at->num_val > now) {
        if (!arh2_bdef(agent, "_restore_wait_logged", false)) {
            json_set(agent, "_restore_wait_logged", json_bool(true));
            json_t *a = json_object();
            json_set(a, "ts", json_copy(next_at));
            json_t *iso = arh2_call(ctx, "datetime.isoformat", a);
            json_t *args = json_array();
            json_append(args, json_string(primary_provider ? primary_provider : "?"));
            json_append(args, iso ? iso : json_null());
            json_append(args, json_string(arh2_sdef(agent, "provider", "")));
            json_append(args, json_string(arh2_sdef(agent, "model", "")));
            arh2_log(ctx, "", "info",
                     "Primary %s rate-limited until %s; staying on fallback %s/%s until the reset elapses",
                     args, false);
        }
        json_t *r = json_array();
        json_append(r, json_bool(true));
        json_append(r, prefetched_pool ? json_copy(prefetched_pool) : json_null());
        json_append(r, json_bool(prefetched));
        json_free(next_at);
        return r;
    }
    json_free(next_at);
    json_t *r = json_array();
    json_append(r, json_bool(false));
    json_append(r, prefetched_pool ? json_copy(prefetched_pool) : json_null());
    json_append(r, json_bool(prefetched));
    return r;
}

/* ── _rebind_primary_credential_pool ─────────────────────────────────────── */
static json_t *arh2_pool_has_available(arh2_ctx *ctx, const char *model) {
    json_t *a = json_object();
    if (model && model[0]) json_set(a, "model", json_string(model));
    return arh2_call(ctx, "pool.has_available", a);
}
static json_t *arh2_pool_select(arh2_ctx *ctx, const char *model) {
    json_t *a = json_object();
    if (model && model[0]) json_set(a, "model", json_string(model));
    return arh2_call(ctx, "pool.select", a);
}
/* PoP: arh2_rebind_primary_credential_pool @ agent/agent_runtime_helpers.py:_rebind_primary_credential_pool */
static json_t *arh2_rebind_primary_credential_pool(arh2_ctx *ctx, json_t *agent,
        const char *primary_provider, const char *primary_model,
        json_t *prefetched_pool, bool prefetched) {
    json_t *pool = arh2_get(agent, "_credential_pool");
    char *pp = arh2_field_lower_strip(pool, "provider");
    bool matches = false;
    if (arh2_truthy(pool) && pp[0]) {
        json_t *m = arh2_matches_primary(ctx, pool);
        matches = arh2_truthy(m); json_free(m);
    }
    if (arh2_truthy(pool) && pp[0] && !matches) {
        json_set(agent, "_credential_pool", json_null());
        json_set(agent, "_credential_pool_entry_id", json_null());
        json_t *reloaded = prefetched ? json_copy(prefetched_pool) : arh2_load_primary_pool(ctx);
        json_set(agent, "_credential_pool", reloaded ? reloaded : json_null());
        if (arh2_exc_pending(ctx)) {
            json_t *args = json_array();
            json_append(args, json_string(primary_provider ? primary_provider : ""));
            json_append(args, arh2_exc_arg(ctx));
            arh2_log(ctx, "", "warning",
                     "Restore could not reload primary credential pool for %s: %s", args, false);
            arh2_exc_clear(ctx);
        }
    }
    free(pp);
    json_set(agent, "_credential_pool_entry_id", json_null());
    pool = arh2_get(agent, "_credential_pool");
    json_t *entry = NULL;
    if (arh2_truthy(pool)) {
        json_t *h = arh2_pool_has_available(ctx, primary_model);
        bool avail = arh2_truthy(h); json_free(h);
        if (avail) entry = arh2_pool_select(ctx, primary_model);
    }
    bool has_key = arh2_truthy(arh2_get(entry, "runtime_api_key")) ||
                   arh2_truthy(arh2_get(entry, "access_token"));
    if (!arh2_truthy(entry) || !has_key) { json_free(entry); return NULL; }
    json_t *me = arh2_matches_primary(ctx, entry);
    bool em = arh2_truthy(me); json_free(me);
    if (em) {
        json_t *a = json_object();
        arh2_arg(a, "entry", entry);
        json_t *r = arh2_call(ctx, "agent._swap_credential", a);
        json_free(r);
        json_t *args = json_array();
        json_append(args, json_copy(arh2_get_def(entry, "id", json_string("?"))));
        json_append(args, json_copy(arh2_get_def(entry, "label", json_string("?"))));
        arh2_log(ctx, "", "info", "Restore re-selected pool entry %s (%s)", args, false);
    } else {
        json_t *args = json_array();
        json_append(args, json_copy(arh2_get_def(entry, "id", json_string("?"))));
        json_append(args, json_copy(arh2_get_def(entry, "label", json_string("?"))));
        char *ep = arh2_field_lower_strip(entry, "provider");
        json_append(args, json_string(ep[0] ? ep : "?"));
        json_append(args, json_string(primary_provider ? primary_provider : "?"));
        free(ep);
        arh2_log(ctx, "", "info",
                 "Restore skipped pool entry %s (%s): provider %s does not match primary provider %s",
                 args, false);
    }
    json_free(entry);
    return NULL;
}

/* ── _primary_quota_reopened_early ───────────────────────────────────────── */
static json_t *arh2_pool_lift(arh2_ctx *ctx, const char *model) {
    json_t *a = json_object();
    if (model && model[0]) json_set(a, "model", json_string(model));
    return arh2_call(ctx, "pool.lift_reopened_cooldowns", a);
}
/* PoP: arh2_primary_quota_reopened_early @ agent/agent_runtime_helpers.py:_primary_quota_reopened_early */
static json_t *arh2_primary_quota_reopened_early(arh2_ctx *ctx, json_t *agent,
        const char *primary_provider, const char *primary_model) {
    if (!arh2_str_eq(primary_provider, "openai-codex")) return json_bool(false);
    double mono = arh2_ndef(ctx->env, "monotonic", 0.0);
    double interval = arh2_ndef(ctx->env, "codex_quota_probe_min_interval", 30.0);
    json_t *checked = arh2_get(agent, "_codex_reopen_checked_at");
    double prev = arh2_is_num(checked) ? checked->num_val : -1e18;
    if (mono - prev < interval) return json_bool(false);
    json_set(agent, "_codex_reopen_checked_at", json_number(mono));
    json_t *pool = arh2_get(agent, "_credential_pool");
    bool matches = false;
    /* Python short-circuits `pool is None or not matches_primary(pool)`:
     * no matches_primary seam fires when the agent has no pool. */
    if (arh2_truthy(pool)) {
        json_t *m = arh2_matches_primary(ctx, pool);
        matches = arh2_truthy(m); json_free(m);
    }
    if (!arh2_truthy(pool) || !matches) {
        pool = arh2_load_primary_pool(ctx);
    }
    if (!arh2_truthy(pool)) return json_bool(false);
    json_t *until = arh2_pool_next(ctx, primary_model);
    double now = arh2_ndef(ctx->env, "now", 0.0);
    bool want = arh2_is_num(until) && until->num_val > now;
    json_free(until);
    if (!want) { json_free(pool); return json_bool(false); }
    json_t *r = arh2_pool_lift(ctx, primary_model);
    bool lifted = arh2_truthy(r); json_free(r); json_free(pool);
    return json_bool(lifted);
}

/* ── _moa_aggregator_cache_policy ────────────────────────────────────────── */
static json_t *arh2_anthropic_cache_policy(arh2_ctx *ctx, json_t *agent,
        const char *provider, const char *base_url, const char *api_mode, const char *model) {
    json_t *a = json_object();
    arh2_arg(a, "agent", agent);
    arh2_arg_str(a, "provider", provider);
    arh2_arg_str(a, "base_url", base_url);
    arh2_arg_str(a, "api_mode", api_mode);
    arh2_arg_str(a, "model", model);
    return arh2_call(ctx, "anthropic_prompt_cache_policy", a);
}
/* PoP: arh2_moa_aggregator_cache_policy @ agent/agent_runtime_helpers.py:_moa_aggregator_cache_policy */
static json_t *arh2_moa_aggregator_cache_policy(arh2_ctx *ctx, json_t *agent, const char *eff_model) {
    json_t *cfg = arh2_call(ctx, "load_config", json_object());
    if (arh2_exc_pending(ctx)) {
        /* Python's try/except wraps load_config too: log once, fail closed. */
        json_t *args = json_array(); json_append(args, arh2_exc_arg(ctx));
        arh2_log(ctx, "", "debug", "MoA aggregator cache-policy resolution failed: %s", args, false);
        arh2_exc_clear(ctx);
        json_free(cfg);
        return arh2_pair_bool(false, false);
    }
    /* Python: resolve_moa_preset(_load_moa_cfg().get("moa") or {}, eff_model or None)
     * -> pass only the "moa" subsection, empty dict when absent/falsy. */
    json_t *moa = arh2_get(cfg, "moa");
    json_t *moa_cfg = arh2_truthy(moa) ? json_copy(moa) : json_object();
    json_free(cfg);
    json_t *a = json_object();
    arh2_arg(a, "config", moa_cfg);
    if (eff_model && *eff_model) arh2_arg_str(a, "model", eff_model);  /* "" -> None -> dropped */
    json_free(moa_cfg);
    json_t *preset = arh2_call(ctx, "resolve_moa_preset", a);
    json_t *agg = arh2_get(preset, "aggregator");
    const char *prov = arh2_strip(arh2_sdef(agg, "provider", ""));
    const char *model = arh2_strip(arh2_sdef(agg, "model", ""));
    json_t *result = arh2_pair_bool(false, false);
    if (prov[0] && model[0]) {
        const char *base = "", *mode = "";
        json_t *b = json_object();
        arh2_arg_str(b, "requested", prov);
        arh2_arg_str(b, "target_model", model);
        json_t *rt = arh2_call(ctx, "resolve_runtime_provider", b);
        if (arh2_exc_pending(ctx)) arh2_exc_clear(ctx);
        else {
            base = arh2_sdef(rt, "base_url", "");
            mode = arh2_sdef(rt, "api_mode", "");
        }
        json_free(rt);
        json_t *pol = arh2_anthropic_cache_policy(ctx, agent, prov, base, mode, model);
        json_free(result); result = pol;
    }
    json_free(preset);
    if (arh2_exc_pending(ctx)) {
        json_t *args = json_array(); json_append(args, arh2_exc_arg(ctx));
        arh2_log(ctx, "", "debug", "MoA aggregator cache-policy resolution failed: %s", args, false);
        arh2_exc_clear(ctx);
        json_free(result); result = arh2_pair_bool(false, false);
    }
    return result;
}

/* ── _route_may_be_custom ────────────────────────────────────────────────── */
static json_t *arh2_norm_url(arh2_ctx *ctx, const char *url) {
    json_t *a = json_object();
    arh2_arg_str(a, "url", url);
    return arh2_call(ctx, "normalize_route_base_url", a);
}
static json_t *arh2_hostname(arh2_ctx *ctx, const char *url) {
    json_t *a = json_object();
    arh2_arg_str(a, "url", url);
    return arh2_call(ctx, "base_url_hostname", a);
}
/* PoP: arh2_route_may_be_custom @ agent/agent_runtime_helpers.py:_route_may_be_custom */
static json_t *arh2_route_may_be_custom(arh2_ctx *ctx, json_t *agent,
        const char *eff_provider, const char *provider_lower, const char *eff_base_url) {
    json_t *custom = arh2_get(agent, "_custom_providers");
    if (arh2_truthy(custom)) {
        json_t *eff_norm = arh2_norm_url(ctx, eff_base_url);
        const char *en = arh2_jstr(eff_norm);
        bool any = false;
        if (arh2_is_arr(custom)) {
            for (size_t i = 0; i < custom->c.count; i++) {
                json_t *e = custom->c.items[i];
                if (!arh2_is_obj(e)) continue;
                json_t *a = json_object();
                json_set(a, "name", json_string(arh2_sdef(e, "name", "")));
                json_set(a, "provider_key", json_string(arh2_sdef(e, "provider_key", "")));
                json_t *aliases = arh2_call(ctx, "custom_provider_aliases", a);
                bool hit = false;
                if (arh2_is_arr(aliases)) {
                    for (size_t j = 0; j < aliases->c.count; j++) {
                        const char *x = arh2_jstr(aliases->c.items[j]);
                        if (arh2_str_eq(x, provider_lower) ||
                            arh2_str_eq(x, arh2_removeprefix(provider_lower, "custom:"))) { hit = true; break; }
                    }
                }
                json_free(aliases);
                if (!hit && en[0]) {
                    json_t *n = arh2_norm_url(ctx, arh2_sdef(e, "base_url", ""));
                    hit = arh2_str_eq(arh2_jstr(n), en);
                    json_free(n);
                }
                if (hit) { any = true; break; }
            }
        }
        json_free(eff_norm);
        return json_bool(any);
    }
    if (!arh2_is_none(custom)) return json_bool(false);
    json_t *a = json_object();
    arh2_arg_str(a, "provider", eff_provider);
    json_set(a, "allow_network", json_bool(false));
    json_t *pd = arh2_call(ctx, "get_provider", a);
    if (arh2_exc_pending(ctx)) {
        json_t *args = json_array(); json_append(args, arh2_exc_arg(ctx));
        arh2_log(ctx, "", "debug", "provider lookup failed during cache-policy pre-gate: %s", args, false);
        arh2_exc_clear(ctx);
        return json_bool(arh2_starts(provider_lower, "custom:"));
    }
    if (!arh2_truthy(pd)) { json_free(pd); return json_bool(true); }
    const char *pu = arh2_sdef(pd, "base_url", "");
    bool res = false;
    if (pu[0]) {
        json_t *h1 = arh2_hostname(ctx, pu);
        json_t *h2 = arh2_hostname(ctx, eff_base_url);
        res = !arh2_str_eq(arh2_jstr(h1), arh2_jstr(h2));
        json_free(h1); json_free(h2);
    }
    json_free(pd);
    return json_bool(res);
}

/* ── _provider_supplied_client ───────────────────────────────────────────── */
static json_t *arh2_profile_call(arh2_ctx *ctx, const char *name, json_t *args) {
    return arh2_call(ctx, name, args);
}
/* PoP: arh2_provider_supplied_client @ agent/agent_runtime_helpers.py:_provider_supplied_client */
static json_t *arh2_provider_supplied_client(arh2_ctx *ctx, json_t *agent, json_t *client_kwargs) {
    if (!arh2_bdef(ctx->env, "providers_import_ok", true)) return json_null();
    const char *pname = arh2_strip(arh2_sdef(agent, "provider", ""));
    json_t *profile = NULL;
    if (pname[0]) {
        json_t *a = json_object();
        json_set(a, "name", json_string(pname));
        profile = arh2_profile_call(ctx, "get_provider_profile", a);
        if (arh2_exc_pending(ctx)) { arh2_exc_clear(ctx); json_free(profile); profile = NULL; }
    }
    if (!arh2_truthy(profile)) {
        json_free(profile);
        const char *base = arh2_strip(arh2_sdef(client_kwargs, "base_url", ""));
        if (base[0]) {
            json_t *a = json_object();
            json_set(a, "base_url", json_string(base));
            profile = arh2_profile_call(ctx, "_profile_for_base_url", a);
        }
    }
    if (!arh2_truthy(profile)) { json_free(profile); return json_null(); }
    json_t *a = json_object();
    if (arh2_is_obj(client_kwargs))
        for (size_t i = 0; i < client_kwargs->c.count; i++)
            arh2_arg(a, client_kwargs->c.keys[i], client_kwargs->c.items[i]);
    json_t *client = arh2_profile_call(ctx, "profile.create_client", a);
    if (arh2_exc_pending(ctx)) {
        const char *auth = arh2_sdef(profile, "auth_type", "");
        if (arh2_str_eq(auth, "external_process")) { /* propagate */ return NULL; }
        json_t *args = json_array();
        json_append(args, json_string(arh2_sdef(profile, "name", pname)[0] ? arh2_sdef(profile, "name", pname) : "?"));
        arh2_log(ctx, "ra.", "warning",
                  "Provider profile %r failed to create a client; falling back to the standard client path",
                  args, true);
        arh2_exc_clear(ctx);
        json_free(client);
        json_free(profile);
        return json_null();
    }
    json_free(profile);
    return client;
}

/* ── _ensure_route_client_headers ────────────────────────────────────────── */
/* PoP: arh2_ensure_route_client_headers @ agent/agent_runtime_helpers.py:_ensure_route_client_headers */
static json_t *arh2_ensure_route_client_headers(arh2_ctx *ctx, json_t *client_kwargs, const char *provider) {
    json_t *a = json_object();
    json_set(a, "kind", json_string("client_headers"));
    arh2_arg_str(a, "provider", provider);
    json_set(a, "base_url", json_string(arh2_sdef(client_kwargs, "base_url", "")));
    json_set(a, "api_key", json_string(arh2_sdef(client_kwargs, "api_key", "")));
    json_t *required = arh2_call(ctx, "route_answer", a);
    if (!arh2_truthy(required)) { json_free(required); return NULL; }
    json_t *existing = json_object();
    json_t *dh = arh2_get(client_kwargs, "default_headers");
    if (arh2_is_obj(dh))
        for (size_t i = 0; i < dh->c.count; i++) {
            char *lk = arh2_lower_dup(dh->c.keys[i]);
            json_set(existing, lk, json_copy(dh->c.items[i]));
            free(lk);
        }
    if (arh2_is_obj(required)) {
        for (size_t i = 0; i < required->c.count; i++) {
            char *lk = arh2_lower_dup(required->c.keys[i]);
            bool present = arh2_is_obj(existing) && json_obj_get(existing, lk) != NULL;
            free(lk);
            if (!present) json_set(existing, required->c.keys[i], json_copy(required->c.items[i]));
        }
    }
    json_set(client_kwargs, "default_headers", existing);
    json_free(required);
    return NULL;
}

/* ── _gemini_native_client ───────────────────────────────────────────────── */
/* PoP: arh2_gemini_native_client @ agent/agent_runtime_helpers.py:_gemini_native_client */
static json_t *arh2_gemini_native_client(arh2_ctx *ctx, json_t *agent, json_t *client_kwargs,
        json_t *httpx_verify, const char *reason, bool shared) {
    const char *base = arh2_sdef(client_kwargs, "base_url", "");
    json_t *a = json_object();
    json_set(a, "base_url", json_string(base));
    json_t *native = arh2_call(ctx, "is_native_gemini_base_url", a);
    bool is_native = arh2_truthy(native); json_free(native);
    if (!is_native) return json_null();
    static const char *KEEP[] = {"api_key", "base_url", "default_headers", "timeout", "http_client"};
    json_t *safe = json_object();
    for (size_t i = 0; i < sizeof(KEEP) / sizeof(KEEP[0]); i++)
        if (arh2_has(client_kwargs, KEEP[i]))
            json_set(safe, KEEP[i], json_copy(arh2_get(client_kwargs, KEEP[i])));
    if (!arh2_has(safe, "http_client")) {
        json_t *b = json_object();
        json_set(b, "base_url", json_string(base));
        arh2_arg(b, "verify", httpx_verify);
        json_t *ka = arh2_call(ctx, "agent._build_keepalive_http_client", b);
        if (arh2_truthy(ka)) json_set(safe, "http_client", ka);
        else json_free(ka);
    }
    json_t *c = json_object();
    for (size_t i = 0; i < safe->c.count; i++) json_set(c, safe->c.keys[i], json_copy(safe->c.items[i]));
    json_t *client = arh2_call(ctx, "GeminiNativeClient", c);
    json_t *lc = arh2_call(ctx, "agent._client_log_context", json_object());
    json_t *args = json_array();
    json_append(args, json_string(reason ? reason : ""));
    json_append(args, json_bool(shared));
    json_append(args, lc ? lc : json_null());
    arh2_log(ctx, "ra.", "info", "Gemini native client created (%s, shared=%s) %s", args, false);
    json_free(safe);
    return client;
}

/* ── _apply_switched_provider_request_overrides ──────────────────────────── */
/* PoP: arh2_apply_switched_provider_request_overrides @ agent/agent_runtime_helpers.py:_apply_switched_provider_request_overrides */
static json_t *arh2_apply_switched_provider_request_overrides(arh2_ctx *ctx, json_t *agent, const char *new_provider) {
    json_t *cp = json_copy(arh2_get(agent, "_custom_providers"));
    if (arh2_is_none(cp)) {
        json_t *cfg = arh2_call(ctx, "load_config", json_object());
        json_t *a = json_object();
        arh2_arg(a, "config", cfg);
        cp = arh2_call(ctx, "get_compatible_custom_providers", a);
        json_free(cfg);
        if (arh2_exc_pending(ctx)) { arh2_exc_clear(ctx); cp = json_array(); }
    }
    json_t *a = json_object();
    arh2_arg_str(a, "provider", new_provider);
    /* Python passes getattr(agent,"model","") or "" -- missing/falsy becomes
     * an empty STRING in the recorded kwargs, not an omitted key. */
    json_t *am = arh2_get(agent, "model");
    json_t *ab = arh2_get(agent, "base_url");
    json_set(a, "model", json_string(arh2_truthy(am) ? arh2_jstr(am) : ""));
    json_set(a, "base_url", json_string(arh2_truthy(ab) ? arh2_jstr(ab) : ""));
    arh2_arg(a, "custom_providers", cp ? cp : json_array());
    json_t *extra = arh2_call(ctx, "_custom_provider_extra_body_for_agent", a);
    if (arh2_exc_pending(ctx)) {
        /* Python's except leaves agent.request_overrides UNCHANGED (None here);
         * the caller logs the debug line.  Do not install an empty dict. */
        json_free(extra);
        return NULL;
    }
    json_t *ov = json_object();
    json_t *old = arh2_get(agent, "request_overrides");
    if (arh2_is_obj(old))
        for (size_t i = 0; i < old->c.count; i++)
            if (!arh2_str_eq(old->c.keys[i], "extra_body"))
                json_set(ov, old->c.keys[i], json_copy(old->c.items[i]));
    if (arh2_truthy(extra)) {
        /* Python: overrides["extra_body"] = dict(new_extra_body) -- nested,
         * never flattened into the top-level overrides dict. */
        json_set(ov, "extra_body", json_copy(extra));
    }
    json_free(extra);
    json_set(agent, "request_overrides", ov);
    return NULL;
}

/* ── _resolve_switch_destination ─────────────────────────────────────────── */
static json_t *arh2_determine_api_mode(arh2_ctx *ctx, const char *provider, const char *base_url, const char *model) {
    json_t *a = json_object();
    arh2_arg_str(a, "provider", provider);
    arh2_arg_str(a, "base_url", base_url);
    arh2_arg_str(a, "model", model);
    return arh2_call(ctx, "determine_api_mode", a);
}
static json_t *arh2_is_actual_route(arh2_ctx *ctx, const char *provider, const char *base_url) {
    json_t *a = json_object();
    arh2_arg_str(a, "provider", provider);
    arh2_arg_str(a, "base_url", base_url);
    return arh2_call(ctx, "is_actual_route", a);
}
/* PoP: arh2_resolve_switch_destination @ agent/agent_runtime_helpers.py:_resolve_switch_destination */
static json_t *arh2_resolve_switch_destination(arh2_ctx *ctx, json_t *agent,
        const char *new_model, const char *new_provider, const char *base_url,
        const char *api_mode, json_t *capabilities, const char *old_norm, const char *new_norm) {
    char *mode = strdup(api_mode ? api_mode : "");
    char *url = strdup(base_url ? base_url : "");
    if (!mode[0]) {
        json_t *dm = arh2_determine_api_mode(ctx, new_provider, url, new_model);
        free(mode);
        mode = strdup(arh2_jstr(dm));
        json_free(dm);
    }
    if (!url[0] && arh2_str_eq(new_norm, "openai")) { free(url); url = strdup("https://api.openai.com/v1"); }
    char *eff = strdup(url);
    if (!eff[0] && arh2_str_eq(old_norm, new_norm)) { free(eff); eff = strdup(arh2_sdef(agent, "base_url", "")); }
    json_t *actual = arh2_is_actual_route(ctx, new_provider, eff);
    bool is_actual = arh2_truthy(actual); json_free(actual);
    if (is_actual) {
        free(mode); mode = strdup("chat_completions");
        if (eff[0]) {
            json_t *a = json_object(); json_set(a, "base_url", json_string(eff));
            json_t *n = arh2_call(ctx, "normalize_actual_base_url", a);
            free(url); url = strdup(arh2_jstr(n)); json_free(n);
        }
    }
    json_t *caps = NULL;
    if (arh2_is_obj(capabilities)) caps = json_copy(capabilities);
    else {
        json_t *a = json_object();
        arh2_arg_str(a, "model", new_model);
        arh2_arg_str(a, "base_url", eff);
        arh2_arg_str(a, "provider", new_provider);
        json_set(a, "is_codex_backend", json_bool(arh2_str_eq(new_norm, "openai-codex")));
        caps = arh2_call(ctx, "resolve_native_compaction_capabilities", a);
    }
    if (arh2_str_eq(mode, "anthropic_messages")) {
        json_t *a = json_object(); json_set(a, "provider", json_string(new_provider ? new_provider : ""));
        json_t *fam = arh2_call(ctx, "opencode_provider_family", a);
        bool oc = arh2_truthy(fam); json_free(fam);
        if (oc && url[0]) {
            size_t n = strlen(url);
            if (n >= 3 && strcmp(url + n - 3, "/v1") == 0) url[n - 3] = '\0';
            else if (n >= 4 && strcmp(url + n - 4, "/v1/") == 0) url[n - 4] = '\0';
        }
    }
    json_t *r = json_array();
    json_append(r, json_string(mode));
    json_append(r, json_string(url));
    json_append(r, caps ? caps : json_null());
    free(mode); free(url); free(eff);
    return r;
}

/* ── _build_switched_client ──────────────────────────────────────────────── */
/* PoP: arh2_build_switched_client @ agent/agent_runtime_helpers.py:_build_switched_client */
static json_t *arh2_build_switched_client(arh2_ctx *ctx, json_t *agent, const char *new_provider,
        json_t *api_key, const char *base_url, const char *api_mode, const char *new_norm) {
    if (arh2_str_eq(new_norm, "moa")) {
        json_t *a = json_object();
        arh2_arg(a, "agent", agent);
        arh2_arg(a, "model", arh2_get(agent, "model"));
        arh2_arg(a, "api_key", api_key);
        json_t *r = arh2_call(ctx, "bind_moa_runtime", a); json_free(r);
        return NULL;
    }
    if (arh2_str_eq(new_provider, "bedrock") &&
        (arh2_str_eq(api_mode, "anthropic_messages") || arh2_str_eq(api_mode, "bedrock_converse"))) {
        json_t *a = json_object();
        arh2_arg(a, "agent", agent);
        json_set(a, "base_url", json_string((base_url && base_url[0]) ? base_url : arh2_sdef(agent, "base_url", "")));
        arh2_arg_str(a, "api_mode", api_mode);
        json_t *r = arh2_call(ctx, "bind_bedrock_runtime", a); json_free(r);
        return NULL;
    }
    if (arh2_str_eq(api_mode, "anthropic_messages")) {
        bool native = arh2_str_eq(new_provider, "anthropic");
        json_t *eff = json_copy(api_key ? api_key : json_null());
        if (!arh2_truthy(eff)) { json_free(eff); eff = json_copy(arh2_get(agent, "api_key")); }
        if (!arh2_truthy(eff) && native) {
            json_t *a = json_object();
            arh2_arg(a, "base_url", arh2_get(agent, "_anthropic_base_url"));
            arh2_arg(a, "model", arh2_get(agent, "model"));
            json_free(eff);
            eff = arh2_call(ctx, "native_anthropic_fallback_token", a);
        }
        /* Live: (... or "") -- a non-native route with no key yields "". */
        if (!arh2_truthy(eff)) { json_free(eff); eff = json_string(""); }
        bool is_mm = arh2_str_eq(new_provider, "minimax-oauth") && arh2_is_str(eff) && eff->str_val[0];
        if (is_mm) {
            json_t *tok = arh2_call(ctx, "build_minimax_oauth_token_provider", json_object());
            if (arh2_exc_pending(ctx)) {
                json_t *args = json_array(); json_append(args, arh2_exc_arg(ctx));
                arh2_log(ctx, "", "warning",
                    "MiniMax OAuth: failed to install per-request token provider on switch (%s); using static bearer.",
                    args, false);
                arh2_exc_clear(ctx);
            } else { json_free(eff); eff = tok; }
        }
        json_set(agent, "api_key", arh2_copy_or_null(eff));
        json_set(agent, "_anthropic_api_key", arh2_copy_or_null(eff));
        json_set(agent, "_anthropic_base_url", json_string((base_url && base_url[0]) ? base_url : arh2_sdef(agent, "_anthropic_base_url", "")));
        json_t *t = arh2_get_timeout(ctx, arh2_sdef(agent, "provider", ""), arh2_sdef(agent, "model", ""));
        json_t *a = json_object();
        arh2_arg(a, "api_key", eff);
        arh2_arg(a, "base_url", arh2_get(agent, "_anthropic_base_url"));
        arh2_arg(a, "timeout", t);
        json_t *cli = arh2_call(ctx, "build_anthropic_client", a);
        json_set(agent, "_anthropic_client", cli ? cli : json_null());
        json_free(t);
        json_t *b = json_object();
        arh2_arg(b, "base_url", arh2_get(agent, "_anthropic_base_url"));
        arh2_arg(b, "api_key", eff);
        arh2_arg_str(b, "provider", new_provider);
        json_t *oauth = arh2_call(ctx, "anthropic_route_is_oauth", b);
        json_set(agent, "_is_anthropic_oauth", oauth ? oauth : json_null());
        json_set(agent, "client", json_null());
        json_set(agent, "_client_kwargs", json_object());
        json_free(eff);
        return NULL;
    }
    const char *effbase = (base_url && base_url[0]) ? base_url : arh2_sdef(agent, "base_url", "");
    json_t *ck = json_object();
    /* Python: {"api_key": api_key or agent.api_key, ...} -- the new api_key
     * argument wins when truthy; the agent's old key is only a fallback. */
    json_set(ck, "api_key", arh2_truthy(api_key) ? json_copy(api_key)
                                                 : json_copy(arh2_get(agent, "api_key")));
    json_set(ck, "base_url", json_string(effbase));
    json_set(agent, "_client_kwargs", json_copy(ck));
    json_t *cfg = arh2_call(ctx, "load_config_readonly", json_object());
    json_t *a = json_object();
    arh2_arg(a, "client_kwargs", ck);
    json_set(a, "base_url", json_string(effbase));
    json_t *b = json_object(); arh2_arg(b, "config", cfg);
    json_t *cps = arh2_call(ctx, "get_compatible_custom_providers", b);
    /* The live call passes custom_providers POSITIONALLY; the seam projection
     * reads only kwargs, so it is never recorded in the TLS-call args. */
    json_free(cps);
    json_t *r = arh2_call(ctx, "apply_custom_provider_tls_to_client_kwargs", a);
    json_free(cfg); json_free(r);
    if (arh2_exc_pending(ctx)) {
        arh2_log(ctx, "", "debug", "custom-provider TLS resolution skipped on switch_model", json_array(), true);
        arh2_exc_clear(ctx);
    }
    json_t *t = arh2_get_timeout(ctx, arh2_sdef(agent, "provider", ""), arh2_sdef(agent, "model", ""));
    json_t *fck = arh2_get(agent, "_client_kwargs");
    if (arh2_truthy(t)) json_set(fck, "timeout", json_copy(t));
    json_free(t);
    json_t *h = json_object(); json_set(h, "base_url", json_string(effbase));
    json_t *hr = arh2_call(ctx, "agent._apply_client_headers_for_base_url", h); json_free(hr);
    json_t *ca = json_object();
    arh2_arg(ca, "client_kwargs", fck);
    arh2_arg_str(ca, "reason", "switch_model");
    json_set(ca, "shared", json_bool(true));
    json_t *cli = arh2_call(ctx, "agent._create_openai_client", ca);
    json_set(agent, "client", cli ? cli : json_null());
    return NULL;
}

/* ── _swap_switch_runtime ────────────────────────────────────────────────── */
/* PoP: arh2_swap_switch_runtime @ agent/agent_runtime_helpers.py:_swap_switch_runtime */
static json_t *arh2_swap_switch_runtime(arh2_ctx *ctx, json_t *agent,
        const char *new_model, const char *new_provider, json_t *api_key,
        const char *base_url, const char *api_mode, const char *old_provider,
        const char *old_norm, const char *new_norm) {
    json_set(agent, "_config_context_length", json_null());
    json_set(agent, "model", json_string(new_model ? new_model : ""));
    json_set(agent, "provider", json_string(new_provider ? new_provider : ""));
    json_set(agent, "requested_provider", json_string(new_provider ? new_provider : ""));
    json_t *flag = arh2_call(ctx, "agent._read_reasoning_echo_from_config", json_object());
    json_set(agent, "_reasoning_echo_flag", flag ? flag : json_null());
    if (base_url && base_url[0]) json_set(agent, "base_url", json_string(base_url));
    else if (!arh2_str_eq(old_norm, new_norm)) {
        char msg[512];
        snprintf(msg, sizeof msg,
                 "switch_model: no base_url resolved for provider '%s' (switching from '%s'); refusing to keep the previous provider's endpoint",
                 new_provider ? new_provider : "", old_provider ? old_provider : "");
        arh2_raise(ctx, "ValueError", msg);
        return NULL;
    }
    json_set(agent, "api_mode", json_string(api_mode ? api_mode : ""));
    if (arh2_has(agent, "_transport_cache")) json_set(agent, "_transport_cache", json_object());
    json_t *rva = json_object(); arh2_arg(rva, "agent", agent);
    json_t *rv = arh2_call(ctx, "reset_route_verdicts", rva); json_free(rv);
    if (arh2_truthy(api_key)) json_set(agent, "api_key", json_copy(api_key));
    if (!arh2_str_eq(old_norm, new_norm) || !arh2_truthy(arh2_get(agent, "_credential_pool"))) {
        json_set(agent, "_credential_pool", json_null());
        json_set(agent, "_credential_pool_entry_id", json_null());
        /* Live: load_route_pool(new_provider, agent.base_url) -- both
         * positional; its projection reads only kwargs, so record no args. */
        json_t *a = json_object();
        json_t *pool = arh2_call(ctx, "load_route_pool", a);
        if (arh2_exc_pending(ctx)) {
            json_t *args = json_array();
            json_append(args, json_string(new_provider ? new_provider : ""));
            json_append(args, arh2_exc_arg(ctx));
            arh2_log(ctx, "", "warning",
                "switch_model: credential pool reload failed for %s (%s); continuing without pool rotation this turn",
                args, false);
            arh2_exc_clear(ctx);
        } else json_set(agent, "_credential_pool", pool ? pool : json_null());
    }
    arh2_build_switched_client(ctx, agent, new_provider, api_key, base_url, api_mode, new_norm);
    if (arh2_exc_pending(ctx)) return NULL;
    json_t *a = json_object(); arh2_arg(a, "agent", agent);
    json_t *r = arh2_call(ctx, "sync_credential_pool_entry_id", a); json_free(r);
    return NULL;
}

/* ── _resolve_switch_context_length ──────────────────────────────────────── */
/* PoP: arh2_resolve_switch_context_length @ agent/agent_runtime_helpers.py:_resolve_switch_context_length */
static json_t *arh2_resolve_switch_context_length(arh2_ctx *ctx, json_t *agent, json_t *snapshot) {
    json_t *cp = NULL, *intent = NULL;
    bool cfg_failed = false;
    json_t *cfg = arh2_call(ctx, "load_config", json_object());
    if (arh2_exc_pending(ctx)) {
        /* Python: `except Exception: intent = None` -- no further config seams
         * fire and custom_providers stays None (not []).  A null (non-raising)
         * config does NOT skip the block: the seams still fire. */
        arh2_exc_clear(ctx);
        json_free(cfg);
        cfg = NULL;
        cfg_failed = true;
    }
    if (!cfg_failed) {
        json_t *a = json_object(); arh2_arg(a, "config", cfg);
        cp = arh2_call(ctx, "get_compatible_custom_providers", a);
        if (arh2_exc_pending(ctx)) { arh2_exc_clear(ctx); json_free(cp); cp = NULL; }
        json_t *b = json_object();
        arh2_arg(b, "agent", agent);
        arh2_arg(b, "config", cfg);  /* live: config_context_length_for_runtime(agent, switch_cfg) */
        intent = arh2_call(ctx, "config_context_length_for_runtime", b);
        json_free(cfg);
        if (!arh2_truthy(intent)) {
            json_free(intent);
            json_t *c = json_object();
            arh2_arg(c, "model", arh2_get(agent, "model"));
            arh2_arg(c, "base_url", arh2_get(agent, "base_url"));
            arh2_arg(c, "custom_providers", cp);
            intent = arh2_call(ctx, "get_custom_provider_context_length", c);
        }
        if (arh2_exc_pending(ctx)) { arh2_exc_clear(ctx); json_free(intent); intent = NULL; }
    }
    /* Every following call passes its value POSITIONALLY; the corresponding
     * seam projections read only kwargs, so record no value args. */
    json_t *d = json_object();
    arh2_arg(d, "agent", agent);
    json_t *r = arh2_call(ctx, "set_config_context_length", d); json_free(r);
    json_t *runtime = NULL;
    if (arh2_bdef(ctx->env, "has_lmstudio", true)) {
        json_t *e = json_object();
        runtime = arh2_call(ctx, "agent._ensure_lmstudio_runtime_loaded", e);
        if (arh2_exc_pending(ctx)) {
            arh2_restore_switch_snapshot(ctx, agent, snapshot);
            return NULL; /* keep exc pending: the Python caller re-raises */
        }
    }
    if (arh2_bdef(ctx->env, "has_lmstudio_unverified", false)) {
        json_t *f = json_object();
        json_t *uv = arh2_call(ctx, "agent._lmstudio_load_was_unverified", f);
        if (arh2_truthy(uv)) arh2_log(ctx, "", "warning",
            "LM Studio model activation was rejected or completed without a verifiable active context length during model switch; continuing with configured context",
            json_array(), false);
        json_free(uv);
    }
    json_t *eff = intent;
    if (arh2_bdef(ctx->env, "has_effective_lmstudio", false)) {
        json_t *g = json_object();
        json_free(eff);
        eff = arh2_call(ctx, "agent._effective_lmstudio_context_length", g);
    }
    json_t *out = json_array();
    json_append(out, cp ? cp : json_null());
    json_append(out, eff ? eff : json_null());
    json_free(runtime);
    return out;
}

/* ── _update_switch_compressor ───────────────────────────────────────────── */
/* PoP: arh2_update_switch_compressor @ agent/agent_runtime_helpers.py:_update_switch_compressor */
static json_t *arh2_update_switch_compressor(arh2_ctx *ctx, json_t *agent,
        json_t *custom_providers, json_t *effective_context_length, json_t *snapshot) {
    if (arh2_is_none(custom_providers)) {
        json_t *cfg = arh2_call(ctx, "load_config", json_object());
        json_t *a = json_object(); arh2_arg(a, "config", cfg);
        custom_providers = arh2_call(ctx, "get_compatible_custom_providers", a);
        json_free(cfg);
        if (arh2_exc_pending(ctx)) { arh2_exc_clear(ctx); custom_providers = json_null(); }
    }
    json_t *key = arh2_get(agent, "api_key");
    json_t *a = json_object();
    /* get_model_context_length takes agent.model POSITIONALLY; its projection
     * reads kwargs only, so `model` must not appear in the recorded args. */
    arh2_arg(a, "base_url", arh2_get(agent, "base_url"));
    json_set(a, "api_key", arh2_is_str(key) ? json_copy(key) : json_string(""));
    arh2_arg(a, "provider", arh2_get(agent, "provider"));
    arh2_arg(a, "config_context_length", effective_context_length);
    arh2_arg(a, "custom_providers", custom_providers);
    json_t *clen = arh2_call(ctx, "get_model_context_length", a);
    if (arh2_exc_pending(ctx)) {
        /* Python's try/except re-raises: no update_model, no revalidate. */
        arh2_restore_switch_snapshot(ctx, agent, snapshot);
        json_free(clen);
        return NULL;
    }
    json_t *b = json_object();
    arh2_arg(b, "model", arh2_get(agent, "model"));
    arh2_arg(b, "context_length", clen);
    arh2_arg(b, "base_url", arh2_get(agent, "base_url"));
    arh2_arg(b, "api_key", arh2_get(agent, "api_key"));
    arh2_arg(b, "provider", arh2_get(agent, "provider"));
    arh2_arg(b, "api_mode", arh2_get(agent, "api_mode"));
    json_t *r = arh2_call(ctx, "agent.context_compressor.update_model", b);
    json_free(r);
    json_free(clen);
    if (arh2_exc_pending(ctx)) {
        arh2_restore_switch_snapshot(ctx, agent, snapshot);
        return NULL;
    }
    json_t *c = json_object(); arh2_arg(c, "agent", agent);
    json_t *rr = arh2_call(ctx, "revalidate_compression_feasibility", c); json_free(rr);
    return NULL;
}

/* ── _finish_switch ──────────────────────────────────────────────────────── */
/* PoP: arh2_finish_switch @ agent/agent_runtime_helpers.py:_finish_switch */
static json_t *arh2_finish_switch(arh2_ctx *ctx, json_t *agent, const char *new_provider,
        const char *old_norm, const char *new_norm) {
    json_set(agent, "_fallback_activated", json_bool(false));
    json_set(agent, "_provider_fallback_active", json_bool(false));
    json_set(agent, "_provider_fallback_route", json_null());
    json_set(agent, "_fallback_index", json_number(0));
    json_set(agent, "_credential_pool_revert_id", json_null());
    json_t *chain = json_array();
    json_t *old = arh2_get(agent, "_fallback_chain");
    if (arh2_is_arr(old))
        for (size_t i = 0; i < old->c.count; i++) json_append(chain, json_copy(old->c.items[i]));
    if (old_norm[0] && new_norm[0] && !arh2_str_eq(old_norm, new_norm)) {
        json_t *f = json_array();
        for (size_t i = 0; i < chain->c.count; i++) {
            json_t *e = chain->c.items[i];
            char *p = arh2_lower_dup(arh2_strip(arh2_sdef(e, "provider", "")));
            bool skip = arh2_str_eq(p, old_norm) || arh2_str_eq(p, new_norm);
            free(p);
            if (!skip) json_append(f, json_copy(e));
        }
        json_free(chain); chain = f;
    }
    json_set(agent, "_fallback_chain", chain);
    json_set(agent, "_fallback_model", chain->c.count ? json_copy(chain->c.items[0]) : json_null());
    arh2_apply_switched_provider_request_overrides(ctx, agent, new_provider);
    if (arh2_exc_pending(ctx)) {
        arh2_log(ctx, "", "debug", "switch_model: request_overrides re-derivation failed", json_array(), true);
        arh2_exc_clear(ctx);
    }
    return NULL;
}

/* ── _persist_switch_billing_route ───────────────────────────────────────── */
/* PoP: arh2_persist_switch_billing_route @ agent/agent_runtime_helpers.py:_persist_switch_billing_route */
static json_t *arh2_persist_switch_billing_route(arh2_ctx *ctx, json_t *agent) {
    json_t *db = arh2_get(agent, "_session_db");
    json_t *sid = arh2_get(agent, "session_id");
    /* Python guards with `session_db is None`, NOT truthiness -- an empty {}
     * session_db is a valid store selector and must still fire both seams. */
    if (!db || db->type == JSON_NULL || !arh2_truthy(sid)) return NULL;
    json_t *a = json_object(); arh2_arg(a, "agent", agent);
    json_t *store = arh2_call(ctx, "agent_interface", a);
    /* session_id is passed positionally in Python, but both billing-route
     * seam projections use dict(kwargs) -- never record it as an arg. */
    json_t *b = json_object();
    arh2_arg(b, "provider", arh2_get(agent, "provider"));
    arh2_arg(b, "base_url", arh2_get(agent, "base_url"));
    arh2_arg(b, "billing_mode", arh2_get(agent, "api_mode"));
    json_t *r = arh2_call(ctx, arh2_truthy(store) ? "store.usage.set_billing_route" : "session_db.update_session_billing_route", b);
    json_free(r); json_free(store);
    if (arh2_exc_pending(ctx)) {
        arh2_log(ctx, "", "warning", "Failed to persist billing route after model switch", json_array(), true);
        arh2_exc_clear(ctx);
    }
    return NULL;
}

/* ── _log_empty_non_final_heal ───────────────────────────────────────────── */
/* PoP: arh2_log_empty_non_final_heal @ agent/agent_runtime_helpers.py:_log_empty_non_final_heal */
static json_t *arh2_log_empty_non_final_heal(arh2_ctx *ctx, double healed) {
    json_t *skey = arh2_call(ctx, "_session_id_for_heal_log", json_object());
    const char *key = arh2_truthy(skey) ? arh2_jstr(skey) : "-";
    json_t *th = arh2_call(ctx, "_heal_escalation_threshold", json_object());
    double threshold = arh2_is_num(th) ? th->num_val : 0.0; json_free(th);
    double now = arh2_ndef(ctx->env, "monotonic", 0.0);
    double window = arh2_ndef(ctx->env, "empty_heal_window_s", 600.0);
    json_t *state = json_object();
    json_t *prior = arh2_get(ctx->env, "heal_state");
    if (arh2_is_obj(prior)) {
        json_t *ps = arh2_get(prior, key);
        if (arh2_is_obj(ps)) state = json_copy(ps);
    }
    double started = arh2_ndef(state, "window_start", now);
    bool fresh = !arh2_is_obj(state) || arh2_is_none(arh2_get(state, "window_start")) || (now - started) > window;
    if (fresh) {
        double te = arh2_ndef(state, "total_events", 0.0);
        double th2 = arh2_ndef(state, "total_healed", 0.0);
        json_t *ns = json_object();
        json_set(ns, "count", json_number(0));
        json_set(ns, "window_start", json_number(now));
        json_set(ns, "escalated", json_bool(false));
        json_set(ns, "total_events", json_number(te));
        json_set(ns, "total_healed", json_number(th2));
        json_free(state); state = ns;
    }
    json_t *a = json_object();
    json_set(a, "key", json_string(key));
    json_set(a, "state", json_copy(state));
    json_t *lr = arh2_call(ctx, "publish_lru", a); json_free(lr);
    double count = arh2_ndef(state, "count", 0.0) + 1.0;
    json_set(state, "count", json_number(count));
    double te = arh2_ndef(state, "total_events", 0.0) + 1.0;
    json_set(state, "total_events", json_number(te));
    double total_healed = arh2_ndef(state, "total_healed", 0.0) + healed;
    json_set(state, "total_healed", json_number(total_healed));
    bool escalated = arh2_bdef(state, "escalated", false);
    if (escalated) { json_free(skey); json_free(state); return NULL; }
    bool escalate = threshold > 0.0 && count >= threshold;
    if (escalate) {
        json_set(state, "escalated", json_bool(true));
        bool notified = arh2_bdef(arh2_get(ctx->env, "heal_notified"), key, false);
        if (!notified) {
            json_t *n = json_object();
            json_set(n, "key", json_string(key));
            json_t *r = arh2_call(ctx, "heal_user_notice.add", n); json_free(r);
            json_t *p = json_object();
            json_set(p, "key", json_string(key));
            /* Only the key is projected: the notice VALUE is the message
             * string, so Python's total_events extraction yields None and is
             * dropped from the recorded args.  Do not pass it from C. */
            json_t *r2 = arh2_call(ctx, "heal_pending_notice.set", p); json_free(r2);
        }
    }
    if (escalate) {
        json_t *args = json_array();
        json_append(args, json_string(key));
        json_append(args, json_number(healed));
        json_append(args, json_number(te));
        json_append(args, json_number(total_healed));
        json_append(args, json_number(count));
        json_append(args, json_number(threshold));
        arh2_log(ctx, "ra.", "error",
            "Pre-call sanitizer: repeated-heal escalation for session %s — healed %d empty non-final message(s) this send; heal pattern: %d heal events / %d messages healed this session (%d in the current session window, threshold %d). The transcript is being repaired on every send; /new drops the poisoned turns.",
            args, false);
    } else {
        json_t *args = json_array(); json_append(args, json_number(healed));
        arh2_log(ctx, "ra.", "warning",
            "Pre-call sanitizer: healed %d empty non-final message(s) by substituting placeholder content — an empty-content turn was in the transcript and would 400 the request ('messages must have non-empty content' / INVALID_REQUEST_BODY). Self-recovering the poisoned transcript in memory; no restart needed.",
            args, false);
    }
    json_free(skey);
    json_free(state);
    return NULL;
}

/* ── looks_like_degenerate_final ─────────────────────────────────────────── */
static size_t arh2_py_len(const char *s) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}
static bool arh2_any_alpha(const char *s) {
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++)
        if (isalpha(*p) || *p >= 0x80) return true;
    return false;
}
static bool arh2_any_ascii_alnum(const char *s) {
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++)
        if (isalnum(*p)) return true;
    return false;
}
static bool arh2_any_non_ascii_alpha(const char *s) {
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++)
        if (*p >= 0x80) return true;   /* non-ASCII code unit counted as non-ASCII alpha (ASCII fixture domain) */
    return false;
}
/* PoP: arh2_looks_like_degenerate_final @ agent/agent_runtime_helpers.py:looks_like_degenerate_final */
static json_t *arh2_looks_like_degenerate_final(arh2_ctx *ctx, const char *text, json_t *user_message) {
    char *t = arh2_strip(text);
    size_t len = arh2_py_len(t);
    json_t *terms = arh2_get(ctx->env, "sentence_terminals");
    if (!t[0] || len > (size_t)arh2_ndef(ctx->env, "degenerate_final_max_chars", 24.0) ||
        arh2_ends_any(t, terms)) { free(t); return json_bool(false); }
    const char *punct = arh2_sdef(ctx->env, "degenerate_leading_punct", "?!,;:)]}");
    if (t[0] && strchr(punct, t[0]) && len > 1) {
        /* Python: t[1].isalpha() -- inspect the SECOND codepoint, not the
         * leading punctuation itself (the old loop only skipped UTF-8
         * continuation bytes of t[0] and never advanced to t[1]). */
        const unsigned char *q = (const unsigned char *)(t + 1);
        while ((*q & 0xC0) == 0x80) q++;
        if (*q && (isalpha(*q) || *q >= 0x80)) { free(t); return json_bool(true); }
    }
    if (!arh2_any_alpha(t) || arh2_any_ascii_alnum(t)) { free(t); return json_bool(false); }
    json_t *ut = NULL;
    if (arh2_truthy(user_message)) {
        json_t *a = json_object();
        arh2_arg(a, "message", user_message);
        ut = arh2_call(ctx, "_summarize_user_message_for_log", a);
    } else ut = json_string("");
    bool res = !arh2_any_non_ascii_alpha(arh2_jstr(ut));
    json_free(ut); free(t);
    return json_bool(res);
}

/* ── promoted_reasoning_announces_action ─────────────────────────────────── */
static char *arh2_last_codepoints(const char *s, size_t n) {
    if (!s) return strdup("");
    /* find start byte of the last n codepoints */
    const char *start = s + strlen(s);
    size_t cnt = 0;
    while (start > s && cnt < n) {
        start--;
        while (start > s && (*start & 0xC0) == 0x80) start--;
        cnt++;
    }
    return strdup(start);
}
/* PoP: arh2_promoted_reasoning_announces_action @ agent/agent_runtime_helpers.py:promoted_reasoning_announces_action */
static json_t *arh2_promoted_reasoning_announces_action(arh2_ctx *ctx, const char *text) {
    char *t = arh2_strip(text);
    if (!t[0]) { free(t); return json_bool(false); }
    char *tail = arh2_last_codepoints(t, 240);
    json_t *a = json_object();
    json_set(a, "text", json_string(tail));
    json_t *m = arh2_call(ctx, "promoted_reasoning_plan_tail_re.search", a);
    bool res = arh2_truthy(m); json_free(m);
    free(tail); free(t);
    return json_bool(res);
}

/* ── socket leaves ───────────────────────────────────────────────────────── */
/* PoP: arh2_socket_from_stream @ agent/agent_runtime_helpers.py:_socket_from_stream */
static json_t *arh2_socket_from_stream(arh2_ctx *ctx, json_t *stream) {
    json_t *sock = arh2_get(stream, "_sock");
    if (!arh2_truthy(sock) && arh2_truthy(arh2_get(stream, "get_extra_info_callable"))) {
        json_t *a = json_object(); json_set(a, "name", json_string("socket"));
        sock = arh2_call(ctx, "stream.get_extra_info", a);
        if (arh2_exc_pending(ctx)) { arh2_exc_clear(ctx); sock = NULL; }
    }
    if (!arh2_truthy(sock)) {
        json_t *inner = arh2_get(stream, "stream");
        sock = arh2_get(inner, "_sock");
    }
    if (!arh2_truthy(sock)) {
        json_t *us = arh2_get(stream, "_stream");
        if (arh2_truthy(arh2_get(us, "extra_callable"))) {
            sock = arh2_call(ctx, "stream._stream.extra", json_object());
            if (arh2_exc_pending(ctx)) { arh2_exc_clear(ctx); sock = NULL; }
        }
    }
    return arh2_truthy(sock) ? json_copy(sock) : json_null();
}
/* PoP: arh2_socket_from_candidate @ agent/agent_runtime_helpers.py:_socket_from_candidate */
static json_t *arh2_socket_from_candidate(arh2_ctx *ctx, json_t *candidate) {
    json_t *stream = arh2_get(candidate, "_network_stream");
    if (!arh2_truthy(stream)) stream = arh2_get(candidate, "_stream");
    json_t *sock = NULL;
    if (arh2_truthy(stream)) sock = arh2_socket_from_stream(ctx, stream);
    if (arh2_truthy(sock)) return sock;
    json_free(sock);
    return arh2_socket_from_stream(ctx, candidate);
}
static json_t *arh2_connection_candidates(arh2_ctx *ctx, json_t *start) {
    json_t *a = json_object(); arh2_arg(a, "start", start);
    return arh2_call(ctx, "connection_candidates", a);
}
/* PoP: arh2_socket_from_response @ agent/agent_runtime_helpers.py:_socket_from_response */
static json_t *arh2_socket_from_response(arh2_ctx *ctx, json_t *response) {
    json_t *exts = arh2_get(response, "extensions");
    json_t *direct = arh2_is_obj(exts) ? arh2_get(exts, "network_stream") : NULL;
    json_t *starts[2] = {direct, arh2_get(response, "stream")};
    for (int i = 0; i < 2; i++) {
        /* Python guards with `if start is None` -- an empty {} network_stream
         * is still a valid start and must yield connection candidates. */
        if (!starts[i] || starts[i]->type == JSON_NULL) continue;
        json_t *cands = arh2_connection_candidates(ctx, starts[i]);
        if (arh2_is_arr(cands)) {
            for (size_t j = 0; j < cands->c.count; j++) {
                json_t *sock = arh2_socket_from_candidate(ctx, cands->c.items[j]);
                if (arh2_truthy(sock)) { json_free(cands); return sock; }
                json_free(sock);
            }
        }
        json_free(cands);
    }
    return json_null();
}
/* PoP: arh2_shutdown_socket @ agent/agent_runtime_helpers.py:_shutdown_socket */
static json_t *arh2_shutdown_socket(arh2_ctx *ctx, json_t *sock) {
    if (arh2_truthy(arh2_get(sock, "settimeout_callable"))) {
        json_t *a = json_object(); json_set(a, "timeout", json_number(0));
        json_t *r = arh2_call(ctx, "sock.settimeout", a); json_free(r);
        if (arh2_exc_pending(ctx)) arh2_exc_clear(ctx); /* suppress OSError */
    }
    json_t *b = json_object(); json_set(b, "how", json_number(2));
    json_t *r = arh2_call(ctx, "sock.shutdown", b); json_free(r);
    if (arh2_exc_pending(ctx)) arh2_exc_clear(ctx);
    return NULL;
}
