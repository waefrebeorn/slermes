/* gemini_transport_flow.c — FLOW + leaves port of the UNCLAIMED remainder of
 * the Gemini native transport. In this quarry snapshot the Gemini transport
 * lives at agent/gemini_native_adapter.py (the task's agent/transports/gemini.py
 * path does not exist here; the module docstring is "OpenAI-compatible facade
 * over Google AI Studio's native Gemini API"). PoP lines therefore name the
 * LIVE module path. The claimed siblings live in port_gemini_native_adapter.c /
 * gemini_native_adapter_ports.c / chat_completions helpers (cct_*) and are
 * SEAMED here; ported functions call each other REAL (internal gem_*
 * composition: _multimodal_part -> _text_of / _inline_data_part;
 * _merge_alternating -> _has_function_response; _new_call_id / _tool_call_slot
 * -> _provider_call_id; effective_output_cap -> _long_decode_cap).
 *
 * Decision-sequence port of the transport's request-translation leaves, the
 * response/error projection layer, the SSE line parser, the streamed tool-call
 * slot identity, the usage folding, the envelope builders, the prompt-block
 * terminal raise, and the GeminiNativeClient auth/error/cap methods.
 *
 * ALL collaborator I/O (claimed siblings, foreign-module imports, the
 * nondeterministic uuid4/time.time clocks, the module logger) rides the
 * injected universal seam gem_call; the harness scripts results and records
 * the ordered trace the oracle diffs against LIVE agent/gemini_native_adapter.py
 * under the same scripts.
 *
 * Seam name table (name -> live call):
 *   hermes_user_agent                hermes_cli.version_info.hermes_user_agent
 *                                    (module-top import; args {product})
 *   is_vertex_express_base_url       CLAIMED sibling (args {base_url})
 *   prepare_gemini_tool_parameters   agent.gemini_schema (module-top import;
 *                                    args {parameters})
 *   sanitize_gemini_tool_parameters  agent.gemini_schema (args {parameters})
 *   uuid4                            uuid.uuid4(); script result is the 32-char
 *                                    hex string; the port slices [:12] and the
 *                                    oracle materializes a .hex attribute double
 *   time_time                        time.time(); script result is an epoch
 *                                    number; the port truncates via int()
 *   logger_debug                     module logger.debug(fmt, arg) — args
 *                                    {msg, arg}; result ignored
 *   gemini_http_error                CLAIMED sibling; script result is an
 *                                    EXCEPTION DESCRIPTOR {"message": m, ...}
 *                                    the oracle materializes to a real
 *                                    GeminiAPIError; the port projects it to
 *                                    {"__raise__":"GeminiAPIError","msg":m}
 *                                    as the RETURNED value (not a pending exc)
 *   _effective_gemini_max_output_tokens  CLAIMED sibling (cct_* port; args
 *                                    {max_tokens, thinking_config})
 *
 * Representation contract (shared with tests/oracle_gemini_transport_flow.py):
 *   - scripts: MISSING key -> fail-closed RuntimeError("no script for <name>");
 *     null -> None; {"$seq":[...]} per-call queue (exhaustion raises
 *     RuntimeError("script exhausted: <name>")); {"__raise__":cls,"msg":m}
 *     raises (trace ret is the WHOLE marker); any other value reused (a bare
 *     ARRAY is a LIST RESULT, not a queue). None-valued seam arg keys dropped
 *     on BOTH sides.
 *   - tuple -> array; None -> JSON null; integral floats collapse to ints on
 *     both sides; uncaught exception -> {"__raise__":cls,"msg":str(e)} (plus
 *     code/status_code/retry_after/details for the real GeminiAPIError raise).
 *   - except Exception sites (_response_text, _inline_data_part) swallow the
 *     matching family; the BaseException trio (SystemExit/KeyboardInterrupt/
 *     GeneratorExit) passes through. Full builtin exception-parent table below
 *     (+ GeminiAPIError -> Exception, the one custom class in the flow).
 *   - _SSE_DONE (module object() sentinel) is modeled by the string marker
 *     "__SSE_DONE__"; the oracle maps the live sentinel identity to it.
 *   - SimpleNamespace results project to dicts in live key order.
 *
 * Fixture domain (ASCII scope):
 *   - strings ASCII (str.strip/lower/upper transcribed byte-wise; str.strip set
 *     is White_Space incl \x1c-\x1f; int() strips the NARROWER set without
 *     \x1c-\x1f). Non-str scalar AttributeError raise positions use
 *     int/bool/str/list/null only (no floats — JSON numbers lose int/float
 *     identity for the type name).
 *   - _dump_call_args / _tool_call_slot dumps values: str/int/bool/null/list/
 *     dict only (no floats — json.dumps renders 1.0 vs 1 differently); kwargs
 *     limited to sort_keys / the ensure_ascii duplicate / unknown keys.
 *   - _long_decode_cap max_output_tokens budget: integral number or non-number
 *     (isinstance int identity); max_tokens int/str/bool/null (int() domain,
 *     |value| < 1e15).
 *   - _usage_from_metadata count values: int/str/bool/null/float (int()
 *     truncates floats; bad str raises ValueError with the live repr message).
 *   - _inline_data_part base64 domain (probe-verified py3.14 non-strict):
 *     canonical padded (roundtrip identity), junk-only (-> ""), canonical with
 *     discarded junk/extra padding, missing padding / %4==1 data chars (-> None
 *     via except Exception), non-ASCII (-> None). No mid-string padding.
 *   - _parse_sse_line / _error_object JSON: strict grammar (libjson json_parse
 *     is LENIENT — pre-validated here), no NaN/Infinity, \u escapes ASCII-range
 *     only, depth <= 8.
 *   - _tool_call_slot slots carry index/id/last_arguments (the live state
 *     model); tool_call_indices insertion order is part of the contract.
 *   - scripted __raise__ markers carry ONLY cls+msg; the real GeminiAPIError
 *     raise (raise_on_prompt_block) carries code/status_code/retry_after/details.
 *   - _response_text response doubles: {"text": s} attribute double, or
 *     {"__raise__": cls, "msg": m} raising-text double (str __raise__ only), or
 *     a raw non-dict value (AttributeError swallowed).
 *
 * libjson-only (default oracle deps suffice). Self-contained: the harness
 * #includes "agent/transports/gemini_transport_flow.c".
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

/* ── context + seam plumbing ─────────────────────────────────────────────── */

typedef struct gem_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} gem_ctx;

static bool gem_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool gem_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool gem_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool gem_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool gem_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool gem_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool gem_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
/* `x is True` identity gate (JSON_BOOL true only). */
static bool gem_is_true(const json_t *v) { return v && v->type == JSON_BOOL && v->bool_val; }

static bool gem_truthy(const json_t *v) {
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
/* String value of a VALUE node (never call json_get_str on a value node). */
static const char *gem_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
/* borrowed object field or NULL */
static json_t *gem_get(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}

/* ── string helpers (caller frees) ───────────────────────────────────────── */

static char *gem_dup_span(const char *s, size_t len) {
    char *r = (char *)malloc(len + 1);
    if (!r) return NULL;
    memcpy(r, s, len);
    r[len] = '\0';
    return r;
}
static bool gem_is_ws(char c) {
    unsigned char u = (unsigned char)c;
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r'
        || (u >= 0x1c && u <= 0x1f);
}
static char *gem_py_strip(const char *s) {
    if (!s) return gem_dup_span("", 0);
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && gem_is_ws(s[a])) a++;
    while (b > a && gem_is_ws(s[b - 1])) b--;
    return gem_dup_span(s + a, b - a);
}
static char *gem_py_lower(const char *s) {
    char *r = gem_dup_span(s ? s : "", strlen(s ? s : ""));
    if (!r) return NULL;
    for (char *p = r; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    return r;
}
static char *gem_py_upper(const char *s) {
    char *r = gem_dup_span(s ? s : "", strlen(s ? s : ""));
    if (!r) return NULL;
    for (char *p = r; *p; p++)
        if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 'a' + 'A');
    return r;
}
/* str(v) for a scalar (null -> "None"; containers outside the str() domain). */
static char *gem_pystr(const json_t *v) {
    if (gem_is_none(v)) return gem_dup_span("None", 4);
    if (gem_is_bool(v)) return gem_dup_span(v->bool_val ? "True" : "False", v->bool_val ? 4 : 5);
    if (gem_is_num(v)) {
        char buf[64]; double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
        else snprintf(buf, sizeof buf, "%g", d);
        return gem_dup_span(buf, strlen(buf));
    }
    if (gem_is_str(v)) return gem_dup_span(gem_jstr(v), strlen(gem_jstr(v)));
    return gem_dup_span("", 0);
}
/* str(v or "") for a scalar (falsy -> ""). Caller frees. */
static char *gem_str_or_empty(const json_t *v) {
    if (!gem_truthy(v)) return gem_dup_span("", 0);
    return gem_pystr(v);
}
/* Python type name of a JSON value (no-float domain: numbers read as int). */
static const char *gem_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return "int";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}
static bool gem_str_has(const char *hay, const char *needle) {
    return strstr(hay ? hay : "", needle) != NULL;
}
static bool gem_str_starts(const char *s, const char *prefix) {
    return strncmp(s ? s : "", prefix, strlen(prefix)) == 0;
}
static bool gem_str_ends(const char *s, const char *suffix) {
    size_t n = strlen(s ? s : ""), m = strlen(suffix);
    return n >= m && strcmp((s ? s : "") + n - m, suffix) == 0;
}

/* ── exception plumbing (full builtin hierarchy) ─────────────────────────── */

static bool gem_exc_pending(const gem_ctx *ctx) { return ctx && ctx->exc != NULL; }
static void gem_exc_clear(gem_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}
static void gem_raise_msg(gem_ctx *ctx, const char *cls, const char *msg) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
    json_set(ctx->exc, "msg", json_string(msg ? msg : ""));
}
static void gem_raise_attr(gem_ctx *ctx, const json_t *recv, const char *attr) {
    char msg[256];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", gem_typename(recv), attr);
    gem_raise_msg(ctx, "AttributeError", msg);
}
static void gem_raise_key(gem_ctx *ctx, const char *key) {
    char msg[256];
    snprintf(msg, sizeof msg, "'%s'", key);
    gem_raise_msg(ctx, "KeyError", msg);
}
static const char *gem_exc_class(const gem_ctx *ctx) {
    return json_get_str(ctx->exc, "__raise__", "");
}
/* FULL builtin exception subclass chain (parent lookup) + the one custom
 * class in this flow (GeminiAPIError(Exception)). */
static const char *gem_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        {"Exception", "BaseException"},
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"}, {"BaseExceptionGroup", "BaseException"},
        {"ValueError", "Exception"}, {"RuntimeError", "Exception"},
        {"TypeError", "Exception"}, {"AttributeError", "Exception"},
        {"OSError", "Exception"}, {"LookupError", "Exception"},
        {"ArithmeticError", "Exception"}, {"StopIteration", "Exception"},
        {"StopAsyncIteration", "Exception"}, {"AssertionError", "Exception"},
        {"BufferError", "Exception"}, {"EOFError", "Exception"},
        {"ImportError", "Exception"}, {"MemoryError", "Exception"},
        {"NameError", "Exception"}, {"ReferenceError", "Exception"},
        {"SyntaxError", "Exception"}, {"SystemError", "Exception"},
        {"Warning", "Exception"}, {"ExceptionGroup", "Exception"},
        {"NotImplementedError", "RuntimeError"}, {"RecursionError", "RuntimeError"},
        {"ModuleNotFoundError", "ImportError"}, {"UnboundLocalError", "NameError"},
        {"IndentationError", "SyntaxError"}, {"TabError", "IndentationError"},
        {"UnicodeError", "ValueError"}, {"UnicodeEncodeError", "UnicodeError"},
        {"UnicodeDecodeError", "UnicodeError"}, {"UnicodeTranslateError", "UnicodeError"},
        {"JSONDecodeError", "ValueError"},
        {"FloatingPointError", "ArithmeticError"}, {"ZeroDivisionError", "ArithmeticError"},
        {"OverflowError", "ArithmeticError"},
        {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
        {"FileNotFoundError", "OSError"}, {"PermissionError", "OSError"},
        {"IsADirectoryError", "OSError"}, {"NotADirectoryError", "OSError"},
        {"FileExistsError", "OSError"}, {"InterruptedError", "OSError"},
        {"BlockingIOError", "OSError"}, {"ChildProcessError", "OSError"},
        {"ProcessLookupError", "OSError"}, {"ConnectionError", "OSError"},
        {"BrokenPipeError", "ConnectionError"}, {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
        {"TimeoutError", "OSError"},
        {"UserWarning", "Warning"}, {"DeprecationWarning", "Warning"},
        {"PendingDeprecationWarning", "Warning"}, {"SyntaxWarning", "Warning"},
        {"RuntimeWarning", "Warning"}, {"FutureWarning", "Warning"},
        {"ImportWarning", "Warning"}, {"UnicodeWarning", "Warning"},
        {"BytesWarning", "Warning"}, {"ResourceWarning", "Warning"},
        {"EncodingWarning", "Warning"},
        {"GeminiAPIError", "Exception"},   /* custom class, Exception subclass */
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (gem_str_eq(T[i].c, c)) return T[i].p;
    return NULL;
}
static bool gem_exc_matches(const gem_ctx *ctx, const char *const *classes, size_t n) {
    if (!gem_exc_pending(ctx)) return false;
    const char *cls = gem_exc_class(ctx);
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (gem_str_eq(cls, classes[i])) return true;
        const char *p = gem_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}
/* except Exception (NOT the BaseException trio). */
static bool gem_exc_is_exception(const gem_ctx *ctx) {
    static const char *E[] = {"Exception"};
    return gem_exc_matches(ctx, E, 1);
}

/* ── universal seam (fail-closed) ────────────────────────────────────────── */

/* args ownership is TRANSFERRED (NULL -> empty object). Result is NEW. */
static json_t *gem_call(gem_ctx *ctx, const char *name, json_t *args) {
    if (!args) args = json_object();
    json_t *script = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    json_t *chosen = NULL;
    json_t *fc_marker = NULL;   /* owned fail-closed / exhaustion marker */
    if (!script) {              /* MISSING key -> fail-closed */
        fc_marker = json_object();
        json_set(fc_marker, "__raise__", json_string("RuntimeError"));
        char msg[256];
        snprintf(msg, sizeof(msg), "no script for %s", name);
        json_set(fc_marker, "msg", json_string(msg));
        chosen = fc_marker;
    } else if (script->type == JSON_OBJECT) {
        json_t *seq = json_obj_get(script, "$seq");
        if (seq && seq->type == JSON_ARRAY) {
            double cur = json_get_num(ctx->counters, name, 0);
            size_t idx = (size_t)cur;
            if (idx >= seq->c.count) {
                fc_marker = json_object();
                json_set(fc_marker, "__raise__", json_string("RuntimeError"));
                char msg[256];
                snprintf(msg, sizeof(msg), "script exhausted: %s", name);
                json_set(fc_marker, "msg", json_string(msg));
                chosen = fc_marker;
            } else {
                json_set(ctx->counters, name, json_number(cur + 1));
                chosen = seq->c.items[idx];
            }
        } else {
            chosen = script;
        }
    } else {
        chosen = script;
    }
    json_t *ret = NULL;
    bool raises = false;
    if (fc_marker) {
        if (ctx->exc) json_free(ctx->exc);
        ctx->exc = json_copy(fc_marker);
        raises = true;
    } else if (chosen && chosen->type == JSON_OBJECT) {
        json_t *r = json_obj_get(chosen, "__raise__");
        if (r && r->type == JSON_STRING) {
            if (ctx->exc) json_free(ctx->exc);
            ctx->exc = json_copy(chosen);
            raises = true;
        } else {
            ret = json_copy(chosen);
        }
    } else if (chosen && chosen->type != JSON_NULL) {
        ret = json_copy(chosen);
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args);
    if (raises) json_set(ev, "ret", json_copy(ctx->exc));
    else json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(ctx->trace, ev);
    if (fc_marker) json_free(fc_marker);
    return ret;
}

/* Arg builders: None-valued keys dropped on BOTH sides ("" kept). Borrowed val
 * is copied; the caller keeps ownership of the source node. */
static void gem_arg_node(json_t *args, const char *key, const json_t *val) {
    if (!gem_is_none(val)) json_set(args, key, json_copy(val));
}

/* ── Python int() coercion (ASCII scope) ─────────────────────────────────── */

/* int() strips the NARROWER whitespace set (no \x1c-\x1f). */
static bool gem_int_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}
/* repr(s) for the ValueError message (ASCII scope: quote selection + escapes). */
static char *gem_py_repr(const char *s) {
    size_t n = strlen(s);
    bool has_sq = strchr(s, '\'') != NULL, has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    char *r = (char *)malloc(n * 4 + 3);
    if (!r) return NULL;
    size_t o = 0;
    r[o++] = q;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == (unsigned char)q || c == '\\') { r[o++] = '\\'; r[o++] = (char)c; }
        else if (c == '\n') { r[o++] = '\\'; r[o++] = 'n'; }
        else if (c == '\r') { r[o++] = '\\'; r[o++] = 'r'; }
        else if (c == '\t') { r[o++] = '\\'; r[o++] = 't'; }
        else if (c < 0x20 || c == 0x7f) {
            static const char H[] = "0123456789abcdef";
            r[o++] = '\\'; r[o++] = 'x'; r[o++] = H[c >> 4]; r[o++] = H[c & 15];
        } else r[o++] = (char)c;
    }
    r[o++] = q; r[o] = '\0';
    return r;
}
/* int(v) on a JSON value. Returns 0 ok (*out set), 1 ValueError, 2 TypeError.
 * str grammar: narrower-ws strip, optional sign, digits with underscores ONLY
 * between two digits, at least one digit (leading zeros ok). */
static int gem_pyint(const json_t *v, long long *out) {
    if (gem_is_none(v)) return 2;
    if (gem_is_bool(v)) { *out = v->bool_val ? 1 : 0; return 0; }
    if (gem_is_num(v)) { *out = (long long)trunc(v->num_val); return 0; }
    if (!gem_is_str(v)) return 2;
    const char *s = gem_jstr(v);
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && gem_int_ws(s[a])) a++;
    while (b > a && gem_int_ws(s[b - 1])) b--;
    bool neg = false;
    size_t i = a;
    if (i < b && (s[i] == '+' || s[i] == '-')) { neg = (s[i] == '-'); i++; }
    bool anydigit = false;
    long long val = 0;
    for (; i < b; i++) {
        char c = s[i];
        if (c == '_') {
            if (i == a || !anydigit || i + 1 >= b || s[i + 1] < '0' || s[i + 1] > '9'
                || s[i - 1] < '0' || s[i - 1] > '9')
                return 1;
            continue;
        }
        if (c < '0' || c > '9') return 1;
        anydigit = true;
        val = val * 10 + (c - '0');
    }
    if (!anydigit) return 1;
    *out = neg ? -val : val;
    return 0;
}
/* int(v) with the live raise messages wired into ctx. Returns true on success. */
static bool gem_pyint_ctx(gem_ctx *ctx, const json_t *v, long long *out) {
    int rc = gem_pyint(v, out);
    if (rc == 0) return true;
    if (rc == 1) {
        char *rep = gem_py_repr(gem_jstr(v));
        char msg[512];
        snprintf(msg, sizeof msg, "invalid literal for int() with base 10: %s", rep ? rep : "''");
        free(rep);
        gem_raise_msg(ctx, "ValueError", msg);
    } else {
        char msg[256];
        snprintf(msg, sizeof msg,
                 "int() argument must be a string, a bytes-like object or a real number, not '%s'",
                 gem_typename(v));
        gem_raise_msg(ctx, "TypeError", msg);
    }
    return false;
}

/* ── Python json.dumps emulator (ensure_ascii=False, ASCII scope) ────────── */

typedef struct gem_buf { char *s; size_t len, cap; } gem_buf;
static void gem_buf_init(gem_buf *b) { b->cap = 128; b->len = 0; b->s = (char *)malloc(b->cap); if (b->s) b->s[0] = '\0'; }
static void gem_buf_put(gem_buf *b, const char *s, size_t n) {
    if (!b->s) return;
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap * 2;
        while (nc < b->len + n + 1) nc *= 2;
        char *ns = (char *)realloc(b->s, nc);
        if (!ns) { free(b->s); b->s = NULL; return; }
        b->s = ns; b->cap = nc;
    }
    memcpy(b->s + b->len, s, n);
    b->len += n;
    b->s[b->len] = '\0';
}
static void gem_buf_puts(gem_buf *b, const char *s) { gem_buf_put(b, s, strlen(s)); }
static void gem_buf_putc(gem_buf *b, char c) { gem_buf_put(b, &c, 1); }

static void gem_dump_str(gem_buf *b, const char *s) {
    static const char H[] = "0123456789abcdef";
    gem_buf_putc(b, '"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        switch (c) {
            case '"': gem_buf_puts(b, "\\\""); break;
            case '\\': gem_buf_puts(b, "\\\\"); break;
            case '\b': gem_buf_puts(b, "\\b"); break;
            case '\f': gem_buf_puts(b, "\\f"); break;
            case '\n': gem_buf_puts(b, "\\n"); break;
            case '\r': gem_buf_puts(b, "\\r"); break;
            case '\t': gem_buf_puts(b, "\\t"); break;
            default:
                if (c < 0x20) {
                    gem_buf_puts(b, "\\u00");
                    gem_buf_putc(b, H[c >> 4]);
                    gem_buf_putc(b, H[c & 15]);
                } else gem_buf_putc(b, (char)c);
        }
    }
    gem_buf_putc(b, '"');
}
static void gem_sort_keys(const json_t *obj, size_t *idx) {
    for (size_t i = 1; i < obj->c.count; i++) {
        size_t cur = idx[i];
        size_t j = i;
        while (j > 0 && strcmp(obj->c.keys[idx[j - 1]], obj->c.keys[cur]) > 0) {
            idx[j] = idx[j - 1];
            j--;
        }
        idx[j] = cur;
    }
}
static void gem_dumps_rec(gem_buf *b, const json_t *v, bool sort_keys) {
    if (!v || v->type == JSON_NULL) { gem_buf_puts(b, "null"); return; }
    switch (v->type) {
        case JSON_BOOL: gem_buf_puts(b, v->bool_val ? "true" : "false"); return;
        case JSON_NUMBER: {
            char buf[64]; double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
            else snprintf(buf, sizeof buf, "%.17g", d);   /* floats outside domain */
            gem_buf_puts(b, buf);
            return;
        }
        case JSON_STRING: gem_dump_str(b, gem_jstr(v)); return;
        case JSON_ARRAY:
            gem_buf_putc(b, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) gem_buf_puts(b, ", ");
                gem_dumps_rec(b, v->c.items[i], sort_keys);
            }
            gem_buf_putc(b, ']');
            return;
        case JSON_OBJECT: {
            gem_buf_putc(b, '{');
            size_t n = v->c.count;
            size_t *idx = (size_t *)malloc((n ? n : 1) * sizeof(size_t));
            if (!idx) { gem_buf_putc(b, '}'); return; }
            for (size_t i = 0; i < n; i++) idx[i] = i;
            if (sort_keys) gem_sort_keys(v, idx);
            for (size_t i = 0; i < n; i++) {
                if (i) gem_buf_puts(b, ", ");
                gem_dump_str(b, v->c.keys[idx[i]]);
                gem_buf_puts(b, ": ");
                gem_dumps_rec(b, v->c.items[idx[i]], sort_keys);
            }
            free(idx);
            gem_buf_putc(b, '}');
            return;
        }
        default: gem_buf_puts(b, "null"); return;
    }
}
/* json.dumps(v, ensure_ascii=False[, sort_keys=True]) — caller frees. */
static char *gem_dumps(const json_t *v, bool sort_keys) {
    gem_buf b; gem_buf_init(&b);
    gem_dumps_rec(&b, v, sort_keys);
    return b.s ? b.s : gem_dup_span("", 0);
}

/* ── strict JSON grammar validator (libjson json_parse is LENIENT) ───────── */

typedef struct gem_jv { const char *s; size_t i, n; int depth; } gem_jv;
static void gem_jv_ws(gem_jv *p) {
    while (p->i < p->n) {
        char c = p->s[p->i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') p->i++;
        else break;
    }
}
static bool gem_jv_value(gem_jv *p);
static bool gem_jv_string(gem_jv *p) {
    if (p->i >= p->n || p->s[p->i] != '"') return false;
    p->i++;
    while (p->i < p->n) {
        unsigned char c = (unsigned char)p->s[p->i];
        if (c == '"') { p->i++; return true; }
        if (c == '\\') {
            p->i++;
            if (p->i >= p->n) return false;
            char e = p->s[p->i];
            if (e == '"' || e == '\\' || e == '/' || e == 'b' || e == 'f'
                || e == 'n' || e == 'r' || e == 't') { p->i++; continue; }
            if (e == 'u') {
                p->i++;
                for (int k = 0; k < 4; k++) {
                    if (p->i >= p->n) return false;
                    char h = p->s[p->i];
                    if (!((h >= '0' && h <= '9') || (h >= 'a' && h <= 'f') || (h >= 'A' && h <= 'F')))
                        return false;
                    p->i++;
                }
                continue;
            }
            return false;
        }
        if (c < 0x20) return false;   /* raw control chars invalid */
        p->i++;
    }
    return false;
}
static bool gem_jv_number(gem_jv *p) {
    size_t start = p->i;
    if (p->i < p->n && p->s[p->i] == '-') p->i++;
    if (p->i >= p->n) return false;
    if (p->s[p->i] == '0') {
        p->i++;
        if (p->i < p->n && p->s[p->i] >= '0' && p->s[p->i] <= '9') return false;  /* leading zero */
    } else if (p->s[p->i] >= '1' && p->s[p->i] <= '9') {
        while (p->i < p->n && p->s[p->i] >= '0' && p->s[p->i] <= '9') p->i++;
    } else return false;
    if (p->i < p->n && p->s[p->i] == '.') {
        p->i++;
        size_t f = p->i;
        while (p->i < p->n && p->s[p->i] >= '0' && p->s[p->i] <= '9') p->i++;
        if (p->i == f) return false;
    }
    if (p->i < p->n && (p->s[p->i] == 'e' || p->s[p->i] == 'E')) {
        p->i++;
        if (p->i < p->n && (p->s[p->i] == '+' || p->s[p->i] == '-')) p->i++;
        size_t e = p->i;
        while (p->i < p->n && p->s[p->i] >= '0' && p->s[p->i] <= '9') p->i++;
        if (p->i == e) return false;
    }
    return p->i > start;
}
static bool gem_jv_value(gem_jv *p) {
    gem_jv_ws(p);
    if (p->i >= p->n) return false;
    if (++p->depth > 8) return false;   /* fixture depth cap */
    char c = p->s[p->i];
    bool ok = false;
    if (c == '{') {
        p->i++;
        gem_jv_ws(p);
        if (p->i < p->n && p->s[p->i] == '}') { p->i++; ok = true; }
        else {
            ok = true;
            for (;;) {
                gem_jv_ws(p);
                if (!gem_jv_string(p)) { ok = false; break; }
                gem_jv_ws(p);
                if (p->i >= p->n || p->s[p->i] != ':') { ok = false; break; }
                p->i++;
                if (!gem_jv_value(p)) { ok = false; break; }
                gem_jv_ws(p);
                if (p->i < p->n && p->s[p->i] == ',') { p->i++; continue; }
                if (p->i < p->n && p->s[p->i] == '}') { p->i++; break; }
                ok = false; break;
            }
        }
    } else if (c == '[') {
        p->i++;
        gem_jv_ws(p);
        if (p->i < p->n && p->s[p->i] == ']') { p->i++; ok = true; }
        else {
            ok = true;
            for (;;) {
                if (!gem_jv_value(p)) { ok = false; break; }
                gem_jv_ws(p);
                if (p->i < p->n && p->s[p->i] == ',') { p->i++; continue; }
                if (p->i < p->n && p->s[p->i] == ']') { p->i++; break; }
                ok = false; break;
            }
        }
    } else if (c == '"') {
        ok = gem_jv_string(p);
    } else if (c == 't') {
        ok = p->n - p->i >= 4 && strncmp(p->s + p->i, "true", 4) == 0; p->i += 4;
    } else if (c == 'f') {
        ok = p->n - p->i >= 5 && strncmp(p->s + p->i, "false", 5) == 0; p->i += 5;
    } else if (c == 'n') {
        ok = p->n - p->i >= 4 && strncmp(p->s + p->i, "null", 4) == 0; p->i += 4;
    } else {
        ok = gem_jv_number(p);
    }
    p->depth--;
    return ok;
}
/* Strict json.loads: NULL when the Python grammar rejects (JSONDecodeError). */
static json_t *gem_json_loads(const char *s) {
    if (!s) return NULL;
    gem_jv p = { s, 0, strlen(s), 0 };
    if (!gem_jv_value(&p)) return NULL;
    gem_jv_ws(&p);
    if (p.i != p.n) return NULL;
    return json_parse(s, NULL);
}

/* ── base64 (py3.14 non-strict b64decode domain + canonical b64encode) ───── */

static int gem_b64_val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}
/* Non-strict b64decode: discard non-alphabet chars; quads of 4 (data or '=');
 * incomplete tail: 0 data chars ok (trailing pads), 1 data char -> the
 * "number of data characters" error, otherwise Incorrect padding. A complete
 * quad with 1 data char is the same error; data after a pad is outside the
 * fixture domain (rejected). Returns false on any raise (message swallowed by
 * _inline_data_part's except Exception). */
static bool gem_b64_decode(const char *s, unsigned char **out, size_t *olen) {
    size_t n = strlen(s);
    char *filt = (char *)malloc(n + 1);
    if (!filt) return false;
    size_t fn = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 0x80) { free(filt); return false; }   /* non-ASCII ValueError */
        if (gem_b64_val((char)c) >= 0 || c == '=') filt[fn++] = (char)c;
    }
    filt[fn] = '\0';
    unsigned char *buf = (unsigned char *)malloc(fn / 4 * 3 + 4);
    if (!buf) { free(filt); return false; }
    size_t ol = 0;
    bool ok = true;
    size_t i = 0;
    while (i < fn) {
        size_t rem = fn - i;
        size_t qn = rem < 4 ? rem : 4;
        size_t d = 0;
        bool pad_seen = false, bad = false;
        unsigned bits = 0;
        for (size_t k = 0; k < qn; k++) {
            char c = filt[i + k];
            if (c == '=') { pad_seen = true; continue; }
            if (pad_seen) { bad = true; break; }   /* data after pad: outside domain */
            d++;
            bits = (bits << 6) | (unsigned)gem_b64_val(c);
        }
        if (bad) { ok = false; break; }
        if (qn < 4) {
            if (d == 0) break;                     /* trailing pads only */
            ok = false; break;                     /* d==1 or unpadded tail */
        }
        if (d == 1) { ok = false; break; }
        if (d == 2) { buf[ol++] = (unsigned char)(bits >> 4); }
        else if (d == 3) { buf[ol++] = (unsigned char)(bits >> 10); buf[ol++] = (unsigned char)((bits >> 2) & 0xff); }
        else if (d == 4) { buf[ol++] = (unsigned char)(bits >> 16); buf[ol++] = (unsigned char)((bits >> 8) & 0xff); buf[ol++] = (unsigned char)(bits & 0xff); }
        i += qn;
    }
    free(filt);
    if (!ok) { free(buf); return false; }
    *out = buf; *olen = ol;
    return true;
}
static char *gem_b64_encode(const unsigned char *data, size_t len) {
    static const char A[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t ol = (len + 2) / 3 * 4;
    char *r = (char *)malloc(ol + 1);
    if (!r) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < len; i += 3) {
        unsigned b = (unsigned)data[i] << 16;
        if (i + 1 < len) b |= (unsigned)data[i + 1] << 8;
        if (i + 2 < len) b |= (unsigned)data[i + 2];
        r[o++] = A[(b >> 18) & 63];
        r[o++] = A[(b >> 12) & 63];
        r[o++] = (i + 1 < len) ? A[(b >> 6) & 63] : '=';
        r[o++] = (i + 2 < len) ? A[b & 63] : '=';
    }
    r[o] = '\0';
    return r;
}

/* ── module constants (verbatim from the live module) ────────────────────── */

static const char *GEM_EXPRESS_KEY_ON_STUDIO_GUIDANCE =
    "\n\nGoogle issues 'AQ.' keys for both Google AI Studio and Vertex AI express mode, and each surface "
    "only accepts its own keys: a Vertex express key always gets 403 PERMISSION_DENIED on "
    "generativelanguage.googleapis.com. If this key came from Google Cloud / Vertex (express mode), point "
    "the provider at the express surface, e.g. set base_url / GEMINI_BASE_URL to "
    "https://aiplatform.googleapis.com/v1beta1.";
static const char *GEM_STUDIO_KEY_ON_EXPRESS_GUIDANCE =
    "\n\nIf this 'AQ.' key came from Google AI Studio (https://aistudio.google.com/apikey), it only works "
    "on the default generativelanguage.googleapis.com host \xe2\x80\x94 remove the explicit aiplatform base_url so "
    "the default surface is used.";
static const char *GEM_INTERRUPTED_RESPONSE_PLACEHOLDER =
    "[The previous response was interrupted before it completed.]";
static const char *GEM_BLOCK_REASON_DEFAULT =
    "The prompt was blocked. Adjust it and try again.";
static const char *gem_block_guidance(const char *reason) {
    if (gem_str_eq(reason, "BLOCKLIST")) return "The prompt includes blocked terms. Rephrase and try again.";
    if (gem_str_eq(reason, "PROHIBITED_CONTENT")) return "The prompt may contain prohibited content. Adjust it and try again.";
    if (gem_str_eq(reason, "SAFETY")) return "The prompt was blocked by Gemini's safety filters. Adjust it and try again.";
    if (gem_str_eq(reason, "IMAGE_SAFETY")) return "The image was blocked for safety reasons.";
    return GEM_BLOCK_REASON_DEFAULT;
}

/* ── pure leaves ─────────────────────────────────────────────────────────── */

/* PoP: gem_text_of @ agent/gemini_native_adapter.py:_text_of */
static json_t *gem_text_of(const json_t *item) {
    if (gem_is_obj(item)) {
        json_t *ty = gem_get(item, "type");
        json_t *text = gem_get(item, "text");
        if (gem_is_str(ty) && gem_str_eq(gem_jstr(ty), "text") && gem_is_str(text))
            return json_copy(text);
    }
    if (gem_is_str(item)) return json_copy(item);
    return json_null();
}

/* PoP: gem_tool_call_id @ agent/gemini_native_adapter.py:_tool_call_id */
static json_t *gem_tool_call_id(gem_ctx *ctx, const json_t *tool_call) {
    if (!gem_is_obj(tool_call)) { gem_raise_attr(ctx, tool_call, "get"); return NULL; }
    json_t *id = gem_get(tool_call, "id");
    json_t *v = gem_truthy(id) ? id : gem_get(tool_call, "call_id");
    if (!gem_truthy(v)) return json_string("");
    char *s = gem_pystr(v);
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

/* PoP: gem_looks_like_json_schema @ agent/gemini_native_adapter.py:_looks_like_json_schema */
static bool gem_looks_like_json_schema(const json_t *node) {
    if (gem_is_obj(node)) {
        for (size_t i = 0; i < node->c.count; i++) {
            const char *k = node->c.keys[i];
            json_t *v = node->c.items[i];
            if (gem_str_eq(k, "$ref") && gem_is_str(v) && gem_str_starts(gem_jstr(v), "#/"))
                return true;
            if (gem_looks_like_json_schema(v)) return true;
        }
        return false;
    }
    if (gem_is_arr(node)) {
        for (size_t i = 0; i < node->c.count; i++)
            if (gem_looks_like_json_schema(node->c.items[i])) return true;
    }
    return false;
}

/* PoP: gem_has_function_response @ agent/gemini_native_adapter.py:_has_function_response */
static json_t *gem_has_function_response(gem_ctx *ctx, const json_t *content) {
    if (!gem_is_obj(content)) { gem_raise_attr(ctx, content, "get"); return NULL; }
    json_t *parts = gem_get(content, "parts");
    if (!parts) return json_bool(false);   /* .get("parts", []) default */
    if (gem_is_str(parts)) return json_bool(false);   /* iterates chars, none dict */
    if (gem_is_obj(parts)) return json_bool(false);   /* iterates keys (str), none dict */
    if (!gem_is_arr(parts)) {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", gem_typename(parts));
        gem_raise_msg(ctx, "TypeError", msg);
        return NULL;
    }
    for (size_t i = 0; i < parts->c.count; i++) {
        json_t *part = parts->c.items[i];
        if (gem_is_obj(part) && json_has(part, "functionResponse")) return json_bool(true);
    }
    return json_bool(false);
}

/* PoP: gem_provider_call_id @ agent/gemini_native_adapter.py:_provider_call_id */
static json_t *gem_provider_call_id(gem_ctx *ctx, const json_t *fc) {
    if (!gem_is_obj(fc)) { gem_raise_attr(ctx, fc, "get"); return NULL; }
    json_t *id = gem_get(fc, "id");
    if (gem_is_str(id) && gem_jstr(id)[0] != '\0') return json_copy(id);
    return json_null();
}

/* PoP: gem_part_text @ agent/gemini_native_adapter.py:_part_text */
static json_t *gem_part_text(gem_ctx *ctx, const json_t *part) {
    if (!gem_is_obj(part)) { gem_raise_attr(ctx, part, "get"); return NULL; }
    json_t *text = gem_get(part, "text");
    json_t *r = json_array();
    if (gem_is_str(text)) {
        json_append(r, json_copy(text));
        json_append(r, json_bool(gem_is_true(gem_get(part, "thought"))));
    } else {
        json_append(r, json_null());
        json_append(r, json_bool(false));
    }
    return r;
}

/* PoP: gem_text_part_signature @ agent/gemini_native_adapter.py:_text_part_signature */
static json_t *gem_text_part_signature(gem_ctx *ctx, const json_t *part) {
    if (!gem_is_obj(part)) { gem_raise_attr(ctx, part, "get"); return NULL; }
    json_t *sig = gem_get(part, "thoughtSignature");
    if (gem_is_str(sig) && gem_jstr(sig)[0] != '\0' && !json_has(part, "functionCall"))
        return json_copy(sig);
    return json_null();
}

/* PoP: gem_part_function_call @ agent/gemini_native_adapter.py:_part_function_call */
static json_t *gem_part_function_call(gem_ctx *ctx, const json_t *part) {
    if (!gem_is_obj(part)) { gem_raise_attr(ctx, part, "get"); return NULL; }
    json_t *fc = gem_get(part, "functionCall");
    if (gem_is_obj(fc) && gem_truthy(gem_get(fc, "name"))) return json_copy(fc);
    return json_null();
}

/* PoP: gem_tool_call_ns @ agent/gemini_native_adapter.py:_tool_call_ns */
static json_t *gem_tool_call_ns(const char *name, const char *arguments, int index,
                                const char *call_id, const json_t *extra_content) {
    json_t *r = json_object();
    json_set(r, "id", json_string(call_id ? call_id : ""));
    json_set(r, "type", json_string("function"));
    json_set(r, "index", json_number((double)index));
    json_t *fn = json_object();
    json_set(fn, "name", json_string(name ? name : ""));
    json_set(fn, "arguments", json_string(arguments ? arguments : ""));
    json_set(r, "function", fn);
    if (gem_is_obj(extra_content)) json_set(r, "extra_content", json_copy(extra_content));
    return r;
}

/* PoP: gem_error_info @ agent/gemini_native_adapter.py:_error_info */
static json_t *gem_error_info(gem_ctx *ctx, const json_t *err_obj) {
    if (!gem_is_obj(err_obj)) { gem_raise_attr(ctx, err_obj, "get"); return NULL; }
    char *reason = gem_dup_span("", 0);
    json_t *metadata = json_object();
    json_t *details = gem_get(err_obj, "details");
    if (gem_is_arr(details)) {
        for (size_t i = 0; i < details->c.count; i++) {
            json_t *detail = details->c.items[i];
            if (!gem_is_obj(detail) || reason[0] != '\0') continue;
            char *ty = gem_str_or_empty(gem_get(detail, "@type"));
            bool match = gem_str_ends(ty, "/google.rpc.ErrorInfo");
            free(ty);
            if (!match) continue;
            json_t *r = gem_get(detail, "reason");
            if (gem_is_str(r)) { free(reason); reason = gem_dup_span(gem_jstr(r), strlen(gem_jstr(r))); }
            json_t *m = gem_get(detail, "metadata");
            if (gem_is_obj(m)) { json_free(metadata); metadata = json_copy(m); }
        }
    }
    json_t *out = json_array();
    json_append(out, json_string(reason ? reason : ""));
    json_append(out, metadata);
    free(reason);
    return out;
}

/* PoP: gem_error_object @ agent/gemini_native_adapter.py:_error_object */
static json_t *gem_error_object(const char *body_text) {
    if (!body_text || !body_text[0]) return json_object();
    json_t *parsed = gem_json_loads(body_text);
    if (!parsed) return json_object();
    json_t *err = gem_is_obj(parsed) ? gem_get(parsed, "error") : NULL;
    json_t *r = gem_is_obj(err) ? json_copy(err) : json_object();
    json_free(parsed);
    return r;
}

/* PoP: gem_long_decode_cap @ agent/gemini_native_adapter.py:_long_decode_cap */
static json_t *gem_long_decode_cap(gem_ctx *ctx, const json_t *max_tokens, const json_t *extra_body) {
    if (!gem_is_obj(extra_body)) { gem_raise_attr(ctx, extra_body, "get"); return NULL; }
    json_t *budget = gem_get(extra_body, "max_output_tokens");
    /* isinstance(budget, int) gates FIRST (short-circuit): int(max_tokens or 0)
     * is only evaluated when the budget is an integral number. */
    if (gem_is_num(budget) && budget->num_val == floor(budget->num_val)) {
        const json_t *base = gem_truthy(max_tokens) ? max_tokens : NULL;
        long long mt = 0;
        if (base && !gem_pyint_ctx(ctx, base, &mt)) return NULL;
        if (budget->num_val > (double)mt) return json_copy(budget);
    }
    return max_tokens ? json_copy(max_tokens) : json_null();
}

/* ── seamed leaves ───────────────────────────────────────────────────────── */

/* PoP: gem_api_client @ agent/gemini_native_adapter.py:_api_client */
static json_t *gem_api_client(gem_ctx *ctx) {
    json_t *a = json_object();
    json_set(a, "product", json_string("hermes-agent"));
    json_t *r = gem_call(ctx, "hermes_user_agent", a);
    if (gem_exc_pending(ctx)) { if (r) json_free(r); return NULL; }
    return r ? r : json_null();
}

/* PoP: gem_response_text @ agent/gemini_native_adapter.py:_response_text */
static json_t *gem_response_text(gem_ctx *ctx, const json_t *response) {
    /* try: return response.text or "" except Exception: return "" */
    if (gem_is_obj(response)) {
        json_t *rm = gem_get(response, "__raise__");
        if (gem_is_str(rm)) {
            /* raising-text attribute double: the .text access raises */
            if (ctx->exc) json_free(ctx->exc);
            ctx->exc = json_copy(response);
            if (gem_exc_is_exception(ctx)) { gem_exc_clear(ctx); return json_string(""); }
            return NULL;   /* BaseException trio passes through */
        }
        json_t *text = gem_get(response, "text");
        if (!text) return json_string("");   /* AttributeError swallowed */
        return gem_truthy(text) ? json_copy(text) : json_string("");
    }
    return json_string("");   /* non-carrier .text AttributeError swallowed */
}

/* PoP: gem_wrong_gemini_surface_guidance @ agent/gemini_native_adapter.py:wrong_gemini_surface_guidance */
static json_t *gem_wrong_gemini_surface_guidance(gem_ctx *ctx, const json_t *base_url,
                                                 const json_t *api_key, const json_t *err_status) {
    const json_t *es = gem_truthy(err_status) ? err_status : NULL;
    if (es && !gem_is_str(es)) { gem_raise_attr(ctx, es, "strip"); return NULL; }
    char *stripped = gem_py_strip(es ? gem_jstr(es) : "");
    char *upper = gem_py_upper(stripped);
    free(stripped);
    bool denied = gem_str_eq(upper, "PERMISSION_DENIED");
    free(upper);
    if (!denied) return json_string("");
    char *norm = gem_str_or_empty(base_url);
    char *norm2 = gem_py_strip(norm);
    free(norm);
    char *normalized = gem_py_lower(norm2);
    free(norm2);
    json_t *a = json_object();
    json_set(a, "base_url", json_string(normalized));
    json_t *vr = gem_call(ctx, "is_vertex_express_base_url", a);
    if (gem_exc_pending(ctx)) { free(normalized); if (vr) json_free(vr); return NULL; }
    bool express = gem_truthy(vr);
    if (vr) json_free(vr);
    if (express) { free(normalized); return json_string(GEM_STUDIO_KEY_ON_EXPRESS_GUIDANCE); }
    bool studio_host = gem_str_has(normalized, "generativelanguage.googleapis.com");
    free(normalized);
    if (!studio_host) return json_string("");
    const json_t *ak = gem_truthy(api_key) ? api_key : NULL;
    if (ak && !gem_is_str(ak)) { gem_raise_attr(ctx, ak, "strip"); return NULL; }
    char *aks = gem_py_strip(ak ? gem_jstr(ak) : "");
    char *aku = gem_py_upper(aks);
    free(aks);
    bool aq = gem_str_starts(aku, "AQ.");
    free(aku);
    if (aq) return json_string(GEM_EXPRESS_KEY_ON_STUDIO_GUIDANCE);
    return json_string("");
}

/* PoP: gem_inline_data_part @ agent/gemini_native_adapter.py:_inline_data_part */
static json_t *gem_inline_data_part(gem_ctx *ctx, const json_t *item) {
    if (!gem_is_obj(item)) { gem_raise_attr(ctx, item, "get"); return NULL; }
    /* url = (item.get("image_url") or {}).get("url") or "" — BEFORE the type check */
    json_t *iu = gem_get(item, "image_url");
    if (!gem_truthy(iu)) iu = NULL;   /* or {} */
    if (iu && !gem_is_obj(iu)) { gem_raise_attr(ctx, iu, "get"); return NULL; }
    json_t *url_node = iu ? gem_get(iu, "url") : NULL;
    if (!gem_truthy(url_node)) url_node = NULL;   /* or "" */
    json_t *ty = gem_get(item, "type");
    if (!gem_is_str(ty) || !gem_str_eq(gem_jstr(ty), "image_url")
        || !gem_is_str(url_node) || !gem_str_starts(gem_jstr(url_node), "data:"))
        return json_null();
    /* try: header, encoded = url.split(",", 1); b64 roundtrip; mime splits */
    const char *url = gem_jstr(url_node);
    const char *comma = strchr(url, ',');
    if (!comma) return json_null();   /* ValueError swallowed */
    char *header = gem_dup_span(url, (size_t)(comma - url));
    const char *encoded = comma + 1;
    unsigned char *data = NULL;
    size_t dlen = 0;
    if (!gem_b64_decode(encoded, &data, &dlen)) { free(header); return json_null(); }
    char *b64 = gem_b64_encode(data, dlen);
    free(data);
    const char *colon = strchr(header, ':');
    if (!colon) { free(header); free(b64); return json_null(); }   /* IndexError swallowed */
    const char *mime_start = colon + 1;
    const char *semi = strchr(mime_start, ';');
    char *mime = gem_dup_span(mime_start, semi ? (size_t)(semi - mime_start) : strlen(mime_start));
    json_t *inner = json_object();
    json_set(inner, "mimeType", json_string(mime ? mime : ""));
    json_set(inner, "data", json_string(b64 ? b64 : ""));
    json_t *r = json_object();
    json_set(r, "inlineData", inner);
    free(header); free(b64); free(mime);
    return r;
}

/* PoP: gem_multimodal_part @ agent/gemini_native_adapter.py:_multimodal_part */
static json_t *gem_multimodal_part(gem_ctx *ctx, const json_t *item) {
    json_t *text = gem_text_of(item);
    if (gem_truthy(text) || gem_is_str(item)) {
        json_t *r = json_object();
        json_set(r, "text", text);   /* ownership transferred */
        return r;
    }
    if (text) json_free(text);
    if (gem_is_obj(item)) return gem_inline_data_part(ctx, item);
    return json_null();
}

/* PoP: gem_merge_alternating @ agent/gemini_native_adapter.py:_merge_alternating */
static json_t *gem_merge_alternating(gem_ctx *ctx, const json_t *contents) {
    if (!gem_is_arr(contents)) {
        /* live iterates `for content in contents` — non-list raises TypeError */
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", gem_typename(contents));
        gem_raise_msg(ctx, "TypeError", msg);
        return NULL;
    }
    json_t **mitems = NULL;   /* borrowed content nodes + owned placeholders */
    size_t mn = 0, mcap = 0;
    json_t **ph = NULL;       /* owned placeholder free-list */
    size_t phn = 0, phcap = 0;
    json_t *result = NULL;
    for (size_t ci = 0; ci < contents->c.count; ci++) {
        json_t *content = contents->c.items[ci];
        json_t *prev = mn > 0 ? mitems[mn - 1] : NULL;
        bool same_role = false;
        if (prev) {
            json_t *pr = gem_get(prev, "role");
            if (!pr) { gem_raise_key(ctx, "role"); goto fail; }
            json_t *cr = gem_get(content, "role");
            if (!cr) { gem_raise_key(ctx, "role"); goto fail; }
            same_role = gem_is_str(pr) && gem_is_str(cr) && gem_str_eq(gem_jstr(pr), gem_jstr(cr));
            if (same_role && gem_str_eq(gem_jstr(cr), "user")) {
                json_t *hp = gem_has_function_response(ctx, prev);
                if (gem_exc_pending(ctx)) { if (hp) json_free(hp); goto fail; }
                json_t *hc = gem_has_function_response(ctx, content);
                if (gem_exc_pending(ctx)) { json_free(hp); if (hc) json_free(hc); goto fail; }
                bool diff = gem_truthy(hp) != gem_truthy(hc);
                json_free(hp); json_free(hc);
                if (diff) {
                    same_role = false;
                    json_t *pholder = json_object();
                    json_set(pholder, "role", json_string("model"));
                    json_t *parts = json_array();
                    json_t *tp = json_object();
                    json_set(tp, "text", json_string(GEM_INTERRUPTED_RESPONSE_PLACEHOLDER));
                    json_append(parts, tp);
                    json_set(pholder, "parts", parts);
                    if (phn == phcap) { phcap = phcap ? phcap * 2 : 4; ph = (json_t **)realloc(ph, phcap * sizeof(json_t *)); }
                    ph[phn++] = pholder;
                    if (mn == mcap) { mcap = mcap ? mcap * 2 : 8; mitems = (json_t **)realloc(mitems, mcap * sizeof(json_t *)); }
                    mitems[mn++] = pholder;
                }
            }
        }
        if (same_role) {
            json_t *pparts = gem_get(prev, "parts");
            if (!pparts) { gem_raise_key(ctx, "parts"); goto fail; }
            json_t *cparts = gem_get(content, "parts");
            if (!cparts) { gem_raise_key(ctx, "parts"); goto fail; }
            if (gem_is_arr(pparts) && gem_is_arr(cparts))
                for (size_t k = 0; k < cparts->c.count; k++)
                    json_append(pparts, json_copy(cparts->c.items[k]));
        } else {
            if (mn == mcap) { mcap = mcap ? mcap * 2 : 8; mitems = (json_t **)realloc(mitems, mcap * sizeof(json_t *)); }
            mitems[mn++] = content;   /* borrowed */
        }
    }
    result = json_array();
    for (size_t i = 0; i < mn; i++) json_append(result, json_copy(mitems[i]));
    for (size_t i = 0; i < phn; i++) json_free(ph[i]);
    free(ph); free(mitems);
    return result;
fail:
    for (size_t i = 0; i < phn; i++) json_free(ph[i]);
    free(ph); free(mitems);
    if (result) json_free(result);
    return NULL;
}

/* PoP: gem_function_declaration @ agent/gemini_native_adapter.py:_function_declaration */
static json_t *gem_function_declaration(gem_ctx *ctx, const json_t *tool, bool json_schema) {
    /* fn = (tool.get("function") or {}) if isinstance(tool, dict) else None;
     * a falsy function value becomes {} (a dict), which then fails the name gate. */
    json_t *fn_eff = NULL;
    json_t *empty_obj = NULL;
    if (gem_is_obj(tool)) {
        json_t *f = gem_get(tool, "function");
        if (gem_truthy(f)) fn_eff = f;
        else { empty_obj = json_object(); fn_eff = empty_obj; }
    }
    if (!gem_is_obj(fn_eff)) { if (empty_obj) json_free(empty_obj); return json_null(); }
    json_t *name = gem_get(fn_eff, "name");
    if (!gem_is_str(name) || gem_jstr(name)[0] == '\0') { if (empty_obj) json_free(empty_obj); return json_null(); }
    json_t *decl = json_object();
    json_set(decl, "name", json_copy(name));
    json_t *desc = gem_get(fn_eff, "description");
    if (gem_is_str(desc) && gem_jstr(desc)[0] != '\0') json_set(decl, "description", json_copy(desc));
    json_t *params = gem_get(fn_eff, "parameters");
    if (gem_is_obj(params)) {
        json_t *a = json_object();
        json_set(a, "parameters", json_copy(params));
        json_t *prep = gem_call(ctx, json_schema ? "prepare_gemini_tool_parameters"
                                                 : "sanitize_gemini_tool_parameters", a);
        if (gem_exc_pending(ctx)) {
            if (prep) json_free(prep);
            json_free(decl); if (empty_obj) json_free(empty_obj);
            return NULL;
        }
        json_set(decl, json_schema ? "parametersJsonSchema" : "parameters",
                 prep ? prep : json_null());
    }
    if (empty_obj) json_free(empty_obj);
    return decl;
}

/* PoP: gem_translate_response_format @ agent/gemini_native_adapter.py:_translate_response_format */
static json_t *gem_translate_response_format(gem_ctx *ctx, const json_t *response_format, bool json_schema) {
    if (!gem_is_obj(response_format)) return json_object();
    json_t *ty = gem_get(response_format, "type");
    const char *t = gem_is_str(ty) ? gem_jstr(ty) : "";
    bool is_schema = gem_str_eq(t, "json_schema");
    if (!is_schema && !gem_str_eq(t, "json_object")) return json_object();
    json_t *spec = is_schema ? gem_get(response_format, "json_schema") : NULL;
    json_t *schema = gem_is_obj(spec) ? gem_get(spec, "schema") : NULL;
    json_t *r = json_object();
    if (!gem_is_obj(schema)) {
        json_set(r, "responseMimeType", json_string("application/json"));
        return r;
    }
    json_t *a = json_object();
    json_set(a, "parameters", json_copy(schema));
    json_t *prep = gem_call(ctx, json_schema ? "prepare_gemini_tool_parameters"
                                             : "sanitize_gemini_tool_parameters", a);
    if (gem_exc_pending(ctx)) { if (prep) json_free(prep); json_free(r); return NULL; }
    json_set(r, "responseMimeType", json_string("application/json"));
    json_set(r, json_schema ? "responseJsonSchema" : "responseSchema", prep ? prep : json_null());
    return r;
}

/* PoP: gem_new_call_id @ agent/gemini_native_adapter.py:_new_call_id */
static json_t *gem_new_call_id(gem_ctx *ctx, const json_t *fc) {
    json_t *pid = gem_provider_call_id(ctx, fc);
    if (gem_exc_pending(ctx)) { if (pid) json_free(pid); return NULL; }
    if (gem_truthy(pid)) return pid;
    if (pid) json_free(pid);
    json_t *ur = gem_call(ctx, "uuid4", json_object());
    if (gem_exc_pending(ctx)) { if (ur) json_free(ur); return NULL; }
    if (!gem_is_str(ur)) {
        gem_raise_attr(ctx, ur, "hex");
        if (ur) json_free(ur);
        return NULL;
    }
    const char *hex = gem_jstr(ur);
    size_t hl = strlen(hex);
    char id[32];
    snprintf(id, sizeof id, "call_%.*s", (int)(hl < 12 ? hl : 12), hex);
    json_free(ur);
    return json_string(id);
}

/* PoP: gem_dump_call_args @ agent/gemini_native_adapter.py:_dump_call_args */
static json_t *gem_dump_call_args(gem_ctx *ctx, const json_t *fc, const json_t *kwargs) {
    if (!gem_is_obj(fc)) { gem_raise_attr(ctx, fc, "get"); return NULL; }
    /* json.dumps(..., ensure_ascii=False, **kwargs): a duplicate ensure_ascii or
     * an unknown keyword raises TypeError -> caught -> "{}". Known kwargs other
     * than sort_keys are outside the fixture domain. */
    if (gem_is_obj(kwargs)) {
        static const char *KNOWN[] = {"skipkeys", "ensure_ascii", "check_circular", "allow_nan",
                                      "cls", "indent", "separators", "default", "sort_keys"};
        for (size_t i = 0; i < kwargs->c.count; i++) {
            const char *k = kwargs->c.keys[i];
            if (gem_str_eq(k, "ensure_ascii")) return json_string("{}");
            bool known = false;
            for (size_t j = 0; j < sizeof(KNOWN) / sizeof(KNOWN[0]); j++)
                if (gem_str_eq(k, KNOWN[j])) { known = true; break; }
            if (!known) return json_string("{}");
        }
    }
    bool sort_keys = gem_is_obj(kwargs) && gem_truthy(gem_get(kwargs, "sort_keys"));
    json_t *args = gem_get(fc, "args");
    json_t *empty = NULL;
    if (!gem_truthy(args)) { empty = json_object(); args = empty; }
    char *s = gem_dumps(args, sort_keys);
    json_t *r = json_string(s ? s : "{}");
    free(s);
    if (empty) json_free(empty);
    return r;
}

/* PoP: gem_usage_from_metadata @ agent/gemini_native_adapter.py:_usage_from_metadata */
static json_t *gem_usage_from_metadata(gem_ctx *ctx, const json_t *usage_meta) {
    if (!gem_is_obj(usage_meta)) { gem_raise_attr(ctx, usage_meta, "get"); return NULL; }
    long long prompt = 0, candidates = 0, total = 0, thoughts = 0, cached = 0;
    long long init_prompt = 0, init_cached = 0;
    json_t *zero = json_number(0);   /* the `or 0` fallback */
    json_t *v;
    /* live evaluation order: thoughts first, then the initial pair (when
     * present), then the SimpleNamespace kwargs left to right. */
    v = gem_get(usage_meta, "thoughtsTokenCount");
    if (!gem_pyint_ctx(ctx, gem_truthy(v) ? v : zero, &thoughts)) { json_free(zero); return NULL; }
    bool has_initial = json_has(usage_meta, "initialPromptTokenCount");
    if (has_initial) {
        v = gem_get(usage_meta, "initialPromptTokenCount");
        if (!gem_pyint_ctx(ctx, gem_truthy(v) ? v : zero, &init_prompt)) { json_free(zero); return NULL; }
        v = gem_get(usage_meta, "initialCachedContentTokenCount");
        if (!gem_pyint_ctx(ctx, gem_truthy(v) ? v : zero, &init_cached)) { json_free(zero); return NULL; }
    }
    v = gem_get(usage_meta, "promptTokenCount");
    if (!gem_pyint_ctx(ctx, gem_truthy(v) ? v : zero, &prompt)) { json_free(zero); return NULL; }
    v = gem_get(usage_meta, "candidatesTokenCount");
    if (!gem_pyint_ctx(ctx, gem_truthy(v) ? v : zero, &candidates)) { json_free(zero); return NULL; }
    v = gem_get(usage_meta, "totalTokenCount");
    if (!gem_pyint_ctx(ctx, gem_truthy(v) ? v : zero, &total)) { json_free(zero); return NULL; }
    v = gem_get(usage_meta, "cachedContentTokenCount");
    if (!gem_pyint_ctx(ctx, gem_truthy(v) ? v : zero, &cached)) { json_free(zero); return NULL; }
    json_free(zero);
    json_t *r = json_object();
    json_set(r, "prompt_tokens", json_number((double)prompt));
    json_set(r, "completion_tokens", json_number((double)(candidates + thoughts)));
    json_set(r, "total_tokens", json_number((double)total));
    json_t *pd = json_object();
    json_set(pd, "cached_tokens", json_number((double)cached));
    json_set(r, "prompt_tokens_details", pd);
    json_t *cd = json_object();
    json_set(cd, "reasoning_tokens", json_number((double)thoughts));
    json_set(r, "completion_tokens_details", cd);
    if (has_initial) {
        json_set(r, "initial_prompt_tokens", json_number((double)init_prompt));
        json_set(r, "initial_cached_tokens", json_number((double)init_cached));
    }
    return r;
}

/* PoP: gem_envelope @ agent/gemini_native_adapter.py:_envelope */
static json_t *gem_envelope(gem_ctx *ctx, const char *model, const char *object_,
                            const json_t *choice, const json_t *usage) {
    json_t *ur = gem_call(ctx, "uuid4", json_object());
    if (gem_exc_pending(ctx)) { if (ur) json_free(ur); return NULL; }
    if (!gem_is_str(ur)) { gem_raise_attr(ctx, ur, "hex"); if (ur) json_free(ur); return NULL; }
    const char *hex = gem_jstr(ur);
    size_t hl = strlen(hex);
    char id[32];
    snprintf(id, sizeof id, "chatcmpl-%.*s", (int)(hl < 12 ? hl : 12), hex);
    json_free(ur);
    json_t *tr = gem_call(ctx, "time_time", json_object());
    if (gem_exc_pending(ctx)) { if (tr) json_free(tr); return NULL; }
    long long created = 0;
    if (!gem_pyint_ctx(ctx, tr, &created)) { if (tr) json_free(tr); return NULL; }
    if (tr) json_free(tr);
    json_t *r = json_object();
    json_set(r, "id", json_string(id));
    json_set(r, "object", json_string(object_ ? object_ : ""));
    json_set(r, "created", json_number((double)created));
    json_set(r, "model", json_string(model ? model : ""));
    json_t *choices = json_array();
    json_append(choices, choice ? json_copy(choice) : json_null());
    json_set(r, "choices", choices);
    json_set(r, "usage", usage ? json_copy(usage) : json_null());
    return r;
}

/* PoP: gem_raise_on_prompt_block @ agent/gemini_native_adapter.py:raise_on_prompt_block */
static json_t *gem_raise_on_prompt_block(gem_ctx *ctx, const json_t *payload) {
    if (!gem_is_obj(payload)) { gem_raise_attr(ctx, payload, "get"); return NULL; }
    json_t *feedback = gem_get(payload, "promptFeedback");
    json_t *reason = gem_is_obj(feedback) ? gem_get(feedback, "blockReason") : NULL;
    if (!gem_truthy(reason)) return json_null();
    char *rs = gem_pystr(reason);
    char *reason_up = gem_py_upper(rs);
    free(rs);
    json_t *message = gem_get(feedback, "blockReasonMessage");
    char *guidance = NULL;
    bool owned_guidance = false;
    if (gem_is_str(message)) {
        char *ms = gem_py_strip(gem_jstr(message));
        if (ms && ms[0] != '\0') { guidance = ms; owned_guidance = true; }
        else free(ms);
    }
    if (!guidance) guidance = (char *)gem_block_guidance(reason_up);
    char msg[1024];
    snprintf(msg, sizeof msg, "Gemini blocked this prompt (blockReason=%s): %s", reason_up, guidance);
    if (owned_guidance) free(guidance);
    free(reason_up);
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string("GeminiAPIError"));
    json_set(ctx->exc, "msg", json_string(msg));
    json_set(ctx->exc, "code", json_string("gemini_prompt_blocked"));
    json_set(ctx->exc, "status_code", json_null());
    json_set(ctx->exc, "retry_after", json_null());
    json_t *details = json_object();
    json_set(details, "promptFeedback", json_copy(feedback));
    json_set(ctx->exc, "details", details);
    return NULL;
}

/* PoP: gem_parse_sse_line @ agent/gemini_native_adapter.py:_parse_sse_line */
static json_t *gem_parse_sse_line(gem_ctx *ctx, const char *line) {
    if (!line) { gem_raise_attr(ctx, NULL, "rstrip"); return NULL; }
    size_t n = strlen(line);
    while (n > 0 && line[n - 1] == '\r') n--;   /* rstrip("\r") */
    char *stripped = gem_dup_span(line, n);
    if (!gem_str_starts(stripped, "data: ")) { free(stripped); return json_null(); }
    const char *data = stripped + 6;
    if (gem_str_eq(data, "[DONE]")) { free(stripped); return json_string("__SSE_DONE__"); }
    json_t *payload = gem_json_loads(data);
    if (!payload) {
        /* logger.debug("Non-JSON Gemini SSE line: %s", data[:200]) */
        size_t dl = strlen(data);
        char *arg = gem_dup_span(data, dl < 200 ? dl : 200);
        json_t *a = json_object();
        json_set(a, "msg", json_string("Non-JSON Gemini SSE line: %s"));
        json_set(a, "arg", json_string(arg ? arg : ""));
        free(arg);
        json_t *lr = gem_call(ctx, "logger_debug", a);
        if (lr) json_free(lr);
        free(stripped);
        if (gem_exc_pending(ctx)) return NULL;
        return json_null();
    }
    free(stripped);
    if (!gem_is_obj(payload)) { json_free(payload); return json_null(); }
    return payload;
}

/* PoP: gem_tool_call_slot @ agent/gemini_native_adapter.py:_tool_call_slot */
static json_t *gem_tool_call_slot(gem_ctx *ctx, const json_t *fc, const json_t *part,
                                  int part_index, const char *args_str,
                                  const json_t *tool_call_indices) {
    json_t *pid = gem_provider_call_id(ctx, fc);
    if (gem_exc_pending(ctx)) { if (pid) json_free(pid); return NULL; }
    char *key = NULL;
    json_t *slot = NULL;
    json_t *result = NULL;
    if (gem_truthy(pid)) {
        json_t *ko = json_object();
        json_set(ko, "provider_call_id", pid);   /* ownership transferred */
        pid = NULL;
        key = gem_dumps(ko, true);
        json_free(ko);
        if (!gem_is_obj(tool_call_indices)) { gem_raise_attr(ctx, tool_call_indices, "get"); free(key); return NULL; }
        slot = gem_get(tool_call_indices, key);
        result = json_array();
        json_append(result, json_string(key ? key : ""));
        json_append(result, slot ? json_copy(slot) : json_null());
        free(key);
        return result;
    }
    if (pid) json_free(pid);
    if (!gem_is_obj(part)) { gem_raise_attr(ctx, part, "get"); return NULL; }
    json_t *sig = gem_get(part, "thoughtSignature");
    const char *sig_str = gem_is_str(sig) ? gem_jstr(sig) : "";
    json_t *name = gem_get(fc, "name");
    if (!name) { gem_raise_key(ctx, "name"); return NULL; }
    json_t *ko = json_object();
    json_set(ko, "part_index", json_number((double)part_index));
    json_set(ko, "name", json_copy(name));
    json_set(ko, "thought_signature", json_string(sig_str));
    key = gem_dumps(ko, true);
    json_free(ko);
    if (!gem_is_obj(tool_call_indices)) { gem_raise_attr(ctx, tool_call_indices, "get"); free(key); return NULL; }
    slot = gem_get(tool_call_indices, key);
    bool prefix = false;
    if (slot) {
        json_t *la = gem_get(slot, "last_arguments");
        if (!la) { gem_raise_key(ctx, "last_arguments"); free(key); return NULL; }
        if (!gem_is_str(la)) {
            char msg[256];
            snprintf(msg, sizeof msg, "startswith first arg must be str or a tuple of str, not %s",
                     gem_typename(la));
            gem_raise_msg(ctx, "TypeError", msg);
            free(key);
            return NULL;
        }
        prefix = gem_str_starts(args_str ? args_str : "", gem_jstr(la));
    }
    if (!slot || prefix) {
        result = json_array();
        json_append(result, json_string(key ? key : ""));
        json_append(result, slot ? json_copy(slot) : json_null());
        free(key);
        return result;
    }
    /* scan key#N siblings in insertion order */
    size_t kl = strlen(key);
    char *prefix_key = (char *)malloc(kl + 2);
    if (prefix_key) { memcpy(prefix_key, key, kl); prefix_key[kl] = '#'; prefix_key[kl + 1] = '\0'; }
    for (size_t i = 0; i < tool_call_indices->c.count; i++) {
        const char *ok = tool_call_indices->c.keys[i];
        if (!prefix_key || !gem_str_starts(ok, prefix_key)) continue;
        json_t *other = tool_call_indices->c.items[i];
        json_t *la = gem_get(other, "last_arguments");
        if (!la) { gem_raise_key(ctx, "last_arguments"); free(prefix_key); free(key); return NULL; }
        if (!gem_is_str(la)) {
            char msg[256];
            snprintf(msg, sizeof msg, "startswith first arg must be str or a tuple of str, not %s",
                     gem_typename(la));
            gem_raise_msg(ctx, "TypeError", msg);
            free(prefix_key); free(key);
            return NULL;
        }
        if (gem_str_starts(args_str ? args_str : "", gem_jstr(la))) {
            result = json_array();
            json_append(result, json_string(ok));
            json_append(result, json_copy(other));
            free(prefix_key); free(key);
            return result;
        }
    }
    free(prefix_key);
    char nk[512];
    snprintf(nk, sizeof nk, "%s#%zu", key ? key : "", tool_call_indices->c.count);
    result = json_array();
    json_append(result, json_string(nk));
    json_append(result, json_null());
    free(key);
    return result;
}

/* ── GeminiNativeClient methods ──────────────────────────────────────────── */

/* PoP: gem_auth_headers @ agent/gemini_native_adapter.py:_auth_headers */
static json_t *gem_auth_headers(gem_ctx *ctx, const json_t *slf) {
    /* self double is a SimpleNamespace; direct self.api_key read */
    json_t *api_key = gem_is_obj(slf) ? gem_get(slf, "api_key") : NULL;
    if (!api_key) {
        gem_raise_msg(ctx, "AttributeError",
                      "'types.SimpleNamespace' object has no attribute 'api_key'");
        return NULL;
    }
    json_t *r = json_object();
    json_set(r, "x-goog-api-key", json_copy(api_key));
    return r;
}

/* PoP: gem_http_error_method @ agent/gemini_native_adapter.py:_http_error */
static json_t *gem_http_error_method(gem_ctx *ctx, const json_t *slf, const json_t *response,
                                     const json_t *body_text) {
    json_t *api_key = gem_is_obj(slf) ? gem_get(slf, "api_key") : NULL;
    if (!api_key) {
        gem_raise_msg(ctx, "AttributeError",
                      "'types.SimpleNamespace' object has no attribute 'api_key'");
        return NULL;
    }
    json_t *base_url = gem_is_obj(slf) ? gem_get(slf, "base_url") : NULL;
    if (!base_url) {
        gem_raise_msg(ctx, "AttributeError",
                      "'types.SimpleNamespace' object has no attribute 'base_url'");
        return NULL;
    }
    json_t *a = json_object();
    gem_arg_node(a, "response", response);
    gem_arg_node(a, "body_text", body_text);
    json_set(a, "api_key", json_copy(api_key));
    json_set(a, "base_url", json_copy(base_url));
    json_t *r = gem_call(ctx, "gemini_http_error", a);
    if (gem_exc_pending(ctx)) { if (r) json_free(r); return NULL; }
    /* exception descriptor -> returned GeminiAPIError projection */
    if (gem_is_obj(r) && gem_is_str(gem_get(r, "message"))) {
        json_t *out = json_object();
        json_set(out, "__raise__", json_string("GeminiAPIError"));
        json_set(out, "msg", json_copy(gem_get(r, "message")));
        json_free(r);
        return out;
    }
    return r ? r : json_null();
}

/* PoP: gem_effective_output_cap @ agent/gemini_native_adapter.py:effective_output_cap */
static json_t *gem_effective_output_cap(gem_ctx *ctx, const json_t *max_tokens, const json_t *extra_body) {
    json_t *empty = NULL;
    const json_t *extra = extra_body;
    if (!gem_is_obj(extra)) { empty = json_object(); extra = empty; }
    json_t *cap = gem_long_decode_cap(ctx, max_tokens, extra);
    if (gem_exc_pending(ctx)) { if (cap) json_free(cap); if (empty) json_free(empty); return NULL; }
    json_t *cfg = gem_get(extra, "thinking_config");
    if (!gem_truthy(cfg)) cfg = gem_get(extra, "thinkingConfig");
    json_t *a = json_object();
    gem_arg_node(a, "max_tokens", cap);
    gem_arg_node(a, "thinking_config", cfg);
    json_t *r = gem_call(ctx, "_effective_gemini_max_output_tokens", a);
    if (cap) json_free(cap);
    if (empty) json_free(empty);
    if (gem_exc_pending(ctx)) { if (r) json_free(r); return NULL; }
    return r ? r : json_null();
}