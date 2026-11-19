/*
 * tool_executor_flow.c — execution-decision FLOW cores + leaves of
 * agent/tool_executor.py (LIVE authority): admission/guard chains, approval
 * gate ordering, result projection branches, timeout/error paths.
 *
 * Decision sequences only: every live sibling / hook / clock / callback effect
 * enters through tef_io_t.call (the universal seam). Python truthiness, `or`
 * short-circuits, `is None` vs truthy tests, kwarg evaluation order, dict
 * insertion order, exception catch scopes (Exception only where the live code
 * catches Exception; a scripted BaseException — KeyboardInterrupt / SystemExit /
 * GeneratorExit / CancelledError / _BatchAbandoned — propagates past those
 * guards exactly as live) are transcribed per call site.
 *
 * Value model (shared with tests/oracle_tool_executor_flow.py):
 *   - attribute doubles: {"__attr__": {...}}  <-> types.SimpleNamespace
 *   - callable doubles:  {"__callable__": true|"<seam>"}
 *   - facade doubles:    {"__gate__"}, {"__future__"}, {"__budget__"},
 *                        {"__guardrails__"} (methods enter through seams)
 *   - dataclasses:       {"_type": "_ToolCallRef"|"_ManagedToolResult"|
 *                        "_ParsedCall", ...fields}
 *   - marker strings:    {"_type": "_ToolTimeoutResult"|"_ToolCancelledResult",
 *                        "value": ...}
 *   - frozenset[str]:    sorted, deduped JSON array
 *   - raise specs:       {"__raise__": "<ClassName>", "msg": str(e)}
 *     (KeyError msg carries the str(e) form, e.g. "'error'").
 *
 * Self-contained: libjson only (the gate links lib/libjson/json.o). The
 * harness #includes this .c via the area-qualified "agent/tool_executor_flow.c".
 *
 * Gate: bash tests/run_one_oracle.sh tool_executor_flow \
 *           oracle_tool_executor_flow.py "" "lib/libjson/json.o"
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

/* ── module constants (agent/tool_executor.py + tool_search catalog) ── */
#define TEF_TOOL_CALL_NAME "tool_call"
#define TEF_CONNECTOR_BATCH_SENTINEL "connectors__execute"
#define TEF_START_ORDER_GATE_TIMEOUT_S 120.0
#define TEF_SEQUENTIAL_INTERRUPT_POLL_SECONDS 1.0
#define TEF_PRUNED_TOOL_ARGUMENTS_ERROR "suspected_pruned_tool_arguments"
/* _PRUNED_TOOL_ARGUMENTS_MESSAGE: f-string over
 * _COMPRESSION_MARKER_PREFIX.strip('⟪:') == "HERMES-CONTEXT-COMPRESSION". */
#define TEF_PRUNED_TOOL_ARGUMENTS_MESSAGE \
    "Tool was not executed because effect-capable arguments contain a Hermes " \
    "context-compression artifact. Recover the exact content from its durable " \
    "source or re-read it, then issue a complete new call; do not retry these " \
    "arguments. To remove a marker that already landed in a file, match it by " \
    "its HERMES-CONTEXT-COMPRESSION prefix (e.g. a terminal sed on that line) " \
    "instead of quoting the full marker."

/* ── universal seam ── */
typedef struct tef_io {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} tef_io_t;

/* ── JSON / Python-value helpers ─────────────────────────────────────────── */

static bool tef_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool tef_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool tef_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool tef_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool tef_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }

static const char *tef_strval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

/* Python truthiness (NULL pointer == None). */
static bool tef_truth(const json_t *v) {
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

static double tef_num(const json_t *v, double def) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : def;
}

static bool tef_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}

/* Python type name of a fixture value (for TypeError/AttributeError texts).
 * Integral JSON numbers materialize as int live (json.loads), non-integral as
 * float; fixtures keep integral floats out of type-name-sensitive positions. */
static const char *tef_type_name(const json_t *v) {
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
            return json_obj_get(v, "__attr__") ? "types.SimpleNamespace" : "dict";
    }
    return "object";
}

/* getattr double access: {"__attr__": {...}} is a SimpleNamespace; any other
 * value has no attributes (getattr default applies). */
static const json_t *tef_getattr(const json_t *v, const char *name) {
    if (!tef_is_obj(v)) return NULL;
    const json_t *inner = json_obj_get(v, "__attr__");
    if (!inner || inner->type != JSON_OBJECT) return NULL;
    return json_obj_get(inner, name);
}

/* setattr on an attribute double: live `obj.name = v` writes INSIDE the
 * {"__attr__": {...}} wrapper (SimpleNamespace); any other carrier raises
 * AttributeError exactly as live (dict/None/list have no settable attrs).
 * Takes ownership of val. */
static json_t *tef_attr_error(const json_t *v, const char *name);
static void tef_setattr(json_t *v, const char *name, json_t *val, json_t **exc) {
    json_t *inner = tef_is_obj(v) ? json_obj_get(v, "__attr__") : NULL;
    if (!inner || inner->type != JSON_OBJECT) {
        json_free(val);
        *exc = tef_attr_error(v, name);
        return;
    }
    json_set(inner, name, val);
}

/* ── raise specs ── */

static json_t *tef_raise(const char *cls, const char *msg) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(cls));
    json_set(e, "msg", json_string(msg ? msg : ""));
    return e;
}

static json_t *tef_raise_fmt(const char *cls, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return tef_raise(cls, buf);
}

/* AttributeError for a required attribute read (live `obj.name`). */
static json_t *tef_attr_error(const json_t *v, const char *name) {
    return tef_raise_fmt("AttributeError", "'%s' object has no attribute '%s'",
                         tef_type_name(v), name);
}

/* BaseException-only classes: `except Exception` does NOT catch these live. */
static bool tef_is_base_only(const char *cls) {
    static const char *const base_only[] = {
        "CancelledError", "SystemExit", "KeyboardInterrupt", "GeneratorExit",
        "_BatchAbandoned",
    };
    for (size_t i = 0; i < sizeof(base_only) / sizeof(base_only[0]); i++)
        if (tef_str_eq(cls, base_only[i])) return true;
    return false;
}

/* Universal seam call: frees args, returns the NEW scripted result (NULL ==
 * None) or transfers a raise-spec into *exc. */
static json_t *tef_call(const tef_io_t *io, const char *name, json_t *args,
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
 * BaseException-only classes are NOT caught by `except Exception` live, so
 * they propagate: returns false and leaves *exc set. */
static bool tef_swallow(json_t **exc) {
    if (!*exc) return true;
    if (tef_is_base_only(json_get_str(*exc, "__raise__", ""))) return false;
    json_free(*exc); *exc = NULL;
    return true;
}

/* ── args builders (None-valued keys dropped on both sides) ── */

static void tef_arg_s(json_t *a, const char *k, const char *v) {
    if (v) json_set(a, k, json_string(v));
}
static void tef_arg_j(json_t *a, const char *k, const json_t *v) {
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void tef_arg_b(json_t *a, const char *k, bool v) {
    json_set(a, k, json_bool(v));
}
static void tef_arg_n(json_t *a, const char *k, double v) {
    json_set(a, k, json_number(v));
}
/* Callable projection: a callable double becomes "<callable>"; other non-None
 * values pass through; None drops the key. */
static void tef_arg_cb(json_t *a, const char *k, const json_t *v) {
    if (tef_is_null(v)) return;
    if (tef_is_obj(v) && json_obj_get(v, "__callable__"))
        json_set(a, k, json_string("<callable>"));
    else
        json_set(a, k, json_copy(v));
}

/* ── scalar str() / float repr ── */

static void tef_float_repr(double v, char *out, size_t cap) {
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(out, cap, "%.*g", prec, v);
        if (strtod(out, NULL) == v) return;
    }
    snprintf(out, cap, "%.17g", v);
}

/* Python str() of a scalar; containers fall back to JSON serialization
 * (outside the fixture domain of the str()-using leaves). malloc'd. */
static char *tef_pystr(const json_t *v) {
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
                tef_float_repr(d, buf, sizeof(buf));
            return strdup(buf);
        }
        default: {
            char *s = json_serialize(v);
            return s ? s : strdup("");
        }
    }
}

/* ── UTF-8 codepoint helpers (Python str len/slice domain) ── */

static size_t tef_cp_len(const char *s) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}

/* malloc'd prefix of n codepoints (n clamped to the string length). */
static char *tef_cp_prefix(const char *s, size_t n) {
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

/* text[:limit] — Python slice semantics for a possibly-negative stop. */
static char *tef_slice_limit(const char *s, long long limit) {
    size_t len = tef_cp_len(s);
    long long end = limit;
    if (end < 0) end += (long long)len;
    if (end < 0) end = 0;
    if ((size_t)end > len) end = (long long)len;
    return tef_cp_prefix(s, (size_t)end);
}

/* ── str.strip() / str.lower() (fixture domain: ASCII + Latin-1 + the full
 * Unicode whitespace set for strip) ── */

static bool tef_is_ws_cp(uint32_t cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F))
        return true;
    if (cp == 0x85 || cp == 0xA0 || cp == 0x1680) return true;
    if (cp >= 0x2000 && cp <= 0x200A) return true;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000)
        return true;
    return false;
}

/* Decode one codepoint; advances *p. */
static uint32_t tef_cp_next(const unsigned char **p) {
    const unsigned char *s = *p;
    uint32_t cp = s[0];
    if (cp < 0x80) { *p = s + 1; return cp; }
    if ((cp & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        *p = s + 2; return ((cp & 0x1F) << 6) | (s[1] & 0x3F);
    }
    if ((cp & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        *p = s + 3;
        return ((cp & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    }
    if ((cp & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80
        && (s[3] & 0xC0) == 0x80) {
        *p = s + 4;
        return ((cp & 0x07) << 18) | ((s[1] & 0x3F) << 12)
             | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
    }
    *p = s + 1;
    return cp;
}

static char *tef_strip(const char *s) {
    if (!s) s = "";
    const unsigned char *p = (const unsigned char *)s;
    const unsigned char *start = p;
    while (*p) {
        const unsigned char *q = p;
        uint32_t cp = tef_cp_next(&q);
        if (!tef_is_ws_cp(cp)) break;
        p = q;
    }
    start = p;
    const unsigned char *end = p + strlen((const char *)p);
    while (end > start) {
        /* walk back one codepoint */
        const unsigned char *q = end - 1;
        while (q > start && (*q & 0xC0) == 0x80) q--;
        const unsigned char *r = q;
        uint32_t cp = tef_cp_next(&r);
        if (r != end || !tef_is_ws_cp(cp)) break;
        end = q;
    }
    size_t n = (size_t)(end - start);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, start, n);
    out[n] = '\0';
    return out;
}

/* lower() for ASCII + Latin-1 supplement (fixture domain). malloc'd. */
static char *tef_lower(const char *s) {
    char *out = strdup(s ? s : "");
    if (!out) return NULL;
    unsigned char *p = (unsigned char *)out;
    while (*p) {
        if (*p < 0x80) { *p = (unsigned char)tolower(*p); p++; continue; }
        if (p[0] == 0xC3 && p[1] >= 0x80 && p[1] <= 0x9E && p[1] != 0x97) {
            p[1] = (unsigned char)(p[1] + 0x20);   /* À..Þ except × */
            p += 2;
            continue;
        }
        p++;
        while ((*p & 0xC0) == 0x80) p++;
    }
    return out;
}

/* ── Python json.dumps(ensure_ascii=False, default separators) ──
 * libjson's serializer is compact and %.17g; the live result strings use
 * Python's ", " / ": " separators and shortest float repr, so transcribe. */

typedef struct { char *p; size_t n, cap; } tef_buf_t;
static void tef_buf_init(tef_buf_t *b) { b->p = NULL; b->n = 0; b->cap = 0; }
static void tef_buf_add(tef_buf_t *b, const char *s, size_t n) {
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
static void tef_buf_adds(tef_buf_t *b, const char *s) { tef_buf_add(b, s, strlen(s)); }
static void tef_buf_free(tef_buf_t *b) { free(b->p); b->p = NULL; b->n = b->cap = 0; }

static void tef_dumps_str(tef_buf_t *b, const char *s) {
    tef_buf_adds(b, "\"");
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        unsigned char c = *p;
        switch (c) {
            case '"':  tef_buf_adds(b, "\\\""); break;
            case '\\': tef_buf_adds(b, "\\\\"); break;
            case '\b': tef_buf_adds(b, "\\b"); break;
            case '\f': tef_buf_adds(b, "\\f"); break;
            case '\n': tef_buf_adds(b, "\\n"); break;
            case '\r': tef_buf_adds(b, "\\r"); break;
            case '\t': tef_buf_adds(b, "\\t"); break;
            default:
                if (c < 0x20) {
                    char esc[8];
                    snprintf(esc, sizeof(esc), "\\u%04x", c);
                    tef_buf_adds(b, esc);
                } else {
                    tef_buf_add(b, (const char *)&c, 1);
                }
        }
    }
    tef_buf_adds(b, "\"");
}

static void tef_dumps_val(tef_buf_t *b, const json_t *v) {
    if (!v || v->type == JSON_NULL) { tef_buf_adds(b, "null"); return; }
    switch (v->type) {
        case JSON_BOOL: tef_buf_adds(b, v->bool_val ? "true" : "false"); break;
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && fabs(d) < 9.2e18)
                snprintf(buf, sizeof(buf), "%lld", (long long)d);
            else
                tef_float_repr(d, buf, sizeof(buf));
            tef_buf_adds(b, buf);
            break;
        }
        case JSON_STRING: tef_dumps_str(b, v->str_val); break;
        case JSON_ARRAY:
            tef_buf_adds(b, "[");
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) tef_buf_adds(b, ", ");
                tef_dumps_val(b, v->c.items[i]);
            }
            tef_buf_adds(b, "]");
            break;
        case JSON_OBJECT:
            tef_buf_adds(b, "{");
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) tef_buf_adds(b, ", ");
                tef_dumps_str(b, v->c.keys[i]);
                tef_buf_adds(b, ": ");
                tef_dumps_val(b, v->c.items[i]);
            }
            tef_buf_adds(b, "}");
            break;
        default: tef_buf_adds(b, "null");
    }
}

static char *tef_json_dumps(const json_t *v) {
    tef_buf_t b; tef_buf_init(&b);
    tef_dumps_val(&b, v);
    char *out = b.p ? b.p : strdup("null");
    return out;
}

/* Scalar value equality (replaced != text); containers compare serialized. */
static bool tef_json_eq(const json_t *a, const json_t *b) {
    if (tef_is_null(a) && tef_is_null(b)) return true;
    if (!a || !b || a->type != b->type) return false;
    switch (a->type) {
        case JSON_BOOL:   return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return tef_str_eq(a->str_val, b->str_val);
        default: {
            char *sa = json_serialize(a), *sb = json_serialize(b);
            bool eq = tef_str_eq(sa, sb);
            free(sa); free(sb);
            return eq;
        }
    }
}

/* frozenset[str] normalization: sorted, deduped array. */
static json_t *tef_frozenset_norm(const json_t *v) {
    json_t *out = json_array();
    if (!tef_is_arr(v)) return out;
    for (size_t i = 0; i < v->c.count; i++) {
        char *s = tef_pystr(v->c.items[i]);
        bool dup = false;
        for (size_t j = 0; j < out->c.count; j++) {
            if (tef_str_eq(out->c.items[j]->str_val, s)) { dup = true; break; }
        }
        if (!dup) json_append(out, json_string(s ? s : ""));
        free(s);
    }
    /* insertion sort by strcmp */
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

/* ── tuple unpack with live error texts ── */

/* Validate ret as an n-tuple; on mismatch sets *exc exactly as live unpack.
 * Returns the array (borrowed) or NULL. str/dict iterate chars/keys live. */
static const json_t *tef_unpack(const json_t *ret, size_t n, json_t **exc) {
    if (tef_is_arr(ret)) {
        if (ret->c.count < n) {
            *exc = tef_raise_fmt("ValueError",
                "not enough values to unpack (expected %zu, got %zu)", n, ret->c.count);
            return NULL;
        }
        if (ret->c.count > n) {
            *exc = tef_raise_fmt("ValueError",
                "too many values to unpack (expected %zu, got %zu)", n, ret->c.count);
            return NULL;
        }
        return ret;
    }
    if (tef_is_null(ret) || tef_is_num(ret) || (ret && ret->type == JSON_BOOL)) {
        *exc = tef_raise_fmt("TypeError", "cannot unpack non-iterable %s object",
                             tef_type_name(ret));
        return NULL;
    }
    /* str / dict / other containers: outside the fixture domain; treat as a
     * length mismatch so the error class stays ValueError/TypeError-live. */
    *exc = tef_raise_fmt("TypeError", "cannot unpack non-iterable %s object",
                         tef_type_name(ret));
    return NULL;
}

/* ── _ToolCallRef.emit_post transcription ──
 * _emit_terminal_post_tool_call(agent, function_name=..., function_args=...,
 *     result=..., effective_task_id=..., tool_call_id=...,
 *     middleware_trace=list(trace), **outcome) */
static void tef_emit_post(const tef_io_t *io, const json_t *ref,
                          const json_t *result, const json_t *outcome,
                          json_t **exc) {
    json_t *a = json_object();
    tef_arg_j(a, "function_name", json_obj_get(ref, "name"));
    tef_arg_j(a, "function_args", json_obj_get(ref, "args"));
    tef_arg_j(a, "result", result);
    tef_arg_j(a, "effective_task_id", json_obj_get(ref, "task_id"));
    tef_arg_j(a, "tool_call_id", json_obj_get(ref, "call_id"));
    tef_arg_j(a, "middleware_trace", json_obj_get(ref, "trace"));
    if (tef_is_obj(outcome)) {
        for (size_t i = 0; i < outcome->c.count; i++)
            tef_arg_j(a, outcome->c.keys[i], outcome->c.items[i]);
    }
    json_t *r = tef_call(io, "_emit_terminal_post_tool_call", a, exc);
    json_free(r);
}

/* ── 1. _tc_name ────────────────────────────────────────────────────────────
 *   return getattr(getattr(tool_call, "function", None), "name", "") or "tool"
 */
/* PoP: tef_tc_name @ agent/tool_executor.py:_tc_name */
json_t *tef_tc_name(const json_t *in, const tef_io_t *io, json_t *state, json_t **exc) {
    (void)io; (void)state; (void)exc;
    const json_t *tc = json_obj_get(in, "tool_call");
    const json_t *fn = tef_getattr(tc, "function");
    const json_t *name = tef_getattr(fn, "name");
    /* getattr(..., "name", "") default; `or "tool"` on the result. */
    if (tef_truth(name)) return json_copy(name);
    return json_string("tool");
}

/* ── 2. _tool_progress_enabled ──────────────────────────────────────────────
 *   return not agent.quiet_mode and getattr(agent, "tool_progress_mode", "all") != "off"
 */
/* PoP: tef_tool_progress_enabled @ agent/tool_executor.py:_tool_progress_enabled */
json_t *tef_tool_progress_enabled(const json_t *in, const tef_io_t *io, json_t *state, json_t **exc) {
    (void)io; (void)state;
    const json_t *agent = json_obj_get(in, "agent");
    const json_t *quiet = tef_getattr(agent, "quiet_mode");
    if (!quiet) { *exc = tef_attr_error(agent, "quiet_mode"); return NULL; }
    if (tef_truth(quiet)) return json_bool(false);   /* short-circuit */
    const json_t *mode = tef_getattr(agent, "tool_progress_mode");
    const char *m = tef_strval(mode);
    if (!mode || mode->type == JSON_NULL) m = "all";   /* getattr default */
    return json_bool(!tef_str_eq(m, "off"));
}

/* ── 3. _preview ────────────────────────────────────────────────────────────
 *   return text[:limit] + "..." if len(text) > limit else text
 */
/* PoP: tef_preview @ agent/tool_executor.py:_preview */
json_t *tef_preview(const json_t *in, const tef_io_t *io, json_t *state, json_t **exc) {
    (void)io; (void)state;
    const json_t *text = json_obj_get(in, "text");
    const json_t *limit = json_obj_get(in, "limit");
    if (!tef_is_str(text)) {
        *exc = tef_raise_fmt("TypeError", "object of type '%s' has no len()",
                             tef_type_name(text));
        return NULL;
    }
    long long lim;
    if (limit && limit->type == JSON_BOOL) {
        lim = limit->bool_val ? 1 : 0;   /* bool is an int subclass live */
    } else if (tef_is_num(limit)) {
        lim = (long long)limit->num_val;
    } else {
        /* live evaluates len(text) > limit FIRST: the comparison raises. */
        *exc = tef_raise_fmt("TypeError",
            "'>' not supported between instances of 'int' and '%s'",
            tef_type_name(limit));
        return NULL;
    }
    size_t len = tef_cp_len(text->str_val);
    if ((long long)len > lim) {
        char *pre = tef_slice_limit(text->str_val, lim);
        size_t n = strlen(pre ? pre : "");
        char *joined = (char *)malloc(n + 4);
        if (joined) { memcpy(joined, pre ? pre : "", n); memcpy(joined + n, "...", 4); }
        free(pre);
        json_t *s = json_string(joined ? joined : "...");
        free(joined);
        return s;
    }
    return json_copy(text);
}

/* ── 4. _start_order_gate_timeout ───────────────────────────────────────────
 *   if batch_timeout is None: return _START_ORDER_GATE_TIMEOUT_S
 *   return min(_START_ORDER_GATE_TIMEOUT_S, batch_timeout / 2)
 */
/* PoP: tef_start_order_gate_timeout @ agent/tool_executor.py:_start_order_gate_timeout */
json_t *tef_start_order_gate_timeout(const json_t *in, const tef_io_t *io, json_t *state, json_t **exc) {
    (void)io; (void)state; (void)exc;
    const json_t *bt = json_obj_get(in, "batch_timeout");
    if (tef_is_null(bt)) return json_number(TEF_START_ORDER_GATE_TIMEOUT_S);
    double half = tef_num(bt, 0.0) / 2.0;
    double r = half < TEF_START_ORDER_GATE_TIMEOUT_S ? half : TEF_START_ORDER_GATE_TIMEOUT_S;
    return json_number(r);
}

/* ── 5. _delegate_spinner_label ─────────────────────────────────────────────
 *   action = str(function_args.get("action") or "").strip().lower()
 *   tasks = function_args.get("tasks")
 *   if action in ("list", "steer", "stop"): return f"🔀 subagent {action}"
 *   if tasks and isinstance(tasks, list):
 *       return f"🔀 delegating {len(tasks)} tasks · (/agents to monitor)"
 *   goal_preview = (function_args.get("goal") or "")[:30]
 *   return f"🔀 {goal_preview} · (/agents to monitor)" if goal_preview else \
 *          "🔀 delegating · (/agents to monitor)"
 */
/* PoP: tef_delegate_spinner_label @ agent/tool_executor.py:_delegate_spinner_label */
json_t *tef_delegate_spinner_label(const json_t *in, const tef_io_t *io, json_t *state, json_t **exc) {
    (void)io; (void)state;
    const json_t *fa = json_obj_get(in, "function_args");
    const json_t *action_v = tef_is_obj(fa) ? json_obj_get(fa, "action") : NULL;
    char *action = NULL;
    {
        char *s = tef_truth(action_v) ? tef_pystr(action_v) : strdup("");
        char *st = tef_strip(s);
        free(s);
        action = tef_lower(st);
        free(st);
    }
    if (tef_str_eq(action, "list") || tef_str_eq(action, "steer") || tef_str_eq(action, "stop")) {
        char buf[128];
        snprintf(buf, sizeof(buf), "🔀 subagent %s", action);
        free(action);
        return json_string(buf);
    }
    free(action);
    const json_t *tasks = tef_is_obj(fa) ? json_obj_get(fa, "tasks") : NULL;
    if (tef_truth(tasks) && tef_is_arr(tasks)) {
        char buf[160];
        snprintf(buf, sizeof(buf), "🔀 delegating %zu tasks · (/agents to monitor)", tasks->c.count);
        return json_string(buf);
    }
    const json_t *goal = tef_is_obj(fa) ? json_obj_get(fa, "goal") : NULL;
    const json_t *gv = tef_truth(goal) ? goal : NULL;   /* (goal or "") */
    if (gv && !tef_is_str(gv)) {
        *exc = tef_raise_fmt("TypeError", "'%s' object is not subscriptable",
                             tef_type_name(gv));
        return NULL;
    }
    char *preview = tef_slice_limit(gv ? gv->str_val : "", 30);
    json_t *ret;
    if (preview && preview[0]) {
        size_t n = strlen(preview);
        char *buf = (char *)malloc(n + 64);
        if (buf) snprintf(buf, n + 64, "🔀 %s · (/agents to monitor)", preview);
        ret = json_string(buf ? buf : "");
        free(buf);
    } else {
        ret = json_string("🔀 delegating · (/agents to monitor)");
    }
    free(preview);
    return ret;
}

/* ── 6. _safe_callback ──────────────────────────────────────────────────────
 *   if not callback: return
 *   try: callback(*args, **kwargs)
 *   except Exception as callback_error:
 *       logging.debug("%s callback error: %s", label, callback_error)
 */
/* PoP: tef_safe_callback @ agent/tool_executor.py:_safe_callback */
json_t *tef_safe_callback(const json_t *in, const tef_io_t *io, json_t *state, json_t **exc) {
    (void)state;
    const json_t *cb = json_obj_get(in, "callback");
    if (!tef_truth(cb)) return NULL;
    const char *label = json_get_str(in, "label", "");
    json_t *call_exc = NULL;
    if (tef_is_obj(cb) && json_obj_get(cb, "__callable__")) {
        json_t *a = json_object();
        tef_arg_j(a, "args", json_obj_get(in, "args"));
        tef_arg_j(a, "kwargs", json_obj_get(in, "kwargs"));
        json_t *r = tef_call(io, "callback", a, &call_exc);
        json_free(r);
    } else {
        /* truthy non-callable: the call itself raises TypeError, caught by
         * the same except Exception. */
        call_exc = tef_raise_fmt("TypeError", "'%s' object is not callable",
                                 tef_type_name(cb));
    }
    if (call_exc) {
        if (tef_is_base_only(json_get_str(call_exc, "__raise__", ""))) {
            *exc = call_exc;   /* BaseException propagates past except Exception */
            return NULL;
        }
        /* logging.debug("%s callback error: %s", label, callback_error) —
         * the exception instance normalizes to its raise spec in the args. */
        json_t *la = json_object();
        tef_arg_s(la, "fmt", "%s callback error: %s");
        json_t *arr = json_array();
        json_append(arr, json_string(label));
        json_append(arr, call_exc);   /* adopted */
        json_set(la, "args", arr);
        json_t *lr = tef_call(io, "logging.debug", la, exc);
        json_free(lr);
        if (*exc) return NULL;
    }
    return NULL;
}

/* ── 7. _interrupt_worker_tids ──────────────────────────────────────────────
 *   kwargs = {} if reason is _NO_REASON else {"reason": reason}
 *   for tid in tids:
 *       with contextlib.suppress(Exception):
 *           _ra()._set_interrupt(True, tid, **kwargs)
 */
/* PoP: tef_interrupt_worker_tids @ agent/tool_executor.py:_interrupt_worker_tids */
json_t *tef_interrupt_worker_tids(const json_t *in, const tef_io_t *io, json_t *state, json_t **exc) {
    (void)state;
    const json_t *tids = json_obj_get(in, "tids");
    bool has_reason = tef_is_obj(in) && json_obj_get(in, "reason") != NULL;
    const json_t *reason = has_reason ? json_obj_get(in, "reason") : NULL;
    if (!tef_is_arr(tids)) return NULL;
    for (size_t i = 0; i < tids->c.count; i++) {
        json_t *ra = tef_call(io, "_ra", json_object(), exc);
        json_free(ra);
        if (*exc) {
            if (!tef_swallow(exc)) return NULL;
            continue;
        }
        json_t *a = json_object();
        tef_arg_b(a, "value", true);
        tef_arg_j(a, "tid", tids->c.items[i]);
        if (has_reason) tef_arg_j(a, "reason", reason);
        json_t *r = tef_call(io, "_set_interrupt", a, exc);
        json_free(r);
        if (*exc) {
            if (!tef_swallow(exc)) return NULL;
            continue;
        }
    }
    return NULL;
}

/* ── 8. _blocked_tool_result ────────────────────────────────────────────────
 *   if block_body is not None:
 *       result = json.dumps(block_body, ensure_ascii=False)
 *       error_type, error_message = block_error_type, block_body.get("message") or block_body["error"]
 *   else:
 *       result = agent._guardrail_block_result(guardrail_decision)
 *       error_type = "guardrail_block"
 *       error_message = getattr(guardrail_decision, "message", None) or "Tool blocked by guardrail policy"
 *   ref.emit_post(agent, result, status="blocked", error_type=..., error_message=...)
 *   return result
 */
/* PoP: tef_blocked_tool_result @ agent/tool_executor.py:_blocked_tool_result */
json_t *tef_blocked_tool_result(const json_t *in, const tef_io_t *io, json_t *state, json_t **exc) {
    (void)state;
    const json_t *ref = json_obj_get(in, "ref");
    const json_t *block_body = json_obj_get(in, "block_body");
    const json_t *decision = json_obj_get(in, "guardrail_decision");
    json_t *result = NULL;   /* owned */
    const char *error_type = NULL;
    json_t *error_message = NULL;   /* owned */
    if (!tef_is_null(block_body)) {
        if (!tef_is_obj(block_body)) {
            *exc = tef_attr_error(block_body, "get");
            return NULL;
        }
        char *s = tef_json_dumps(block_body);
        result = json_string(s ? s : "");
        free(s);
        error_type = json_get_str(in, "block_error_type", "");
        const json_t *msg = json_obj_get(block_body, "message");
        if (tef_truth(msg)) {
            error_message = json_copy(msg);
        } else {
            const json_t *err = json_obj_get(block_body, "error");
            if (!err) {
                json_free(result);
                *exc = tef_raise("KeyError", "'error'");
                return NULL;
            }
            error_message = json_copy(err);
        }
    } else {
        json_t *a = json_object();
        tef_arg_j(a, "decision", decision);
        result = tef_call(io, "guardrail_block_result", a, exc);
        if (*exc) { json_free(result); return NULL; }
        if (!result) result = json_null();
        error_type = "guardrail_block";
        const json_t *dm = tef_getattr(decision, "message");
        if (tef_truth(dm)) error_message = json_copy(dm);
        else error_message = json_string("Tool blocked by guardrail policy");
    }
    json_t *outcome = json_object();
    tef_arg_s(outcome, "status", "blocked");
    tef_arg_s(outcome, "error_type", error_type);
    tef_arg_j(outcome, "error_message", error_message);
    json_free(error_message);
    tef_emit_post(io, ref, result, outcome, exc);
    json_free(outcome);
    if (*exc) { json_free(result); return NULL; }
    return result;
}

/* ── 9. _pre_tool_block ─────────────────────────────────────────────────────
 *   try:
 *       from hermes_cli.plugins import _dispatch_pre_tool_call_hooks
 *       block_msg, modified_args = _dispatch_pre_tool_call_hooks(
 *           ref.name, ref.args, **tool_hook_ids(agent, ref.task_id, ref.call_id),
 *           middleware_trace=list(ref.trace))
 *       return block_msg, (ref.args if modified_args is None else modified_args)
 *   except Exception:
 *       return None, ref.args
 */
/* PoP: tef_pre_tool_block @ agent/tool_executor.py:_pre_tool_block */
json_t *tef_pre_tool_block(const json_t *in, const tef_io_t *io, json_t *state, json_t **exc) {
    (void)state;
    const json_t *ref = json_obj_get(in, "ref");
    const json_t *ref_args = json_obj_get(ref, "args");
    json_t *fallback = json_array();
    json_append(fallback, json_null());
    json_append(fallback, ref_args ? json_copy(ref_args) : json_object());

    json_t *imp = tef_call(io, "import_dispatch_pre_tool_call_hooks", json_object(), exc);
    json_free(imp);
    if (*exc) {
        if (!tef_swallow(exc)) { json_free(fallback); return NULL; }
        return fallback;
    }
    json_t *ha = json_object();
    tef_arg_j(ha, "task_id", json_obj_get(ref, "task_id"));
    tef_arg_j(ha, "call_id", json_obj_get(ref, "call_id"));
    json_t *hook_ids = tef_call(io, "tool_hook_ids", ha, exc);
    if (*exc) {
        json_free(hook_ids);
        if (!tef_swallow(exc)) { json_free(fallback); return NULL; }
        return fallback;
    }
    if (!tef_is_obj(hook_ids)) {
        /* ** unpack of a non-mapping raises TypeError inside the try;
         * except Exception returns (None, ref.args). */
        json_free(hook_ids);
        return fallback;
    }
    json_t *a = json_object();
    tef_arg_j(a, "name", json_obj_get(ref, "name"));
    tef_arg_j(a, "args", ref_args);
    for (size_t i = 0; i < hook_ids->c.count; i++)
        tef_arg_j(a, hook_ids->c.keys[i], hook_ids->c.items[i]);
    tef_arg_j(a, "middleware_trace", json_obj_get(ref, "trace"));
    json_free(hook_ids);
    json_t *r = tef_call(io, "_dispatch_pre_tool_call_hooks", a, exc);
    if (*exc) {
        json_free(r);
        if (!tef_swallow(exc)) { json_free(fallback); return NULL; }
        return fallback;
    }
    const json_t *tup = tef_unpack(r, 2, exc);
    if (!tup) {
        json_free(r);
        if (!tef_swallow(exc)) { json_free(fallback); return NULL; }
        return fallback;
    }
    json_t *ret = json_array();
    json_append(ret, json_copy(tup->c.items[0]));
    const json_t *modified = tup->c.items[1];
    if (tef_is_null(modified))
        json_append(ret, ref_args ? json_copy(ref_args) : json_object());
    else
        json_append(ret, json_copy(modified));
    json_free(r);
    json_free(fallback);
    return ret;
}

/* ── 10. _unwrap_tool_search_call ───────────────────────────────────────────
 *   scope_block = None
 *   try:
 *       from tools import tool_search as _ts
 *       if function_name != _ts.TOOL_CALL_NAME: return name, args, None
 *       underlying, underlying_args, err = _ts.resolve_underlying_call(function_args)
 *       if err or not underlying: return name, args, None
 *       if underlying == _ts.CONNECTOR_BATCH_SENTINEL: return name, args, None
 *       if underlying not in _tool_search_scoped_names(agent):
 *           return name, args, _ts.scope_block_reason(underlying)
 *       scope_block = _ts.validate_deferred_call_args(underlying, underlying_args)
 *       if scope_block is None: return underlying, underlying_args, None
 *       if flatten_probe:
 *           probe = json.loads(scope_block)
 *           scope_block = (f"{probe.get('error','')} Parameters schema: "
 *                          f"{json.dumps(probe.get('parameters',{}), ensure_ascii=False)}. "
 *                          f"{probe.get('hint','')}").strip()
 *   except Exception: pass
 *   return function_name, function_args, scope_block
 */
/* PoP: tef_unwrap_tool_search_call @ agent/tool_executor.py:_unwrap_tool_search_call */
json_t *tef_unwrap_tool_search_call(const json_t *in, const tef_io_t *io, json_t *state, json_t **exc) {
    (void)state;
    const json_t *agent = json_obj_get(in, "agent");
    const json_t *fname = json_obj_get(in, "function_name");
    const json_t *fargs = json_obj_get(in, "function_args");
    bool flatten = tef_truth(json_obj_get(in, "flatten_probe"));
    json_t *scope_block = NULL;   /* owned; None until validate assigns */
    json_t *ret = NULL;

    json_t *imp = tef_call(io, "import_tool_search", json_object(), exc);
    json_free(imp);
    if (*exc) {
        if (!tef_swallow(exc)) return NULL;
        goto fallback;
    }
    if (!tef_str_eq(tef_strval(fname), TEF_TOOL_CALL_NAME)) {
        ret = json_array();
        json_append(ret, fname ? json_copy(fname) : json_null());
        json_append(ret, fargs ? json_copy(fargs) : json_object());
        json_append(ret, json_null());
        return ret;
    }
    {
        json_t *a = json_object();
        tef_arg_j(a, "function_args", fargs);
        json_t *r = tef_call(io, "resolve_underlying_call", a, exc);
        if (*exc) { json_free(r); if (!tef_swallow(exc)) return NULL; goto fallback; }
        const json_t *tup = tef_unpack(r, 3, exc);
        if (!tup) { json_free(r); if (!tef_swallow(exc)) return NULL; goto fallback; }
        const json_t *underlying = tup->c.items[0];
        const json_t *underlying_args = tup->c.items[1];
        const json_t *err = tup->c.items[2];
        if (tef_truth(err) || !tef_truth(underlying)) {
            json_free(r);
            ret = json_array();
            json_append(ret, fname ? json_copy(fname) : json_null());
            json_append(ret, fargs ? json_copy(fargs) : json_object());
            json_append(ret, json_null());
            return ret;
        }
        if (tef_is_str(underlying) && tef_str_eq(underlying->str_val, TEF_CONNECTOR_BATCH_SENTINEL)) {
            json_free(r);
            ret = json_array();
            json_append(ret, fname ? json_copy(fname) : json_null());
            json_append(ret, fargs ? json_copy(fargs) : json_object());
            json_append(ret, json_null());
            return ret;
        }
        json_t *sa = json_object();
        tef_arg_j(sa, "agent", agent);
        json_t *names = tef_call(io, "_tool_search_scoped_names", sa, exc);
        if (*exc) { json_free(r); json_free(names); if (!tef_swallow(exc)) return NULL; goto fallback; }
        bool in_scope = false;
        if (tef_is_arr(names)) {
            for (size_t i = 0; i < names->c.count; i++) {
                if (tef_json_eq(names->c.items[i], underlying)) { in_scope = true; break; }
            }
        } else if (!tef_is_null(names)) {
            /* membership on a non-container raises TypeError inside the try */
            json_free(r); json_free(names);
            goto fallback;
        } else {
            /* `underlying not in None` -> TypeError caught */
            json_free(r); json_free(names);
            goto fallback;
        }
        json_free(names);
        if (!in_scope) {
            json_t *ba = json_object();
            tef_arg_j(ba, "underlying", underlying);
            json_t *reason = tef_call(io, "scope_block_reason", ba, exc);
            if (*exc) { json_free(r); json_free(reason); if (!tef_swallow(exc)) return NULL; goto fallback; }
            json_free(r);
            ret = json_array();
            json_append(ret, fname ? json_copy(fname) : json_null());
            json_append(ret, fargs ? json_copy(fargs) : json_object());
            json_append(ret, reason ? reason : json_null());
            return ret;
        }
        json_t *va = json_object();
        tef_arg_j(va, "underlying", underlying);
        tef_arg_j(va, "underlying_args", underlying_args);
        json_t *vb = tef_call(io, "validate_deferred_call_args", va, exc);
        if (*exc) { json_free(r); json_free(vb); if (!tef_swallow(exc)) return NULL; goto fallback; }
        json_free(scope_block);
        scope_block = vb;   /* may be None */
        if (tef_is_null(scope_block)) {
            /* Copy the borrowed tuple children BEFORE freeing r. */
            ret = json_array();
            json_append(ret, json_copy(underlying));
            json_append(ret, json_copy(underlying_args));
            json_append(ret, json_null());
            json_free(r);
            return ret;
        }
        if (flatten) {
            /* json.loads(scope_block) — non-str or bad JSON raises inside try */
            json_t *probe = NULL;
            if (tef_is_str(scope_block)) {
                char *perr = NULL;
                probe = json_parse(scope_block->str_val, &perr);
                free(perr);
            }
            if (!probe || !tef_is_obj(probe)) {
                json_free(probe);
                goto fallback;   /* scope_block keeps the raw validate value */
            }
            const json_t *perr_v = json_obj_get(probe, "error");
            const json_t *pparams = json_obj_get(probe, "parameters");
            const json_t *phint = json_obj_get(probe, "hint");
            char *e = perr_v ? tef_pystr(perr_v) : strdup("");
            char *h = phint ? tef_pystr(phint) : strdup("");
            json_t *empty = json_object();
            char *params = tef_json_dumps(pparams ? pparams : empty);
            json_free(empty);
            tef_buf_t b; tef_buf_init(&b);
            tef_buf_adds(&b, e ? e : "");
            tef_buf_adds(&b, " Parameters schema: ");
            tef_buf_adds(&b, params ? params : "{}");
            tef_buf_adds(&b, ". ");
            tef_buf_adds(&b, h ? h : "");
            char *stripped = tef_strip(b.p ? b.p : "");
            tef_buf_free(&b);
            free(e); free(h); free(params);
            json_free(probe);
            json_free(scope_block);
            scope_block = json_string(stripped ? stripped : "");
            free(stripped);
        }
        json_free(r);
    }
fallback:
    ret = json_array();
    json_append(ret, fname ? json_copy(fname) : json_null());
    json_append(ret, fargs ? json_copy(fargs) : json_object());
    json_append(ret, scope_block ? scope_block : json_null());
    return ret;
}

/* ── 11. _parse_tool_call ───────────────────────────────────────────────────
 *   name = _canonical_tool_name(tool_call.function.name)
 *   args, parse_error = _parse_tool_arguments(tool_call.function.arguments)
 *   scope_block = None
 *   if parse_error is None:
 *       name, args, scope_block = _unwrap_tool_search_call(agent, name, args, flatten_probe=flatten_probe)
 *   return _ParsedCall(tool_call, name, args, [], parse_error, scope_block)
 */
/* PoP: tef_parse_tool_call @ agent/tool_executor.py:_parse_tool_call */
json_t *tef_parse_tool_call(const json_t *in, const tef_io_t *io, json_t *state, json_t **exc) {
    (void)state;
    const json_t *tc = json_obj_get(in, "tool_call");
    bool flatten = tef_truth(json_obj_get(in, "flatten_probe"));
    const json_t *fn = tef_getattr(tc, "function");
    if (!fn) { *exc = tef_attr_error(tc, "function"); return NULL; }
    const json_t *raw_name = tef_getattr(fn, "name");
    if (!raw_name) { *exc = tef_attr_error(fn, "name"); return NULL; }
    json_t *ca = json_object();
    tef_arg_j(ca, "function_name", raw_name);
    json_t *name = tef_call(io, "_canonical_tool_name", ca, exc);
    if (*exc) { json_free(name); return NULL; }
    const json_t *raw_args = tef_getattr(fn, "arguments");
    if (!raw_args) { json_free(name); *exc = tef_attr_error(fn, "arguments"); return NULL; }
    json_t *pa = json_object();
    tef_arg_j(pa, "raw_arguments", raw_args);
    json_t *pr = tef_call(io, "_parse_tool_arguments", pa, exc);
    if (*exc) { json_free(name); json_free(pr); return NULL; }
    const json_t *ptup = tef_unpack(pr, 2, exc);
    if (!ptup) { json_free(name); json_free(pr); return NULL; }
    json_t *args = json_copy(ptup->c.items[0]);
    json_t *parse_error = json_copy(ptup->c.items[1]);
    json_free(pr);
    json_t *scope_block = json_null();
    if (tef_is_null(parse_error)) {
        json_t *ua = json_object();
        tef_arg_j(ua, "function_name", name);
        tef_arg_j(ua, "function_args", args);
        tef_arg_b(ua, "flatten_probe", flatten);
        json_t *ur = tef_call(io, "_unwrap_tool_search_call", ua, exc);
        if (*exc) {
            json_free(name); json_free(args); json_free(parse_error);
            json_free(scope_block); json_free(ur);
            return NULL;
        }
        const json_t *utup = tef_unpack(ur, 3, exc);
        if (!utup) {
            json_free(name); json_free(args); json_free(parse_error);
            json_free(scope_block); json_free(ur);
            return NULL;
        }
        json_free(name); json_free(args); json_free(scope_block);
        name = json_copy(utup->c.items[0]);
        args = json_copy(utup->c.items[1]);
        scope_block = json_copy(utup->c.items[2]);
        json_free(ur);
    }
    json_t *ret = json_object();
    json_set(ret, "_type", json_string("_ParsedCall"));
    json_set(ret, "tool_call", tc ? json_copy(tc) : json_null());
    json_set(ret, "name", name ? name : json_null());
    json_set(ret, "args", args ? args : json_object());
    json_set(ret, "middleware_trace", json_array());
    json_set(ret, "parse_error", parse_error ? parse_error : json_null());
    json_set(ret, "scope_block", scope_block ? scope_block : json_null());
    return ret;
}

/* ── 12. _dispatch_authorized_once ──────────────────────────────────────────
 * Hermes policy (scope → plugin pre-hooks → pruned-arg check → guardrails)
 * then the one real dispatch; begin_execution advanced exactly once on every
 * path. Transcribed seam-by-seam; see the live body for the commentary.
 */
/* PoP: tef_dispatch_authorized_once @ agent/tool_executor.py:_dispatch_authorized_once */
json_t *tef_dispatch_authorized_once(const json_t *in, const tef_io_t *io, json_t *state, json_t **exc) {
    json_t *agent = json_obj_get(state, "agent");
    json_t *managed = json_obj_get(state, "state");
    json_t *ref = json_obj_get(state, "ref");
    const json_t *scope_block = json_obj_get(in, "scope_block");
    const json_t *begin = json_obj_get(in, "begin_execution");
    const json_t *gate = json_obj_get(in, "authorization_gate");
    bool has_begin = !tef_is_null(begin);
    bool has_gate = !tef_is_null(gate);

    /* from tools.hosted_tool_policy import hosted_tool_refusal (no try: a
     * raise here propagates). */
    json_t *imp = tef_call(io, "import_hosted_tool_policy", json_object(), exc);
    json_free(imp);
    if (*exc) return NULL;

    json_t *ha = json_object();
    tef_arg_j(ha, "name", json_obj_get(ref, "name"));
    json_t *block_message = tef_call(io, "hosted_tool_refusal", ha, exc);
    if (*exc) { json_free(block_message); return NULL; }
    const char *block_error_type = "hosted_tool_refused";
    if (tef_is_null(block_message)) {
        json_free(block_message);
        block_message = scope_block ? json_copy(scope_block) : json_null();
        block_error_type = "tool_scope_block";
    }
    if (tef_is_null(block_message)) {
        block_error_type = "plugin_block";
        json_t *r;
        if (has_gate) {
            /* authorization_gate.run(resolve) — the harness seam fires the
             * resolve callback (_pre_tool_block) after recording gate.run. */
            json_t *ga = json_object();
            tef_arg_s(ga, "callback", "<callable>");
            r = tef_call(io, "gate.run", ga, exc);
        } else {
            json_t *pa = json_object();
            tef_arg_j(pa, "agent", agent);
            tef_arg_j(pa, "ref", ref);
            r = tef_call(io, "_pre_tool_block", pa, exc);
        }
        if (*exc) { json_free(block_message); json_free(r); return NULL; }
        const json_t *tup = tef_unpack(r, 2, exc);
        if (!tup) { json_free(block_message); json_free(r); return NULL; }
        json_free(block_message);
        block_message = json_copy(tup->c.items[0]);
        json_set(ref, "args", json_copy(tup->c.items[1]));
        json_free(r);
        json_set(managed, "args", json_copy(json_obj_get(ref, "args")));
    }
    json_t *block_body = NULL;
    if (!tef_is_null(block_message)) {
        block_body = json_object();
        json_set(block_body, "error", json_copy(block_message));
    }
    json_free(block_message);

    if (!block_body) {
        json_t *pa = json_object();
        tef_arg_j(pa, "name", json_obj_get(ref, "name"));
        tef_arg_j(pa, "args", json_obj_get(ref, "args"));
        json_t *pruned = tef_call(io, "_context_pruned_argument_paths", pa, exc);
        if (*exc) { json_free(pruned); return NULL; }
        if (tef_truth(pruned)) {
            block_body = json_object();
            json_set(block_body, "error", json_string(TEF_PRUNED_TOOL_ARGUMENTS_ERROR));
            json_set(block_body, "message", json_string(TEF_PRUNED_TOOL_ARGUMENTS_MESSAGE));
            json_set(block_body, "argument_paths", pruned);
            pruned = NULL;
            block_error_type = TEF_PRUNED_TOOL_ARGUMENTS_ERROR;
        }
        json_free(pruned);
    }

    json_t *decision = NULL;
    if (!block_body) {
        json_t *ga = json_object();
        tef_arg_j(ga, "name", json_obj_get(ref, "name"));
        tef_arg_j(ga, "args", json_obj_get(ref, "args"));
        decision = tef_call(io, "guardrails.before_call", ga, exc);
        if (*exc) { json_free(decision); return NULL; }
        const json_t *allows = tef_getattr(decision, "allows_execution");
        if (!allows) {
            *exc = tef_attr_error(decision, "allows_execution");
            json_free(decision);
            return NULL;
        }
        if (tef_truth(allows)) { json_free(decision); decision = NULL; }
    }

    if (block_body || decision) {
        /* _advance_start_order() — no callback. */
        if (has_begin) {
            json_t *r = tef_call(io, "begin_execution", json_object(), exc);
            json_free(r);
            if (*exc) { json_free(block_body); json_free(decision); return NULL; }
        }
        json_set(managed, "blocked", json_bool(true));
        json_t *ba = json_object();
        tef_arg_j(ba, "ref", ref);
        tef_arg_j(ba, "block_body", block_body);
        tef_arg_s(ba, "block_error_type", block_error_type);
        tef_arg_j(ba, "guardrail_decision", decision);
        json_t *result = tef_call(io, "_blocked_tool_result", ba, exc);
        json_free(block_body); json_free(decision);
        if (*exc) { json_free(result); return NULL; }
        return result;
    }

    {
        const char *name = json_get_str(ref, "name", "");
        if (tef_str_eq(name, "memory"))
            tef_setattr(agent, "_turns_since_memory", json_number(0), exc);
        else if (tef_str_eq(name, "skill_manage"))
            tef_setattr(agent, "_iters_since_skill", json_number(0), exc);
        if (*exc) return NULL;
    }

    imp = tef_call(io, "import_terminal_approval_batch", json_object(), exc);
    json_free(imp);
    if (*exc) return NULL;
    json_t *pta = json_object();
    tef_arg_j(pta, "ref", ref);
    json_t *pt = tef_call(io, "prepare_current_terminal", pta, exc);
    json_free(pt);
    if (*exc) return NULL;

    /* _advance_start_order(lambda: _begin_tool_execution(agent, ref, display_index)) */
    if (has_begin) {
        json_t *ba = json_object();
        tef_arg_s(ba, "callback", "<callable>");
        json_t *r = tef_call(io, "begin_execution", ba, exc);
        json_free(r);
        if (*exc) return NULL;
        /* the harness begin_execution seam fired _begin_tool_execution */
    } else {
        json_t *ba = json_object();
        tef_arg_j(ba, "ref", ref);
        tef_arg_j(ba, "display_index", json_obj_get(in, "display_index"));
        json_t *r = tef_call(io, "_begin_tool_execution", ba, exc);
        json_free(r);
        if (*exc) return NULL;
    }

    imp = tef_call(io, "import_tool_progress_relay", json_object(), exc);
    json_free(imp);
    if (*exc) return NULL;
    imp = tef_call(io, "import_data_use", json_object(), exc);
    json_free(imp);
    if (*exc) return NULL;
    imp = tef_call(io, "import_mcp_elicitation_form", json_object(), exc);
    json_free(imp);
    if (*exc) return NULL;

    /* with bound_tool_progress(...), collect_data_use() as data_use,
     *      bound_form_asker(...): try: execute finally: data_use */
    unsigned entered = 0;
    json_t *data_use_ret = NULL;
    json_t *ret = NULL;
    {
        const json_t *tpc = tef_getattr(agent, "tool_progress_callback");
        if (!tpc) { *exc = tef_attr_error(agent, "tool_progress_callback"); return NULL; }
        json_t *ca = json_object();
        tef_arg_cb(ca, "callback", tpc);
        tef_arg_j(ca, "name", json_obj_get(ref, "name"));
        tef_arg_j(ca, "call_id", json_obj_get(ref, "call_id"));
        json_t *r = tef_call(io, "bound_tool_progress", ca, exc);
        json_free(r);
        if (*exc) return NULL;
        entered |= 1u;
    }
    {
        json_t *r = tef_call(io, "collect_data_use", json_object(), exc);
        if (*exc) { json_free(r); goto cm_exits; }
        data_use_ret = r;
        entered |= 2u;
    }
    {
        json_t *ca = json_object();
        tef_arg_cb(ca, "callback", tef_getattr(agent, "clarify_callback"));
        json_t *r = tef_call(io, "bound_form_asker", ca, exc);
        json_free(r);
        if (*exc) goto cm_exits;
        entered |= 4u;
    }
    {
        json_t *hba = json_object();
        tef_arg_j(hba, "name", json_obj_get(ref, "name"));
        ret = tef_call(io, "run_with_activity_heartbeat", hba, exc);
        /* the harness heartbeat seam fired execute(ref.args) */
    }
    /* finally: state.data_use = frozenset(data_use) */
    if (tef_is_null(data_use_ret)) {
        json_free(*exc);
        *exc = tef_raise("TypeError", "'NoneType' object is not iterable");
        json_free(ret);
        ret = NULL;
    } else {
        json_set(managed, "data_use", tef_frozenset_norm(data_use_ret));
    }
cm_exits:
    /* reverse-order __exit__ for every entered context manager; an exit
     * raise replaces the in-flight exception (live unwinding semantics). */
    {
        static const char *const exit_names[3] = {
            "bound_tool_progress.__exit__", "collect_data_use.__exit__",
            "bound_form_asker.__exit__",
        };
        /* bit order == enter order; exit runs 2 -> 0 (reverse of enter). */
        for (int bit = 2; bit >= 0; bit--) {
            if (!(entered & (1u << bit))) continue;
            json_t *xexc = NULL;
            json_t *r = tef_call(io, exit_names[bit], json_object(), &xexc);
            json_free(r);
            if (xexc) {
                json_free(*exc); *exc = xexc;
                json_free(ret); ret = NULL;   /* the return is discarded */
            }
        }
    }
    json_free(data_use_ret);
    return ret;
}

/* ── 13. _poll_sequential_future ────────────────────────────────────────────
 *   _last_heartbeat = 0
 *   while True:
 *       wait_slice = _SEQUENTIAL_INTERRUPT_POLL_SECONDS
 *       if deadline is not None:
 *           remaining = deadline + authorization_gate.excluded_seconds() - time.monotonic()
 *           if remaining <= 0: return "timeout", None
 *           wait_slice = min(wait_slice, remaining)
 *       try: return "done", future.result(timeout=wait_slice)
 *       except concurrent.futures.TimeoutError:
 *           if future.done(): return "done", future.result()
 *           if agent._interrupt_requested: return "interrupted", None
 *           elapsed = int(time.monotonic() - started)
 *           if elapsed - _last_heartbeat >= 30:
 *               _last_heartbeat = elapsed
 *               agent._touch_activity(f"sequential tool running ({elapsed}s): {function_name}")
 */
/* PoP: tef_poll_sequential_future @ agent/tool_executor.py:_poll_sequential_future */
json_t *tef_poll_sequential_future(const json_t *in, const tef_io_t *io, json_t *state, json_t **exc) {
    (void)state;
    const json_t *agent = json_obj_get(in, "agent");
    const json_t *fname = json_obj_get(in, "function_name");
    const json_t *deadline = json_obj_get(in, "deadline");
    double started = tef_num(json_obj_get(in, "started"), 0.0);
    bool has_deadline = !tef_is_null(deadline);
    double dl = tef_num(deadline, 0.0);
    long long last_heartbeat = 0;
    for (;;) {
        double wait_slice = TEF_SEQUENTIAL_INTERRUPT_POLL_SECONDS;
        if (has_deadline) {
            json_t *ex = tef_call(io, "gate.excluded_seconds", json_object(), exc);
            if (*exc) { json_free(ex); return NULL; }
            double excluded = tef_num(ex, 0.0);
            json_free(ex);
            json_t *mo = tef_call(io, "time.monotonic", json_object(), exc);
            if (*exc) { json_free(mo); return NULL; }
            double now = tef_num(mo, 0.0);
            json_free(mo);
            double remaining = dl + excluded - now;
            if (remaining <= 0) {
                json_t *ret = json_array();
                json_append(ret, json_string("timeout"));
                json_append(ret, json_null());
                return ret;
            }
            if (remaining < wait_slice) wait_slice = remaining;
        }
        json_t *fa = json_object();
        tef_arg_n(fa, "timeout", wait_slice);
        json_t *r = tef_call(io, "future.result", fa, exc);
        if (!*exc) {
            json_t *ret = json_array();
            json_append(ret, json_string("done"));
            json_append(ret, r ? r : json_null());
            return ret;
        }
        const char *cls = json_get_str(*exc, "__raise__", "");
        if (!tef_str_eq(cls, "TimeoutError")) return NULL;   /* propagate */
        json_free(*exc); *exc = NULL;
        json_free(r);
        json_t *dn = tef_call(io, "future.done", json_object(), exc);
        if (*exc) { json_free(dn); return NULL; }
        bool done = tef_truth(dn);
        json_free(dn);
        if (done) {
            json_t *r2 = tef_call(io, "future.result", json_object(), exc);
            if (*exc) { json_free(r2); return NULL; }
            json_t *ret = json_array();
            json_append(ret, json_string("done"));
            json_append(ret, r2 ? r2 : json_null());
            return ret;
        }
        const json_t *irq = tef_getattr(agent, "_interrupt_requested");
        if (!irq) { *exc = tef_attr_error(agent, "_interrupt_requested"); return NULL; }
        if (tef_truth(irq)) {
            json_t *ret = json_array();
            json_append(ret, json_string("interrupted"));
            json_append(ret, json_null());
            return ret;
        }
        json_t *mo2 = tef_call(io, "time.monotonic", json_object(), exc);
        if (*exc) { json_free(mo2); return NULL; }
        double now2 = tef_num(mo2, 0.0);
        json_free(mo2);
        long long elapsed = (long long)(now2 - started);   /* int(): toward zero */
        if (elapsed - last_heartbeat >= 30) {
            last_heartbeat = elapsed;
            char label[512];
            snprintf(label, sizeof(label), "sequential tool running (%llds): %s",
                     elapsed, tef_strval(fname) ? tef_strval(fname) : "");
            json_t *ta = json_object();
            tef_arg_s(ta, "label", label);
            json_t *tr = tef_call(io, "_touch_activity", ta, exc);
            json_free(tr);
            if (*exc) return NULL;
        }
    }
}

/* ── 14. _unfinished_tool_result ────────────────────────────────────────────
 *   timed_out / interrupt / thread-missing projection + terminal emit_post.
 */
/* PoP: tef_unfinished_tool_result @ agent/tool_executor.py:_unfinished_tool_result */
json_t *tef_unfinished_tool_result(const json_t *in, const tef_io_t *io, json_t *state, json_t **exc) {
    (void)state;
    const json_t *agent = json_obj_get(in, "agent");
    const json_t *ref = json_obj_get(in, "ref");
    bool timed_out = tef_truth(json_obj_get(in, "timed_out"));
    const json_t *timeout_s = json_obj_get(in, "timeout_s");
    const char *name = json_get_str(ref, "name", "");
    char *function_result = NULL;
    json_t *outcome = json_object();
    double tool_duration = 0.0;
    const char *effect_disposition = NULL;
    char buf[1024];

    if (timed_out) {
        char suffix[64];
        if (!tef_is_null(timeout_s))
            snprintf(suffix, sizeof(suffix), "%.1fs", tef_num(timeout_s, 0.0));
        else
            snprintf(suffix, sizeof(suffix), "the configured timeout");
        snprintf(buf, sizeof(buf), "Error executing tool '%s': timed out after %s", name, suffix);
        function_result = strdup(buf);
        double ts = tef_truth(timeout_s) ? tef_num(timeout_s, 0.0) : 0.0;
        tef_arg_n(outcome, "duration_ms", (double)(long long)(ts * 1000.0));
        tef_arg_s(outcome, "status", "timeout");
        tef_arg_s(outcome, "error_type", "tool_timeout");
        tef_arg_s(outcome, "error_message", function_result);
        tool_duration = ts;
        effect_disposition = "unknown";
    } else {
        const json_t *irq = tef_getattr(agent, "_interrupt_requested");
        if (!irq) { json_free(outcome); *exc = tef_attr_error(agent, "_interrupt_requested"); return NULL; }
        if (tef_truth(irq)) {
            json_t *wa = json_object();
            tef_arg_j(wa, "agent", agent);
            json_t *why_j = tef_call(io, "interrupt_skip_wording", wa, exc);
            if (*exc) { json_free(outcome); json_free(why_j); return NULL; }
            char *why = tef_pystr(why_j);
            json_free(why_j);
            snprintf(buf, sizeof(buf), "[Tool execution cancelled — %s was skipped. %s]", name, why);
            function_result = strdup(buf);
            tef_arg_s(outcome, "status", "cancelled");
            tef_arg_s(outcome, "error_type", "keyboard_interrupt");
            snprintf(buf, sizeof(buf), "Tool execution cancelled. %s", why);
            tef_arg_s(outcome, "error_message", buf);
            free(why);
            tool_duration = 0.0;
            effect_disposition = NULL;
        } else {
            snprintf(buf, sizeof(buf), "Error executing tool '%s': thread did not return a result", name);
            function_result = strdup(buf);
            tef_arg_s(outcome, "status", "error");
            tef_arg_s(outcome, "error_type", "thread_missing_result");
            tef_arg_s(outcome, "error_message", function_result);
            tool_duration = 0.0;
            effect_disposition = NULL;
        }
    }
    json_t *result_j = json_string(function_result ? function_result : "");
    tef_emit_post(io, ref, result_j, outcome, exc);
    json_free(outcome);
    if (*exc) { json_free(result_j); free(function_result); return NULL; }
    json_t *ret = json_array();
    json_append(ret, result_j);
    json_append(ret, json_number(tool_duration));
    json_append(ret, effect_disposition ? json_string(effect_disposition) : json_null());
    free(function_result);
    return ret;
}

/* ── 15. _abandoned_sequential_result ───────────────────────────────────────
 *   ref.emit_post(agent, message, **outcome)
 *   return _ManagedToolResult(result=result_cls(message), args=ref.args,
 *       middleware_trace=ref.trace, blocked=False, dispatched=True)
 */
/* PoP: tef_abandoned_sequential_result @ agent/tool_executor.py:_abandoned_sequential_result */
json_t *tef_abandoned_sequential_result(const json_t *in, const tef_io_t *io, json_t *state, json_t **exc) {
    (void)state;
    const json_t *ref = json_obj_get(in, "ref");
    const json_t *message = json_obj_get(in, "message");
    const char *result_cls = json_get_str(in, "result_cls", "_ToolTimeoutResult");
    const json_t *outcome = json_obj_get(in, "outcome");
    tef_emit_post(io, ref, message, outcome, exc);
    if (*exc) return NULL;
    json_t *marker = json_object();
    json_set(marker, "_type", json_string(result_cls));
    json_set(marker, "value", message ? json_copy(message) : json_null());
    json_t *ret = json_object();
    json_set(ret, "_type", json_string("_ManagedToolResult"));
    json_set(ret, "result", marker);
    json_set(ret, "args", json_copy(json_obj_get(ref, "args")));
    json_set(ret, "middleware_trace", json_copy(json_obj_get(ref, "trace")));
    json_set(ret, "blocked", json_bool(false));
    json_set(ret, "dispatched", json_bool(true));
    json_set(ret, "data_use", json_array());
    return ret;
}

/* ── 16. _persist_multimodal_text_parts ─────────────────────────────────────
 *   spill oversized TEXT parts; one spill file per part; summary reuse.
 */
/* PoP: tef_persist_multimodal_text_parts @ agent/tool_executor.py:_persist_multimodal_text_parts */
json_t *tef_persist_multimodal_text_parts(const json_t *in, const tef_io_t *io, json_t *state, json_t **exc) {
    (void)state;
    const json_t *result = json_obj_get(in, "result");
    const json_t *tool_name = json_obj_get(in, "tool_name");
    const json_t *tool_call_id = json_obj_get(in, "tool_call_id");
    const json_t *env = json_obj_get(in, "env");
    const json_t *budget = json_obj_get(in, "budget");
    const json_t *content = tef_is_obj(result) ? json_obj_get(result, "content") : NULL;
    const json_t *parts = tef_truth(content) ? content : NULL;   /* or [] */
    if (parts && !tef_is_arr(parts)) {
        if (tef_is_num(parts) || parts->type == JSON_BOOL) {
            *exc = tef_raise_fmt("TypeError", "'%s' object is not iterable",
                                 tef_type_name(parts));
            return NULL;
        }
        /* str/dict iterate non-dict items: no text part can spill. */
        return result ? json_copy(result) : json_null();
    }
    json_t *bounded_parts = json_array();
    json_t *first_replacement = NULL;   /* owned; None until a replacement */
    long long spilled = 0;
    if (parts) {
        for (size_t i = 0; i < parts->c.count; i++) {
            const json_t *part = parts->c.items[i];
            const json_t *text = NULL;
            if (tef_is_obj(part)) {
                const json_t *ptype = json_obj_get(part, "type");
                if (tef_is_str(ptype) && tef_str_eq(ptype->str_val, "text"))
                    text = json_obj_get(part, "text");
            }
            if (tef_is_str(text)) {
                char part_id[512];
                /* tool_call_id is a STRING node: read str_val directly
                 * (json_get_str is an object+key accessor). */
                const char *tcid = tef_strval(tool_call_id);
                if (!tcid) tcid = "";
                if (spilled == 0)
                    snprintf(part_id, sizeof(part_id), "%s", tcid);
                else
                    snprintf(part_id, sizeof(part_id), "%s_part%lld", tcid, spilled);
                json_t *ma = json_object();
                tef_arg_j(ma, "content", text);
                tef_arg_j(ma, "tool_name", tool_name);
                tef_arg_s(ma, "tool_use_id", part_id);
                tef_arg_j(ma, "env", env);
                tef_arg_j(ma, "config", budget);
                json_t *replaced = tef_call(io, "maybe_persist_tool_result", ma, exc);
                if (*exc) {
                    json_free(replaced); json_free(bounded_parts);
                    json_free(first_replacement);
                    return NULL;
                }
                if (!tef_json_eq(replaced, text)) {
                    json_t *np = json_copy(part);
                    json_set(np, "text", replaced ? replaced : json_null());
                    replaced = NULL;
                    part = np;
                    if (!tef_truth(first_replacement)) {
                        json_free(first_replacement);
                        first_replacement = json_copy(json_obj_get(np, "text"));
                    }
                    spilled++;
                    json_append(bounded_parts, np);
                    continue;
                }
                json_free(replaced);
            }
            json_append(bounded_parts, json_copy(part));
        }
    }
    if (tef_is_null(first_replacement)) {
        json_free(bounded_parts);
        json_free(first_replacement);
        return result ? json_copy(result) : json_null();
    }
    json_t *bounded = result ? json_copy(result) : json_object();
    json_set(bounded, "content", bounded_parts);
    const json_t *summary = json_obj_get(bounded, "text_summary");
    if (tef_is_str(summary)) {
        json_t *ta = json_object();
        tef_arg_j(ta, "tool_name", tool_name);
        json_t *th = tef_call(io, "budget.resolve_threshold", ta, exc);
        if (*exc) {
            json_free(th); json_free(bounded); json_free(first_replacement);
            return NULL;
        }
        double threshold = tef_num(th, 0.0);
        json_free(th);
        if ((double)tef_cp_len(summary->str_val) > threshold)
            json_set(bounded, "text_summary", json_copy(first_replacement));
    }
    json_free(first_replacement);
    return bounded;
}

/* ── 17. _emit_tool_complete_and_risk ───────────────────────────────────────
 *   if not blocked and agent.tool_complete_callback:
 *       try: display_args = _redact_tool_args_for_display(ref.name, ref.args) or ref.args
 *       except Exception as cb_err: logging.debug("Tool complete callback error: %s", cb_err)
 *       else: _safe_callback(agent.tool_complete_callback, "Tool complete",
 *                            ref.call_id, ref.name, display_args, result)
 *   if risk_metadata is not None and risk_metadata.get("risk") != "low":
 *       _safe_callback(agent.tool_progress_callback, "Tool output risk",
 *                      "tool.output_risk", ref.name, None, None,
 *                      tool_call_id=ref.call_id, risk_metadata=risk_metadata)
 */
/* PoP: tef_emit_tool_complete_and_risk @ agent/tool_executor.py:_emit_tool_complete_and_risk */
json_t *tef_emit_tool_complete_and_risk(const json_t *in, const tef_io_t *io, json_t *state, json_t **exc) {
    (void)state;
    const json_t *agent = json_obj_get(in, "agent");
    const json_t *ref = json_obj_get(in, "ref");
    const json_t *result = json_obj_get(in, "result");
    const json_t *risk = json_obj_get(in, "risk_metadata");
    bool blocked = tef_truth(json_obj_get(in, "blocked"));

    if (!blocked) {
        const json_t *cb = tef_getattr(agent, "tool_complete_callback");
        if (!cb) { *exc = tef_attr_error(agent, "tool_complete_callback"); return NULL; }
        if (tef_truth(cb)) {
            json_t *ra = json_object();
            tef_arg_j(ra, "name", json_obj_get(ref, "name"));
            tef_arg_j(ra, "args", json_obj_get(ref, "args"));
            json_t *redacted = tef_call(io, "_redact_tool_args_for_display", ra, exc);
            if (*exc) {
                if (tef_is_base_only(json_get_str(*exc, "__raise__", ""))) {
                    json_free(redacted);
                    return NULL;   /* BaseException propagates */
                }
                json_t *spec = *exc;
                *exc = NULL;
                /* logging.debug("Tool complete callback error: %s", cb_err) */
                json_t *la = json_object();
                tef_arg_s(la, "fmt", "Tool complete callback error: %s");
                json_t *arr = json_array();
                json_append(arr, spec);   /* adopted */
                json_set(la, "args", arr);
                json_t *lr = tef_call(io, "logging.debug", la, exc);
                json_free(lr);
                json_free(redacted);
                if (*exc) return NULL;
            } else {
                const json_t *display_args = tef_truth(redacted) ? redacted : json_obj_get(ref, "args");
                json_t *sa = json_object();
                tef_arg_cb(sa, "callback", cb);
                tef_arg_s(sa, "label", "Tool complete");
                json_t *arr = json_array();
                json_append(arr, json_copy(json_obj_get(ref, "call_id")));
                json_append(arr, json_copy(json_obj_get(ref, "name")));
                json_append(arr, display_args ? json_copy(display_args) : json_null());
                json_append(arr, result ? json_copy(result) : json_null());
                json_set(sa, "args", arr);
                json_set(sa, "kwargs", json_object());
                json_t *sr = tef_call(io, "_safe_callback", sa, exc);
                json_free(sr);
                json_free(redacted);
                if (*exc) return NULL;
            }
        }
    }
    if (!tef_is_null(risk)) {
        if (!tef_is_obj(risk)) {
            *exc = tef_attr_error(risk, "get");
            return NULL;
        }
        const json_t *rv = json_obj_get(risk, "risk");
        if (!tef_is_str(rv) || !tef_str_eq(rv->str_val, "low")) {
            const json_t *pcb = tef_getattr(agent, "tool_progress_callback");
            if (!pcb) { *exc = tef_attr_error(agent, "tool_progress_callback"); return NULL; }
            json_t *sa = json_object();
            tef_arg_cb(sa, "callback", pcb);
            tef_arg_s(sa, "label", "Tool output risk");
            json_t *arr = json_array();
            json_append(arr, json_string("tool.output_risk"));
            json_append(arr, json_copy(json_obj_get(ref, "name")));
            json_append(arr, json_null());
            json_append(arr, json_null());
            json_set(sa, "args", arr);
            json_t *kw = json_object();
            tef_arg_j(kw, "tool_call_id", json_obj_get(ref, "call_id"));
            tef_arg_j(kw, "risk_metadata", risk);
            json_set(sa, "kwargs", kw);
            json_t *sr = tef_call(io, "_safe_callback", sa, exc);
            json_free(sr);
            if (*exc) return NULL;
        }
    }
    return NULL;
}

/* ── 18. _WorkerStartOnce.advance ───────────────────────────────────────────
 *   if self._advanced: return
 *   self._advanced = True
 *   if not self._gate.begin_in_order(self._order, callback, tool_name=self._tool_name):
 *       raise _BatchAbandoned(self._tool_name)
 */
/* PoP: tef_worker_start_once_advance @ agent/tool_executor.py:advance */
json_t *tef_worker_start_once_advance(const json_t *in, const tef_io_t *io, json_t *state, json_t **exc) {
    if (tef_truth(json_obj_get(state, "_advanced"))) return NULL;
    json_set(state, "_advanced", json_bool(true));
    json_t *a = json_object();
    tef_arg_j(a, "order", json_obj_get(state, "_order"));
    const json_t *cb = json_obj_get(in, "callback");
    if (!tef_is_null(cb)) tef_arg_s(a, "callback", "<callable>");
    tef_arg_j(a, "tool_name", json_obj_get(state, "_tool_name"));
    json_t *r = tef_call(io, "gate.begin_in_order", a, exc);
    if (*exc) { json_free(r); return NULL; }
    bool ok = tef_truth(r);
    json_free(r);
    if (!ok) {
        *exc = tef_raise("_BatchAbandoned", json_get_str(state, "_tool_name", ""));
        return NULL;
    }
    return NULL;
}