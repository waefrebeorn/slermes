/* ===========================================================================
 * tool_guardrails_flow3.c -- FLOW port of the UNCLAIMED remainder of
 * agent/tool_guardrails.py.
 *
 * CRLF-aware PoP grep over src/lib/include (the scanner credits the DEFINING
 * module) shows these claimed against agent/tool_guardrails.py:
 *   _as_bool, _build_result_reference_stub, _check_loop_cap, _coerce_args,
 *   _is_idempotent, _positive_int (stale alias of _int_at_least), _result_hash,
 *   _sha256, _subagent_spawn_count, _tool_failure_recovery_hint, after_call,
 *   allows_execution, append_toolguard_guidance, before_call,
 *   canonical_tool_args, classify_tool_failure, from_call, from_mapping,
 *   halt_decision, reset_for_turn, should_halt, to_metadata,
 *   toolguard_synthetic_result  -- 23 concrete defs, all in the non-flow ports
 *   (tool_guardrails.c / tool_guardrails_gaps.c / tool_guardrails_ports.c).
 * The module has 35 AST defs; the 11 UNCLAIMED ones are ported here REAL:
 *   is_stall_guard_repeatable, _is_non_interactive_platform, _canonical_json,
 *   _int_at_least, _ordinal, _without_execute_code_metadata,
 *   ToolCallGuardrailController.__init__, ._decide, .observe_call,
 *   ._detect_identical_cycle, .record_persisted_result.
 * NOT fully claimed -> this port is required; >=5 FIXTURES per function.
 *
 * Self-contained: libjson only. Every dependency NOT ported here is injected
 * through ONE universal seam:
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * args is a BORROWED object (the port owns and frees it after the call); the
 * result is a NEW json_t owned by the caller (NULL == None; a
 * {"__raise__":kind,"message":msg} object == a raised exception). Fail-closed:
 * no channel, or a missing/exhausted script, yields RuntimeError
 * "no script for <name>".
 *
 * ENTRIES. static json_t *tg3_<fn>(tg3_io_t *io, const json_t *in, json_t *st)
 *   io -- universal seam channel (NULL => fail-closed production default)
 *   in -- borrowed case input object (never mutated)
 *   st -- mutable state carrier (the controller's `self` projection). PURE
 *         entries leave st untouched; stateful entries mutate it in place.
 * Caller owns the ret node.
 *
 * Seam name -> live call (args record exactly the call-site values; None-valued
 * keys are dropped on BOTH sides by the harness/oracle):
 *   signature_from_call        ToolCallSignature.from_call(tool_name, args)
 *   coerce_args                _coerce_args(args)
 *   result_hash                _result_hash(result, tool_name)
 *   build_result_reference_stub self._build_result_reference_stub(tool_name,args)
 *   reset_for_turn             self.reset_for_turn()
 *   ToolCallGuardrailConfig    ToolCallGuardrailConfig()
 *   py_int                     int(value)               [_int_at_least]
 *
 * FULL builtin exception hierarchy (tg3_exc_parent / tg3_caught_by, probed on
 * quarry CPython) is carried below and mirrored in oracle_tool_guardrails_flow3.py.
 * `except (TypeError, ValueError)` in _int_at_least is subclass-aware: only
 * TypeError/ValueError (and their subclasses) are swallowed; the BaseException
 * trio, CancelledError and the *Group kinds propagate.
 *
 * Python semantics transcribed:
 *   - state carrier + call_history deque(maxlen=64) modelled as a JSON array;
 *     the harness seeds `self` from in["self"] and emits the final carrier.
 *   - `_canonical_json` reimplements json.dumps(ensure_ascii=False,
 *     sort_keys=True, separators=(",", ":")) for the JSON fixture domain.
 *   - `_int_at_least`: junk/None/below-minimum fall back to default; 0 disables.
 *   - `_detect_identical_cycle`: smallest period 2..4 whose trailing laps match
 *     on (signature,result_hash); all-poller cycles exempt.
 *   - `observe_call`: consecutive (tool,args,result) streak, threshold notice,
 *     batch-cycle detection, byte-identical result reference stub.
 * =========================================================================== */

#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>

typedef struct tg3_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} tg3_io_t;

#define TG3_STALL_THRESHOLD 3
#define TG3_MAX_CYCLE_PERIOD 4
#define TG3_CYCLE_HISTORY 64
#define TG3_STUB_MIN_CHARS 512
#define TG3_ARGS_PREVIEW_CHARS 120

#if defined(__GNUC__)
#define TG3_UNUSED __attribute__((unused))
#else
#define TG3_UNUSED
#endif

/* ------------------------------------------------------------------ truth */
static int tg3_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}
static int tg3_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static int tg3_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int tg3_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int tg3_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int tg3_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static int tg3_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static const char *tg3_sval(const json_t *v) { return (tg3_str(v) && v->str_val) ? v->str_val : ""; }
static double tg3_nval(const json_t *v)
{
    if (tg3_num(v)) return v->num_val;
    if (tg3_bool(v)) return v->bool_val ? 1.0 : 0.0;
    return 0.0;
}
static json_t *tg3_get(json_t *o, const char *k)
{
    return (tg3_obj(o) && o->c.keys) ? json_obj_get(o, k) : NULL;
}
static int tg3_str_eq(const json_t *v, const char *s)
{
    return (tg3_str(v) && s && strcmp(v->str_val ? v->str_val : "", s) == 0) ? 1 : 0;
}
static const char *tg3_typename(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
    case JSON_BOOL:   return "bool";
    case JSON_NUMBER: return "int";
    case JSON_STRING: return "str";
    case JSON_ARRAY:  return "list";
    case JSON_OBJECT: return "dict";
    default:          return "object";
    }
}
static json_t *tg3_copy_or_null(const json_t *v) { return v ? json_copy(v) : NULL; }

/* recursive JSON equality (order-independent for objects) */
static int tg3_json_eq(const json_t *a, const json_t *b)
{
    if (a == b) return 1;
    if (!a || !b) return 0;
    if (a->type != b->type) {
        /* bool and number are distinct in Python (True != 1 in tuple compare
         * only by identity of type here); keep them distinct. */
        return 0;
    }
    switch (a->type) {
    case JSON_NULL:  return 1;
    case JSON_BOOL:  return a->bool_val == b->bool_val;
    case JSON_NUMBER:return a->num_val == b->num_val;
    case JSON_STRING:return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    case JSON_ARRAY: {
        if (a->c.count != b->c.count) return 0;
        for (size_t i = 0; i < a->c.count; i++)
            if (!tg3_json_eq(a->c.items[i], b->c.items[i])) return 0;
        return 1;
    }
    case JSON_OBJECT: {
        if (a->c.count != b->c.count) return 0;
        for (size_t i = 0; i < a->c.count; i++) {
            const char *k = a->c.keys[i];
            const json_t *bv = (b->c.keys) ? json_obj_get((json_t *)b, k) : NULL;
            if (!bv || !tg3_json_eq(a->c.items[i], bv)) return 0;
        }
        return 1;
    }
    }
    return 0;
}

/* ------------------------------------------------------------ raise specs */
static int tg3_is_raise(const json_t *r)
{
    return tg3_obj(r) && json_obj_get(r, "__raise__") != NULL;
}
static const char *tg3_kind(const json_t *r)
{
    const char *k = tg3_sval(tg3_get((json_t *)r, "__raise__"));
    return k ? k : "";
}
static const char *tg3_rmsg(const json_t *r)
{
    const char *m = tg3_sval(tg3_get((json_t *)r, "message"));
    return m ? m : "";
}
static json_t *tg3_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind ? kind : "RuntimeError"));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}
static char *tg3_xfmt(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return NULL;
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) return NULL;
    va_start(ap, fmt);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return buf;
}

/* FULL builtin exception-parent hierarchy (probed on quarry CPython). */
typedef struct { const char *name; const char *parent; } tg3_exc_row;
static const tg3_exc_row TG3_EXC_PARENT[] TG3_UNUSED = {
    {"BaseException", "object"},
    {"BaseExceptionGroup", "BaseException"},
    {"Exception", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"SystemExit", "BaseException"},
    {"CancelledError", "BaseException"},
    {"ArithmeticError", "Exception"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"EnvironmentError", "OSError"},
    {"IOError", "OSError"},
    {"ImportError", "Exception"},
    {"LookupError", "Exception"},
    {"MemoryError", "Exception"},
    {"NameError", "Exception"},
    {"OSError", "Exception"},
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"Warning", "Exception"},
    {"WindowsError", "OSError"},
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"ModuleNotFoundError", "ImportError"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"UnboundLocalError", "NameError"},
    {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"NotImplementedError", "RuntimeError"},
    {"PythonFinalizationError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"IndentationError", "SyntaxError"},
    {"_IncompleteInputError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"BytesWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"UserWarning", "Warning"},
    {NULL, NULL}
};

static const char *tg3_exc_parent(const char *kind) TG3_UNUSED;
static const char *tg3_exc_parent(const char *kind)
{
    for (size_t i = 0; TG3_EXC_PARENT[i].name; i++)
        if (strcmp(TG3_EXC_PARENT[i].name, kind) == 0)
            return TG3_EXC_PARENT[i].parent;
    return NULL;
}
static int tg3_is_exception(const char *kind) TG3_UNUSED;
static int tg3_is_exception(const char *kind)
{
    if (strcmp(kind, "ExceptionGroup") == 0) return 1;
    const char *cur = kind;
    for (int depth = 0; depth < 32 && cur; depth++) {
        if (strcmp(cur, "Exception") == 0) return 1;
        if (strcmp(cur, "BaseException") == 0 || strcmp(cur, "object") == 0) return 0;
        cur = tg3_exc_parent(cur);
        if (!cur) return 1;
    }
    return 1;
}
/* True iff kind is caught by `except (names...)` -- subclass-aware. */
static int tg3_caught_by(const char *kind, const char *const *names, size_t n) TG3_UNUSED;
static int tg3_caught_by(const char *kind, const char *const *names, size_t n)
{
    const char *cur = kind;
    for (int depth = 0; depth < 32 && cur; depth++) {
        for (size_t i = 0; i < n; i++)
            if (strcmp(cur, names[i]) == 0) return 1;
        if (strcmp(cur, "BaseException") == 0 || strcmp(cur, "object") == 0) return 0;
        cur = tg3_exc_parent(cur);
        if (!cur) return 0;
    }
    return 0;
}

/* --------------------------------------------------------- universal seam */
static json_t *tg3_call(tg3_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = tg3_xfmt("no seam installed for %s", name);
        r = tg3_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}
static json_t *tg3_args(void) { return json_object(); }

static json_t *tg3_seam_sig_from_call(tg3_io_t *io, const json_t *tool_name, const json_t *args)
{
    json_t *a = tg3_args();
    json_set(a, "tool_name", tg3_copy_or_null(tool_name));
    json_set(a, "args", tg3_copy_or_null(args));
    return tg3_call(io, "signature_from_call", a);
}
static json_t *tg3_seam_coerce_args(tg3_io_t *io, const json_t *args)
{
    json_t *a = tg3_args();
    json_set(a, "args", tg3_copy_or_null(args));
    return tg3_call(io, "coerce_args", a);
}
static json_t *tg3_seam_result_hash(tg3_io_t *io, const json_t *result, const json_t *tool_name)
{
    json_t *a = tg3_args();
    json_set(a, "result", tg3_copy_or_null(result));
    json_set(a, "tool_name", tg3_copy_or_null(tool_name));
    return tg3_call(io, "result_hash", a);
}
static json_t *tg3_seam_stub(tg3_io_t *io, const json_t *tool_name, const json_t *args)
{
    json_t *a = tg3_args();
    json_set(a, "tool_name", tg3_copy_or_null(tool_name));
    json_set(a, "args", tg3_copy_or_null(args));
    return tg3_call(io, "build_result_reference_stub", a);
}
static json_t *tg3_seam_reset(tg3_io_t *io)
{
    return tg3_call(io, "reset_for_turn", tg3_args());
}
static json_t *tg3_seam_config(tg3_io_t *io)
{
    return tg3_call(io, "ToolCallGuardrailConfig", tg3_args());
}
static json_t *tg3_seam_py_int(tg3_io_t *io, const json_t *value)
{
    json_t *a = tg3_args();
    json_set(a, "value", tg3_copy_or_null(value));
    return tg3_call(io, "py_int", a);
}

/* --------------------------------------------------- canonical json writer */
typedef struct { char *p; size_t n, cap; } tg3_buf;
static void tg3_buf_putc(tg3_buf *b, char c)
{
    if (b->n + 2 > b->cap) {
        b->cap = b->cap ? b->cap * 2 : 128;
        b->p = (char *)realloc(b->p, b->cap);
    }
    b->p[b->n++] = c;
    b->p[b->n] = '\0';
}
static void tg3_buf_puts(tg3_buf *b, const char *s)
{
    while (*s) tg3_buf_putc(b, *s++);
}
static void tg3_buf_num(tg3_buf *b, double d)
{
    char tmp[64];
    if (d == (double)(long long)d && d > -9.0e15 && d < 9.0e15)
        snprintf(tmp, sizeof(tmp), "%.0f", d);
    else
        snprintf(tmp, sizeof(tmp), "%g", d);
    tg3_buf_puts(b, tmp);
}
static void tg3_buf_jsonstr(tg3_buf *b, const json_t *v)
{
    const char *s = tg3_sval(v);
    tg3_buf_putc(b, '"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        switch (c) {
        case '"':  tg3_buf_puts(b, "\\\""); break;
        case '\\': tg3_buf_puts(b, "\\\\"); break;
        case '\b': tg3_buf_puts(b, "\\b");  break;
        case '\f': tg3_buf_puts(b, "\\f");  break;
        case '\n': tg3_buf_puts(b, "\\n");  break;
        case '\r': tg3_buf_puts(b, "\\r");  break;
        case '\t': tg3_buf_puts(b, "\\t");  break;
        default:
            if (c < 0x20) {
                char tmp[8];
                snprintf(tmp, sizeof(tmp), "\\u%04x", c);
                tg3_buf_puts(b, tmp);
            } else {
                tg3_buf_putc(b, (char)c);   /* ensure_ascii=False: raw UTF-8 */
            }
        }
    }
    tg3_buf_putc(b, '"');
}
static int tg3_keycmp(const void *pa, const void *pb)
{
    const json_t *a = *(const json_t *const *)pa;
    const json_t *b = *(const json_t *const *)pb;
    return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "");
}
static void tg3_canon_rec(tg3_buf *b, const json_t *v)
{
    if (!v || v->type == JSON_NULL) { tg3_buf_puts(b, "null"); return; }
    switch (v->type) {
    case JSON_BOOL:   tg3_buf_puts(b, v->bool_val ? "true" : "false"); break;
    case JSON_NUMBER: tg3_buf_num(b, v->num_val); break;
    case JSON_STRING: tg3_buf_jsonstr(b, v); break;
    case JSON_ARRAY: {
        tg3_buf_putc(b, '[');
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) tg3_buf_putc(b, ',');
            tg3_canon_rec(b, v->c.items[i]);
        }
        tg3_buf_putc(b, ']');
        break;
    }
    case JSON_OBJECT: {
        tg3_buf_putc(b, '{');
        json_t **keys = (json_t **)malloc((v->c.count ? v->c.count : 1) * sizeof(json_t *));
        for (size_t i = 0; i < v->c.count; i++) keys[i] = json_string(v->c.keys[i]);
        qsort(keys, v->c.count, sizeof(json_t *), tg3_keycmp);
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) tg3_buf_putc(b, ',');
            tg3_buf_jsonstr(b, keys[i]);
            tg3_buf_putc(b, ':');
            const json_t *val = json_obj_get((json_t *)v, keys[i]->str_val);
            tg3_canon_rec(b, val);
            json_free(keys[i]);
        }
        free(keys);
        tg3_buf_putc(b, '}');
        break;
    }
    }
}
static json_t *tg3_canon_string(const json_t *v)
{
    tg3_buf b = {0};
    tg3_canon_rec(&b, v);
    json_t *r = json_string(b.p ? b.p : "");
    free(b.p);
    return r;
}

/* Python str.strip()/lower() for the ASCII fixture domain. */
static void tg3_strip_lower(const char *s, char *out, size_t cap)
{
    size_t i = 0, o = 0;
    while (s[i] && isspace((unsigned char)s[i])) i++;
    size_t e = strlen(s);
    while (e > i && isspace((unsigned char)s[e - 1])) e--;
    while (i < e && o + 1 < cap) {
        out[o++] = (char)tolower((unsigned char)s[i++]);
    }
    out[o] = '\0';
}

/* Python str.format with named fields, values taken from a JSON object. */
static json_t *tg3_format_map(const char *tpl, const json_t *kv)
{
    tg3_buf b = {0};
    for (const char *p = tpl ? tpl : ""; *p; ) {
        if (*p == '{' && p[1] != '{') {
            const char *close = strchr(p, '}');
            if (close) {
                size_t n = (size_t)(close - p - 1);
                char key[64];
                if (n < sizeof(key)) {
                    memcpy(key, p + 1, n); key[n] = '\0';
                    const json_t *v = tg3_get((json_t *)kv, key);
                    if (v && tg3_str(v)) tg3_buf_puts(&b, tg3_sval(v));
                    else if (v && (tg3_num(v) || tg3_bool(v))) {
                        char t[64]; snprintf(t, sizeof(t), "%.0f", tg3_nval(v)); tg3_buf_puts(&b, t);
                    } else {
                        tg3_buf_putc(&b, '{'); tg3_buf_puts(&b, key); tg3_buf_putc(&b, '}');
                    }
                    p = close + 1;
                    continue;
                }
            }
        }
        if (*p == '{' && p[1] == '{') { tg3_buf_putc(&b, '{'); p += 2; continue; }
        if (*p == '}' && p[1] == '}') { tg3_buf_putc(&b, '}'); p += 2; continue; }
        tg3_buf_putc(&b, *p++);
    }
    json_t *r = json_string(b.p ? b.p : "");
    free(b.p);
    return r;
}

/* Python str.format with named fields: {tool_name},{count},{cap},{period}. */
static json_t *tg3_fmt_message(const char *tpl, const char *tool_name, double count,
                               const json_t *fmt)
{
    json_t *kv = json_object();
    json_set(kv, "tool_name", json_string(tool_name ? tool_name : ""));
    json_set(kv, "count", json_number(count));
    if (tg3_obj(fmt)) {
        for (size_t i = 0; i < fmt->c.count; i++)
            json_set(kv, fmt->c.keys[i], json_copy(fmt->c.items[i]));
    }
    json_t *r = tg3_format_map(tpl, kv);
    json_free(kv);
    return r;
}

/* _DECISION_MESSAGES (module constant, seeded identically in the oracle). */
static const char *TG3_DECISION_MSG(const char *code)
{
    static const struct { const char *c, *m; } M[] = {
        {"repeated_exact_failure_block",
         "Blocked {tool_name}: the same tool call failed {count} times with identical arguments. Stop retrying it unchanged; change strategy or explain the blocker."},
        {"idempotent_no_progress_block",
         "Blocked {tool_name}: this read-only call returned the same result {count} times. Stop repeating it unchanged; use the result already provided or try a different query."},
        {"same_tool_failure_halt",
         "Stopped {tool_name}: it failed {count} times this turn. Stop retrying the same failing tool path and choose a different approach."},
        {"repeated_exact_failure_warning",
         "{tool_name} has failed {count} times with identical arguments. This looks like a loop; inspect the error and change strategy instead of retrying it unchanged."},
        {"idempotent_no_progress_warning",
         "{tool_name} returned the same result {count} times. Use the result already provided or change the query instead of repeating it unchanged."},
        {"identical_call_streak_halt",
         "Stopped {tool_name}: the same call with identical arguments returned the same result {count} times in a row. Stop repeating it unchanged; use the result already provided or change strategy."},
        {"identical_cycle_halt",
         "Stopped {tool_name}: the same repeating cycle of tool calls (period {period}) with identical arguments and identical results has run {count} times. Repeating the batch unchanged is not progress; use the results already provided or change strategy."},
        {"loop_web_search_cap",
         "Blocked web_search: this turn has already made {cap} web searches, the per-turn limit. This looks like a runaway search loop. Work with the results you already have and give the user your answer."},
        {"loop_subagent_cap",
         "Blocked delegate_task: this turn has already spawned {count} subagents (limit {cap}). This looks like a runaway delegation loop. Finish the work with the results you have and answer the user."},
        {NULL, NULL}
    };
    for (size_t i = 0; M[i].c; i++) if (strcmp(M[i].c, code) == 0) return M[i].m;
    return NULL;   /* unknown code -> live _DECISION_MESSAGES[code] raises KeyError */
}
#define TG3_IDENTICAL_CALL_NOTICE \
    "[hermes note: this is the {ordinal} consecutive identical call to {tool_name} with identical arguments returning the same result. Do not repeat it — change arguments, use a different tool, or proceed with what you have.]"
#define TG3_IDENTICAL_CYCLE_NOTICE \
    "[hermes note: the last {count} rounds repeated the same cycle of {period} tool calls (ending with {tool_name}) with identical arguments and identical results. Do not repeat the batch — change arguments, use a different tool, or proceed with what you have.]"

/* -------------------------------------------------- ported REAL leaf helpers */
/* PoP: tg3_is_stall_guard_repeatable @ agent/tool_guardrails.py:is_stall_guard_repeatable */
static json_t *tg3_is_stall_guard_repeatable(tg3_io_t *io_unused, const json_t *in, json_t *st_unused)
{
    (void)io_unused; (void)st_unused;
    const char *tn = tg3_sval(tg3_get((json_t *)in, "tool_name"));
    if (strcmp(tn, "process_manage") == 0) return json_bool(1);
    size_t n = strlen(tn);
    if (n >= 11 && strcmp(tn + n - 11, "_get_result") == 0) return json_bool(1);
    if (n >= 5 && strcmp(tn + n - 5, "_poll") == 0) return json_bool(1);
    return json_bool(0);
}

/* PoP: tg3_is_non_interactive_platform @ agent/tool_guardrails.py:_is_non_interactive_platform */
static json_t *tg3_is_non_interactive_platform(tg3_io_t *io_unused, const json_t *in, json_t *st_unused)
{
    (void)io_unused; (void)st_unused;
    const json_t *p = tg3_get((json_t *)in, "platform");
    if (!tg3_str(p)) return json_bool(0);
    char low[512];
    tg3_strip_lower(tg3_sval(p), low, sizeof(low));
    if (low[0] == '\0') return json_bool(0);
    static const char *ATT[] = {"cli", "tui", "desktop", "acp", "subagent", "api_server"};
    for (size_t i = 0; i < sizeof(ATT)/sizeof(ATT[0]); i++)
        if (strcmp(low, ATT[i]) == 0) return json_bool(0);
    return json_bool(1);
}

/* PoP: tg3_ordinal @ agent/tool_guardrails.py:_ordinal */
static json_t *tg3_ordinal(tg3_io_t *io_unused, const json_t *in, json_t *st_unused)
{
    (void)io_unused; (void)st_unused;
    long long c = (long long)tg3_nval(tg3_get((json_t *)in, "count"));
    long long mod100 = c % 100; if (mod100 < 0) mod100 += 100;
    const char *suf;
    if (mod100 >= 11 && mod100 <= 13) suf = "th";
    else {
        long long mod10 = c % 10; if (mod10 < 0) mod10 += 10;
        suf = (mod10 == 1) ? "st" : (mod10 == 2) ? "nd" : (mod10 == 3) ? "rd" : "th";
    }
    char out[64];
    snprintf(out, sizeof(out), "%lld%s", c, suf);
    return json_string(out);
}

/* PoP: tg3_without_execute_code_metadata @ agent/tool_guardrails.py:_without_execute_code_metadata */
static json_t *tg3_without_execute_code_metadata(tg3_io_t *io_unused, const json_t *in, json_t *st_unused)
{
    (void)io_unused; (void)st_unused;
    const json_t *parsed = tg3_get((json_t *)in, "parsed");
    if (!tg3_obj(parsed)) return parsed ? json_copy(parsed) : json_null();
    json_t *cleaned = json_object();
    for (size_t i = 0; i < parsed->c.count; i++) {
        const char *k = parsed->c.keys[i];
        if (strcmp(k, "duration_seconds") == 0) continue;
        const json_t *v = parsed->c.items[i];
        if (strcmp(k, "kernel") == 0 && tg3_obj(v)) {
            json_t *kern = json_object();
            for (size_t j = 0; j < v->c.count; j++) {
                if (strcmp(v->c.keys[j], "execution_count") == 0) continue;
                json_set(kern, v->c.keys[j], json_copy(v->c.items[j]));
            }
            json_set(cleaned, k, kern);
        } else {
            json_set(cleaned, k, json_copy(v));
        }
    }
    return cleaned;
}

/* PoP: tg3_canonical_json @ agent/tool_guardrails.py:_canonical_json */
static json_t *tg3_canonical_json(tg3_io_t *io_unused, const json_t *in, json_t *st_unused)
{
    (void)io_unused; (void)st_unused;
    return tg3_canon_string(tg3_get((json_t *)in, "value"));
}

/* PoP: tg3_int_at_least @ agent/tool_guardrails.py:_int_at_least */
static json_t *tg3_int_at_least(tg3_io_t *io, const json_t *in, json_t *st_unused)
{
    (void)st_unused;
    const json_t *value = tg3_get((json_t *)in, "value");
    double dflt = tg3_nval(tg3_get((json_t *)in, "default"));
    double minimum = tg3_nval(tg3_get((json_t *)in, "minimum"));
    json_t *iv = tg3_seam_py_int(io, value);
    if (tg3_is_raise(iv)) {
        const char *kind = tg3_kind(iv);
        static const char *const CATCH[] = {"TypeError", "ValueError"};
        if (tg3_caught_by(kind, CATCH, 2)) {
            json_free(iv);
            return json_number(dflt);
        }
        return iv;                       /* propagate (BaseException trio etc.) */
    }
    double parsed = tg3_nval(iv);
    json_free(iv);
    return json_number(parsed >= minimum ? parsed : dflt);
}

/* ------------------------------------------------ stateful controller helpers */
static json_t *tg3_decision(const char *action, const char *code, const char *message,
                            const char *tool_name, double count, const json_t *sig)
{
    json_t *d = json_object();
    json_set(d, "action", json_string(action ? action : "allow"));
    json_set(d, "code", json_string(code ? code : "allow"));
    json_set(d, "message", json_string(message ? message : ""));
    json_set(d, "tool_name", json_string(tool_name ? tool_name : ""));
    json_set(d, "count", json_number(count));
    json_set(d, "signature", sig ? json_copy(sig) : json_null());
    return d;
}
static int tg3_should_halt_action(const char *action)
{
    return (strcmp(action, "block") == 0 || strcmp(action, "halt") == 0) ? 1 : 0;
}

/* PoP: tg3_decide @ agent/tool_guardrails.py:ToolCallGuardrailController._decide */
static json_t *tg3_decide(tg3_io_t *io_unused, const json_t *in, json_t *st)
{
    (void)io_unused;
    const char *action = tg3_sval(tg3_get((json_t *)in, "action"));
    const char *code = tg3_sval(tg3_get((json_t *)in, "code"));
    const char *tool_name = tg3_sval(tg3_get((json_t *)in, "tool_name"));
    double count = tg3_nval(tg3_get((json_t *)in, "count"));
    const json_t *sig = tg3_get((json_t *)in, "signature");
    const json_t *msg = tg3_get((json_t *)in, "message");
    const json_t *fmt = tg3_get((json_t *)in, "fmt");

    json_t *message;
    if (tg3_str(msg)) {
        message = json_copy(msg);
    } else {
        const char *tpl = TG3_DECISION_MSG(code);
        if (!tpl) {
            char *m = tg3_xfmt("'%s'", code);
            json_t *e = tg3_mkraise("KeyError", m ? m : code);
            free(m);
            return e;
        }
        message = tg3_fmt_message(tpl, tool_name, count, fmt);
    }

    json_t *d = tg3_decision(action, code, tg3_sval(message), tool_name, count, sig);
    json_free(message);
    if (tg3_should_halt_action(action)) {
        /* json_set frees the replaced value — do NOT free the old node here. */
        json_set(st, "_halt_decision", json_copy(d));
    }
    return d;
}

/* PoP: tg3_record_persisted_result @ agent/tool_guardrails.py:ToolCallGuardrailController.record_persisted_result */
static json_t *tg3_record_persisted_result(tg3_io_t *io_unused, const json_t *in, json_t *st)
{
    (void)io_unused;
    const json_t *cid = tg3_get((json_t *)in, "tool_call_id");
    const json_t *fp = tg3_get((json_t *)in, "file_path");
    if (tg3_truth(cid) && tg3_truth(fp)) {
        json_t *paths = tg3_get(st, "_persisted_result_paths");
        if (!tg3_obj(paths)) { paths = json_object(); json_set(st, "_persisted_result_paths", paths); }
        json_set(paths, tg3_sval(cid), json_copy(fp));
    }
    return json_null();
}

/* _detect_identical_cycle -- reads st["_call_history"]; no seams. */
static json_t *tg3_detect_identical_cycle(tg3_io_t *io_unused, const json_t *in_unused, json_t *st)
{
    (void)io_unused; (void)in_unused;
    json_t *hist = tg3_get(st, "_call_history");
    size_t H = tg3_arr(hist) ? hist->c.count : 0;
    for (int period = 2; period <= TG3_MAX_CYCLE_PERIOD; period++) {
        if (H < (size_t)period * TG3_STALL_THRESHOLD) continue;
        int laps = 1;
        while (1) {
            long long base = (long long)H - (long long)period * (laps + 1);
            if (base < 0) break;
            int eq = 1;
            for (int i = 0; i < period; i++) {
                const json_t *a = hist->c.items[(size_t)base + (size_t)i];
                const json_t *b = hist->c.items[H - (size_t)period + (size_t)i];
                if (!tg3_arr(a) || !tg3_arr(b) || a->c.count < 2 || b->c.count < 2 ||
                    !tg3_json_eq(a->c.items[0], b->c.items[0]) ||
                    !tg3_json_eq(a->c.items[1], b->c.items[1])) { eq = 0; break; }
            }
            if (!eq) break;
            laps++;
        }
        if (laps >= TG3_STALL_THRESHOLD) {
            int all_repeatable = 1;
            for (int i = 0; i < period; i++) {
                const json_t *tail = hist->c.items[H - (size_t)period + (size_t)i];
                if (!tg3_arr(tail) || tail->c.count < 3 || !tg3_truth(tail->c.items[2])) {
                    all_repeatable = 0; break;
                }
            }
            if (all_repeatable) continue;
            json_t *r = json_array();
            json_append(r, json_number(period));
            json_append(r, json_number(laps));
            return r;
        }
    }
    return json_null();
}

/* PoP: tg3_detect_identical_cycle_entry @ agent/tool_guardrails.py:ToolCallGuardrailController._detect_identical_cycle */
static json_t *tg3_detect_identical_cycle_entry(tg3_io_t *io, const json_t *in, json_t *st)
{
    return tg3_detect_identical_cycle(io, in, st);
}

/* PoP: tg3_init @ agent/tool_guardrails.py:ToolCallGuardrailController.__init__ */
static json_t *tg3_init(tg3_io_t *io, const json_t *in, json_t *st)
{
    const json_t *cfg = tg3_get((json_t *)in, "config");
    json_t *resolved;
    if (tg3_truth(cfg)) {
        resolved = json_copy((json_t *)cfg);       /* config as-is */
    } else {
        resolved = tg3_seam_config(io);            /* ToolCallGuardrailConfig() */
        if (tg3_is_raise(resolved)) return resolved;
    }
    /* set config BEFORE reset_for_turn: the live __init__ assigns self.config
     * first, so a raising reset still leaves the resolved config on self. */
    json_set(st, "config", resolved);
    json_t *r = tg3_seam_reset(io);
    if (tg3_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: tg3_observe_call @ agent/tool_guardrails.py:ToolCallGuardrailController.observe_call */
static json_t *tg3_observe_call(tg3_io_t *io, const json_t *in, json_t *st)
{
    const char *tool_name = tg3_sval(tg3_get((json_t *)in, "tool_name"));
    const json_t *args = tg3_get((json_t *)in, "args");
    const json_t *result = tg3_get((json_t *)in, "result");
    const char *call_id = tg3_sval(tg3_get((json_t *)in, "tool_call_id"));
    int failed = tg3_truth(tg3_get((json_t *)in, "failed"));
    int is_plain_str = tg3_str(result);

    /* signature = ToolCallSignature.from_call(tool_name, _coerce_args(args)) */
    json_t *coerced = tg3_seam_coerce_args(io, args);
    if (tg3_is_raise(coerced)) return coerced;
    json_t *sig = tg3_seam_sig_from_call(io, tg3_get((json_t *)in, "tool_name"), coerced);
    json_free(coerced);
    if (tg3_is_raise(sig)) return sig;

    /* result_hash = _result_hash(result, tool_name) if is_plain_str else "" */
    json_t *rh = NULL;
    if (is_plain_str) {
        rh = tg3_seam_result_hash(io, result, tg3_get((json_t *)in, "tool_name"));
        if (tg3_is_raise(rh)) { json_free(sig); return rh; }
    }
    /* copy the hash out of rh before freeing it: rh_s must not dangle */
    char rh_buf[512];
    if (is_plain_str) snprintf(rh_buf, sizeof(rh_buf), "%s", tg3_sval(rh));
    else rh_buf[0] = '\0';
    const char *rh_s = rh_buf;

    /* streak compare */
    json_t *prev_sig = tg3_get(st, "_identical_streak_sig");
    const char *prev_rh = tg3_sval(tg3_get(st, "_identical_streak_result_hash"));
    int match = is_plain_str && prev_sig && !tg3_none(prev_sig) &&
                tg3_json_eq(sig, prev_sig) && strcmp(rh_s, prev_rh) == 0;
    long long count;
    if (match) {
        count = (long long)tg3_nval(tg3_get(st, "_identical_streak_count")) + 1;
        json_set(st, "_identical_streak_count", json_number((double)count));
    } else {
        /* json_set frees the replaced value — do NOT free the old sig node. */
        json_set(st, "_identical_streak_sig", is_plain_str ? json_copy(sig) : json_null());
        json_set(st, "_identical_streak_result_hash", json_string(rh_s));
        count = is_plain_str ? 1 : 0;
        json_set(st, "_identical_streak_count", json_number((double)count));
        json_set(st, "_identical_streak_first_call_id", json_string(call_id ? call_id : ""));
    }
    if (rh) json_free(rh);

    json_t *notice = NULL, *kind = NULL; /* owned json strings or NULL */
    json_t *repeatable = tg3_is_stall_guard_repeatable(NULL, (json_t *)in, NULL);
    int rep = tg3_truth(repeatable);
    json_free(repeatable);

    /* build ordinal explicitly (tg3_ordinal reads in["count"]) */
    if (!rep && count >= TG3_STALL_THRESHOLD) {
        json_t *oin = json_object();
        json_set(oin, "count", json_number((double)count));
        json_t *ord = tg3_ordinal(NULL, oin, NULL);
        json_free(oin);
        json_t *kv = json_object();
        json_set(kv, "ordinal", ord);
        json_set(kv, "tool_name", json_string(tool_name));
        notice = tg3_format_map(TG3_IDENTICAL_CALL_NOTICE, kv);
        json_free(kv);
        kind = json_string("identical_call_streak");

        json_t *cfg = tg3_get(st, "config");
        int hard_stop = cfg ? tg3_truth(tg3_get(cfg, "hard_stop_enabled")) : 0;
        double nopa = cfg ? tg3_nval(tg3_get(cfg, "no_progress_block_after")) : 0.0;
        json_t *hd = tg3_get(st, "_halt_decision");
        int halt_none = (!hd || hd->type == JSON_NULL);
        if (hard_stop && count >= nopa && halt_none) {
            json_t *din = json_object();
            json_set(din, "action", json_string("halt"));
            json_set(din, "code", json_string("identical_call_streak_halt"));
            json_set(din, "tool_name", json_string(tool_name));
            json_set(din, "count", json_number((double)count));
            json_set(din, "signature", json_copy(sig));
            json_set(din, "message", json_null());
            json_set(din, "fmt", json_object());
            json_t *d = tg3_decide(NULL, din, st);
            json_free(din); json_free(d);
        }
    }

    /* _call_history.append / clear */
    json_t *hist = tg3_get(st, "_call_history");
    if (!tg3_arr(hist)) { hist = json_array(); json_set(st, "_call_history", hist); }
    if (is_plain_str) {
        json_t *entry = json_array();
        json_append(entry, json_copy(sig));       /* signature */
        json_append(entry, json_string(rh_s));     /* result hash (string) */
        json_append(entry, json_bool(rep));        /* repeatable */
        /* enforce deque(maxlen=64) */
        if (hist->c.count >= TG3_CYCLE_HISTORY) {
            json_free(hist->c.items[0]);
            for (size_t i = 1; i < hist->c.count; i++) hist->c.items[i-1] = hist->c.items[i];
            hist->c.count--;
        }
        json_append(hist, entry);
    } else {
        while (hist->c.count) {
            size_t last = hist->c.count - 1;
            json_free(hist->c.items[last]);
            hist->c.count--;
        }
    }

    /* batch-cycle detection if notice is None and is_plain_str */
    if (!notice && is_plain_str) {
        json_t *cyc = tg3_detect_identical_cycle(io, in, st);
        if (!tg3_none(cyc) && tg3_arr(cyc) && cyc->c.count >= 2) {
            long long period = (long long)tg3_nval(cyc->c.items[0]);
            long long laps = (long long)tg3_nval(cyc->c.items[1]);
            json_t *kv = json_object();
            json_set(kv, "count", json_number((double)laps));
            json_set(kv, "period", json_number((double)period));
            json_set(kv, "tool_name", json_string(tool_name));
            json_t *nf = tg3_format_map(TG3_IDENTICAL_CYCLE_NOTICE, kv);
            json_free(kv);
            if (notice) json_free(notice);
            notice = nf;
            if (kind) json_free(kind);
            kind = json_string("identical_cycle");

            json_t *cfg = tg3_get(st, "config");
            int hard_stop = cfg ? tg3_truth(tg3_get(cfg, "hard_stop_enabled")) : 0;
            double nopa = cfg ? tg3_nval(tg3_get(cfg, "no_progress_block_after")) : 0.0;
            json_t *hd = tg3_get(st, "_halt_decision");
            int halt_none = (!hd || hd->type == JSON_NULL);
            if (hard_stop && laps >= nopa && halt_none) {
                json_t *din = json_object();
                json_set(din, "action", json_string("halt"));
                json_set(din, "code", json_string("identical_cycle_halt"));
                json_set(din, "tool_name", json_string(tool_name));
                json_set(din, "count", json_number((double)laps));
                json_set(din, "signature", json_copy(sig));
                json_set(din, "message", json_null());
                json_t *fmt = json_object();
                json_set(fmt, "period", json_number((double)period));
                json_set(din, "fmt", fmt);
                json_t *d = tg3_decide(NULL, din, st);
                json_free(din); json_free(d);
            }
        }
        json_free(cyc);
    }

    /* stub */
    json_t *stub = NULL;
    const char *first_id = tg3_sval(tg3_get(st, "_identical_streak_first_call_id"));
    size_t rlen = is_plain_str && result->str_val ? strlen(result->str_val) : 0;
    if (is_plain_str && count >= 2 && !failed && rlen >= (size_t)TG3_STUB_MIN_CHARS) {
        stub = tg3_seam_stub(io, tg3_get((json_t *)in, "tool_name"), args);
        if (tg3_is_raise(stub)) { json_free(sig); if (notice) json_free(notice); if (kind) json_free(kind); return stub; }
    }

    json_t *obs = json_object();
    json_set(obs, "notice", notice ? notice : json_null());
    json_set(obs, "stub", stub ? stub : json_null());
    json_set(obs, "kind", kind ? kind : json_null());
    json_free(sig);
    return obs;
}

/* --------------------------------------------------------------- dispatch */
static json_t *tg3_dispatch(const char *func, tg3_io_t *io, const json_t *in, json_t *st)
{
    if (func[0] == '_') func++;
#define TG3_D(nm) if (strcmp(func, #nm) == 0) return tg3_##nm(io, in, st)
    TG3_D(is_stall_guard_repeatable);
    TG3_D(is_non_interactive_platform);
    TG3_D(canonical_json);
    TG3_D(int_at_least);
    TG3_D(ordinal);
    TG3_D(without_execute_code_metadata);
    TG3_D(init);
    TG3_D(decide);
    TG3_D(observe_call);
    TG3_D(detect_identical_cycle_entry);
    TG3_D(record_persisted_result);
    /* method-qualified aliases used by the harness for controller methods */
    if (strcmp(func, "ToolCallGuardrailController._decide") == 0 ||
        strcmp(func, "_decide") == 0) return tg3_decide(io, in, st);
    if (strcmp(func, "ToolCallGuardrailController.observe_call") == 0 ||
        strcmp(func, "observe_call") == 0) return tg3_observe_call(io, in, st);
    if (strcmp(func, "ToolCallGuardrailController._detect_identical_cycle") == 0 ||
        strcmp(func, "_detect_identical_cycle") == 0) return tg3_detect_identical_cycle_entry(io, in, st);
    if (strcmp(func, "ToolCallGuardrailController.record_persisted_result") == 0 ||
        strcmp(func, "record_persisted_result") == 0) return tg3_record_persisted_result(io, in, st);
    if (strcmp(func, "ToolCallGuardrailController.__init__") == 0 ||
        strcmp(func, "__init__") == 0) return tg3_init(io, in, st);
#undef TG3_D
    return tg3_mkraise("RuntimeError", "unknown func");
}
