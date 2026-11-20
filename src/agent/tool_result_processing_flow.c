/*
 * tool_result_processing_flow.c — tool-result-processing FLOW cores + leaves
 * of agent/tool_executor.py (LIVE authority): the observe -> commit -> project
 * pipeline (_commit_tool_result / _publish_sequential_result), skipped /
 * invalid-argument / batch result appends, sequential dispatch resolution and
 * execution, spinner lifecycle, persisted-path stub recording, and the worker
 * activity / registration flows.
 *
 * NOTE ON MODULE NAME: the task brief names "agent/tool_result_processing.py";
 * no such file exists in the quarry (C:/Users/eman5/hermes-agent). The live
 * tool-result-processing code lives in agent/tool_executor.py; PoP lines point
 * at the real file so the coverage scanner credits the real defs. The 24
 * ported names were verified UNCLAIMED by grepping PoP annotations across src/
 * (the claimed siblings — _tc_name, _pairing_tool_call_id, _preview,
 * _safe_callback, _tool_progress_enabled, _parse_tool_call, _budget_for_agent,
 * _run_sequential_tool_execution_middleware, _unfinished_tool_result,
 * _emit_tool_complete_and_risk, _persist_multimodal_text_parts,
 * _delegate_spinner_label, _flush_session_db_after_tool_progress, ... — enter
 * through the universal seam).
 *
 * Decision sequences only: every live sibling / hook / clock / callback effect
 * enters through trp_io_t.call (the universal seam). Python truthiness, `or`
 * short-circuits, `is None` vs truthy tests, kwarg evaluation order, dict
 * insertion order, and exception catch scopes are transcribed per call site:
 * `except Exception` / contextlib.suppress(Exception) swallow Exception
 * subclasses ONLY (full builtin parent table below); KeyboardInterrupt /
 * SystemExit / GeneratorExit / CancelledError propagate past those guards
 * exactly as live. `except KeyboardInterrupt` catches only that class.
 *
 * Value model (shared with tests/oracle_tool_result_processing_flow.py):
 *   - attribute doubles: {"__attr__": {...}}  <-> types.SimpleNamespace
 *   - callable doubles:  {"__callable__": true|"<seam>"}
 *   - facade doubles:    {"__budget__"}, {"__guardrails__"}, {"__subdir__"},
 *                        {"__spinner__"}, {"__event__"}, {"__thread__"},
 *                        {"__lock__"}, {"__set__": true, "items": [...]}
 *   - dataclasses:       {"_type": "_ToolCallRef"|"_ManagedToolResult"|
 *                        "_ParsedCall"|"_SequentialDispatch", ...fields}
 *   - dispatch callables: execute {"kind": "inline"|"delegate"|
 *                        "context_engine"|"memory"|"registry", ...},
 *                        error_result {"kind": ..., "name": ...}
 *   - marker strings:    {"_type": "_ToolTimeoutResult"|"_ToolCancelledResult",
 *                        "value": ...}
 *   - frozenset[str]:    sorted, deduped JSON array
 *   - raise specs:       {"__raise__": "<ClassName>", "msg": str(e)}
 *
 * Fixture domain (documented restrictions, verified by the shadow diff):
 *   - str() of non-str results uses scalar str() (None/bool/int/float/str);
 *     container repr is outside the domain of the str()-using leaves.
 *   - persisted_result += subdir_hints: persisted is str (or None -> the live
 *     TypeError); list += str iterable-extension is outside the domain.
 *   - print()/_wrap_verbose/_preview/_get_cute_tool_message_impl scripts
 *     return str; unpack scripts (_unfinished_tool_result, _commit_tool_result,
 *     _detect_tool_failure, _run_sequential_call) return fixed-length arrays.
 *   - duration f-strings (.1f/.2f) format the exact binary double, matching
 *     CPython's round-half-even on the same value.
 *
 * Self-contained: libjson only (the gate links lib/libjson/json.o). The
 * harness #includes this .c via the area-qualified
 * "agent/tool_result_processing_flow.c".
 *
 * Gate: bash tests/run_one_oracle.sh tool_result_processing_flow \
 *           oracle_tool_result_processing_flow.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── module constants (agent/tool_executor.py) ── */
#define TRP_LARGE_TOOL_RESULT_TRIM_CHARS 1000000.0
#define TRP_TOOL_ACTIVITY_HEARTBEAT_INTERVAL_S 30.0
#define TRP_REASON_USER_INTERRUPT "user interrupt"

/* ── universal seam ── */
typedef struct trp_io {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} trp_io_t;

/* ── JSON / Python-value helpers ─────────────────────────────────────────── */

static bool trp_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool trp_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool trp_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool trp_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool trp_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }

static const char *trp_strval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

/* Python truthiness (NULL pointer == None). */
static bool trp_truth(const json_t *v) {
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

static double trp_num(const json_t *v, double def) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : def;
}

static bool trp_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}

/* Python type name of a fixture value. Integral JSON numbers materialize as
 * int live (json.loads), non-integral as float. */
static const char *trp_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_NULL:   return "NoneType";
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            return (d == (double)(long long)d && fabs(d) < 9.2e18) ? "int" : "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT:
            if (json_obj_get(v, "__attr__")) return "types.SimpleNamespace";
            if (json_obj_get(v, "__spinner__")) return "KawaiiSpinner";
            if (json_obj_get(v, "__event__")) return "threading.Event";
            if (json_obj_get(v, "__thread__")) return "threading.Thread";
            if (json_obj_get(v, "__lock__")) return "threading.Lock";
            if (json_obj_get(v, "__set__")) return "set";
            if (json_obj_get(v, "__guardrails__")) return "ToolCallGuardrailController";
            if (json_obj_get(v, "__subdir__")) return "SubdirectoryHints";
            if (json_obj_get(v, "__budget__")) return "BudgetConfig";
            if (json_obj_get(v, "_type")) return json_get_str(v, "_type", "object");
            return "dict";
    }
    return "object";
}

/* getattr double access: {"__attr__": {...}} is a SimpleNamespace; any other
 * value has no attributes (getattr default applies). */
static const json_t *trp_getattr(const json_t *v, const char *name) {
    if (!trp_is_obj(v)) return NULL;
    const json_t *inner = json_obj_get(v, "__attr__");
    if (!inner || inner->type != JSON_OBJECT) return NULL;
    return json_obj_get(inner, name);
}

/* ── raise specs ── */

static json_t *trp_raise(const char *cls, const char *msg) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(cls));
    json_set(e, "msg", json_string(msg ? msg : ""));
    return e;
}

static json_t *trp_raise_fmt(const char *cls, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return trp_raise(cls, buf);
}

/* AttributeError for a required attribute read (live `obj.name`). */
static json_t *trp_attr_error(const json_t *v, const char *name) {
    return trp_raise_fmt("AttributeError", "'%s' object has no attribute '%s'",
                         trp_type_name(v), name);
}

/* setattr on an attribute double: live `obj.name = v` writes INSIDE the
 * {"__attr__": {...}} wrapper; any other carrier raises AttributeError.
 * Takes ownership of val. */
static void trp_setattr(json_t *v, const char *name, json_t *val, json_t **exc) {
    json_t *inner = trp_is_obj(v) ? json_obj_get(v, "__attr__") : NULL;
    if (!inner || inner->type != JSON_OBJECT) {
        json_free(val);
        *exc = trp_attr_error(v, name);
        return;
    }
    json_set(inner, name, val);
}

/* Required attribute read: returns the borrowed value or NULL with *exc set.
 * A present JSON null is a real value (returns the node, *exc stays NULL). */
static const json_t *trp_attr_req(const json_t *v, const char *name, json_t **exc) {
    const json_t *r = trp_getattr(v, name);
    if (!r) *exc = trp_attr_error(v, name);
    return r;
}

/* ── FULL builtin exception hierarchy (parent table) ──
 * `except Exception` catches every class whose chain reaches Exception;
 * SystemExit / KeyboardInterrupt / GeneratorExit / CancelledError reach
 * BaseException ONLY and propagate. Unknown scripted classes are synthetic
 * Exception subclasses live (the oracle builds type(cls, (Exception,), {})),
 * so their default parent is Exception. */
static const char *trp_exc_parent(const char *cls) {
    static const struct { const char *c, *p; } tbl[] = {
        {"Exception", "BaseException"},
        {"SystemExit", "BaseException"},
        {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"},
        {"CancelledError", "BaseException"},
        {"BaseExceptionGroup", "BaseException"},
        {"RuntimeError", "Exception"},
        {"ValueError", "Exception"},
        {"TypeError", "Exception"},
        {"AttributeError", "Exception"},
        {"NameError", "Exception"},
        {"LookupError", "Exception"},
        {"ArithmeticError", "Exception"},
        {"OSError", "Exception"},
        {"StopIteration", "Exception"},
        {"StopAsyncIteration", "Exception"},
        {"AssertionError", "Exception"},
        {"SyntaxError", "Exception"},
        {"SystemError", "Exception"},
        {"ImportError", "Exception"},
        {"Warning", "Exception"},
        {"EOFError", "Exception"},
        {"MemoryError", "Exception"},
        {"ReferenceError", "Exception"},
        {"BufferError", "Exception"},
        {"UnicodeError", "Exception"},
        {"ExceptionGroup", "Exception"},
        {"RecursionError", "RuntimeError"},
        {"NotImplementedError", "RuntimeError"},
        {"IndentationError", "SyntaxError"},
        {"TabError", "IndentationError"},
        {"UnicodeDecodeError", "UnicodeError"},
        {"UnicodeEncodeError", "UnicodeError"},
        {"UnicodeTranslateError", "UnicodeError"},
        {"ModuleNotFoundError", "ImportError"},
        {"IndexError", "LookupError"},
        {"KeyError", "LookupError"},
        {"UnboundLocalError", "NameError"},
        {"FloatingPointError", "ArithmeticError"},
        {"OverflowError", "ArithmeticError"},
        {"ZeroDivisionError", "ArithmeticError"},
        {"FileNotFoundError", "OSError"},
        {"FileExistsError", "OSError"},
        {"PermissionError", "OSError"},
        {"IsADirectoryError", "OSError"},
        {"NotADirectoryError", "OSError"},
        {"InterruptedError", "OSError"},
        {"TimeoutError", "OSError"},
        {"ConnectionError", "OSError"},
        {"BrokenPipeError", "ConnectionError"},
        {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"},
        {"ConnectionResetError", "ConnectionError"},
        {"ProcessLookupError", "OSError"},
        {"BlockingIOError", "OSError"},
        {"ChildProcessError", "OSError"},
        {"WindowsError", "OSError"},
        {"DeprecationWarning", "Warning"},
        {"PendingDeprecationWarning", "Warning"},
        {"RuntimeWarning", "Warning"},
        {"SyntaxWarning", "Warning"},
        {"UserWarning", "Warning"},
        {"FutureWarning", "Warning"},
        {"ImportWarning", "Warning"},
        {"UnicodeWarning", "Warning"},
        {"BytesWarning", "Warning"},
        {"ResourceWarning", "Warning"},
        {"EncodingWarning", "Warning"},
    };
    if (!cls) return NULL;
    for (size_t i = 0; i < sizeof(tbl) / sizeof(tbl[0]); i++)
        if (trp_str_eq(cls, tbl[i].c)) return tbl[i].p;
    return "Exception";   /* synthetic scripted class: Exception subclass live */
}

/* cls is-a target (walks the parent chain; identity included). */
static bool trp_exc_is_a(const char *cls, const char *target) {
    for (int depth = 0; cls && depth < 16; depth++) {
        if (trp_str_eq(cls, target)) return true;
        if (trp_str_eq(cls, "BaseException")) return false;
        cls = trp_exc_parent(cls);
    }
    return false;
}

static bool trp_exc_is_exception(const char *cls) {
    return trp_exc_is_a(cls, "Exception");
}

/* Universal seam call: frees args, returns the NEW scripted result (NULL ==
 * None) or transfers a raise-spec into *exc. */
static json_t *trp_call(const trp_io_t *io, const char *name, json_t *args,
                        json_t **exc) {
    json_t *r = io->call(io->ctx, name, args);
    json_free(args);
    if (r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__")) {
        *exc = r;
        return NULL;
    }
    return r;
}

/* Swallow a caught exception (live `except Exception` / suppress(Exception)).
 * BaseException-only classes propagate: returns false and leaves *exc set. */
static bool trp_swallow(json_t **exc) {
    if (!*exc) return true;
    if (!trp_exc_is_exception(json_get_str(*exc, "__raise__", ""))) return false;
    json_free(*exc); *exc = NULL;
    return true;
}

/* ── args builders (None-valued keys dropped on both sides) ── */

static void trp_arg_s(json_t *a, const char *k, const char *v) {
    if (v) json_set(a, k, json_string(v));
}
static void trp_arg_j(json_t *a, const char *k, const json_t *v) {
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void trp_arg_b(json_t *a, const char *k, bool v) {
    json_set(a, k, json_bool(v));
}
static void trp_arg_n(json_t *a, const char *k, double v) {
    json_set(a, k, json_number(v));
}
/* Callable projection: a callable double becomes "<callable>"; other non-None
 * values pass through; None drops the key. */
static void trp_arg_cb(json_t *a, const char *k, const json_t *v) {
    if (trp_is_null(v)) return;
    if (trp_is_obj(v) && json_obj_get(v, "__callable__"))
        json_set(a, k, json_string("<callable>"));
    else
        json_set(a, k, json_copy(v));
}

/* ── scalar str() / float repr ── */

static void trp_float_repr(double v, char *out, size_t cap) {
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(out, cap, "%.*g", prec, v);
        if (strtod(out, NULL) == v) return;
    }
    snprintf(out, cap, "%.17g", v);
}

/* Python str() of a scalar; containers fall back to JSON serialization
 * (outside the fixture domain of the str()-using leaves). malloc'd. */
static char *trp_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && fabs(d) < 9.2e18)
                snprintf(buf, sizeof(buf), "%lld", (long long)d);
            else
                trp_float_repr(d, buf, sizeof(buf));
            return strdup(buf);
        }
        default: {
            char *s = json_serialize(v);
            return s ? s : strdup("");
        }
    }
}

/* ── UTF-8 codepoint helpers (Python str len/slice domain) ── */

static size_t trp_cp_len(const char *s) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}

/* malloc'd prefix of n codepoints (n clamped to the string length). */
static char *trp_cp_prefix(const char *s, size_t n) {
    const unsigned char *p = (const unsigned char *)s;
    size_t count = 0;
    while (*p && count < n) {
        p++;
        while ((*p & 0xC0) == 0x80) p++;
        count++;
    }
    size_t bytes = (size_t)(p - (const unsigned char *)s);
    char *out = (char *)malloc(bytes + 1);
    if (!out) return NULL;
    memcpy(out, s, bytes);
    out[bytes] = '\0';
    return out;
}

/* len() with the live TypeError for non-sized carriers. */
static long long trp_len_value(const json_t *v, json_t **exc) {
    if (trp_is_str(v)) return (long long)trp_cp_len(v->str_val ? v->str_val : "");
    if (trp_is_arr(v) || trp_is_obj(v)) return (long long)v->c.count;
    *exc = trp_raise_fmt("TypeError", "object of type '%s' has no len()",
                         trp_type_name(v));
    return 0;
}

/* str.replace(old, new) — every literal occurrence. malloc'd. */
static char *trp_str_replace(const char *s, const char *old, const char *news) {
    if (!s) s = "";
    if (!old || !old[0]) return strdup(s);
    size_t olen = strlen(old), nlen = strlen(news ? news : "");
    size_t cap = strlen(s) + 1, n = 0;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    const char *p = s;
    while (*p) {
        if (strncmp(p, old, olen) == 0) {
            while (n + nlen + 1 > cap) { cap *= 2; out = (char *)realloc(out, cap); }
            memcpy(out + n, news ? news : "", nlen);
            n += nlen;
            p += olen;
        } else {
            if (n + 2 > cap) { cap *= 2; out = (char *)realloc(out, cap); }
            out[n++] = *p++;
        }
    }
    out[n] = '\0';
    return out;
}

/* ── Python json.dumps(ensure_ascii=False, default separators) ── */

typedef struct { char *p; size_t n, cap; } trp_buf_t;
static void trp_buf_init(trp_buf_t *b) { b->p = NULL; b->n = 0; b->cap = 0; }
static void trp_buf_add(trp_buf_t *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 64;
        while (nc < b->n + n + 1) nc *= 2;
        char *np = (char *)realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}
static void trp_buf_adds(trp_buf_t *b, const char *s) { trp_buf_add(b, s, strlen(s)); }

static void trp_dumps_str(trp_buf_t *b, const char *s) {
    trp_buf_adds(b, "\"");
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        unsigned char c = *p;
        switch (c) {
            case '"':  trp_buf_adds(b, "\\\""); break;
            case '\\': trp_buf_adds(b, "\\\\"); break;
            case '\b': trp_buf_adds(b, "\\b"); break;
            case '\f': trp_buf_adds(b, "\\f"); break;
            case '\n': trp_buf_adds(b, "\\n"); break;
            case '\r': trp_buf_adds(b, "\\r"); break;
            case '\t': trp_buf_adds(b, "\\t"); break;
            default:
                if (c < 0x20) {
                    char esc[8];
                    snprintf(esc, sizeof(esc), "\\u%04x", c);
                    trp_buf_adds(b, esc);
                } else {
                    trp_buf_add(b, (const char *)&c, 1);
                }
        }
    }
    trp_buf_adds(b, "\"");
}

static void trp_dumps_val(trp_buf_t *b, const json_t *v) {
    if (!v || v->type == JSON_NULL) { trp_buf_adds(b, "null"); return; }
    switch (v->type) {
        case JSON_BOOL: trp_buf_adds(b, v->bool_val ? "true" : "false"); break;
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && fabs(d) < 9.2e18)
                snprintf(buf, sizeof(buf), "%lld", (long long)d);
            else
                trp_float_repr(d, buf, sizeof(buf));
            trp_buf_adds(b, buf);
            break;
        }
        case JSON_STRING: trp_dumps_str(b, v->str_val); break;
        case JSON_ARRAY:
            trp_buf_adds(b, "[");
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) trp_buf_adds(b, ", ");
                trp_dumps_val(b, v->c.items[i]);
            }
            trp_buf_adds(b, "]");
            break;
        case JSON_OBJECT:
            trp_buf_adds(b, "{");
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) trp_buf_adds(b, ", ");
                trp_dumps_str(b, v->c.keys[i]);
                trp_buf_adds(b, ": ");
                trp_dumps_val(b, v->c.items[i]);
            }
            trp_buf_adds(b, "}");
            break;
        default: trp_buf_adds(b, "null");
    }
}

static char *trp_json_dumps(const json_t *v) {
    trp_buf_t b; trp_buf_init(&b);
    trp_dumps_val(&b, v);
    return b.p ? b.p : strdup("null");
}

/* frozenset[str] normalization: sorted, deduped array (new). */
static json_t *trp_frozenset_norm(const json_t *v) {
    json_t *out = json_array();
    if (!trp_is_arr(v)) return out;
    for (size_t i = 0; i < v->c.count; i++) {
        char *s = trp_pystr(v->c.items[i]);
        bool dup = false;
        for (size_t j = 0; j < out->c.count; j++) {
            if (trp_str_eq(out->c.items[j]->str_val, s)) { dup = true; break; }
        }
        if (!dup) json_append(out, json_string(s ? s : ""));
        free(s);
    }
    /* insertion sort by strcmp (ASCII fixture domain) */
    for (size_t i = 1; i < out->c.count; i++) {
        json_t *key = out->c.items[i];
        size_t j = i;
        while (j > 0 && strcmp(out->c.items[j - 1]->str_val, key->str_val) > 0) {
            out->c.items[j] = out->c.items[j - 1];
            j--;
        }
        out->c.items[j] = key;
    }
    return out;
}

/* dict.get with the live AttributeError for non-dict carriers. Borrowed result
 * or NULL (absent); *exc set for a non-dict carrier. */
static const json_t *trp_dict_get(const json_t *v, const char *key, json_t **exc) {
    if (trp_is_obj(v) && !json_obj_get(v, "__attr__")) return json_obj_get(v, key);
    *exc = trp_raise_fmt("AttributeError", "'%s' object has no attribute 'get'",
                         trp_type_name(v));
    return NULL;
}

/* messages state array (borrowed; created empty when absent). */
static json_t *trp_messages(json_t *state) {
    json_t *m = json_obj_get(state, "messages");
    if (!m || m->type != JSON_ARRAY) {
        m = json_array();
        json_set(state, "messages", m);
    }
    return m;
}

/* Agent method call through a callable-double attribute: the fixture's
 * {"__callable__": "<seam>"} names the seam; an inert {"__callable__": true}
 * raises AssertionError when invoked; a non-callable raises TypeError. */
static json_t *trp_method_call(const trp_io_t *io, const json_t *agent,
                               const char *method, json_t *args, json_t **exc) {
    const json_t *m = trp_attr_req(agent, method, exc);
    if (*exc) { json_free(args); return NULL; }
    if (trp_is_obj(m) && json_obj_get(m, "__callable__")) {
        const json_t *cb = json_obj_get(m, "__callable__");
        if (cb->type == JSON_STRING)
            return trp_call(io, cb->str_val, args, exc);
        json_free(args);
        *exc = trp_raise("AssertionError", "inert callable double invoked");
        return NULL;
    }
    json_free(args);
    *exc = trp_raise_fmt("TypeError", "'%s' object is not callable",
                         trp_type_name(m));
    return NULL;
}

/* logger.<level>(fmt, *args) seam: {"fmt": ..., "args": [norm...]}. */
static json_t *trp_log_call(const trp_io_t *io, const char *level,
                            const char *fmt, json_t *args_list, json_t **exc) {
    json_t *a = json_object();
    trp_arg_s(a, "fmt", fmt);
    json_set(a, "args", args_list);   /* ownership transfer */
    char name[32];
    snprintf(name, sizeof(name), "logger.%s", level);
    return trp_call(io, name, a, exc);
}

/* logging.debug(fmt, *args) seam (module-level logging). */
static json_t *trp_logging_debug(const trp_io_t *io, const char *fmt,
                                 json_t *args_list, json_t **exc) {
    json_t *a = json_object();
    trp_arg_s(a, "fmt", fmt);
    json_set(a, "args", args_list);
    return trp_call(io, "logging.debug", a, exc);
}

/* norm of an exception value for logger args: {"__raise__": cls, "msg": msg}. */
static json_t *trp_exc_norm(const json_t *exc) {
    json_t *e = json_object();
    trp_arg_s(e, "__raise__", json_get_str(exc, "__raise__", "Exception"));
    trp_arg_s(e, "msg", json_get_str(exc, "msg", ""));
    return e;
}

/* Unpack a scripted fixed-length array result (live tuple unpack). */
static bool trp_unpack(const json_t *r, size_t n, json_t **exc) {
    if (!trp_is_arr(r)) {
        *exc = trp_raise_fmt("TypeError", "cannot unpack non-iterable %s object",
                             trp_type_name(r));
        return false;
    }
    if (r->c.count < n) {
        *exc = trp_raise_fmt("ValueError",
                             "not enough values to unpack (expected %zu, got %zu)",
                             n, r->c.count);
        return false;
    }
    if (r->c.count > n) {
        *exc = trp_raise_fmt("ValueError",
                             "too many values to unpack (expected %zu)", n);
        return false;
    }
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Ported flows. Signature: (in, io, state, exc) -> NEW ret or NULL.
 * state carries the mutable carriers (agent / messages / ref); the harness
 * echoes it after the call.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* PoP: trp_record_persisted_path_for_stub @ agent/tool_executor.py:_record_persisted_path_for_stub */
json_t *trp_record_persisted_path_for_stub(const json_t *in, const trp_io_t *io,
                                           json_t *state, json_t **exc) {
    const json_t *agent = json_obj_get(state, "agent");
    const char *tool_call_id = json_get_str(in, "tool_call_id", "");
    const json_t *fr = json_obj_get(in, "function_result");
    /* try: candidates = ... ; path = next(...); if path: record */
    json_t *candidates = json_array();
    if (trp_is_str(fr)) {
        json_append(candidates, json_copy(fr));
    } else {
        json_t *a = json_object();
        trp_arg_j(a, "result", fr);
        json_t *mm = trp_call(io, "_is_multimodal_tool_result", a, exc);
        if (*exc) goto caught;
        bool is_mm = trp_truth(mm);
        json_free(mm);
        if (is_mm) {
            const json_t *ts = trp_dict_get(fr, "text_summary", exc);
            if (*exc) goto caught;
            json_append(candidates, ts ? json_copy(ts) : json_null());
            const json_t *content = trp_dict_get(fr, "content", exc);
            if (*exc) goto caught;
            /* `or []`: falsy content -> empty list */
            if (trp_truth(content) && trp_is_arr(content)) {
                for (size_t i = 0; i < content->c.count; i++) {
                    const json_t *p = content->c.items[i];
                    if (trp_is_obj(p) && !json_obj_get(p, "__attr__")) {
                        const json_t *t = json_obj_get(p, "text");
                        json_append(candidates, t ? json_copy(t) : json_null());
                    }
                }
            }
        }
    }
    /* next((p for p in map(extract_persisted_path, candidates) if p), None) */
    {
        json_t *path = NULL;
        for (size_t i = 0; i < candidates->c.count; i++) {
            json_t *a = json_object();
            trp_arg_j(a, "candidate", candidates->c.items[i]);
            json_t *p = trp_call(io, "extract_persisted_path", a, exc);
            if (*exc) { json_free(path); goto caught; }
            if (trp_truth(p)) { path = p; break; }
            json_free(p);
        }
        if (trp_truth(path)) {
            const json_t *gr = trp_attr_req(agent, "_tool_guardrails", exc);
            if (*exc) { json_free(path); goto caught; }
            if (trp_is_obj(gr) && json_obj_get(gr, "__guardrails__")) {
                json_t *a = json_object();
                trp_arg_s(a, "tool_call_id", tool_call_id);
                trp_arg_j(a, "path", path);
                json_free(path);
                json_t *r = trp_call(io, "guardrails.record_persisted_result", a, exc);
                if (*exc) goto caught;
                json_free(r);
            } else {
                /* live reads .record_persisted_result on the carrier first */
                json_free(path);
                *exc = trp_attr_error(gr, "record_persisted_result");
                goto caught;
            }
        } else {
            json_free(path);
        }
    }
    json_free(candidates);
    return NULL;
caught:
    json_free(candidates);
    /* except Exception as exc: logger.debug(...) — BaseException propagates */
    {
        json_t *spec = *exc ? json_copy(*exc) : NULL;
        if (!trp_swallow(exc)) { json_free(spec); return NULL; }
        json_t *al = json_array();
        json_append(al, trp_exc_norm(spec));
        json_free(spec);
        json_t *lr = trp_log_call(io, "debug",
                                  "persisted-path record for result stub failed: %s",
                                  al, exc);
        if (*exc) return NULL;
        json_free(lr);
    }
    return NULL;
}

/* PoP: trp_canonical_tool_name @ agent/tool_executor.py:_canonical_tool_name */
json_t *trp_canonical_tool_name(const json_t *in, const trp_io_t *io,
                                json_t *state, json_t **exc) {
    (void)state;
    const char *function_name = json_get_str(in, "function_name", "");
    /* from model_tools import _LEGACY_TOOL_ALIASES as _lta */
    json_t *r = trp_call(io, "import_legacy_tool_aliases", json_object(), exc);
    if (*exc) return NULL;
    const json_t *lta = r;
    json_t *empty = NULL;
    if (trp_is_null(lta)) { empty = json_object(); lta = empty; }
    /* return _lta.get(function_name, function_name) */
    const json_t *v = trp_dict_get(lta, function_name, exc);
    json_t *out;
    if (*exc) out = NULL;
    else out = v ? json_copy(v) : json_string(function_name);
    json_free(r);
    json_free(empty);
    return out;
}

/* PoP: trp_append_skipped_tool_results @ agent/tool_executor.py:_append_skipped_tool_results */
json_t *trp_append_skipped_tool_results(const json_t *in, const trp_io_t *io,
                                        json_t *state, json_t **exc) {
    const json_t *agent = json_obj_get(state, "agent");
    json_t *messages = trp_messages(state);
    const json_t *tool_calls = json_obj_get(in, "tool_calls");
    const char *task_id = json_get_str(in, "effective_task_id", "");
    const char *content = json_get_str(in, "content", "");
    const json_t *hook_error_type = json_obj_get(in, "hook_error_type");
    const json_t *hook_id = json_obj_get(in, "hook_id");
    const json_t *flush_stage = json_obj_get(in, "flush_stage");
    bool stop_on_flush_failure = json_get_bool(in, "stop_on_flush_failure", true);
    if (!trp_is_arr(tool_calls)) {
        *exc = trp_raise_fmt("TypeError", "'%s' object is not iterable",
                             trp_type_name(tool_calls));
        return NULL;
    }
    for (size_t i = 0; i < tool_calls->c.count; i++) {
        const json_t *tc = tool_calls->c.items[i];
        /* name = _tc_name(tc) */
        json_t *a = json_object();
        trp_arg_j(a, "tool_call", tc);
        json_t *name_r = trp_call(io, "_tc_name", a, exc);
        if (*exc) return NULL;
        if (!trp_is_str(name_r)) {
            /* live content.replace("{name}", name): arg 2 must be str.
             * CPython's clinic converter names None "None" (not NoneType). */
            const char *tn = trp_type_name(name_r);
            *exc = trp_raise_fmt("TypeError",
                                 "replace() argument 2 must be str, not %s",
                                 trp_str_eq(tn, "NoneType") ? "None" : tn);
            json_free(name_r);
            return NULL;
        }
        const char *name = trp_strval(name_r);
        if (!name) name = "";
        /* result = content.replace("{name}", name) */
        char *result = trp_str_replace(content, "{name}", name);
        /* messages.append(make_tool_result_message(name, result,
         *     _pairing_tool_call_id(tc), effect_disposition="none")) */
        a = json_object();
        trp_arg_j(a, "tool_call", tc);
        json_t *pair = trp_call(io, "_pairing_tool_call_id", a, exc);
        if (*exc) { free(result); json_free(name_r); return NULL; }
        a = json_object();
        trp_arg_s(a, "name", name);
        trp_arg_s(a, "result", result);
        trp_arg_j(a, "tool_call_id", pair);
        trp_arg_s(a, "effect_disposition", "none");
        json_t *msg = trp_call(io, "make_tool_result_message", a, exc);
        json_free(pair);
        if (*exc) { free(result); json_free(name_r); return NULL; }
        json_append(messages, msg ? msg : json_null());
        /* if hook_error_type is not None: _ToolCallRef(...).emit_post(...) */
        if (!trp_is_null(hook_error_type)) {
            /* (hook_id or _pairing_tool_call_id)(tc) */
            json_t *cid = NULL;
            if (trp_truth(hook_id)) {
                a = json_object();
                trp_arg_j(a, "tool_call", tc);
                if (trp_is_obj(hook_id) && json_obj_get(hook_id, "__callable__")) {
                    const json_t *cb = json_obj_get(hook_id, "__callable__");
                    if (cb->type == JSON_STRING)
                        cid = trp_call(io, cb->str_val, a, exc);
                    else {
                        json_free(a);
                        *exc = trp_raise("AssertionError", "inert callable double invoked");
                    }
                } else {
                    json_free(a);
                    *exc = trp_raise_fmt("TypeError", "'%s' object is not callable",
                                         trp_type_name(hook_id));
                }
            } else {
                a = json_object();
                trp_arg_j(a, "tool_call", tc);
                cid = trp_call(io, "_pairing_tool_call_id", a, exc);
            }
            if (*exc) { free(result); json_free(name_r); return NULL; }
            /* error_message f-string: interrupt_skip_wording(agent) */
            a = json_object();
            trp_arg_j(a, "agent", agent);
            json_t *wording = trp_call(io, "interrupt_skip_wording", a, exc);
            if (*exc) { json_free(cid); free(result); json_free(name_r); return NULL; }
            char *wstr = trp_pystr(wording);
            char emsg[512];
            snprintf(emsg, sizeof(emsg), "Tool execution skipped. %s", wstr);
            free(wstr);
            /* emit_post(agent, result, status="cancelled",
             *     error_type=hook_error_type, error_message=emsg) */
            a = json_object();
            trp_arg_s(a, "result", result);
            trp_arg_s(a, "status", "cancelled");
            trp_arg_j(a, "error_type", hook_error_type);
            trp_arg_s(a, "error_message", emsg);
            json_t *er = trp_call(io, "emit_post", a, exc);
            json_free(wording);
            json_free(cid);
            if (*exc) { free(result); json_free(name_r); return NULL; }
            json_free(er);
        }
        /* if flush_stage is not None: flush; stop on first failure */
        if (!trp_is_null(flush_stage)) {
            char stage[256];
            snprintf(stage, sizeof(stage), "%s %s", trp_strval(flush_stage), name);
            a = json_object();
            trp_arg_j(a, "messages", messages);
            trp_arg_s(a, "stage", stage);
            json_t *flushed = trp_call(io, "_flush_session_db_after_tool_progress", a, exc);
            if (*exc) { free(result); json_free(name_r); return NULL; }
            bool ok = trp_truth(flushed);
            json_free(flushed);
            if (!ok && stop_on_flush_failure) {
                free(result); json_free(name_r);
                return json_bool(false);
            }
        }
        free(result);
        json_free(name_r);
    }
    return json_bool(true);
}

/* Apply an optional callable parameter (error_preview / verbose_text):
 * null -> identity copy; callable double -> its seam; other -> TypeError. */
static json_t *trp_apply_cb_param(const trp_io_t *io, const json_t *cb,
                                  const json_t *arg, json_t **exc) {
    if (trp_is_null(cb)) return json_copy(arg);
    if (trp_is_obj(cb) && json_obj_get(cb, "__callable__")) {
        const json_t *c = json_obj_get(cb, "__callable__");
        json_t *a = json_object();
        trp_arg_j(a, "result", arg);
        if (c->type == JSON_STRING) return trp_call(io, c->str_val, a, exc);
        json_free(a);
        *exc = trp_raise("AssertionError", "inert callable double invoked");
        return NULL;
    }
    *exc = trp_raise_fmt("TypeError", "'%s' object is not callable",
                         trp_type_name(cb));
    return NULL;
}

/* PoP: trp_commit_tool_result @ agent/tool_executor.py:_commit_tool_result */
json_t *trp_commit_tool_result(const json_t *in, const trp_io_t *io,
                               json_t *state, json_t **exc) {
    json_t *agent = json_obj_get(state, "agent");
    json_t *messages = trp_messages(state);
    const json_t *ref = json_obj_get(in, "ref");
    const char *function_name = json_get_str(ref, "name", "");
    const json_t *function_args = json_obj_get(ref, "args");
    const char *tool_call_id = json_get_str(ref, "call_id", "");
    const char *effective_task_id = json_get_str(ref, "task_id", "");
    double tool_duration = json_get_num(in, "tool_duration", 0.0);
    bool is_error = json_get_bool(in, "is_error", false);
    bool blocked = json_get_bool(in, "blocked", false);
    const json_t *effect_disposition = json_obj_get(in, "effect_disposition");
    bool observed = json_get_bool(in, "observed", false);
    const json_t *data_use = json_obj_get(in, "data_use");
    const json_t *error_preview = json_obj_get(in, "error_preview");
    const json_t *success_log_chars = json_obj_get(in, "success_log_chars");
    const json_t *verbose_text = json_obj_get(in, "verbose_text");
    const json_t *budget = json_obj_get(in, "budget");

    json_t *function_result = json_copy(json_obj_get(in, "function_result"));
    json_t *persisted = NULL;
    json_t *tool_message = NULL;
    json_t *subdir_hints = NULL;
    json_t *ret = NULL;
    json_t *a, *r;

    /* from hermes_cli.observability.shared_metrics_harness import observe_tool_outcome */
    r = trp_call(io, "import_observe_tool_outcome", json_object(), exc);
    if (*exc) goto done;
    json_free(r);
    /* observe_tool_outcome(agent, function_name, is_error) */
    a = json_object();
    trp_arg_s(a, "function_name", function_name);
    trp_arg_b(a, "is_error", is_error);
    r = trp_call(io, "observe_tool_outcome", a, exc);
    if (*exc) goto done;
    json_free(r);
    /* if not (blocked or is_error): observe_onboarding_result(...) */
    if (!(blocked || is_error)) {
        a = json_object();
        trp_arg_s(a, "function_name", function_name);
        trp_arg_j(a, "function_args", function_args);
        trp_arg_j(a, "result", function_result);
        r = trp_call(io, "observe_onboarding_result", a, exc);
        if (*exc) goto done;
        json_free(function_result);
        function_result = r;
    }
    if (observed) {
        if (!blocked) {
            a = json_object();
            trp_arg_s(a, "function_name", function_name);
            trp_arg_j(a, "function_args", function_args);
            trp_arg_j(a, "result", function_result);
            trp_arg_b(a, "failed", is_error);
            trp_arg_s(a, "tool_call_id", tool_call_id);
            r = trp_method_call(io, agent, "_append_guardrail_observation", a, exc);
            if (*exc) goto done;
            json_free(function_result);
            function_result = r;
        }
        if (is_error) {
            json_t *ep = trp_apply_cb_param(io, error_preview, function_result, exc);
            if (*exc) goto done;
            json_t *al = json_array();
            json_append(al, json_string(function_name));
            json_append(al, json_number(tool_duration));
            json_append(al, ep ? ep : json_null());
            r = trp_log_call(io, "warning", "Tool %s returned error (%.2fs): %s", al, exc);
            if (*exc) goto done;
            json_free(r);
        } else if (!trp_is_null(success_log_chars)) {
            json_t *al = json_array();
            json_append(al, json_string(function_name));
            json_append(al, json_number(tool_duration));
            json_append(al, json_copy(success_log_chars));
            r = trp_log_call(io, "info", "tool %s completed (%.2fs, %d chars)", al, exc);
            if (*exc) goto done;
            json_free(r);
        }
        if (!blocked) {
            /* try: _record_file_mutation_result; except Exception: logging.debug */
            a = json_object();
            trp_arg_s(a, "function_name", function_name);
            trp_arg_j(a, "function_args", function_args);
            trp_arg_j(a, "result", function_result);
            trp_arg_b(a, "is_error", is_error);
            trp_arg_s(a, "task_id", effective_task_id);
            r = trp_method_call(io, agent, "_record_file_mutation_result", a, exc);
            if (*exc) {
                json_t *spec = json_copy(*exc);
                if (!trp_swallow(exc)) { json_free(spec); goto done; }
                json_t *al = json_array();
                json_append(al, trp_exc_norm(spec));
                json_free(spec);
                json_t *lr = trp_logging_debug(io, "file-mutation verifier record failed: %s", al, exc);
                if (*exc) goto done;
                json_free(lr);
            } else {
                json_free(r);
            }
        }
        const json_t *vl = trp_attr_req(agent, "verbose_logging", exc);
        if (*exc) goto done;
        if (trp_truth(vl)) {
            json_t *al = json_array();
            json_append(al, json_string(function_name));
            json_append(al, json_number(tool_duration));
            r = trp_logging_debug(io, "Tool %s completed in %.2fs", al, exc);
            if (*exc) goto done;
            json_free(r);
            json_t *log_result = trp_apply_cb_param(io, verbose_text, function_result, exc);
            if (*exc) goto done;
            long long ll = trp_len_value(log_result, exc);
            if (*exc) { json_free(log_result); goto done; }
            al = json_array();
            json_append(al, json_number((double)ll));
            json_append(al, log_result ? log_result : json_null());
            r = trp_logging_debug(io, "Tool result (%d chars): %s", al, exc);
            if (*exc) goto done;
            json_free(r);
        }
    }
    /* agent._current_tool = None */
    trp_setattr(agent, "_current_tool", json_null(), exc);
    if (*exc) goto done;
    /* agent._touch_activity(f"tool completed: {name} ({dur:.1f}s){suffix}") */
    {
        char label[512];
        snprintf(label, sizeof(label), "tool completed: %s (%.1fs)%s",
                 function_name, tool_duration, is_error ? " (error)" : "");
        a = json_object();
        trp_arg_s(a, "label", label);
        r = trp_method_call(io, agent, "_touch_activity", a, exc);
        if (*exc) goto done;
        json_free(r);
    }
    persisted = json_copy(function_result);
    a = json_object();
    trp_arg_j(a, "result", persisted);
    json_t *mm = trp_call(io, "_is_multimodal_tool_result", a, exc);
    if (*exc) goto done;
    bool is_mm = trp_truth(mm);
    json_free(mm);
    if (is_mm) {
        /* 4th positional arg get_active_env(effective_task_id) evaluates first */
        a = json_object();
        trp_arg_s(a, "task_id", effective_task_id);
        json_t *env = trp_call(io, "get_active_env", a, exc);
        if (*exc) goto done;
        a = json_object();
        trp_arg_j(a, "result", persisted);
        trp_arg_s(a, "tool_name", function_name);
        trp_arg_s(a, "tool_call_id", tool_call_id);
        trp_arg_j(a, "env", env);
        trp_arg_j(a, "config", budget);
        json_free(env);
        r = trp_call(io, "_persist_multimodal_text_parts", a, exc);
        if (*exc) goto done;
        json_free(persisted);
        persisted = r;
    } else {
        /* kwargs left-to-right: content, tool_name, tool_use_id, env=..., config */
        a = json_object();
        trp_arg_s(a, "task_id", effective_task_id);
        json_t *env = trp_call(io, "get_active_env", a, exc);
        if (*exc) goto done;
        a = json_object();
        trp_arg_j(a, "content", persisted);
        trp_arg_s(a, "tool_name", function_name);
        trp_arg_s(a, "tool_use_id", tool_call_id);
        trp_arg_j(a, "env", env);
        trp_arg_j(a, "config", budget);
        json_free(env);
        r = trp_call(io, "maybe_persist_tool_result", a, exc);
        if (*exc) goto done;
        json_free(persisted);
        persisted = r;
    }
    /* _record_persisted_path_for_stub(agent, tool_call_id, persisted_result) */
    a = json_object();
    trp_arg_s(a, "tool_call_id", tool_call_id);
    trp_arg_j(a, "result", persisted);
    r = trp_call(io, "_record_persisted_path_for_stub", a, exc);
    if (*exc) goto done;
    json_free(r);
    /* note_tool_result(agent, name, call_id, function_result, persisted_result) */
    a = json_object();
    trp_arg_s(a, "function_name", function_name);
    trp_arg_s(a, "tool_call_id", tool_call_id);
    trp_arg_j(a, "result", function_result);
    trp_arg_j(a, "persisted_result", persisted);
    r = trp_call(io, "note_tool_result", a, exc);
    if (*exc) goto done;
    json_free(r);
    /* subdir_hints = agent._subdirectory_hints.check_tool_call(name, args) */
    {
        const json_t *sh = trp_attr_req(agent, "_subdirectory_hints", exc);
        if (*exc) goto done;
        if (trp_is_obj(sh) && json_obj_get(sh, "__subdir__")) {
            a = json_object();
            trp_arg_s(a, "name", function_name);
            trp_arg_j(a, "args", function_args);
            subdir_hints = trp_call(io, "subdirectory_hints.check_tool_call", a, exc);
            if (*exc) goto done;
        } else {
            *exc = trp_attr_error(sh, "check_tool_call");
            goto done;
        }
    }
    if (trp_truth(subdir_hints)) {
        a = json_object();
        trp_arg_j(a, "result", persisted);
        mm = trp_call(io, "_is_multimodal_tool_result", a, exc);
        if (*exc) goto done;
        is_mm = trp_truth(mm);
        json_free(mm);
        if (is_mm) {
            a = json_object();
            trp_arg_j(a, "result", persisted);
            trp_arg_j(a, "hints", subdir_hints);
            r = trp_call(io, "_append_subdir_hint_to_multimodal", a, exc);
            if (*exc) goto done;
            json_free(r);
        } else if (trp_is_str(persisted) && trp_is_str(subdir_hints)) {
            /* persisted_result += subdir_hints (str concatenation domain) */
            size_t n1 = strlen(persisted->str_val ? persisted->str_val : "");
            size_t n2 = strlen(subdir_hints->str_val ? subdir_hints->str_val : "");
            char *cat = (char *)malloc(n1 + n2 + 1);
            memcpy(cat, persisted->str_val ? persisted->str_val : "", n1);
            memcpy(cat + n1, subdir_hints->str_val ? subdir_hints->str_val : "", n2);
            cat[n1 + n2] = '\0';
            json_free(persisted);
            persisted = json_string(cat);
            free(cat);
        } else {
            *exc = trp_raise_fmt("TypeError",
                                 "unsupported operand type(s) for +: '%s' and '%s'",
                                 trp_type_name(persisted), trp_type_name(subdir_hints));
            goto done;
        }
    }
    json_free(subdir_hints); subdir_hints = NULL;
    /* _tool_content = agent._tool_result_content_for_active_model(name, persisted) */
    a = json_object();
    trp_arg_s(a, "function_name", function_name);
    trp_arg_j(a, "result", persisted);
    json_t *tool_content = trp_method_call(io, agent, "_tool_result_content_for_active_model", a, exc);
    if (*exc) goto done;
    /* tool_message = make_tool_result_message(name, _tool_content, call_id,
     *     effect_disposition=effect_disposition) */
    a = json_object();
    trp_arg_s(a, "name", function_name);
    trp_arg_j(a, "result", tool_content);
    trp_arg_s(a, "tool_call_id", tool_call_id);
    trp_arg_j(a, "effect_disposition", effect_disposition);
    json_free(tool_content);
    tool_message = trp_call(io, "make_tool_result_message", a, exc);
    if (*exc) goto done;
    /* lsp_deferred.forward_to_agent(agent) */
    r = trp_call(io, "lsp.forward_to_agent", json_object(), exc);
    if (*exc) goto done;
    json_free(r);
    /* plugin_live_notes.attach_to_tool_message(agent, tool_message) */
    a = json_object();
    trp_arg_j(a, "tool_message", tool_message);
    r = trp_call(io, "plugin_live_notes.attach_to_tool_message", a, exc);
    if (*exc) goto done;
    json_free(r);
    /* prepare_metadata = getattr(agent, "tool_result_metadata_callback", None) */
    {
        const json_t *prepare_metadata = trp_getattr(agent, "tool_result_metadata_callback");
        if (!blocked && trp_truth(prepare_metadata)) {
            json_t *display_args = NULL;
            a = json_object();
            trp_arg_s(a, "name", function_name);
            trp_arg_j(a, "args", function_args);
            json_t *red = trp_call(io, "_redact_tool_args_for_display", a, exc);
            if (*exc) goto meta_caught;
            if (trp_truth(red)) {
                display_args = red;
            } else {
                json_free(red);
                display_args = json_copy(function_args);
            }
            a = json_object();
            trp_arg_s(a, "tool_call_id", tool_call_id);
            trp_arg_s(a, "function_name", function_name);
            trp_arg_j(a, "display_args", display_args);
            trp_arg_j(a, "result", function_result);
            json_free(display_args);
            json_t *metadata = NULL;
            if (trp_is_obj(prepare_metadata) && json_obj_get(prepare_metadata, "__callable__")) {
                const json_t *cb = json_obj_get(prepare_metadata, "__callable__");
                if (cb->type == JSON_STRING) {
                    metadata = trp_call(io, cb->str_val, a, exc);
                } else {
                    json_free(a);
                    *exc = trp_raise("AssertionError", "inert callable double invoked");
                }
            } else {
                json_free(a);
                *exc = trp_raise_fmt("TypeError", "'%s' object is not callable",
                                     trp_type_name(prepare_metadata));
            }
            if (*exc) goto meta_caught;
            if (trp_truth(metadata)) {
                if (trp_is_obj(tool_message) && !json_obj_get(tool_message, "__attr__")) {
                    json_set(tool_message, "display_metadata", json_copy(metadata));
                } else {
                    json_free(metadata);
                    *exc = trp_raise_fmt("TypeError",
                                         "'%s' object does not support item assignment",
                                         trp_type_name(tool_message));
                    goto meta_caught;
                }
            }
            json_free(metadata);
            goto meta_done;
        meta_caught:
            {
                json_t *spec = *exc ? json_copy(*exc) : NULL;
                if (!trp_swallow(exc)) { json_free(spec); goto done; }
                json_t *al = json_array();
                json_append(al, trp_exc_norm(spec));
                json_free(spec);
                json_t *lr = trp_logging_debug(io, "Tool result metadata callback error: %s", al, exc);
                if (*exc) goto done;
                json_free(lr);
            }
        meta_done:
            ;
        }
    }
    /* stamp_data_use(tool_message, sorted(data_use)) */
    a = json_object();
    trp_arg_j(a, "tool_message", tool_message);
    json_set(a, "data_use", trp_frozenset_norm(data_use));
    r = trp_call(io, "stamp_data_use", a, exc);
    if (*exc) goto done;
    json_free(r);
    /* messages.append(tool_message) — a None message stays in the list as
     * null; json_append(arr, NULL) would silently drop it. */
    json_append(messages, tool_message ? json_copy(tool_message) : json_null());
    /* if not _flush_session_db_after_tool_progress(...): return None */
    {
        char stage[256];
        snprintf(stage, sizeof(stage), "tool result %s", function_name);
        a = json_object();
        trp_arg_j(a, "messages", messages);
        trp_arg_s(a, "stage", stage);
        json_t *flushed = trp_call(io, "_flush_session_db_after_tool_progress", a, exc);
        if (*exc) goto done;
        bool ok = trp_truth(flushed);
        json_free(flushed);
        if (!ok) goto done;   /* ret stays NULL == None */
    }
    if (!blocked) {
        /* _safe_callback(agent.tool_progress_callback, "Tool progress",
         *     "tool.completed", name, None, None, duration=..., is_error=...,
         *     result=function_result) */
        const json_t *tpc = trp_attr_req(agent, "tool_progress_callback", exc);
        if (*exc) goto done;
        a = json_object();
        trp_arg_cb(a, "callback", tpc);
        trp_arg_s(a, "label", "Tool progress");
        json_t *args_arr = json_array();
        json_append(args_arr, json_string("tool.completed"));
        json_append(args_arr, json_string(function_name));
        json_append(args_arr, json_null());
        json_append(args_arr, json_null());
        json_set(a, "args", args_arr);
        json_t *kw = json_object();
        json_set(kw, "duration", json_number(tool_duration));
        json_set(kw, "is_error", json_bool(is_error));
        json_set(kw, "result", function_result ? json_copy(function_result) : json_null());
        json_set(a, "kwargs", kw);
        r = trp_call(io, "_safe_callback", a, exc);
        if (*exc) goto done;
        json_free(r);
    }
    if (trp_is_str(function_result) &&
        (double)trp_cp_len(function_result->str_val ? function_result->str_val : "")
            >= TRP_LARGE_TOOL_RESULT_TRIM_CHARS) {
        trp_setattr(agent, "_trim_after_tool_batch", json_bool(true), exc);
        if (*exc) goto done;
    }
    /* return persisted_result, function_result, tool_message.get("_tool_output_risk") */
    {
        const json_t *risk = trp_dict_get(tool_message, "_tool_output_risk", exc);
        if (*exc) goto done;
        ret = json_array();
        json_append(ret, persisted ? json_copy(persisted) : json_null());
        json_append(ret, function_result ? json_copy(function_result) : json_null());
        json_append(ret, risk ? json_copy(risk) : json_null());
    }
done:
    json_free(function_result);
    json_free(persisted);
    json_free(tool_message);
    json_free(subdir_hints);
    return ret;
}

/* PoP: trp_finalize_tool_batch @ agent/tool_executor.py:_finalize_tool_batch */
json_t *trp_finalize_tool_batch(const json_t *in, const trp_io_t *io,
                                json_t *state, json_t **exc) {
    json_t *agent = json_obj_get(state, "agent");
    json_t *messages = trp_messages(state);
    const char *task_id = json_get_str(in, "effective_task_id", "");
    long long num_tools = (long long)json_get_num(in, "num_tools", 0);
    const json_t *budget = json_obj_get(in, "budget");
    if (num_tools <= 0) return NULL;
    /* batch = messages[-num_tools:] (clamp to the list length) */
    json_t *batch = json_array();
    json_t *contents_before = json_array();
    size_t n = messages->c.count;
    size_t take = (size_t)num_tools;
    if (take > n) take = n;
    for (size_t i = n - take; i < n; i++)
        json_append(batch, json_copy(messages->c.items[i]));
    /* contents_before = [message.get("content") for message in batch] */
    for (size_t i = 0; i < batch->c.count; i++) {
        const json_t *c = trp_dict_get(batch->c.items[i], "content", exc);
        if (*exc) goto done;
        json_append(contents_before, c ? json_copy(c) : json_null());
    }
    /* enforce_turn_budget(batch, env=get_active_env(task_id), config=budget) */
    {
        json_t *a = json_object();
        trp_arg_s(a, "task_id", task_id);
        json_t *env = trp_call(io, "get_active_env", a, exc);
        if (*exc) goto done;
        a = json_object();
        trp_arg_j(a, "batch", batch);
        trp_arg_j(a, "env", env);
        trp_arg_j(a, "config", budget);
        json_free(env);
        json_t *r = trp_call(io, "enforce_turn_budget", a, exc);
        if (*exc) goto done;
        json_free(r);
    }
    /* plugin_live_notes.keep_after_turn_budget(agent, batch) */
    {
        json_t *a = json_object();
        trp_arg_j(a, "batch", batch);
        json_t *r = trp_call(io, "plugin_live_notes.keep_after_turn_budget", a, exc);
        if (*exc) goto done;
        json_free(r);
    }
    /* sync_rewritten_tool_rows(agent, batch, contents_before) */
    {
        json_t *a = json_object();
        trp_arg_j(a, "batch", batch);
        trp_arg_j(a, "contents_before", contents_before);
        json_t *r = trp_call(io, "sync_rewritten_tool_rows", a, exc);
        if (*exc) goto done;
        json_free(r);
    }
    /* record_tool_batch(agent, batch, contents_before) */
    {
        json_t *a = json_object();
        trp_arg_j(a, "batch", batch);
        trp_arg_j(a, "contents_before", contents_before);
        json_t *r = trp_call(io, "record_tool_batch", a, exc);
        if (*exc) goto done;
        json_free(r);
    }
    /* agent._apply_pending_steer_to_tool_results(messages, num_tools) */
    {
        json_t *a = json_object();
        trp_arg_j(a, "messages", messages);
        trp_arg_n(a, "num_tools", (double)num_tools);
        json_t *r = trp_method_call(io, agent, "_apply_pending_steer_to_tool_results", a, exc);
        if (*exc) goto done;
        json_free(r);
    }
done:
    json_free(batch);
    json_free(contents_before);
    return NULL;
}

/* PoP: trp_print_tool_completed @ agent/tool_executor.py:_print_tool_completed */
json_t *trp_print_tool_completed(const json_t *in, const trp_io_t *io,
                                 json_t *state, json_t **exc) {
    json_t *agent = json_obj_get(state, "agent");
    long long index = (long long)json_get_num(in, "index", 0);
    double dur = json_get_num(in, "tool_duration", 0.0);
    const json_t *result = json_obj_get(in, "result");
    const json_t *vl = trp_attr_req(agent, "verbose_logging", exc);
    if (*exc) return NULL;
    /* "  ✅ Tool {index} completed in {dur:.2f}s" (✅ = U+2705) */
    char line[128];
    snprintf(line, sizeof(line), "  \xE2\x9C\x85 Tool %lld completed in %.2fs", index, dur);
    if (trp_truth(vl)) {
        json_t *a = json_object();
        trp_arg_s(a, "text", line);
        json_t *r = trp_call(io, "print", a, exc);
        if (*exc) return NULL;
        json_free(r);
        /* print(agent._wrap_verbose("Result: ", result)) */
        a = json_object();
        trp_arg_s(a, "prefix", "Result: ");
        trp_arg_j(a, "result", result);
        json_t *wrapped = trp_method_call(io, agent, "_wrap_verbose", a, exc);
        if (*exc) return NULL;
        a = json_object();
        trp_arg_j(a, "text", wrapped);
        json_free(wrapped);
        r = trp_call(io, "print", a, exc);
        if (*exc) return NULL;
        json_free(r);
    } else {
        /* text = result if isinstance(result, str) else str(result) */
        char *text;
        if (trp_is_str(result)) text = strdup(result->str_val ? result->str_val : "");
        else text = trp_pystr(result);
        const json_t *limit = trp_attr_req(agent, "log_prefix_chars", exc);
        if (*exc) { free(text); return NULL; }
        json_t *a = json_object();
        trp_arg_s(a, "text", text);
        trp_arg_j(a, "limit", limit);
        json_t *preview = trp_call(io, "_preview", a, exc);
        free(text);
        if (*exc) return NULL;
        /* f-string str()-ifies the preview (None -> "None") */
        char *pv = trp_pystr(preview);
        size_t cap = strlen(line) + strlen(pv) + 8;
        char *full = (char *)malloc(cap);
        snprintf(full, cap, "%s - %s", line, pv);
        free(pv);
        json_free(preview);
        a = json_object();
        trp_arg_s(a, "text", full);
        free(full);
        json_t *r = trp_call(io, "print", a, exc);
        if (*exc) return NULL;
        json_free(r);
    }
    return NULL;
}

/* PoP: trp_append_batch_results @ agent/tool_executor.py:_append_batch_results */
json_t *trp_append_batch_results(const json_t *in, const trp_io_t *io,
                                 json_t *state, json_t **exc) {
    json_t *agent = json_obj_get(state, "agent");
    json_t *messages = trp_messages(state);
    const char *task_id = json_get_str(in, "effective_task_id", "");
    const json_t *batch = json_obj_get(in, "batch");
    const json_t *budget = json_obj_get(in, "budget");
    const json_t *pcs = trp_attr_req(batch, "parsed_calls", exc);
    if (*exc) return NULL;
    const json_t *results = trp_attr_req(batch, "results", exc);
    if (*exc) return NULL;
    const json_t *toi = trp_attr_req(batch, "timed_out_indices", exc);
    if (*exc) return NULL;
    const json_t *timeout_s = trp_attr_req(batch, "timeout_s", exc);
    if (*exc) return NULL;
    if (!trp_is_arr(pcs)) {
        *exc = trp_raise_fmt("TypeError", "'%s' object is not iterable",
                             trp_type_name(pcs));
        return NULL;
    }
    if (!trp_is_arr(results)) {
        *exc = trp_raise_fmt("TypeError", "'%s' object is not subscriptable",
                             trp_type_name(results));
        return NULL;
    }
    json_t *ret = NULL;
    for (size_t i = 0; i < pcs->c.count; i++) {
        const json_t *pc = pcs->c.items[i];
        json_t *ref = NULL;
        json_t *effect_disposition = NULL;
        json_t *data_use = NULL;
        json_t *display = NULL;
        json_t *risk = NULL;
        json_t *fr_owned = NULL;
        const json_t *function_result = NULL;
        double dur = 0.0;
        bool is_error = false, blocked = false;
        if (i >= results->c.count) {
            *exc = trp_raise("IndexError", "list index out of range");
            goto loop_cleanup;
        }
        const json_t *r = results->c.items[i];
        bool observed = !trp_is_null(r);
        if (!observed) {
            /* ref, is_error, blocked = pc.ref(task_id), True, False */
            json_t *a = json_object();
            trp_arg_s(a, "task_id", task_id);
            ref = trp_call(io, "parsed_call_ref", a, exc);
            if (*exc) goto loop_cleanup;
            is_error = true; blocked = false;
            bool timed_out = false;
            if (trp_is_arr(toi)) {
                for (size_t j = 0; j < toi->c.count; j++) {
                    if (trp_is_num(toi->c.items[j]) &&
                        toi->c.items[j]->num_val == (double)i) { timed_out = true; break; }
                }
            }
            a = json_object();
            trp_arg_j(a, "ref", ref);
            trp_arg_b(a, "timed_out", timed_out);
            trp_arg_j(a, "timeout_s", timeout_s);
            json_t *unf = trp_call(io, "_unfinished_tool_result", a, exc);
            if (*exc) goto loop_cleanup;
            if (!trp_unpack(unf, 3, exc)) { json_free(unf); goto loop_cleanup; }
            function_result = unf->c.items[0];   /* borrowed until copy below */
            dur = trp_num(unf->c.items[1], 0.0);
            effect_disposition = json_copy(unf->c.items[2]);
            json_t *fr_copy = json_copy(function_result);
            json_free(unf);
            function_result = fr_copy;
            fr_owned = fr_copy;
            data_use = json_array();   /* frozenset() */
        } else {
            const json_t *v;
            v = trp_attr_req(r, "ref", exc); if (*exc) goto loop_cleanup;
            ref = json_copy(v);
            v = trp_attr_req(r, "result", exc); if (*exc) goto loop_cleanup;
            function_result = v;
            v = trp_attr_req(r, "duration", exc); if (*exc) goto loop_cleanup;
            dur = trp_num(v, 0.0);
            v = trp_attr_req(r, "is_error", exc); if (*exc) goto loop_cleanup;
            is_error = trp_truth(v);
            v = trp_attr_req(r, "blocked", exc); if (*exc) goto loop_cleanup;
            blocked = trp_truth(v);
            effect_disposition = blocked ? json_string("none") : NULL;
            const json_t *pe = json_obj_get(pc, "parse_error");
            if (!trp_is_null(pe)) {
                json_t *a = json_object();
                trp_arg_j(a, "result", function_result);
                json_t *er = trp_call(io, "emit_invalid_arguments", a, exc);
                if (*exc) goto loop_cleanup;
                json_free(er);
            }
            v = trp_attr_req(r, "data_use", exc); if (*exc) goto loop_cleanup;
            data_use = trp_frozenset_norm(v);
        }
        /* committed = _commit_tool_result(...) */
        {
            json_t *a = json_object();
            trp_arg_j(a, "ref", ref);
            trp_arg_j(a, "result", function_result);
            trp_arg_j(a, "budget", budget);
            trp_arg_n(a, "tool_duration", dur);
            trp_arg_b(a, "is_error", is_error);
            trp_arg_b(a, "blocked", blocked);
            trp_arg_j(a, "effect_disposition", effect_disposition);
            trp_arg_b(a, "observed", observed);
            json_set(a, "data_use", data_use); data_use = NULL;
            json_t *committed = trp_call(io, "_commit_tool_result", a, exc);
            if (*exc) goto loop_cleanup;
            if (trp_is_null(committed)) {
                json_free(committed);
                ret = json_bool(false);
                goto loop_cleanup;
            }
            if (!trp_unpack(committed, 3, exc)) { json_free(committed); goto loop_cleanup; }
            display = json_copy(committed->c.items[1]);
            risk = json_copy(committed->c.items[2]);
            json_free(committed);
        }
        /* presentation: quiet cute line, else progress-enabled completion line */
        {
            json_t *q = trp_method_call(io, agent, "_should_emit_quiet_tool_messages",
                                       json_object(), exc);
            if (*exc) goto loop_cleanup;
            bool quiet = trp_truth(q);
            json_free(q);
            if (quiet) {
                if (trp_is_null(ref)) {
                    *exc = trp_attr_error(ref, "name");
                    goto loop_cleanup;
                }
                json_t *a = json_object();
                trp_arg_s(a, "name", json_get_str(ref, "name", ""));
                trp_arg_j(a, "args", json_obj_get(ref, "args"));
                trp_arg_n(a, "tool_duration", dur);
                trp_arg_j(a, "result", display);
                json_t *cute = trp_call(io, "_get_cute_tool_message_impl", a, exc);
                if (*exc) goto loop_cleanup;
                const char *cs = trp_strval(cute);
                size_t cap = strlen(cs ? cs : "") + 3;
                char *text = (char *)malloc(cap);
                snprintf(text, cap, "  %s", cs ? cs : "");
                json_free(cute);
                a = json_object();
                trp_arg_s(a, "text", text);
                free(text);
                json_t *pr = trp_method_call(io, agent, "_safe_print", a, exc);
                if (*exc) goto loop_cleanup;
                json_free(pr);
            } else {
                json_t *tp = trp_call(io, "_tool_progress_enabled", json_object(), exc);
                if (*exc) goto loop_cleanup;
                bool enabled = trp_truth(tp);
                json_free(tp);
                if (enabled) {
                    /* _multimodal_text_summary(display) evaluates before the call */
                    json_t *a = json_object();
                    trp_arg_j(a, "result", display);
                    json_t *summary = trp_call(io, "_multimodal_text_summary", a, exc);
                    if (*exc) goto loop_cleanup;
                    a = json_object();
                    trp_arg_n(a, "index", (double)(i + 1));
                    trp_arg_n(a, "tool_duration", dur);
                    trp_arg_j(a, "result", summary);
                    json_free(summary);
                    json_t *pr = trp_call(io, "_print_tool_completed", a, exc);
                    if (*exc) goto loop_cleanup;
                    json_free(pr);
                }
            }
        }
        /* _emit_tool_complete_and_risk(agent, ref, display, risk, blocked) */
        {
            json_t *a = json_object();
            trp_arg_j(a, "ref", ref);
            trp_arg_j(a, "result", display);
            trp_arg_j(a, "risk_metadata", risk);
            trp_arg_b(a, "blocked", blocked);
            json_t *er = trp_call(io, "_emit_tool_complete_and_risk", a, exc);
            if (*exc) goto loop_cleanup;
            json_free(er);
        }
        json_free(ref);
        json_free(effect_disposition);
        json_free(display);
        json_free(risk);
        json_free(fr_owned);
        continue;
    loop_cleanup:
        json_free(ref);
        json_free(effect_disposition);
        json_free(data_use);
        json_free(display);
        json_free(risk);
        json_free(fr_owned);
        goto done;
    }
    ret = json_bool(true);
done:
    (void)messages;
    return ret;
}

/* PoP: trp_start_quiet_tool_spinner @ agent/tool_executor.py:_start_quiet_tool_spinner */
json_t *trp_start_quiet_tool_spinner(const json_t *in, const trp_io_t *io,
                                     json_t *state, json_t **exc) {
    json_t *agent = json_obj_get(state, "agent");
    const char *function_name = json_get_str(in, "function_name", "");
    const json_t *function_args = json_obj_get(in, "function_args");
    bool gate = json_get_bool(in, "gate", true);
    const json_t *label_in = json_obj_get(in, "label");
    /* if not _should_emit_quiet_tool_messages() or (gate and not
     * _should_start_quiet_spinner()): return None */
    json_t *q = trp_method_call(io, agent, "_should_emit_quiet_tool_messages",
                               json_object(), exc);
    if (*exc) return NULL;
    bool emit = trp_truth(q);
    json_free(q);
    if (!emit) return NULL;
    if (gate) {
        json_t *s = trp_method_call(io, agent, "_should_start_quiet_spinner",
                                   json_object(), exc);
        if (*exc) return NULL;
        bool start = trp_truth(s);
        json_free(s);
        if (!start) return NULL;
    }
    /* face = random.choice(KawaiiSpinner.get_waiting_faces()) */
    json_t *faces = trp_call(io, "kawaii.get_waiting_faces", json_object(), exc);
    if (*exc) return NULL;
    json_t *a = json_object();
    trp_arg_j(a, "seq", faces);
    json_free(faces);
    json_t *face = trp_call(io, "random.choice", a, exc);
    if (*exc) return NULL;
    char *label = NULL;
    if (trp_is_null(label_in)) {
        a = json_object();
        trp_arg_s(a, "name", function_name);
        trp_arg_j(a, "args", function_args);
        json_t *red = trp_call(io, "_redact_tool_args_for_display", a, exc);
        if (*exc) { json_free(face); return NULL; }
        const json_t *display_args = trp_truth(red) ? red : function_args;
        a = json_object();
        trp_arg_s(a, "name", function_name);
        trp_arg_j(a, "args", display_args);
        json_t *emoji = trp_call(io, "_tool_row_emoji", a, exc);
        if (*exc) { json_free(face); json_free(red); return NULL; }
        a = json_object();
        trp_arg_s(a, "name", function_name);
        trp_arg_j(a, "args", display_args);
        json_t *lbl = trp_call(io, "_build_tool_label", a, exc);
        if (*exc) { json_free(face); json_free(red); json_free(emoji); return NULL; }
        const char *ls = trp_truth(lbl) ? trp_strval(lbl) : function_name;
        const char *es = trp_strval(emoji);
        size_t cap = strlen(es ? es : "") + strlen(ls ? ls : "") + 2;
        label = (char *)malloc(cap);
        snprintf(label, cap, "%s %s", es ? es : "", ls ? ls : "");
        json_free(emoji); json_free(lbl); json_free(red);
    } else {
        label = strdup(trp_strval(label_in) ? trp_strval(label_in) : "");
    }
    /* spinner = KawaiiSpinner(f"{face} {label}", spinner_type='dots',
     *     print_fn=agent._print_fn) */
    const json_t *pfn = trp_attr_req(agent, "_print_fn", exc);
    if (*exc) { free(label); json_free(face); return NULL; }
    const char *fs = trp_strval(face);
    size_t cap = strlen(fs ? fs : "") + strlen(label) + 2;
    char *text = (char *)malloc(cap);
    snprintf(text, cap, "%s %s", fs ? fs : "", label);
    a = json_object();
    trp_arg_s(a, "text", text);
    free(text);
    trp_arg_s(a, "spinner_type", "dots");
    trp_arg_cb(a, "print_fn", pfn);
    json_t *ctor = trp_call(io, "kawaii.ctor", a, exc);
    free(label);
    json_free(face);
    if (*exc) return NULL;
    json_free(ctor);   /* the spinner double is structural, not the script */
    /* spinner.start() */
    json_t *sr = trp_call(io, "spinner.start", json_object(), exc);
    if (*exc) return NULL;
    json_free(sr);
    json_t *ret = json_object();
    json_set(ret, "__spinner__", json_bool(true));
    return ret;
}

/* PoP: trp_finish_quiet_tool_spinner @ agent/tool_executor.py:_finish_quiet_tool_spinner */
json_t *trp_finish_quiet_tool_spinner(const json_t *in, const trp_io_t *io,
                                      json_t *state, json_t **exc) {
    json_t *agent = json_obj_get(state, "agent");
    const json_t *spinner = json_obj_get(in, "spinner");
    const char *function_name = json_get_str(in, "function_name", "");
    const json_t *function_args = json_obj_get(in, "function_args");
    double dur = json_get_num(in, "tool_duration", 0.0);
    const json_t *result = json_obj_get(in, "result");
    bool have_spinner = trp_truth(spinner);
    if (!have_spinner) {
        json_t *q = trp_method_call(io, agent, "_should_emit_quiet_tool_messages",
                                   json_object(), exc);
        if (*exc) return NULL;
        bool emit = trp_truth(q);
        json_free(q);
        if (!emit) return NULL;
    }
    json_t *a = json_object();
    trp_arg_s(a, "name", function_name);
    trp_arg_j(a, "args", function_args);
    trp_arg_n(a, "tool_duration", dur);
    trp_arg_j(a, "result", result);
    json_t *cute = trp_call(io, "_get_cute_tool_message_impl", a, exc);
    if (*exc) return NULL;
    if (have_spinner) {
        /* spinner.stop(cute) */
        a = json_object();
        trp_arg_j(a, "text", cute);
        json_free(cute);
        json_t *r = trp_call(io, "spinner.stop", a, exc);
        if (*exc) return NULL;
        json_free(r);
    } else {
        /* agent._vprint(f"  {cute}") */
        const char *cs = trp_strval(cute);
        size_t cap = strlen(cs ? cs : "") + 3;
        char *text = (char *)malloc(cap);
        snprintf(text, cap, "  %s", cs ? cs : "");
        json_free(cute);
        a = json_object();
        trp_arg_s(a, "text", text);
        free(text);
        json_t *r = trp_method_call(io, agent, "_vprint", a, exc);
        if (*exc) return NULL;
        json_free(r);
    }
    return NULL;
}

/* _SequentialDispatch tagged-dict builder (dataclass value model). */
static json_t *trp_dispatch_obj(const char *kind, const char *name,
                                const json_t *executor) {
    json_t *d = json_object();
    json_set(d, "_type", json_string("_SequentialDispatch"));
    json_t *ex = json_object();
    trp_arg_s(ex, "kind", kind);
    if (executor) json_set(ex, "executor", json_copy(executor));
    else trp_arg_s(ex, "name", name);
    json_set(d, "execute", ex);
    json_set(d, "spinner", json_null());
    json_set(d, "middleware_trace_arg", json_null());
    json_set(d, "error_result", json_null());
    json_set(d, "error_log", json_string(""));
    json_set(d, "handles_keyboard_interrupt", json_bool(false));
    json_set(d, "is_delegate", json_bool(false));
    json_set(d, "finish_spinner", json_bool(true));
    json_set(d, "finish_in_finally", json_bool(true));
    json_set(d, "transform_applied", json_bool(false));
    return d;
}

static json_t *trp_error_result_obj(const char *kind, const char *name) {
    json_t *e = json_object();
    trp_arg_s(e, "kind", kind);
    trp_arg_s(e, "name", name);
    return e;
}

/* PoP: trp_resolve_sequential_dispatch @ agent/tool_executor.py:_resolve_sequential_dispatch */
json_t *trp_resolve_sequential_dispatch(const json_t *in, const trp_io_t *io,
                                        json_t *state, json_t **exc) {
    json_t *agent = json_obj_get(state, "agent");
    json_t *messages = trp_messages(state);
    const json_t *ref = json_obj_get(in, "ref");
    const char *function_name = json_get_str(ref, "name", "");
    const json_t *function_args = json_obj_get(ref, "args");
    const char *effective_task_id = json_get_str(ref, "task_id", "");
    const char *tool_call_id = json_get_str(ref, "call_id", "");
    const json_t *middleware_trace = json_obj_get(ref, "trace");
    const json_t *table = json_obj_get(in, "inline_executors");
    json_t *a, *r;
    /* inline agent-level tools (delegate_task excluded) */
    if (!trp_str_eq(function_name, "delegate_task") &&
        trp_is_obj(table) && json_obj_get(table, function_name)) {
        const json_t *executor = json_obj_get(table, function_name);
        /* InlineToolContext(effective_task_id=..., tool_call_id=..., messages=...) */
        a = json_object();
        trp_arg_s(a, "effective_task_id", effective_task_id);
        trp_arg_s(a, "tool_call_id", tool_call_id);
        trp_arg_j(a, "messages", messages);
        r = trp_call(io, "inline_tool_context.ctor", a, exc);
        if (*exc) return NULL;
        json_free(r);
        json_t *d = trp_dispatch_obj("inline", function_name, executor);
        json_set(d, "finish_in_finally", json_bool(false));
        return d;
    }
    if (trp_str_eq(function_name, "delegate_task")) {
        /* label kwarg evaluates first: _delegate_spinner_label(function_args) */
        a = json_object();
        trp_arg_j(a, "function_args", function_args);
        json_t *label = trp_call(io, "_delegate_spinner_label", a, exc);
        if (*exc) return NULL;
        a = json_object();
        trp_arg_s(a, "name", function_name);
        trp_arg_j(a, "args", function_args);
        trp_arg_b(a, "gate", true);
        trp_arg_j(a, "label", label);
        json_free(label);
        json_t *spinner = trp_call(io, "_start_quiet_tool_spinner", a, exc);
        if (*exc) return NULL;
        /* agent._delegate_spinner = spinner */
        trp_setattr(agent, "_delegate_spinner",
                    spinner ? json_copy(spinner) : json_null(), exc);
        if (*exc) { json_free(spinner); return NULL; }
        /* from tools.arg_validation import call_with_arg_checks */
        r = trp_call(io, "import_arg_validation", json_object(), exc);
        if (*exc) { json_free(spinner); return NULL; }
        json_free(r);
        /* from tools.registry import registry */
        r = trp_call(io, "import_registry", json_object(), exc);
        if (*exc) { json_free(spinner); return NULL; }
        json_free(r);
        json_t *d = trp_dispatch_obj("delegate", function_name, NULL);
        json_set(d, "spinner", spinner ? spinner : json_null());
        json_set(d, "is_delegate", json_bool(true));
        return d;
    }
    /* is_context_engine_tool(agent, function_name) */
    a = json_object();
    trp_arg_s(a, "function_name", function_name);
    r = trp_call(io, "is_context_engine_tool", a, exc);
    if (*exc) return NULL;
    bool is_ce = trp_truth(r);
    json_free(r);
    if (is_ce) {
        a = json_object();
        trp_arg_s(a, "name", function_name);
        trp_arg_j(a, "args", function_args);
        trp_arg_b(a, "gate", false);
        json_t *spinner = trp_call(io, "_start_quiet_tool_spinner", a, exc);
        if (*exc) return NULL;
        json_t *d = trp_dispatch_obj("context_engine", function_name, NULL);
        json_set(d, "spinner", spinner ? spinner : json_null());
        json_set(d, "error_result", trp_error_result_obj("context_engine", function_name));
        json_set(d, "error_log",
                 json_string("context_engine.handle_tool_call raised for %s: %s"));
        return d;
    }
    /* is_memory_provider_tool(agent, function_name) */
    a = json_object();
    trp_arg_s(a, "function_name", function_name);
    r = trp_call(io, "is_memory_provider_tool", a, exc);
    if (*exc) return NULL;
    bool is_mem = trp_truth(r);
    json_free(r);
    if (is_mem) {
        a = json_object();
        trp_arg_s(a, "name", function_name);
        trp_arg_j(a, "args", function_args);
        trp_arg_b(a, "gate", true);
        json_t *spinner = trp_call(io, "_start_quiet_tool_spinner", a, exc);
        if (*exc) return NULL;
        json_t *d = trp_dispatch_obj("memory", function_name, NULL);
        json_set(d, "spinner", spinner ? spinner : json_null());
        json_set(d, "error_result", trp_error_result_obj("memory", function_name));
        json_set(d, "error_log",
                 json_string("memory_manager.handle_tool_call raised for %s: %s"));
        return d;
    }
    /* registry tools */
    {
        const json_t *qm = trp_attr_req(agent, "quiet_mode", exc);
        if (*exc) return NULL;
        bool quiet = trp_truth(qm);
        json_t *spinner = NULL;
        if (quiet) {
            a = json_object();
            trp_arg_s(a, "name", function_name);
            trp_arg_j(a, "args", function_args);
            trp_arg_b(a, "gate", true);
            spinner = trp_call(io, "_start_quiet_tool_spinner", a, exc);
            if (*exc) return NULL;
        }
        json_t *d = trp_dispatch_obj("registry", function_name, NULL);
        json_set(d, "spinner", spinner ? spinner : json_null());
        json_set(d, "middleware_trace_arg",
                 middleware_trace ? json_copy(middleware_trace) : json_null());
        json_set(d, "error_result", trp_error_result_obj("registry", function_name));
        json_set(d, "error_log", json_string("handle_function_call raised for %s: %s"));
        json_set(d, "handles_keyboard_interrupt", json_bool(true));
        json_set(d, "finish_spinner", json_bool(quiet));
        json_set(d, "transform_applied", json_bool(true));
        return d;
    }
}

/* PoP: trp_skip_remaining_sequential @ agent/tool_executor.py:_skip_remaining_sequential */
json_t *trp_skip_remaining_sequential(const json_t *in, const trp_io_t *io,
                                      json_t *state, json_t **exc) {
    json_t *agent = json_obj_get(state, "agent");
    const json_t *remaining = json_obj_get(in, "remaining");
    const char *task_id = json_get_str(in, "effective_task_id", "");
    const char *notice = json_get_str(in, "notice", "");
    const json_t *skip_kwargs = json_obj_get(in, "skip_kwargs");
    /* live loads agent._vprint BEFORE evaluating the f-string argument */
    const json_t *vp = trp_attr_req(agent, "_vprint", exc);
    if (*exc) return NULL;
    const json_t *lp = trp_attr_req(agent, "log_prefix", exc);
    if (*exc) return NULL;
    long long n = trp_len_value(remaining, exc);
    if (*exc) return NULL;
    /* f"{agent.log_prefix}⚡ Interrupt: skipping {len(remaining)} {notice}" */
    char text[512];
    snprintf(text, sizeof(text), "%s\xE2\x9A\xA1 Interrupt: skipping %lld %s",
             trp_strval(lp) ? trp_strval(lp) : "", n, notice);
    json_t *a = json_object();
    trp_arg_s(a, "text", text);
    trp_arg_b(a, "force", true);
    json_t *r;
    if (trp_is_obj(vp) && json_obj_get(vp, "__callable__")) {
        const json_t *cb = json_obj_get(vp, "__callable__");
        if (cb->type == JSON_STRING) {
            r = trp_call(io, cb->str_val, a, exc);
        } else {
            json_free(a);
            *exc = trp_raise("AssertionError", "inert callable double invoked");
            return NULL;
        }
    } else {
        json_free(a);
        *exc = trp_raise_fmt("TypeError", "'%s' object is not callable",
                             trp_type_name(vp));
        return NULL;
    }
    if (*exc) return NULL;
    json_free(r);
    /* return _append_skipped_tool_results(agent, messages, remaining,
     *     effective_task_id, **skip_kwargs) */
    a = json_object();
    trp_arg_j(a, "remaining", remaining);
    trp_arg_s(a, "effective_task_id", task_id);
    if (trp_is_obj(skip_kwargs)) {
        for (size_t i = 0; i < skip_kwargs->c.count; i++) {
            const char *k = skip_kwargs->c.keys[i];
            const json_t *v = skip_kwargs->c.items[i];
            if (trp_str_eq(k, "hook_id")) trp_arg_cb(a, k, v);
            else trp_arg_j(a, k, v);
        }
    }
    return trp_call(io, "_append_skipped_tool_results", a, exc);
}

/* PoP: trp_append_invalid_arguments_result @ agent/tool_executor.py:_append_invalid_arguments_result */
json_t *trp_append_invalid_arguments_result(const json_t *in, const trp_io_t *io,
                                            json_t *state, json_t **exc) {
    json_t *messages = trp_messages(state);
    const json_t *ref = json_obj_get(in, "ref");
    const char *parse_error = json_get_str(in, "parse_error", "");
    if (trp_is_null(ref)) {
        *exc = trp_attr_error(ref, "emit_invalid_arguments");
        return NULL;
    }
    /* ref.emit_invalid_arguments(agent, parse_error) */
    json_t *a = json_object();
    trp_arg_s(a, "result", parse_error);
    json_t *r = trp_call(io, "emit_invalid_arguments", a, exc);
    if (*exc) return NULL;
    json_free(r);
    /* messages.append(make_tool_result_message(ref.name, parse_error, ref.call_id)) */
    a = json_object();
    trp_arg_s(a, "name", json_get_str(ref, "name", ""));
    trp_arg_s(a, "result", parse_error);
    trp_arg_s(a, "tool_call_id", json_get_str(ref, "call_id", ""));
    json_t *msg = trp_call(io, "make_tool_result_message", a, exc);
    if (*exc) return NULL;
    json_append(messages, msg ? msg : json_null());
    /* return _flush_session_db_after_tool_progress(agent, messages,
     *     stage=f"invalid tool arguments {ref.name}") */
    char stage[256];
    snprintf(stage, sizeof(stage), "invalid tool arguments %s",
             json_get_str(ref, "name", ""));
    a = json_object();
    trp_arg_j(a, "messages", messages);
    trp_arg_s(a, "stage", stage);
    return trp_call(io, "_flush_session_db_after_tool_progress", a, exc);
}

/* copy-or-null (ret dicts keep explicit nulls; args builders drop them). */
static json_t *trp_copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}

/* PoP: trp_run_sequential_call @ agent/tool_executor.py:_run_sequential_call */
json_t *trp_run_sequential_call(const json_t *in, const trp_io_t *io,
                                json_t *state, json_t **exc) {
    json_t *agent = json_obj_get(state, "agent");
    json_t *ref = json_obj_get(state, "ref");
    json_t *messages = trp_messages(state);
    const json_t *dispatch = json_obj_get(in, "dispatch");
    const json_t *scope_block = json_obj_get(in, "scope_block");
    const json_t *remaining_calls = json_obj_get(in, "remaining_calls");
    double display_index = json_get_num(in, "display_index", 0);
    double tool_start_time = json_get_num(in, "tool_start_time", 0.0);

    bool handles_ki = json_get_bool(dispatch, "handles_keyboard_interrupt", false);
    bool is_delegate = json_get_bool(dispatch, "is_delegate", false);
    bool finish_spinner = json_get_bool(dispatch, "finish_spinner", true);
    bool finish_in_finally = json_get_bool(dispatch, "finish_in_finally", true);
    const json_t *error_result = json_obj_get(dispatch, "error_result");
    const char *error_log = json_get_str(dispatch, "error_log", "");
    const json_t *dspinner = json_obj_get(dispatch, "spinner");
    const json_t *dtrace = json_obj_get(dispatch, "middleware_trace_arg");
    const json_t *dexecute = json_obj_get(dispatch, "execute");

    json_t *managed = NULL;
    json_t *spinner_result = NULL;
    json_t *pending = NULL;
    json_t *ret = NULL;
    double tool_duration = 0.0;
    bool reraise = false;
    json_t *a;

    /* try: managed = _run_sequential_tool_execution_middleware(agent,
     *     **dict(ref.middleware_kwargs(), middleware_trace=dispatch.middleware_trace_arg),
     *     execute=dispatch.execute, scope_block=scope_block, display_index=display_index) */
    json_t *mk = trp_call(io, "middleware_kwargs", json_object(), exc);
    if (*exc) { pending = *exc; *exc = NULL; goto finally; }
    if (!trp_is_obj(mk)) {
        pending = trp_raise_fmt("TypeError", "'%s' object is not iterable",
                                trp_type_name(mk));
        json_free(mk);
        goto excepts;   /* dict(None,...) raises INSIDE the try -> except Exception */
    }
    a = json_object();
    for (size_t i = 0; i < mk->c.count; i++)
        trp_arg_j(a, mk->c.keys[i], mk->c.items[i]);
    json_free(mk);
    /* dict(..., middleware_trace=dispatch.middleware_trace_arg) override */
    json_set(a, "middleware_trace", trp_copy_or_null(dtrace));
    {
        const json_t *mt = json_obj_get(a, "middleware_trace");
        if (trp_is_null(mt)) json_obj_del(a, "middleware_trace");
    }
    trp_arg_cb(a, "execute", dexecute);
    trp_arg_j(a, "scope_block", scope_block);
    trp_arg_n(a, "display_index", display_index);
    json_t *m = trp_call(io, "_run_sequential_tool_execution_middleware", a, exc);
    if (*exc) { pending = *exc; *exc = NULL; goto excepts; }
    if (!trp_is_obj(m)) {
        pending = trp_attr_error(m, "args");
        json_free(m);
        goto excepts;
    }
    managed = m;
    /* ref.args = managed.args ; _spinner_result = managed.result (inside try) */
    if (trp_is_null(ref)) {
        pending = trp_attr_error(ref, "args");
        goto excepts;
    }
    json_set(ref, "args", trp_copy_or_null(json_obj_get(managed, "args")));
    spinner_result = trp_copy_or_null(json_obj_get(managed, "result"));
    goto finally;

excepts:
    /* except KeyboardInterrupt */
    if (trp_exc_is_a(json_get_str(pending, "__raise__", ""), "KeyboardInterrupt")) {
        if (!handles_ki) { reraise = true; goto finally; }
        /* with contextlib.suppress(Exception):
         *     agent.interrupt("keyboard interrupt", tool_reason=_REASON_USER_INTERRUPT) */
        a = json_object();
        trp_arg_s(a, "reason", "keyboard interrupt");
        trp_arg_s(a, "tool_reason", TRP_REASON_USER_INTERRUPT);
        json_t *ir = trp_method_call(io, agent, "interrupt", a, exc);
        if (*exc) {
            json_t *spec = *exc; *exc = NULL;
            if (!trp_exc_is_exception(json_get_str(spec, "__raise__", ""))) {
                json_free(pending);
                pending = spec;
                reraise = true;
                goto finally;
            }
            json_free(spec);
        } else {
            json_free(ir);
        }
        /* _spinner_result = ref.emit_cancelled(agent, tool_start_time) */
        a = json_object();
        trp_arg_n(a, "start_time", tool_start_time);
        json_t *cr = trp_call(io, "emit_cancelled", a, exc);
        if (*exc) { pending = *exc; *exc = NULL; reraise = true; goto finally; }
        json_free(spinner_result);
        spinner_result = cr;
        /* _append_skipped_tool_results(agent, messages, remaining_calls, ref.task_id,
         *     content=f"[Tool execution cancelled — {{name}} was skipped. {wording}]") */
        a = json_object();
        trp_arg_j(a, "agent", agent);
        json_t *wording = trp_call(io, "interrupt_skip_wording", a, exc);
        if (*exc) { pending = *exc; *exc = NULL; reraise = true; goto finally; }
        char *wstr = trp_pystr(wording);
        char content[512];
        snprintf(content, sizeof(content),
                 "[Tool execution cancelled \xE2\x80\x94 {name} was skipped. %s]", wstr);
        free(wstr);
        json_free(wording);
        a = json_object();
        trp_arg_j(a, "remaining", remaining_calls);
        trp_arg_s(a, "effective_task_id", json_get_str(ref, "task_id", ""));
        trp_arg_s(a, "content", content);
        json_t *sr = trp_call(io, "_append_skipped_tool_results", a, exc);
        if (*exc) { pending = *exc; *exc = NULL; reraise = true; goto finally; }
        json_free(sr);
        reraise = true;   /* bare raise: the original KeyboardInterrupt */
        goto finally;
    }
    /* except Exception as tool_error */
    if (trp_exc_is_exception(json_get_str(pending, "__raise__", ""))) {
        if (trp_is_null(error_result)) { reraise = true; goto finally; }
        /* function_result = dispatch.error_result(tool_error) */
        json_t *fr = NULL;
        a = json_object();
        json_set(a, "error", trp_exc_norm(pending));
        if (trp_is_obj(error_result) && json_obj_get(error_result, "__callable__")) {
            const json_t *cb = json_obj_get(error_result, "__callable__");
            if (cb->type == JSON_STRING) {
                fr = trp_call(io, cb->str_val, a, exc);
            } else {
                json_free(a);
                *exc = trp_raise("AssertionError", "inert callable double invoked");
            }
        } else {
            json_free(a);
            *exc = trp_raise_fmt("TypeError", "'%s' object is not callable",
                                 trp_type_name(error_result));
        }
        if (*exc) { pending = *exc; *exc = NULL; reraise = true; goto finally; }
        /* logger.error(dispatch.error_log, ref.name, tool_error, exc_info=True) */
        {
            json_t *al = json_array();
            json_append(al, json_string(json_get_str(ref, "name", "")));
            json_append(al, trp_exc_norm(pending));
            a = json_object();
            trp_arg_s(a, "fmt", error_log);
            json_set(a, "args", al);
            trp_arg_b(a, "exc_info", true);
            json_t *lr = trp_call(io, "logger.error", a, exc);
            if (*exc) {
                json_free(fr);
                pending = *exc; *exc = NULL; reraise = true;
                goto finally;
            }
            json_free(lr);
        }
        /* managed = _ManagedToolResult(result=function_result, args=ref.args,
         *     middleware_trace=ref.trace, blocked=False, dispatched=False) */
        json_free(managed);
        managed = json_object();
        json_set(managed, "_type", json_string("_ManagedToolResult"));
        json_set(managed, "result", fr ? fr : json_null());
        json_set(managed, "args", trp_copy_or_null(json_obj_get(ref, "args")));
        json_set(managed, "middleware_trace", trp_copy_or_null(json_obj_get(ref, "trace")));
        json_set(managed, "blocked", json_bool(false));
        json_set(managed, "dispatched", json_bool(false));
        json_set(managed, "data_use", json_array());
        json_free(pending); pending = NULL;
        goto finally;
    }
    /* other BaseException (SystemExit / GeneratorExit / CancelledError): propagate */
    reraise = true;

finally:
    /* if dispatch.is_delegate: agent._delegate_spinner = None */
    if (is_delegate) {
        trp_setattr(agent, "_delegate_spinner", json_null(), exc);
        if (*exc) {
            json_free(pending);
            pending = *exc; *exc = NULL;
            reraise = true;
            goto after;   /* a finally raise skips the remaining finally steps */
        }
    }
    /* tool_duration = time.time() - tool_start_time */
    {
        json_t *t = trp_call(io, "time.time", json_object(), exc);
        if (*exc) {
            json_free(pending);
            pending = *exc; *exc = NULL;
            reraise = true;
            goto after;
        }
        if (!trp_is_num(t)) {
            json_free(pending);
            pending = trp_raise_fmt("TypeError",
                                    "unsupported operand type(s) for -: '%s' and '%s'",
                                    trp_type_name(t),
                                    trp_type_name(json_obj_get(in, "tool_start_time")));
            json_free(t);
            reraise = true;
            goto after;
        }
        tool_duration = trp_num(t, 0.0) - tool_start_time;
        json_free(t);
    }
    /* if dispatch.finish_spinner and dispatch.finish_in_finally:
     *     _finish_quiet_tool_spinner(agent, dispatch.spinner, ref.name, ref.args,
     *         tool_duration, _spinner_result) */
    if (finish_spinner && finish_in_finally) {
        a = json_object();
        trp_arg_j(a, "spinner", dspinner);
        trp_arg_s(a, "name", json_get_str(ref, "name", ""));
        trp_arg_j(a, "args", json_obj_get(ref, "args"));
        trp_arg_n(a, "tool_duration", tool_duration);
        trp_arg_j(a, "result", spinner_result);
        json_t *fr = trp_call(io, "_finish_quiet_tool_spinner", a, exc);
        if (*exc) {
            json_free(pending);
            pending = *exc; *exc = NULL;
            reraise = true;
            goto after;
        }
        json_free(fr);
    }
after:
    if (reraise) {
        *exc = pending;
        json_free(managed);
        json_free(spinner_result);
        (void)messages;
        return NULL;
    }
    /* if dispatch.finish_spinner and not dispatch.finish_in_finally: finish again */
    if (finish_spinner && !finish_in_finally) {
        a = json_object();
        trp_arg_j(a, "spinner", dspinner);
        trp_arg_s(a, "name", json_get_str(ref, "name", ""));
        trp_arg_j(a, "args", json_obj_get(ref, "args"));
        trp_arg_n(a, "tool_duration", tool_duration);
        trp_arg_j(a, "result", spinner_result);
        json_t *fr = trp_call(io, "_finish_quiet_tool_spinner", a, exc);
        if (*exc) {
            json_free(managed);
            json_free(spinner_result);
            return NULL;
        }
        json_free(fr);
    }
    /* return managed, tool_duration */
    ret = json_array();
    json_append(ret, managed ? managed : json_null());
    json_append(ret, json_number(tool_duration));
    json_free(spinner_result);
    (void)messages;
    return ret;
}

/* PoP: trp_publish_sequential_result @ agent/tool_executor.py:_publish_sequential_result */
json_t *trp_publish_sequential_result(const json_t *in, const trp_io_t *io,
                                      json_t *state, json_t **exc) {
    json_t *agent = json_obj_get(state, "agent");
    json_t *messages = trp_messages(state);
    json_t *ref = json_obj_get(state, "ref");
    const json_t *managed = json_obj_get(in, "managed");
    double tool_duration = json_get_num(in, "tool_duration", 0.0);
    double index = json_get_num(in, "index", 0);
    const json_t *budget = json_obj_get(in, "budget");
    bool transform_applied = json_get_bool(in, "transform_applied", false);
    json_t *function_result = NULL;
    json_t *display = NULL;
    json_t *risk = NULL;
    json_t *is_error_val = NULL;
    json_t *a, *r;
    (void)agent; (void)messages;

    /* ref.args, ref.trace, function_result = managed.args, managed.middleware_trace,
     *     managed.result */
    if (trp_is_null(ref)) { *exc = trp_attr_error(ref, "args"); return NULL; }
    if (!trp_is_obj(managed)) { *exc = trp_attr_error(managed, "args"); return NULL; }
    json_set(ref, "args", trp_copy_or_null(json_obj_get(managed, "args")));
    json_set(ref, "trace", trp_copy_or_null(json_obj_get(managed, "middleware_trace")));
    function_result = trp_copy_or_null(json_obj_get(managed, "result"));
    bool blocked = trp_truth(json_obj_get(managed, "blocked"));
    const json_t *data_use = json_obj_get(managed, "data_use");
    /* _execution_timed_out = isinstance(fr, (_ToolTimeoutResult, _ToolCancelledResult)) */
    const char *fr_type = trp_is_obj(function_result)
        ? json_get_str(function_result, "_type", "") : "";
    bool timed_out = trp_str_eq(fr_type, "_ToolTimeoutResult") ||
                     trp_str_eq(fr_type, "_ToolCancelledResult");
    long long duration_ms = (long long)(tool_duration * 1000.0);
    if (!blocked && !timed_out) {
        /* ref.emit_post(agent, function_result, duration_ms=int(tool_duration*1000)) */
        a = json_object();
        trp_arg_j(a, "result", function_result);
        trp_arg_n(a, "duration_ms", (double)duration_ms);
        r = trp_call(io, "emit_post", a, exc);
        if (*exc) goto fail;
        json_free(r);
        if (!transform_applied) {
            a = json_object();
            trp_arg_s(a, "function_name", json_get_str(ref, "name", ""));
            trp_arg_j(a, "function_args", json_obj_get(ref, "args"));
            trp_arg_j(a, "result", function_result);
            trp_arg_s(a, "effective_task_id", json_get_str(ref, "task_id", ""));
            trp_arg_s(a, "tool_call_id", json_get_str(ref, "call_id", ""));
            trp_arg_n(a, "duration_ms", (double)duration_ms);
            r = trp_call(io, "apply_transform_tool_result", a, exc);
            if (*exc) goto fail;
            json_free(function_result);
            function_result = r;
        }
    }
    /* _result_len = len(fr) if isinstance(fr, str) else len(str(fr));
     * marker strings ARE str subclasses (len of their value). */
    long long result_len;
    if (timed_out) {
        result_len = (long long)trp_cp_len(json_get_str(function_result, "value", ""));
    } else if (trp_is_str(function_result)) {
        result_len = (long long)trp_cp_len(trp_strval(function_result)
                                           ? trp_strval(function_result) : "");
    } else {
        char *s = trp_pystr(function_result);
        result_len = (long long)trp_cp_len(s);
        free(s);
    }
    /* _is_error_result, _ = _detect_tool_failure(ref.name, function_result) */
    a = json_object();
    trp_arg_s(a, "name", json_get_str(ref, "name", ""));
    trp_arg_j(a, "result", function_result);
    json_t *det = trp_call(io, "_detect_tool_failure", a, exc);
    if (*exc) goto fail;
    if (!trp_unpack(det, 2, exc)) { json_free(det); goto fail; }
    is_error_val = json_copy(det->c.items[0]);
    json_free(det);
    /* committed = _commit_tool_result(...) */
    a = json_object();
    trp_arg_j(a, "ref", ref);
    trp_arg_j(a, "result", function_result);
    trp_arg_j(a, "budget", budget);
    trp_arg_n(a, "tool_duration", tool_duration);
    trp_arg_j(a, "is_error", is_error_val);
    trp_arg_b(a, "blocked", blocked);
    if (timed_out) trp_arg_s(a, "effect_disposition", "unknown");
    trp_arg_b(a, "observed", true);
    json_set(a, "data_use", trp_frozenset_norm(data_use));
    trp_arg_n(a, "success_log_chars", (double)result_len);
    json_t *committed = trp_call(io, "_commit_tool_result", a, exc);
    if (*exc) goto fail;
    if (trp_is_null(committed)) {
        json_free(committed);
        json_free(function_result); function_result = NULL;
        json_free(is_error_val);
        return json_bool(false);
    }
    if (!trp_unpack(committed, 3, exc)) { json_free(committed); goto fail; }
    json_free(function_result);
    function_result = json_copy(committed->c.items[0]);
    display = json_copy(committed->c.items[1]);
    risk = json_copy(committed->c.items[2]);
    json_free(committed);
    /* from agent.terminal_approval_batch import mark_batch_outcome */
    r = trp_call(io, "import_mark_batch_outcome", json_object(), exc);
    if (*exc) goto fail;
    json_free(r);
    /* mark_batch_outcome(_is_error_result or bool(managed.blocked)) */
    a = json_object();
    if (trp_truth(is_error_val)) json_set(a, "failed", json_copy(is_error_val));
    else json_set(a, "failed", json_bool(blocked));
    r = trp_call(io, "mark_batch_outcome", a, exc);
    if (*exc) goto fail;
    json_free(r);
    /* _emit_tool_complete_and_risk(agent, ref, display, risk, managed.blocked) */
    a = json_object();
    trp_arg_j(a, "ref", ref);
    trp_arg_j(a, "result", display);
    trp_arg_j(a, "risk_metadata", risk);
    trp_arg_b(a, "blocked", blocked);
    r = trp_call(io, "_emit_tool_complete_and_risk", a, exc);
    if (*exc) goto fail;
    json_free(r);
    /* if _tool_progress_enabled(agent): _print_tool_completed(agent, index, dur, fr) */
    r = trp_call(io, "_tool_progress_enabled", json_object(), exc);
    if (*exc) goto fail;
    bool enabled = trp_truth(r);
    json_free(r);
    if (enabled) {
        a = json_object();
        trp_arg_n(a, "index", index);
        trp_arg_n(a, "tool_duration", tool_duration);
        trp_arg_j(a, "result", function_result);
        r = trp_call(io, "_print_tool_completed", a, exc);
        if (*exc) goto fail;
        json_free(r);
    }
    json_free(display);
    json_free(risk);
    json_free(function_result);
    json_free(is_error_val);
    return json_bool(true);
fail:
    json_free(display);
    json_free(risk);
    json_free(function_result);
    json_free(is_error_val);
    return NULL;
}

/* PoP: trp_execute_tool_calls_sequential @ agent/tool_executor.py:_execute_tool_calls_sequential */
json_t *trp_execute_tool_calls_sequential(const json_t *in, const trp_io_t *io,
                                          json_t *state, json_t **exc) {
    json_t *agent = json_obj_get(state, "agent");
    json_t *messages = trp_messages(state);
    const json_t *assistant_message = json_obj_get(in, "assistant_message");
    const char *task_id = json_get_str(in, "effective_task_id", "");
    bool finalize = json_get_bool(in, "finalize", true);
    json_t *a, *r;
    /* _tool_budget = _budget_for_agent(agent) */
    json_t *budget = trp_call(io, "_budget_for_agent", json_object(), exc);
    if (*exc) return NULL;
    const json_t *tool_calls = trp_attr_req(assistant_message, "tool_calls", exc);
    if (*exc) { json_free(budget); return NULL; }
    if (!trp_is_arr(tool_calls)) {
        *exc = trp_raise_fmt("TypeError", "'%s' object is not iterable",
                             trp_type_name(tool_calls));
        json_free(budget);
        return NULL;
    }
    size_t n = tool_calls->c.count;
    for (size_t idx = 0; idx < n; idx++) {
        size_t i = idx + 1;   /* enumerate(tool_calls, 1) */
        const json_t *tc = tool_calls->c.items[idx];
        /* if getattr(agent, "_incremental_persistence_failed", False): return */
        if (trp_truth(trp_getattr(agent, "_incremental_persistence_failed"))) {
            json_free(budget);
            return NULL;
        }
        /* interrupt check BEFORE each tool */
        const json_t *irq = trp_attr_req(agent, "_interrupt_requested", exc);
        if (*exc) { json_free(budget); return NULL; }
        if (trp_truth(irq)) {
            a = json_object();
            trp_arg_j(a, "agent", agent);
            json_t *wording = trp_call(io, "interrupt_skip_wording", a, exc);
            if (*exc) { json_free(budget); return NULL; }
            char *wstr = trp_pystr(wording);
            char content[512];
            snprintf(content, sizeof(content),
                     "[Tool execution cancelled \xE2\x80\x94 {name} was skipped. %s]", wstr);
            free(wstr);
            json_free(wording);
            a = json_object();
            json_t *rem = json_array();
            for (size_t j = idx; j < n; j++)
                json_append(rem, json_copy(tool_calls->c.items[j]));
            trp_arg_j(a, "remaining", rem);
            json_free(rem);
            trp_arg_s(a, "effective_task_id", task_id);
            trp_arg_s(a, "notice", "tool call(s)");
            trp_arg_s(a, "content", content);
            trp_arg_s(a, "hook_error_type", "user_interrupt");
            trp_arg_s(a, "hook_id", "<callable>");
            trp_arg_s(a, "flush_stage", "cancelled tool result");
            json_t *sr = trp_call(io, "_skip_remaining_sequential", a, exc);
            if (*exc) { json_free(budget); return NULL; }
            bool ok = trp_truth(sr);
            json_free(sr);
            if (!ok) { json_free(budget); return NULL; }
            break;
        }
        /* pc = _parse_tool_call(agent, tool_call, flatten_probe=True) */
        a = json_object();
        trp_arg_j(a, "tool_call", tc);
        trp_arg_b(a, "flatten_probe", true);
        json_t *pc = trp_call(io, "_parse_tool_call", a, exc);
        if (*exc) { json_free(budget); return NULL; }
        if (!trp_is_obj(pc)) {
            /* live reads pc.ref first */
            *exc = trp_attr_error(pc, "ref");
            json_free(pc); json_free(budget);
            return NULL;
        }
        /* ref = pc.ref(effective_task_id) */
        a = json_object();
        trp_arg_s(a, "task_id", task_id);
        json_t *ref = trp_call(io, "parsed_call_ref", a, exc);
        if (*exc) { json_free(pc); json_free(budget); return NULL; }
        const json_t *pe = json_obj_get(pc, "parse_error");
        if (!trp_is_null(pe)) {
            a = json_object();
            trp_arg_j(a, "ref", ref);
            trp_arg_j(a, "parse_error", pe);
            json_t *ar = trp_call(io, "_append_invalid_arguments_result", a, exc);
            if (*exc) { json_free(ref); json_free(pc); json_free(budget); return NULL; }
            bool ok = trp_truth(ar);
            json_free(ar);
            json_free(ref); json_free(pc);
            if (!ok) { json_free(budget); return NULL; }
            continue;
        }
        /* tool_start_time = time.time() (raw value flows into the next seam) */
        json_t *t = trp_call(io, "time.time", json_object(), exc);
        if (*exc) { json_free(ref); json_free(pc); json_free(budget); return NULL; }
        /* dispatch = _resolve_sequential_dispatch(agent, ref, messages) */
        a = json_object();
        trp_arg_j(a, "ref", ref);
        trp_arg_j(a, "messages", messages);
        json_t *dispatch = trp_call(io, "_resolve_sequential_dispatch", a, exc);
        if (*exc) { json_free(ref); json_free(pc); json_free(budget); return NULL; }
        /* managed, tool_duration = _run_sequential_call(...) */
        a = json_object();
        trp_arg_j(a, "dispatch", dispatch);
        trp_arg_j(a, "ref", ref);
        trp_arg_j(a, "scope_block", json_obj_get(pc, "scope_block"));
        json_t *rem = json_array();
        for (size_t j = idx; j < n; j++)
            json_append(rem, json_copy(tool_calls->c.items[j]));
        trp_arg_j(a, "remaining", rem);
        json_free(rem);
        trp_arg_n(a, "display_index", (double)i);
        trp_arg_j(a, "tool_start_time", t);
        json_free(t);
        json_t *rr = trp_call(io, "_run_sequential_call", a, exc);
        if (*exc) {
            json_free(dispatch); json_free(ref); json_free(pc); json_free(budget);
            return NULL;
        }
        if (!trp_unpack(rr, 2, exc)) {
            json_free(rr); json_free(dispatch); json_free(ref); json_free(pc);
            json_free(budget);
            return NULL;
        }
        json_t *managed = json_copy(rr->c.items[0]);
        double dur = trp_num(rr->c.items[1], 0.0);
        json_free(rr);
        /* if not _publish_sequential_result(...): return */
        a = json_object();
        trp_arg_j(a, "ref", ref);
        trp_arg_j(a, "managed", managed);
        trp_arg_n(a, "tool_duration", dur);
        trp_arg_n(a, "index", (double)i);
        trp_arg_j(a, "budget", budget);
        trp_arg_b(a, "transform_applied",
                  trp_truth(json_obj_get(dispatch, "transform_applied")));
        json_t *pub = trp_call(io, "_publish_sequential_result", a, exc);
        json_free(managed);
        if (*exc) {
            json_free(dispatch); json_free(ref); json_free(pc); json_free(budget);
            return NULL;
        }
        bool ok = trp_truth(pub);
        json_free(pub);
        if (!ok) {
            json_free(dispatch); json_free(ref); json_free(pc); json_free(budget);
            return NULL;
        }
        /* post-publish interrupt: skip the remaining calls */
        irq = trp_attr_req(agent, "_interrupt_requested", exc);
        if (*exc) {
            json_free(dispatch); json_free(ref); json_free(pc); json_free(budget);
            return NULL;
        }
        if (trp_truth(irq) && i < n) {
            a = json_object();
            trp_arg_j(a, "agent", agent);
            json_t *wording = trp_call(io, "interrupt_skip_wording", a, exc);
            if (*exc) {
                json_free(dispatch); json_free(ref); json_free(pc); json_free(budget);
                return NULL;
            }
            char *wstr = trp_pystr(wording);
            char content[512];
            snprintf(content, sizeof(content),
                     "[Tool execution skipped \xE2\x80\x94 {name} was not started. %s]", wstr);
            free(wstr);
            json_free(wording);
            a = json_object();
            rem = json_array();
            for (size_t j = i; j < n; j++)
                json_append(rem, json_copy(tool_calls->c.items[j]));
            trp_arg_j(a, "remaining", rem);
            json_free(rem);
            trp_arg_s(a, "effective_task_id", task_id);
            trp_arg_s(a, "notice", "remaining tool call(s)");
            trp_arg_s(a, "content", content);
            trp_arg_s(a, "flush_stage", "skipped tool result");
            json_t *sr = trp_call(io, "_skip_remaining_sequential", a, exc);
            if (*exc) {
                json_free(dispatch); json_free(ref); json_free(pc); json_free(budget);
                return NULL;
            }
            ok = trp_truth(sr);
            json_free(sr);
            json_free(dispatch); json_free(ref); json_free(pc);
            if (!ok) { json_free(budget); return NULL; }
            break;
        }
        json_free(dispatch); json_free(ref); json_free(pc);
    }
    /* if finalize: _finalize_tool_batch(agent, messages, task_id, len(tool_calls), budget) */
    if (finalize) {
        a = json_object();
        trp_arg_j(a, "messages", messages);
        trp_arg_s(a, "effective_task_id", task_id);
        trp_arg_n(a, "num_tools", (double)n);
        trp_arg_j(a, "budget", budget);
        r = trp_call(io, "_finalize_tool_batch", a, exc);
        if (*exc) { json_free(budget); return NULL; }
        json_free(r);
    }
    json_free(budget);
    return NULL;
}

/* PoP: trp_middleware_kwargs @ agent/tool_executor.py:middleware_kwargs */
json_t *trp_middleware_kwargs(const json_t *in, const trp_io_t *io,
                              json_t *state, json_t **exc) {
    (void)io; (void)state; (void)exc;
    const json_t *ref = json_obj_get(in, "ref");
    json_t *d = json_object();
    json_set(d, "function_name", json_string(json_get_str(ref, "name", "")));
    json_set(d, "function_args", trp_copy_or_null(json_obj_get(ref, "args")));
    json_set(d, "effective_task_id", json_string(json_get_str(ref, "task_id", "")));
    json_set(d, "tool_call_id", json_string(json_get_str(ref, "call_id", "")));
    json_set(d, "middleware_trace", trp_copy_or_null(json_obj_get(ref, "trace")));
    return d;
}

/* PoP: trp_emit_post @ agent/tool_executor.py:emit_post */
json_t *trp_emit_post(const json_t *in, const trp_io_t *io,
                      json_t *state, json_t **exc) {
    (void)state;
    const json_t *ref = json_obj_get(in, "ref");
    const json_t *result = json_obj_get(in, "result");
    const json_t *trace = json_obj_get(in, "trace");
    const json_t *outcome = json_obj_get(in, "outcome");
    json_t *a = json_object();
    trp_arg_s(a, "function_name", json_get_str(ref, "name", ""));
    trp_arg_j(a, "function_args", json_obj_get(ref, "args"));
    trp_arg_j(a, "result", result);
    trp_arg_s(a, "effective_task_id", json_get_str(ref, "task_id", ""));
    trp_arg_s(a, "tool_call_id", json_get_str(ref, "call_id", ""));
    /* middleware_trace=list(self.trace if trace is None else trace) */
    const json_t *mt = trp_is_null(trace) ? json_obj_get(ref, "trace") : trace;
    trp_arg_j(a, "middleware_trace", mt);
    if (trp_is_obj(outcome)) {
        for (size_t i = 0; i < outcome->c.count; i++)
            trp_arg_j(a, outcome->c.keys[i], outcome->c.items[i]);
    }
    json_t *r = trp_call(io, "_emit_terminal_post_tool_call", a, exc);
    if (*exc) return NULL;
    json_free(r);
    return NULL;
}

/* PoP: trp_emit_cancelled @ agent/tool_executor.py:emit_cancelled */
json_t *trp_emit_cancelled(const json_t *in, const trp_io_t *io,
                           json_t *state, json_t **exc) {
    (void)state;
    const json_t *agent = json_obj_get(in, "agent");
    double start_time = json_get_num(in, "start_time", 0.0);
    /* message = f"Tool execution cancelled. {interrupt_skip_wording(agent)}" */
    json_t *a = json_object();
    trp_arg_j(a, "agent", agent);
    json_t *wording = trp_call(io, "interrupt_skip_wording", a, exc);
    if (*exc) return NULL;
    char *wstr = trp_pystr(wording);
    char message[512];
    snprintf(message, sizeof(message), "Tool execution cancelled. %s", wstr);
    free(wstr);
    json_free(wording);
    /* result = json.dumps({"error": message, "status": "cancelled"},
     *     ensure_ascii=False) */
    json_t *d = json_object();
    json_set(d, "error", json_string(message));
    json_set(d, "status", json_string("cancelled"));
    char *result = trp_json_dumps(d);
    json_free(d);
    /* duration_ms kwarg evaluates time.time() before the emit_post call */
    json_t *t = trp_call(io, "time.time", json_object(), exc);
    if (*exc) { free(result); return NULL; }
    if (!trp_is_num(t)) {
        *exc = trp_raise_fmt("TypeError",
                             "unsupported operand type(s) for -: '%s' and '%s'",
                             trp_type_name(t),
                             trp_type_name(json_obj_get(in, "start_time")));
        json_free(t);
        free(result);
        return NULL;
    }
    long long dms = (long long)((trp_num(t, 0.0) - start_time) * 1000.0);
    json_free(t);
    a = json_object();
    trp_arg_s(a, "result", result);
    trp_arg_n(a, "duration_ms", (double)dms);
    trp_arg_s(a, "status", "cancelled");
    trp_arg_s(a, "error_type", "keyboard_interrupt");
    trp_arg_s(a, "error_message", message);
    json_t *r = trp_call(io, "emit_post", a, exc);
    if (*exc) { free(result); return NULL; }
    json_free(r);
    json_t *ret = json_string(result);
    free(result);
    return ret;
}

/* PoP: trp_emit_invalid_arguments @ agent/tool_executor.py:emit_invalid_arguments */
json_t *trp_emit_invalid_arguments(const json_t *in, const trp_io_t *io,
                                   json_t *state, json_t **exc) {
    (void)state;
    const json_t *result = json_obj_get(in, "result");
    json_t *a = json_object();
    trp_arg_j(a, "result", result);
    json_set(a, "trace", json_array());   /* explicit trace=[] */
    trp_arg_s(a, "status", "error");
    trp_arg_s(a, "error_type", "invalid_tool_arguments");
    trp_arg_s(a, "error_message", "Tool arguments must be a valid JSON object");
    json_t *r = trp_call(io, "emit_post", a, exc);
    if (*exc) return NULL;
    json_free(r);
    return NULL;
}

/* PoP: trp_parsed_call_ref @ agent/tool_executor.py:ref */
json_t *trp_parsed_call_ref(const json_t *in, const trp_io_t *io,
                            json_t *state, json_t **exc) {
    (void)state;
    const json_t *pc = json_obj_get(in, "pc");
    const char *task_id = json_get_str(in, "task_id", "");
    /* _ToolCallRef(self.name, self.args, task_id,
     *     _pairing_tool_call_id(self.tool_call), self.middleware_trace) */
    json_t *a = json_object();
    trp_arg_j(a, "tool_call", json_obj_get(pc, "tool_call"));
    json_t *cid = trp_call(io, "_pairing_tool_call_id", a, exc);
    if (*exc) return NULL;
    json_t *ref = json_object();
    json_set(ref, "_type", json_string("_ToolCallRef"));
    json_set(ref, "name", json_string(json_get_str(pc, "name", "")));
    json_set(ref, "args", trp_copy_or_null(json_obj_get(pc, "args")));
    json_set(ref, "task_id", json_string(task_id));
    json_set(ref, "call_id", cid ? cid : json_null());
    json_set(ref, "trace", trp_copy_or_null(json_obj_get(pc, "middleware_trace")));
    return ref;
}

/* PoP: trp_set_worker_activity_callback @ agent/tool_executor.py:_set_worker_activity_callback */
json_t *trp_set_worker_activity_callback(const json_t *in, const trp_io_t *io,
                                         json_t *state, json_t **exc) {
    (void)in;
    json_t *agent = json_obj_get(state, "agent");
    /* with contextlib.suppress(Exception): */
    /* from tools.environments.base import set_activity_callback */
    json_t *r = trp_call(io, "import_set_activity_callback", json_object(), exc);
    if (*exc) {
        if (!trp_swallow(exc)) return NULL;
        return NULL;
    }
    json_free(r);
    /* set_activity_callback(agent._touch_activity) */
    {
        const json_t *cb = trp_attr_req(agent, "_touch_activity", exc);
        if (*exc) {
            if (!trp_swallow(exc)) return NULL;
            return NULL;
        }
        json_t *a = json_object();
        trp_arg_cb(a, "callback", cb);
        r = trp_call(io, "set_activity_callback", a, exc);
        if (*exc) {
            if (!trp_swallow(exc)) return NULL;
            return NULL;
        }
        json_free(r);
    }
    return NULL;
}

/* PoP: trp_run_tool_activity_heartbeat @ agent/tool_executor.py:_run_tool_activity_heartbeat */
json_t *trp_run_tool_activity_heartbeat(const json_t *in, const trp_io_t *io,
                                        json_t *state, json_t **exc) {
    json_t *agent = json_obj_get(state, "agent");
    const json_t *stop_event = json_obj_get(in, "stop_event");
    const char *label = json_get_str(in, "label", "");
    double interval = json_get_num(in, "interval", TRP_TOOL_ACTIVITY_HEARTBEAT_INTERVAL_S);
    const json_t *worker_tid = json_obj_get(in, "worker_tid");
    /* from tools.interrupt import is_thread_interrupted (BEFORE the try) */
    json_t *r = trp_call(io, "import_is_thread_interrupted", json_object(), exc);
    if (*exc) return NULL;
    json_free(r);
    /* try: while not stop_event.wait(interval): ... except Exception: pass */
    for (;;) {
        json_t *a = json_object();
        trp_arg_n(a, "timeout", interval);
        json_t *w = NULL;
        if (trp_is_obj(stop_event) && json_obj_get(stop_event, "__event__")) {
            w = trp_call(io, "event.wait", a, exc);
        } else {
            json_free(a);
            *exc = trp_attr_error(stop_event, "wait");
        }
        if (*exc) {
            if (!trp_swallow(exc)) return NULL;
            return NULL;
        }
        bool stop = trp_truth(w);
        json_free(w);
        if (stop) break;
        /* if is_thread_interrupted(worker_tid): return */
        a = json_object();
        trp_arg_j(a, "tid", worker_tid);
        json_t *it = trp_call(io, "is_thread_interrupted", a, exc);
        if (*exc) {
            if (!trp_swallow(exc)) return NULL;
            return NULL;
        }
        bool interrupted = trp_truth(it);
        json_free(it);
        if (interrupted) return NULL;
        /* agent._touch_activity(label) */
        a = json_object();
        trp_arg_s(a, "label", label);
        json_t *ta = trp_method_call(io, agent, "_touch_activity", a, exc);
        if (*exc) {
            if (!trp_swallow(exc)) return NULL;
            return NULL;
        }
        json_free(ta);
    }
    return NULL;
}

/* lock facade enter/exit seams; non-facade carriers raise the live TypeError. */
static json_t *trp_lock_op(const trp_io_t *io, const json_t *lock,
                           const char *op, json_t **exc) {
    if (trp_is_obj(lock) && json_obj_get(lock, "__lock__")) {
        char name[32];
        snprintf(name, sizeof(name), "lock.%s", op);
        return trp_call(io, name, json_object(), exc);
    }
    *exc = trp_raise_fmt("TypeError",
                         "'%s' object does not support the context manager protocol",
                         trp_type_name(lock));
    return NULL;
}

/* scalar value equality for set membership (fixture domain: int tids). */
static bool trp_scalar_eq(const json_t *a, const json_t *b) {
    if (trp_is_null(a) && trp_is_null(b)) return true;
    if (!a || !b || a->type != b->type) return false;
    switch (a->type) {
        case JSON_BOOL:   return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return trp_str_eq(a->str_val, b->str_val);
        default:          return false;
    }
}

/* worker-thread set facade: seam + items mutation (add iff absent, discard). */
static json_t *trp_wt_set_op(const trp_io_t *io, json_t *agent, const char *op,
                             const json_t *tid, json_t **exc) {
    const json_t *set = trp_attr_req(agent, "_tool_worker_threads", exc);
    if (*exc) return NULL;
    if (!trp_is_obj(set) || !json_obj_get(set, "__set__")) {
        *exc = trp_attr_error(set, op);
        return NULL;
    }
    json_t *a = json_object();
    trp_arg_j(a, "tid", tid);
    char name[40];
    snprintf(name, sizeof(name), "worker_threads.%s", op);
    json_t *r = trp_call(io, name, a, exc);
    if (*exc) return NULL;
    json_t *items = json_obj_get(set, "items");
    if (!trp_is_arr(items)) {
        items = json_array();
        json_set((json_t *)set, "items", items);
    }
    if (trp_str_eq(op, "add")) {
        bool found = false;
        for (size_t i = 0; i < items->c.count; i++) {
            if (trp_scalar_eq(items->c.items[i], tid)) { found = true; break; }
        }
        if (!found) json_append(items, tid ? json_copy(tid) : json_null());
    } else {
        for (size_t i = 0; i < items->c.count; i++) {
            if (trp_scalar_eq(items->c.items[i], tid)) {
                json_array_remove(items, i);
                break;
            }
        }
    }
    return r;
}

/* PoP: trp_registered_tool_worker @ agent/tool_executor.py:_registered_tool_worker */
json_t *trp_registered_tool_worker(const json_t *in, const trp_io_t *io,
                                   json_t *state, json_t **exc) {
    (void)in;
    json_t *agent = json_obj_get(state, "agent");
    /* tid = threading.current_thread().ident */
    json_t *tid = trp_call(io, "current_thread_ident", json_object(), exc);
    if (*exc) return NULL;
    /* with agent._tool_worker_threads_lock: agent._tool_worker_threads.add(tid) */
    const json_t *lock = trp_attr_req(agent, "_tool_worker_threads_lock", exc);
    if (*exc) { json_free(tid); return NULL; }
    json_t *r = trp_lock_op(io, lock, "__enter__", exc);
    if (*exc) { json_free(tid); return NULL; }
    json_free(r);
    json_t *add_exc = NULL;
    r = trp_wt_set_op(io, agent, "add", tid, &add_exc);
    json_free(r);
    /* the with-block __exit__ runs even when add raised */
    r = trp_lock_op(io, lock, "__exit__", exc);
    if (*exc) {
        json_free(add_exc);
        json_free(tid);
        return NULL;   /* exit raise replaces the add raise */
    }
    json_free(r);
    if (add_exc) { *exc = add_exc; json_free(tid); return NULL; }
    /* try: yield tid — the with-body is the scripted "body" seam */
    json_t *a = json_object();
    trp_arg_j(a, "tid", tid);
    json_t *body = trp_call(io, "body", a, exc);
    json_free(body);
    json_t *pending = NULL;
    if (*exc) { pending = *exc; *exc = NULL; }
    /* finally: with lock: discard(tid) ; suppress(Exception): _ra()._set_interrupt(False, tid) */
    {
        json_t *fin_exc = NULL;
        r = trp_lock_op(io, lock, "__enter__", exc);
        if (*exc) { fin_exc = *exc; *exc = NULL; goto fin_done; }
        json_free(r);
        json_t *disc_exc = NULL;
        r = trp_wt_set_op(io, agent, "discard", tid, &disc_exc);
        json_free(r);
        r = trp_lock_op(io, lock, "__exit__", exc);
        if (*exc) {
            json_free(disc_exc);
            fin_exc = *exc; *exc = NULL;
            goto fin_done;
        }
        json_free(r);
        if (disc_exc) { fin_exc = disc_exc; goto fin_done; }
        /* with contextlib.suppress(Exception): _ra()._set_interrupt(False, tid) */
        json_t *ra = trp_call(io, "_ra", json_object(), exc);
        if (*exc) {
            if (!trp_swallow(exc)) { fin_exc = *exc; *exc = NULL; goto fin_done; }
        } else {
            json_free(ra);
            a = json_object();
            trp_arg_b(a, "value", false);
            trp_arg_j(a, "tid", tid);
            json_t *si = trp_call(io, "_set_interrupt", a, exc);
            if (*exc) {
                if (!trp_swallow(exc)) { fin_exc = *exc; *exc = NULL; goto fin_done; }
            } else {
                json_free(si);
            }
        }
    fin_done:
        if (fin_exc) { json_free(pending); pending = fin_exc; }
    }
    if (pending) { *exc = pending; json_free(tid); return NULL; }
    return tid;
}

/* PoP: trp_run_with_activity_heartbeat @ agent/tool_executor.py:_run_with_activity_heartbeat */
json_t *trp_run_with_activity_heartbeat(const json_t *in, const trp_io_t *io,
                                        json_t *state, json_t **exc) {
    json_t *agent = json_obj_get(state, "agent");
    const char *function_name = json_get_str(in, "function_name", "");
    const json_t *fn = json_obj_get(in, "fn");
    /* stop = threading.Event() */
    json_t *stop = trp_call(io, "threading.Event", json_object(), exc);
    if (*exc) return NULL;
    json_free(stop);   /* structural event double */
    /* kwargs worker_tid = threading.current_thread().ident */
    json_t *tid = trp_call(io, "current_thread_ident", json_object(), exc);
    if (*exc) return NULL;
    /* thread = threading.Thread(target=_run_tool_activity_heartbeat,
     *     args=(agent, stop, f"tool running: {function_name}"),
     *     kwargs={"interval": 30.0, "worker_tid": tid}, daemon=True,
     *     name=f"tool-activity-hb-{function_name[:24]}") */
    char label[256];
    snprintf(label, sizeof(label), "tool running: %s", function_name);
    char *prefix = trp_cp_prefix(function_name, 24);
    char tname[300];
    snprintf(tname, sizeof(tname), "tool-activity-hb-%s", prefix ? prefix : "");
    free(prefix);
    json_t *a = json_object();
    trp_arg_s(a, "target", "<callable>");
    json_t *args_arr = json_array();
    json_append(args_arr, agent ? json_copy(agent) : json_null());
    json_t *ev = json_object();
    json_set(ev, "__event__", json_bool(true));
    json_append(args_arr, ev);
    json_append(args_arr, json_string(label));
    json_set(a, "args", args_arr);
    json_t *kw = json_object();
    json_set(kw, "interval", json_number(TRP_TOOL_ACTIVITY_HEARTBEAT_INTERVAL_S));
    json_set(kw, "worker_tid", tid ? tid : json_null());
    json_set(a, "kwargs", kw);
    json_set(a, "daemon", json_bool(true));
    trp_arg_s(a, "name", tname);
    json_t *th = trp_call(io, "threading.Thread", a, exc);
    if (*exc) return NULL;
    json_free(th);   /* structural thread double */
    /* thread.start() */
    json_t *r = trp_call(io, "thread.start", json_object(), exc);
    if (*exc) return NULL;
    json_free(r);
    /* try: return fn() */
    json_t *ret = NULL;
    json_t *pending = NULL;
    {
        json_t *fa = json_object();
        json_t *fr = NULL;
        if (trp_is_obj(fn) && json_obj_get(fn, "__callable__")) {
            const json_t *cb = json_obj_get(fn, "__callable__");
            if (cb->type == JSON_STRING) {
                fr = trp_call(io, cb->str_val, fa, exc);
            } else {
                json_free(fa);
                *exc = trp_raise("AssertionError", "inert callable double invoked");
            }
        } else {
            json_free(fa);
            *exc = trp_raise_fmt("TypeError", "'%s' object is not callable",
                                 trp_type_name(fn));
        }
        if (*exc) { pending = *exc; *exc = NULL; }
        else ret = fr;
    }
    /* finally: stop.set() ; thread.join(timeout=2.0) */
    {
        json_t *sr = trp_call(io, "event.set", json_object(), exc);
        if (*exc) {
            json_free(pending);
            pending = *exc; *exc = NULL;
            goto fin2;
        }
        json_free(sr);
        a = json_object();
        trp_arg_n(a, "timeout", 2.0);
        json_t *jr = trp_call(io, "thread.join", a, exc);
        if (*exc) {
            json_free(pending);
            pending = *exc; *exc = NULL;
            goto fin2;
        }
        json_free(jr);
    }
fin2:
    if (pending) { *exc = pending; json_free(ret); return NULL; }
    return ret;
}