/* bedrock_transport_flow.c — FLOW + leaves port of the UNCLAIMED remainder of
 * the Bedrock Converse transport data path. The task names
 * agent/transports/bedrock.py; that module EXISTS in this quarry snapshot but
 * is a thin delegation shell whose every method (api_mode, convert_messages,
 * convert_tools, build_kwargs, normalize_response, validate_response,
 * map_finish_reason) is already CLAIMED by transports_bedrock_ports.c
 * (brt_*). The UNCLAIMED functions of the transport's data path live in the
 * module it delegates to, agent/bedrock_adapter.py ("AWS Bedrock Converse API
 * adapter"), so live defs are ported from there and the PoP lines credit that
 * REAL defining module. The claimed siblings (parse_bedrock_openai_model_id,
 * _decode_redacted, _parse_tool_args, _converse_replay_policy,
 * strip_redacted_reasoning, is_anthropic_bedrock_model,
 * _get_bedrock_control_client, resolve_bedrock_region,
 * _extract_provider_from_arn) are SEAMED here; ported functions call each
 * other REAL (internal bt2_* composition: _anthropic_ordered_blocks ->
 * _reasoning_text_block / _redacted_reasoning_block / _tool_use_block;
 * reasoning_minted_outside_claude -> _issuer -> _minted_outside_claude ->
 * _resolve_inference_profile_model_id; _apply_converse_reasoning ->
 * _claude_converse_reasoning / _nova_converse_reasoning; _list_foundation_models
 * / _list_inference_profiles -> _model_entry).
 *
 * Decision-sequence port of the reasoning-replay policy leaves, the
 * cachePoint / redacted-reasoning self-heal helpers, the block builders and
 * persisted-reasoning projection, the Converse reasoning-config builders, the
 * client-cache-free control-plane discovery leaves, the inference-profile
 * resolution flow, and the Mantle model-id gate.
 *
 * ALL collaborator I/O (claimed siblings, foreign-module imports, the module
 * logger, boto3 client methods) rides the injected universal seam bt2_call;
 * the harness scripts results and records the ordered trace the oracle diffs
 * against LIVE agent/bedrock_adapter.py under the same scripts.
 *
 * Seam name table (name -> live call):
 *   parse_bedrock_openai_model_id   CLAIMED sibling (args {model_id}); result
 *                                   null or a 2-array [normalized, prefix]
 *                                   materialized to a tuple on the live side
 *   _decode_redacted                CLAIMED sibling (args {encoded}); result
 *                                   null or a BYTES MARKER {"__bytes__":[...]}
 *                                   materialized to real bytes live
 *   _parse_tool_args                CLAIMED sibling (args {args})
 *   _converse_replay_policy         CLAIMED sibling (args {model}); result str
 *   strip_redacted_reasoning        CLAIMED sibling (args {kwargs}); result
 *                                   {"__same__":true} -> the SAME kwargs object
 *                                   (identity), any other value -> a new dict
 *   logger_warning                  module logger.warning(fmt, arg) — args
 *                                   {msg, arg}; args stringified (live %s)
 *   logger_debug                    module logger.debug(fmt, a1, a2) — args
 *                                   {msg, arg1, arg2}; arg2 is str(exc)
 *   _thinking_kwargs                agent.anthropic_adapter (in-function
 *                                   import; args {reasoning_config, model,
 *                                   max_tokens}); result dict or null
 *   is_anthropic_bedrock_model      CLAIMED sibling (args {model_id})
 *   _get_bedrock_control_client     CLAIMED sibling (args {region}); result
 *                                   null -> None client, any other value ->
 *                                   a recording client double
 *   get_inference_profile           client method (args
 *                                   {inferenceProfileIdentifier})
 *   resolve_bedrock_region          CLAIMED sibling (args {})
 *   list_foundation_models          client method (args {})
 *   list_inference_profiles         client method (args {nextToken} when set)
 *   _extract_provider_from_arn      CLAIMED sibling (args {arn})
 *
 * Representation contract (shared with tests/oracle_bedrock_transport_flow.py):
 *   - scripts: MISSING key -> fail-closed RuntimeError("no script for <name>");
 *     null -> None; {"$seq":[...]} per-call queue (exhaustion raises
 *     RuntimeError("script exhausted: <name>")); {"__raise__":cls,"msg":m}
 *     raises (trace ret is the WHOLE marker); any other value reused (a bare
 *     ARRAY is a LIST RESULT, not a queue). None-valued seam arg keys dropped
 *     on BOTH sides.
 *   - tuple -> array; None -> JSON null; integral floats collapse to ints on
 *     both sides; bytes/bytearray -> {"__bytes__":[int,...]}; SimpleNamespace
 *     results project to dicts in live key order; uncaught exception ->
 *     {"__raise__":cls,"msg":str(e)}.
 *   - except Exception sites (recover_from_redacted_reasoning_rejection has
 *     none; _resolve_inference_profile_model_id's try/except Exception) swallow
 *     the matching family; the BaseException trio (SystemExit/KeyboardInterrupt/
 *     GeneratorExit) passes through. Full builtin exception-parent table below.
 *   - module state (_CACHE_POINT_REJECTIONS, _inference_profile_model_cache)
 *     rides ctx->state, seeded from in.state and echoed POST-CALL for the keys
 *     PRESENT in the fixture state only; rejection placement sets echo sorted.
 *   - mutation windows: _list_foundation_models / _list_inference_profiles
 *     echo {"models":...}; _apply_converse_reasoning echoes
 *     {"kwargs":...,"inference_config":...} (the mutated input dicts).
 *
 * Fixture domain (ASCII scope):
 *   - strings ASCII (str.strip/lower/upper transcribed byte-wise; str.strip set
 *     is White_Space incl \x1c-\x1f). Non-str scalar AttributeError/TypeError
 *     raise positions use int/bool/str/list/null only (no floats — JSON numbers
 *     lose int/float identity for the type name).
 *   - _tool_call_ns input_dict: str/int/bool/null/list/dict only (no floats —
 *     json.dumps renders 1.0 vs 1 differently).
 *   - _nova_converse_reasoning effort / _model_entry name: str/int/bool/null
 *     (str() domain; no floats — str(2.0) is "2.0", not "2").
 *   - _encode_redacted bytes markers carry 0..255 ints only.
 *   - _decode_redacted seam results: null or {"__bytes__":[0..255 ints]}.
 *   - parse_bedrock_openai_model_id seam results: null or arrays (2 elems =
 *     (normalized, prefix); 1 elem -> IndexError "tuple index out of range").
 *   - _thinking_kwargs seam results: dict or null (null -> TypeError on the
 *     `k in thinking` membership).
 *   - _filter_reasoning keep: "_verbatim"/"_without_reasoning"/
 *     "_unsigned_reasoning" (materialized to the live ported fn), null or an
 *     unknown str (TypeError not callable); blocks: list of dicts or null/int
 *     (TypeError not iterable).
 *   - _anthropic_ordered_blocks tool_calls[].id: str only (dict keys); block
 *     ids may be any scalar (non-str args.get keys always miss).
 *   - recover_from_redacted_reasoning_rejection exc: a message string (null ->
 *     "None"), materialized to RuntimeError(msg) live; kwargs: dict or null.
 *   - client inputs / _get_bedrock_control_client results: null -> None, any
 *     other value -> recording double (null client raises AttributeError on
 *     the method name BEFORE the method seam fires).
 *   - _list_* summaries/profiles/models entries: dicts; filter_set: array of
 *     lowercase strings (live set) or null (empty).
 *   - _resolve_inference_profile_model_id profile_arn: str (list/dict ->
 *     unhashable TypeError; int/bool/null -> regex TypeError).
 *
 * libjson-only (default oracle deps suffice). Self-contained: the harness
 * #includes "agent/transports/bedrock_transport_flow.c".
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

/* ── context + seam plumbing ─────────────────────────────────────────────── */

typedef struct bt2_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
    json_t *state;     /* owned: module-state carrier (seeded from in.state) */
} bt2_ctx;

static bool bt2_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool bt2_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool bt2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool bt2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool bt2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool bt2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool bt2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
/* `x is True` / `x is False` identity gates (JSON_BOOL only). */
static bool bt2_is_true(const json_t *v) { return v && v->type == JSON_BOOL && v->bool_val; }
static bool bt2_is_false(const json_t *v) { return v && v->type == JSON_BOOL && !v->bool_val; }

static bool bt2_truthy(const json_t *v) {
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
static const char *bt2_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
/* borrowed object field or NULL */
static json_t *bt2_get(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}

/* ── string helpers (caller frees) ───────────────────────────────────────── */

static char *bt2_dup_span(const char *s, size_t len) {
    char *r = (char *)malloc(len + 1);
    if (!r) return NULL;
    memcpy(r, s, len);
    r[len] = '\0';
    return r;
}
static bool bt2_is_ws(char c) {
    unsigned char u = (unsigned char)c;
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r'
        || (u >= 0x1c && u <= 0x1f);
}
static char *bt2_py_strip(const char *s) {
    if (!s) return bt2_dup_span("", 0);
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && bt2_is_ws(s[a])) a++;
    while (b > a && bt2_is_ws(s[b - 1])) b--;
    return bt2_dup_span(s + a, b - a);
}
static char *bt2_py_lower(const char *s) {
    char *r = bt2_dup_span(s ? s : "", strlen(s ? s : ""));
    if (!r) return NULL;
    for (char *p = r; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    return r;
}
static char *bt2_py_upper(const char *s) {
    char *r = bt2_dup_span(s ? s : "", strlen(s ? s : ""));
    if (!r) return NULL;
    for (char *p = r; *p; p++)
        if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 'a' + 'A');
    return r;
}
/* str(v) for a scalar (null -> "None"; no-float domain: numbers read as int). */
static char *bt2_pystr(const json_t *v) {
    if (bt2_is_none(v)) return bt2_dup_span("None", 4);
    if (bt2_is_bool(v)) return bt2_dup_span(v->bool_val ? "True" : "False", v->bool_val ? 4 : 5);
    if (bt2_is_num(v)) {
        char buf[64]; double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
        else snprintf(buf, sizeof buf, "%g", d);
        return bt2_dup_span(buf, strlen(buf));
    }
    if (bt2_is_str(v)) return bt2_dup_span(bt2_jstr(v), strlen(bt2_jstr(v)));
    return bt2_dup_span("", 0);
}
/* Python type name of a JSON value (no-float domain: numbers read as int). */
static const char *bt2_typename(const json_t *v) {
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
static bool bt2_str_has(const char *hay, const char *needle) {
    return strstr(hay ? hay : "", needle) != NULL;
}
/* case-insensitive literal prefix at s[i] (ASCII). */
static bool bt2_ci_at(const char *s, size_t i, const char *lit) {
    for (size_t k = 0; lit[k]; k++) {
        char c = s[i + k];
        if (!c) return false;
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        char l = lit[k];
        if (l >= 'A' && l <= 'Z') l = (char)(l - 'A' + 'a');
        if (c != l) return false;
    }
    return true;
}

/* ── exception plumbing (full builtin hierarchy) ─────────────────────────── */

static bool bt2_exc_pending(const bt2_ctx *ctx) { return ctx && ctx->exc != NULL; }
static void bt2_exc_clear(bt2_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}
static void bt2_raise_msg(bt2_ctx *ctx, const char *cls, const char *msg) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
    json_set(ctx->exc, "msg", json_string(msg ? msg : ""));
}
static void bt2_raise_attr(bt2_ctx *ctx, const json_t *recv, const char *attr) {
    char msg[256];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", bt2_typename(recv), attr);
    bt2_raise_msg(ctx, "AttributeError", msg);
}
static void bt2_raise_key(bt2_ctx *ctx, const char *key) {
    char msg[256];
    snprintf(msg, sizeof msg, "'%s'", key);
    bt2_raise_msg(ctx, "KeyError", msg);
}
/* py3.14: `x in non-container` TypeError. */
static void bt2_raise_not_container(bt2_ctx *ctx, const json_t *v) {
    char msg[256];
    snprintf(msg, sizeof msg, "argument of type '%s' is not a container or iterable", bt2_typename(v));
    bt2_raise_msg(ctx, "TypeError", msg);
}
/* iteration / extend / map TypeError. */
static void bt2_raise_not_iterable(bt2_ctx *ctx, const json_t *v) {
    char msg[256];
    snprintf(msg, sizeof msg, "'%s' object is not iterable", bt2_typename(v));
    bt2_raise_msg(ctx, "TypeError", msg);
}
/* re.search/match on a non-str. */
static void bt2_raise_re_type(bt2_ctx *ctx, const json_t *v) {
    char msg[256];
    snprintf(msg, sizeof msg, "expected string or bytes-like object, got '%s'", bt2_typename(v));
    bt2_raise_msg(ctx, "TypeError", msg);
}
/* py3.14 unhashable dict-key TypeError. */
static void bt2_raise_unhashable(bt2_ctx *ctx, const json_t *v) {
    char msg[256];
    snprintf(msg, sizeof msg, "cannot use '%s' as a dict key (unhashable type: '%s')",
             bt2_typename(v), bt2_typename(v));
    bt2_raise_msg(ctx, "TypeError", msg);
}
static void bt2_raise_assign(bt2_ctx *ctx, const json_t *v) {
    char msg[256];
    snprintf(msg, sizeof msg, "'%s' object does not support item assignment", bt2_typename(v));
    bt2_raise_msg(ctx, "TypeError", msg);
}
static const char *bt2_exc_class(const bt2_ctx *ctx) {
    return json_get_str(ctx->exc, "__raise__", "");
}
static const char *bt2_exc_msg(const bt2_ctx *ctx) {
    return json_get_str(ctx->exc, "msg", "");
}
/* FULL builtin exception subclass chain (parent lookup). */
static const char *bt2_exc_parent(const char *c) {
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
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (bt2_str_eq(T[i].c, c)) return T[i].p;
    return NULL;
}
static bool bt2_exc_matches(const bt2_ctx *ctx, const char *const *classes, size_t n) {
    if (!bt2_exc_pending(ctx)) return false;
    const char *cls = bt2_exc_class(ctx);
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (bt2_str_eq(cls, classes[i])) return true;
        const char *p = bt2_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}
/* except Exception (NOT the BaseException trio). */
static bool bt2_exc_is_exception(const bt2_ctx *ctx) {
    static const char *E[] = {"Exception"};
    return bt2_exc_matches(ctx, E, 1);
}

/* ── universal seam (fail-closed) ────────────────────────────────────────── */

/* args ownership is TRANSFERRED (NULL -> empty object). Result is NEW. */
static json_t *bt2_call(bt2_ctx *ctx, const char *name, json_t *args) {
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
static void bt2_arg_node(json_t *args, const char *key, const json_t *val) {
    if (!bt2_is_none(val)) json_set(args, key, json_copy(val));
}

/* ── Python json.dumps emulator (default separators, ASCII scope) ────────── */

typedef struct bt2_buf { char *s; size_t len, cap; } bt2_buf;
static void bt2_buf_init(bt2_buf *b) { b->cap = 128; b->len = 0; b->s = (char *)malloc(b->cap); if (b->s) b->s[0] = '\0'; }
static void bt2_buf_put(bt2_buf *b, const char *s, size_t n) {
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
static void bt2_buf_puts(bt2_buf *b, const char *s) { bt2_buf_put(b, s, strlen(s)); }
static void bt2_buf_putc(bt2_buf *b, char c) { bt2_buf_put(b, &c, 1); }

static void bt2_dump_str(bt2_buf *b, const char *s) {
    static const char H[] = "0123456789abcdef";
    bt2_buf_putc(b, '"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        switch (c) {
            case '"': bt2_buf_puts(b, "\\\""); break;
            case '\\': bt2_buf_puts(b, "\\\\"); break;
            case '\b': bt2_buf_puts(b, "\\b"); break;
            case '\f': bt2_buf_puts(b, "\\f"); break;
            case '\n': bt2_buf_puts(b, "\\n"); break;
            case '\r': bt2_buf_puts(b, "\\r"); break;
            case '\t': bt2_buf_puts(b, "\\t"); break;
            default:
                if (c < 0x20) {
                    bt2_buf_puts(b, "\\u00");
                    bt2_buf_putc(b, H[c >> 4]);
                    bt2_buf_putc(b, H[c & 15]);
                } else bt2_buf_putc(b, (char)c);
        }
    }
    bt2_buf_putc(b, '"');
}
static void bt2_dumps_rec(bt2_buf *b, const json_t *v) {
    if (!v || v->type == JSON_NULL) { bt2_buf_puts(b, "null"); return; }
    switch (v->type) {
        case JSON_BOOL: bt2_buf_puts(b, v->bool_val ? "true" : "false"); return;
        case JSON_NUMBER: {
            char buf[64]; double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
            else snprintf(buf, sizeof buf, "%.17g", d);   /* floats outside domain */
            bt2_buf_puts(b, buf);
            return;
        }
        case JSON_STRING: bt2_dump_str(b, bt2_jstr(v)); return;
        case JSON_ARRAY:
            bt2_buf_putc(b, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) bt2_buf_puts(b, ", ");
                bt2_dumps_rec(b, v->c.items[i]);
            }
            bt2_buf_putc(b, ']');
            return;
        case JSON_OBJECT:
            bt2_buf_putc(b, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) bt2_buf_puts(b, ", ");
                bt2_dump_str(b, v->c.keys[i]);
                bt2_buf_puts(b, ": ");
                bt2_dumps_rec(b, v->c.items[i]);
            }
            bt2_buf_putc(b, '}');
            return;
        default: bt2_buf_puts(b, "null"); return;
    }
}
/* json.dumps(v) — caller frees. */
static char *bt2_dumps(const json_t *v) {
    bt2_buf b; bt2_buf_init(&b);
    bt2_dumps_rec(&b, v);
    return b.s ? b.s : bt2_dup_span("", 0);
}

/* ── base64 encode (canonical, for _encode_redacted) ─────────────────────── */

static char *bt2_b64_encode(const unsigned char *data, size_t len) {
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

/* ── regex transcriptions (verbatim live patterns) ───────────────────────── */

/* _APPLICATION_PROFILE_ARN_RE = re.compile(r":application-inference-profile/") */
static bool bt2_app_profile_arn(const char *s) {
    return bt2_str_has(s, ":application-inference-profile/");
}
/* _ARN_REGION_RE = re.compile(r"^arn:[^:]+:bedrock:([a-z0-9-]+):", re.IGNORECASE)
 * Returns the captured region (original case) or NULL. Caller frees. */
static char *bt2_arn_region(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s), i = 0;
    if (!bt2_ci_at(s, 0, "arn:")) return NULL;
    i = 4;
    size_t start = i;
    while (i < n && s[i] != ':') i++;       /* [^:]+ */
    if (i == start) return NULL;
    if (!bt2_ci_at(s, i, ":bedrock:")) return NULL;
    i += 9;
    size_t cs = i;
    while (i < n) {                          /* [a-z0-9-]+ IGNORECASE */
        char c = s[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-') i++;
        else break;
    }
    if (i == cs) return NULL;
    if (i >= n || s[i] != ':') return NULL;
    return bt2_dup_span(s + cs, i - cs);
}
/* _REDACTED_REASONING_REJECTION_PATTERN =
 *   re.compile(r"ValidationException.*(?:redacted|encrypt)", re.IGNORECASE|re.DOTALL)
 * Equivalent: lowercase; after the FIRST "validationexception" occurrence,
 * "redacted" or "encrypt" appears anywhere (DOTALL .* spans everything). */
static bool bt2_redacted_rejection(const char *msg) {
    if (!msg) return false;
    char *low = bt2_py_lower(msg);
    if (!low) return false;
    const char *ve = strstr(low, "validationexception");
    bool hit = false;
    if (ve) {
        const char *suffix = ve + 19;   /* strlen("validationexception") */
        hit = strstr(suffix, "redacted") != NULL || strstr(suffix, "encrypt") != NULL;
    }
    free(low);
    return hit;
}

/* ── module constants (verbatim from the live module) ────────────────────── */

static const char *BT2_CONVERSE_ISSUER_TYPE = "bedrock.native_assistant";
/* _NOVA_EFFORT: anything stronger -> "high" */
static const char *bt2_nova_effort(const char *effort) {
    if (bt2_str_eq(effort, "minimal") || bt2_str_eq(effort, "low")) return "low";
    if (bt2_str_eq(effort, "medium")) return "medium";
    return "high";
}
/* CACHE_POINT_PLACEMENTS */
static bool bt2_cache_placement(const char *p) {
    return bt2_str_eq(p, "tools") || bt2_str_eq(p, "system") || bt2_str_eq(p, "messages");
}
static const char *BT2_REDACTED_RECOVER_FMT =
    "bedrock: %s rejected replayed redacted reasoning (encrypted content is sealed to the issuing "
    "model/flow) - stripping redacted blocks and resending once.";
static const char *BT2_PROFILE_SKIP_FMT =
    "Inference profile resolution skipped for %s: %s";

/* ── pure block builders ─────────────────────────────────────────────────── */

/* PoP: bt2_tool_use_block @ agent/bedrock_adapter.py:_tool_use_block */
static json_t *bt2_tool_use_block(const json_t *tool_use_id, const json_t *name, const json_t *input_dict) {
    json_t *tu = json_object();
    json_set(tu, "toolUseId", tool_use_id ? json_copy(tool_use_id) : json_null());
    json_set(tu, "name", name ? json_copy(name) : json_null());
    json_set(tu, "input", input_dict ? json_copy(input_dict) : json_null());
    json_t *r = json_object();
    json_set(r, "toolUse", tu);
    return r;
}

/* PoP: bt2_reasoning_text_block @ agent/bedrock_adapter.py:_reasoning_text_block */
static json_t *bt2_reasoning_text_block(const json_t *text, const json_t *signature) {
    json_t *rt = json_object();
    json_set(rt, "text", bt2_is_str(text) ? json_copy(text) : json_string(""));
    if (bt2_is_str(signature) && bt2_truthy(signature))
        json_set(rt, "signature", json_copy(signature));
    json_t *rc = json_object();
    json_set(rc, "reasoningText", rt);
    json_t *r = json_object();
    json_set(r, "reasoningContent", rc);
    return r;
}

/* PoP: bt2_redacted_reasoning_block @ agent/bedrock_adapter.py:_redacted_reasoning_block */
static json_t *bt2_redacted_reasoning_block(bt2_ctx *ctx, const json_t *encoded) {
    json_t *a = json_object();
    bt2_arg_node(a, "encoded", encoded);
    json_t *redacted = bt2_call(ctx, "_decode_redacted", a);
    if (bt2_exc_pending(ctx)) { if (redacted) json_free(redacted); return NULL; }
    if (bt2_is_none(redacted)) { if (redacted) json_free(redacted); return json_null(); }
    json_t *rc = json_object();
    json_set(rc, "redactedContent", redacted);   /* ownership transferred */
    json_t *r = json_object();
    json_set(r, "reasoningContent", rc);
    return r;
}

/* PoP: bt2_anthropic_ordered_blocks @ agent/bedrock_adapter.py:_anthropic_ordered_blocks */
static json_t *bt2_anthropic_ordered_blocks(bt2_ctx *ctx, const json_t *msg) {
    if (!bt2_is_obj(msg)) { bt2_raise_attr(ctx, msg, "get"); return NULL; }
    /* args = {tc.get("id"): _parse_tool_args(...) for tc in tool_calls or [] if dict} */
    json_t *tcs = bt2_get(msg, "tool_calls");
    if (!bt2_truthy(tcs)) tcs = NULL;
    if (tcs && !bt2_is_arr(tcs)) {
        if (bt2_is_str(tcs)) tcs = NULL;   /* str iterates chars; none is a dict */
        else { bt2_raise_not_iterable(ctx, tcs); return NULL; }
    }
    json_t *args = json_object();
    if (tcs) {
        for (size_t i = 0; i < tcs->c.count; i++) {
            json_t *tc = tcs->c.items[i];
            if (!bt2_is_obj(tc)) continue;
            json_t *id = bt2_get(tc, "id");       /* domain: str ids */
            json_t *fn = bt2_get(tc, "function");
            json_t *empty = NULL;
            if (!bt2_truthy(fn)) { empty = json_object(); fn = empty; }
            if (!bt2_is_obj(fn)) {
                bt2_raise_attr(ctx, fn, "get");
                if (empty) json_free(empty);
                json_free(args);
                return NULL;
            }
            json_t *argval = bt2_get(fn, "arguments");
            json_t *a = json_object();
            if (argval) json_set(a, "args", json_copy(argval));
            else json_set(a, "args", json_string("{}"));
            json_t *parsed = bt2_call(ctx, "_parse_tool_args", a);
            if (empty) json_free(empty);
            if (bt2_exc_pending(ctx)) { if (parsed) json_free(parsed); json_free(args); return NULL; }
            json_set(args, bt2_jstr(id), parsed ? parsed : json_null());
        }
    }
    /* converted generator over anthropic_content_blocks */
    json_t *blocks = bt2_get(msg, "anthropic_content_blocks");
    if (!bt2_truthy(blocks)) blocks = NULL;
    if (blocks && !bt2_is_arr(blocks)) {
        if (bt2_is_str(blocks)) blocks = NULL;   /* chars are not dicts */
        else { bt2_raise_not_iterable(ctx, blocks); json_free(args); return NULL; }
    }
    json_t *out = json_array();
    if (blocks) {
        for (size_t i = 0; i < blocks->c.count; i++) {
            json_t *b = blocks->c.items[i];
            if (!bt2_is_obj(b)) continue;
            json_t *tyv = bt2_get(b, "type");
            if (!bt2_is_str(tyv)) continue;
            const char *ty = bt2_jstr(tyv);
            json_t *blk = NULL;
            bool owns_blk = true;
            if (bt2_str_eq(ty, "thinking")) {
                blk = bt2_reasoning_text_block(bt2_get(b, "thinking"), bt2_get(b, "signature"));
            } else if (bt2_str_eq(ty, "redacted_thinking")) {
                blk = bt2_redacted_reasoning_block(ctx, bt2_get(b, "data"));
                if (bt2_exc_pending(ctx)) { if (blk) json_free(blk); json_free(out); json_free(args); return NULL; }
            } else if (bt2_str_eq(ty, "text")) {
                json_t *t = bt2_get(b, "text");
                if (bt2_is_str(t)) {
                    char *st = bt2_py_strip(bt2_jstr(t));
                    if (st && st[0]) {
                        blk = json_object();
                        json_set(blk, "text", json_copy(t));
                    }
                    free(st);
                }
                if (!blk) blk = json_null();
            } else if (bt2_str_eq(ty, "tool_use")) {
                json_t *bid = bt2_get(b, "id");
                json_t *bname = bt2_get(b, "name");
                json_t *input = bt2_get(b, "input");
                json_t *input_use = NULL;
                json_t *hit = bt2_is_str(bid) ? json_obj_get(args, bt2_jstr(bid)) : NULL;
                if (hit) input_use = json_copy(hit);
                else if (input) input_use = json_copy(input);
                else input_use = json_object();   /* b.get("input", {}) */
                json_t *id_or = bid ? json_copy(bid) : json_string("");
                json_t *name_or = bname ? json_copy(bname) : json_string("");
                blk = bt2_tool_use_block(id_or, name_or, input_use);
                json_free(id_or); json_free(name_or); json_free(input_use);
            } else {
                continue;   /* type not in _ANTHROPIC_TO_CONVERSE */
            }
            if (!bt2_is_none(blk)) json_append(out, blk);
            else if (owns_blk && blk) json_free(blk);
        }
    }
    json_free(args);
    return out;
}

/* ── reasoning issuer / policy leaves ────────────────────────────────────── */

/* PoP: bt2_issuer @ agent/bedrock_adapter.py:_issuer */
static json_t *bt2_issuer(bt2_ctx *ctx, const json_t *msg) {
    if (!bt2_is_obj(msg)) { bt2_raise_attr(ctx, msg, "get"); return NULL; }
    json_t *rd = bt2_get(msg, "reasoning_details");
    if (!bt2_truthy(rd)) rd = NULL;
    if (rd && !bt2_is_arr(rd)) {
        if (bt2_is_str(rd)) rd = NULL;   /* chars are not dicts */
        else { bt2_raise_not_iterable(ctx, rd); return NULL; }
    }
    if (rd) {
        for (size_t i = 0; i < rd->c.count; i++) {
            json_t *d = rd->c.items[i];
            if (!bt2_is_obj(d)) continue;
            json_t *ty = bt2_get(d, "type");
            if (!bt2_is_str(ty) || !bt2_str_eq(bt2_jstr(ty), BT2_CONVERSE_ISSUER_TYPE)) continue;
            json_t *model = bt2_get(d, "model");
            if (bt2_is_str(model)) return json_copy(model);
        }
    }
    return json_null();
}

/* PoP: bt2_resolve_inference_profile_model_id @ agent/bedrock_adapter.py:_resolve_inference_profile_model_id */
static json_t *bt2_resolve_inference_profile_model_id(bt2_ctx *ctx, const json_t *profile_arn, const json_t *region) {
    json_t *cache = bt2_get(ctx->state, "_inference_profile_model_cache");
    /* `profile_arn in cache` — unhashable containers raise; non-str hashables
     * never match the str-keyed fixture cache. */
    if (bt2_is_arr(profile_arn) || bt2_is_obj(profile_arn)) {
        bt2_raise_unhashable(ctx, profile_arn);
        return NULL;
    }
    if (bt2_is_str(profile_arn)) {
        json_t *hit = json_obj_get(cache, bt2_jstr(profile_arn));
        if (hit) return json_copy(hit);
    }
    /* arn_region = _ARN_REGION_RE.match(profile_arn) */
    if (!bt2_is_str(profile_arn)) { bt2_raise_re_type(ctx, profile_arn); return NULL; }
    char *arn_region = bt2_arn_region(bt2_jstr(profile_arn));
    /* region = (group or "") or region or resolve_bedrock_region() */
    json_t *region_node = NULL;   /* owned */
    if (arn_region && arn_region[0]) region_node = json_string(arn_region);
    else if (bt2_is_str(region) && bt2_jstr(region)[0]) region_node = json_copy(region);
    else {
        region_node = bt2_call(ctx, "resolve_bedrock_region", json_object());
        if (bt2_exc_pending(ctx)) {
            free(arn_region);
            if (region_node) json_free(region_node);
            return NULL;
        }
        if (!region_node) region_node = json_null();
    }
    json_t *resolved = json_copy(profile_arn);
    /* try: client = _get_bedrock_control_client(region); models = client.get_
     * inference_profile(...).get("models") or []; resolved = next(...) */
    json_t *a = json_object();
    bt2_arg_node(a, "region", region_node);
    json_t *client = bt2_call(ctx, "_get_bedrock_control_client", a);
    bool caught = false;
    if (bt2_exc_pending(ctx)) {
        if (!bt2_exc_is_exception(ctx)) { free(arn_region); json_free(region_node); if (client) json_free(client); json_free(resolved); return NULL; }
        caught = true;
    }
    if (!caught && bt2_is_none(client)) {
        /* None.get_inference_profile — AttributeError BEFORE the method seam */
        bt2_raise_attr(ctx, NULL, "get_inference_profile");
        caught = true;
    }
    json_t *resp = NULL;
    if (!caught) {
        json_t *a2 = json_object();
        json_set(a2, "inferenceProfileIdentifier", json_copy(profile_arn));
        resp = bt2_call(ctx, "get_inference_profile", a2);
        if (bt2_exc_pending(ctx)) {
            if (!bt2_exc_is_exception(ctx)) { free(arn_region); json_free(region_node); if (client) json_free(client); if (resp) json_free(resp); json_free(resolved); return NULL; }
            caught = true;
        }
    }
    if (!caught && !bt2_is_obj(resp)) {
        bt2_raise_attr(ctx, resp, "get");
        caught = true;
    }
    if (!caught) {
        json_t *models = bt2_get(resp, "models");
        if (!bt2_truthy(models)) models = NULL;
        if (models && !bt2_is_arr(models)) {
            if (bt2_is_str(models)) {
                /* chars: m.get AttributeError ('str') */
                bt2_raise_attr(ctx, models, "get");
                caught = true;
            } else {
                bt2_raise_not_iterable(ctx, models);
                caught = true;
            }
        }
        if (!caught && models) {
            for (size_t i = 0; i < models->c.count; i++) {
                json_t *m = models->c.items[i];
                if (!bt2_is_obj(m)) { bt2_raise_attr(ctx, m, "get"); caught = true; break; }
                json_t *ma = bt2_get(m, "modelArn");
                if (bt2_truthy(ma)) {
                    json_free(resolved);
                    resolved = json_copy(ma);
                    break;
                }
            }
        }
    }
    if (client) json_free(client);
    if (resp) json_free(resp);
    free(arn_region);
    json_free(region_node);
    if (caught) {
        /* except Exception as exc: logger.debug(fmt, profile_arn, exc).
         * Snapshot str(exc), then SWALLOW the caught exception BEFORE the
         * logger seam: bt2_call does not clear a pre-existing pending exc,
         * so the post-logger pending check must see only a logger raise. */
        char *exc_str = bt2_dup_span(bt2_exc_msg(ctx), strlen(bt2_exc_msg(ctx)));
        bt2_exc_clear(ctx);
        json_t *a3 = json_object();
        json_set(a3, "msg", json_string(BT2_PROFILE_SKIP_FMT));
        json_set(a3, "arg1", json_copy(profile_arn));
        json_set(a3, "arg2", json_string(exc_str ? exc_str : ""));
        free(exc_str);
        json_t *lr = bt2_call(ctx, "logger_debug", a3);
        if (lr) json_free(lr);
        if (bt2_exc_pending(ctx)) { json_free(resolved); return NULL; }   /* logger raise propagates */
    }
    json_set(cache, bt2_jstr(profile_arn), json_copy(resolved));
    return resolved;
}

/* PoP: bt2_minted_outside_claude @ agent/bedrock_adapter.py:_minted_outside_claude */
static json_t *bt2_minted_outside_claude(bt2_ctx *ctx, const json_t *issuer) {
    if (bt2_is_none(issuer)) return json_bool(false);
    if (!bt2_is_str(issuer)) { bt2_raise_re_type(ctx, issuer); return NULL; }
    if (bt2_app_profile_arn(bt2_jstr(issuer))) {
        json_t *empty_region = json_string("");
        json_t *resolved = bt2_resolve_inference_profile_model_id(ctx, issuer, empty_region);
        json_free(empty_region);
        if (bt2_exc_pending(ctx)) { if (resolved) json_free(resolved); return NULL; }
        bool same = bt2_is_str(resolved) && bt2_str_eq(bt2_jstr(resolved), bt2_jstr(issuer));
        if (resolved) json_free(resolved);
        if (same) return json_bool(false);
    }
    json_t *a = json_object();
    json_set(a, "model", json_copy(issuer));
    json_t *policy = bt2_call(ctx, "_converse_replay_policy", a);
    if (bt2_exc_pending(ctx)) { if (policy) json_free(policy); return NULL; }
    bool signed_policy = bt2_is_str(policy) && bt2_str_eq(bt2_jstr(policy), "signed");
    if (policy) json_free(policy);
    return json_bool(!signed_policy);
}

/* PoP: bt2_reasoning_minted_outside_claude @ agent/bedrock_adapter.py:reasoning_minted_outside_claude */
static json_t *bt2_reasoning_minted_outside_claude(bt2_ctx *ctx, const json_t *msg) {
    json_t *iss = bt2_issuer(ctx, msg);
    if (bt2_exc_pending(ctx)) { if (iss) json_free(iss); return NULL; }
    json_t *r = bt2_minted_outside_claude(ctx, iss);
    if (iss) json_free(iss);
    return r;
}

/* ── per-block reasoning filters ─────────────────────────────────────────── */

/* PoP: bt2_unsigned_reasoning @ agent/bedrock_adapter.py:_unsigned_reasoning */
static json_t *bt2_unsigned_reasoning(bt2_ctx *ctx, const json_t *block) {
    if (!bt2_is_obj(block)) { bt2_raise_attr(ctx, block, "get"); return NULL; }
    json_t *reasoning = bt2_get(block, "reasoningContent");
    if (!bt2_is_obj(reasoning)) return json_copy(block);
    json_t *rt = bt2_get(reasoning, "reasoningText");
    json_t *empty = NULL;
    if (!bt2_truthy(rt)) { empty = json_object(); rt = empty; }
    if (!bt2_is_obj(rt)) {
        bt2_raise_attr(ctx, rt, "get");
        if (empty) json_free(empty);
        return NULL;
    }
    json_t *text = bt2_get(rt, "text");
    json_t *r = json_null();
    if (bt2_is_str(text)) {
        char *st = bt2_py_strip(bt2_jstr(text));
        if (st && st[0]) {
            json_t *inner = json_object();
            json_set(inner, "text", json_copy(text));
            json_t *rc = json_object();
            json_set(rc, "reasoningText", inner);
            json_free(r);
            r = json_object();
            json_set(r, "reasoningContent", rc);
        }
        free(st);
    }
    if (empty) json_free(empty);
    return r;
}

/* PoP: bt2_verbatim @ agent/bedrock_adapter.py:_verbatim */
static json_t *bt2_verbatim(const json_t *block) {
    return block ? json_copy(block) : json_null();
}

/* PoP: bt2_without_reasoning @ agent/bedrock_adapter.py:_without_reasoning */
static json_t *bt2_without_reasoning(bt2_ctx *ctx, const json_t *block) {
    /* "reasoningContent" in block — dict key / str substring / list membership */
    bool has = false;
    if (bt2_is_obj(block)) {
        has = json_obj_get(block, "reasoningContent") != NULL;
    } else if (bt2_is_str(block)) {
        has = bt2_str_has(bt2_jstr(block), "reasoningContent");
    } else if (bt2_is_arr(block)) {
        for (size_t i = 0; i < block->c.count; i++) {
            json_t *el = block->c.items[i];
            if (bt2_is_str(el) && bt2_str_eq(bt2_jstr(el), "reasoningContent")) { has = true; break; }
        }
    } else {
        bt2_raise_not_container(ctx, block);
        return NULL;
    }
    return has ? json_null() : json_copy(block);
}

/* PoP: bt2_filter_reasoning @ agent/bedrock_adapter.py:_filter_reasoning */
static json_t *bt2_filter_reasoning(bt2_ctx *ctx, const json_t *blocks, const json_t *keep) {
    if (!bt2_is_arr(blocks)) { bt2_raise_not_iterable(ctx, blocks); return NULL; }
    /* keep must be one of the ported filter callables (materialized live) */
    int which = 0;   /* 1 verbatim, 2 without_reasoning, 3 unsigned_reasoning */
    if (bt2_is_str(keep)) {
        const char *k = bt2_jstr(keep);
        if (bt2_str_eq(k, "_verbatim")) which = 1;
        else if (bt2_str_eq(k, "_without_reasoning")) which = 2;
        else if (bt2_str_eq(k, "_unsigned_reasoning")) which = 3;
    }
    json_t *out = json_array();
    if (!which) {
        /* map() pulls the first item BEFORE calling keep: an empty blocks
         * list exhausts the iterator without the callability check. */
        if (blocks->c.count == 0) return out;
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object is not callable", bt2_typename(keep));
        bt2_raise_msg(ctx, "TypeError", msg);
        json_free(out);
        return NULL;
    }
    for (size_t i = 0; i < blocks->c.count; i++) {
        json_t *block = blocks->c.items[i];
        json_t *kept = NULL;
        if (which == 1) kept = bt2_verbatim(block);
        else if (which == 2) kept = bt2_without_reasoning(ctx, block);
        else kept = bt2_unsigned_reasoning(ctx, block);
        if (bt2_exc_pending(ctx)) { if (kept) json_free(kept); json_free(out); return NULL; }
        if (!bt2_is_none(kept)) json_append(out, kept);
        else if (kept) json_free(kept);
    }
    return out;
}

/* ── cachePoint / redacted-reasoning self-heal helpers ───────────────────── */

/* PoP: bt2_without_cache_points @ agent/bedrock_adapter.py:_without_cache_points */
static json_t *bt2_without_cache_points(const json_t *blocks) {
    if (!bt2_is_arr(blocks)) return json_null();
    json_t *cleaned = json_array();
    for (size_t i = 0; i < blocks->c.count; i++) {
        json_t *b = blocks->c.items[i];
        bool is_cp = bt2_is_obj(b) && b->c.count == 1 && bt2_str_eq(b->c.keys[0], "cachePoint");
        if (!is_cp) json_append(cleaned, json_copy(b));
    }
    if (cleaned->c.count == blocks->c.count) { json_free(cleaned); return json_null(); }
    return cleaned;
}

/* PoP: bt2_without_redacted_reasoning @ agent/bedrock_adapter.py:_without_redacted_reasoning */
static json_t *bt2_without_redacted_reasoning(const json_t *blocks) {
    if (!bt2_is_arr(blocks)) return json_null();
    json_t *cleaned = json_array();
    for (size_t i = 0; i < blocks->c.count; i++) {
        json_t *b = blocks->c.items[i];
        bool drop = false;
        if (bt2_is_obj(b)) {
            json_t *rc = bt2_get(b, "reasoningContent");
            if (bt2_is_obj(rc) && json_obj_get(rc, "redactedContent")) drop = true;
        }
        if (!drop) json_append(cleaned, json_copy(b));
    }
    if (cleaned->c.count == blocks->c.count) { json_free(cleaned); return json_null(); }
    return cleaned;
}

/* PoP: bt2_note_cache_point_rejection @ agent/bedrock_adapter.py:note_cache_point_rejection */
static json_t *bt2_note_cache_point_rejection(bt2_ctx *ctx, const json_t *model_id, const json_t *placement) {
    if (bt2_is_str(placement) && bt2_cache_placement(bt2_jstr(placement))) {
        if (!bt2_is_str(model_id)) { bt2_raise_attr(ctx, model_id, "lower"); return NULL; }
        char *lower = bt2_py_lower(bt2_jstr(model_id));
        json_t *rej = bt2_get(ctx->state, "_CACHE_POINT_REJECTIONS");
        json_t *entry = json_obj_get(rej, lower ? lower : "");
        if (!entry) {
            entry = json_array();
            json_set(rej, lower ? lower : "", entry);
        }
        bool present = false;
        for (size_t i = 0; i < entry->c.count; i++)
            if (bt2_str_eq(bt2_jstr(entry->c.items[i]), bt2_jstr(placement))) { present = true; break; }
        if (!present) json_append(entry, json_copy(placement));
        free(lower);
    }
    return json_null();
}

/* PoP: bt2_reset_cache_point_rejections @ agent/bedrock_adapter.py:reset_cache_point_rejections */
static json_t *bt2_reset_cache_point_rejections(bt2_ctx *ctx) {
    json_set(ctx->state, "_CACHE_POINT_REJECTIONS", json_object());
    return json_null();
}

/* PoP: bt2_recover_from_redacted_reasoning_rejection @ agent/bedrock_adapter.py:recover_from_redacted_reasoning_rejection */
static json_t *bt2_recover_from_redacted_reasoning_rejection(bt2_ctx *ctx, const char *exc_msg, const json_t *kwargs) {
    if (!bt2_redacted_rejection(exc_msg)) return json_null();
    json_t *a = json_object();
    bt2_arg_node(a, "kwargs", kwargs);
    json_t *retry = bt2_call(ctx, "strip_redacted_reasoning", a);
    if (bt2_exc_pending(ctx)) { if (retry) json_free(retry); return NULL; }
    /* identity: {"__same__":true} marker, or None-result with None-kwargs */
    bool same = (bt2_is_obj(retry) && bt2_is_true(bt2_get(retry, "__same__")))
             || (bt2_is_none(retry) && bt2_is_none(kwargs));
    if (same) { if (retry) json_free(retry); return json_null(); }
    /* logger.warning(fmt, str(kwargs.get("modelId", "")) or "model") */
    if (!bt2_is_obj(kwargs)) { bt2_raise_attr(ctx, kwargs, "get"); if (retry) json_free(retry); return NULL; }
    json_t *mid = bt2_get(kwargs, "modelId");
    char *mid_str = mid ? bt2_pystr(mid) : bt2_dup_span("", 0);
    const char *arg = (mid_str && mid_str[0]) ? mid_str : "model";
    json_t *a2 = json_object();
    json_set(a2, "msg", json_string(BT2_REDACTED_RECOVER_FMT));
    json_set(a2, "arg", json_string(arg));
    free(mid_str);
    json_t *lr = bt2_call(ctx, "logger_warning", a2);
    if (lr) json_free(lr);
    if (bt2_exc_pending(ctx)) { if (retry) json_free(retry); return NULL; }
    return retry ? retry : json_null();
}

/* ── persisted-reasoning / namespace projection leaves ───────────────────── */

/* PoP: bt2_encode_redacted @ agent/bedrock_adapter.py:_encode_redacted */
static json_t *bt2_encode_redacted(const json_t *redacted) {
    /* bytes/bytearray ride {"__bytes__":[...]}/{"__bytearray__":[...]} markers */
    if (bt2_is_obj(redacted)) {
        json_t *bytes = bt2_get(redacted, "__bytes__");
        if (!bytes) bytes = bt2_get(redacted, "__bytearray__");
        if (bt2_is_arr(bytes)) {
            unsigned char *buf = (unsigned char *)malloc(bytes->c.count + 1);
            if (!buf) return json_null();
            for (size_t i = 0; i < bytes->c.count; i++)
                buf[i] = (unsigned char)(bt2_is_num(bytes->c.items[i]) ? (int)bytes->c.items[i]->num_val : 0);
            char *enc = bt2_b64_encode(buf, bytes->c.count);
            free(buf);
            json_t *r = json_string(enc ? enc : "");
            free(enc);
            return r;
        }
    }
    if (bt2_is_str(redacted)) return json_copy(redacted);
    return json_null();
}

/* PoP: bt2_tool_call_ns @ agent/bedrock_adapter.py:_tool_call_ns */
static json_t *bt2_tool_call_ns(const json_t *tool_use_id, const json_t *name, const json_t *input_dict) {
    char *args_str = bt2_dumps(input_dict);
    json_t *fn = json_object();
    json_set(fn, "name", name ? json_copy(name) : json_null());
    json_set(fn, "arguments", json_string(args_str ? args_str : ""));
    free(args_str);
    json_t *r = json_object();
    json_set(r, "id", tool_use_id ? json_copy(tool_use_id) : json_null());
    json_set(r, "type", json_string("function"));
    json_set(r, "function", fn);
    return r;
}

/* PoP: bt2_persisted_reasoning_details @ agent/bedrock_adapter.py:_persisted_reasoning_details */
static json_t *bt2_persisted_reasoning_details(bt2_ctx *ctx, const json_t *ordered_blocks) {
    if (bt2_is_str(ordered_blocks)) return json_array();   /* chars are not dicts */
    if (!bt2_is_arr(ordered_blocks)) { bt2_raise_not_iterable(ctx, ordered_blocks); return NULL; }
    json_t *details = json_array();
    for (size_t i = 0; i < ordered_blocks->c.count; i++) {
        json_t *block = ordered_blocks->c.items[i];
        json_t *reasoning = bt2_is_obj(block) ? bt2_get(block, "reasoningContent") : NULL;
        if (!bt2_is_obj(reasoning)) continue;
        json_t *text = bt2_get(reasoning, "text");
        if (bt2_is_str(text) && bt2_truthy(text)) {
            json_t *d = json_object();
            json_set(d, "type", json_string("thinking"));
            json_set(d, "thinking", json_copy(text));
            json_t *sig = bt2_get(reasoning, "signature");
            if (bt2_truthy(sig)) json_set(d, "signature", json_copy(sig));
            json_append(details, d);
        }
        json_t *enc = bt2_get(reasoning, "redactedContentBase64");
        if (bt2_truthy(enc)) {
            json_t *d = json_object();
            json_set(d, "type", json_string("redacted_thinking"));
            json_set(d, "data", json_copy(enc));
            json_append(details, d);
        }
    }
    return details;
}

/* ── discovery / model-entry leaves ──────────────────────────────────────── */

/* PoP: bt2_model_entry @ agent/bedrock_adapter.py:_model_entry */
static json_t *bt2_model_entry(bt2_ctx *ctx, const json_t *model_id, const json_t *name,
                               const json_t *provider, const json_t *input_mods, const json_t *output_mods) {
    const json_t *name_or = bt2_truthy(name) ? name : model_id;
    if (!bt2_is_str(name_or)) { bt2_raise_attr(ctx, name_or, "strip"); return NULL; }
    char *stripped = bt2_py_strip(bt2_jstr(name_or));
    json_t *r = json_object();
    json_set(r, "id", model_id ? json_copy(model_id) : json_null());
    json_set(r, "name", json_string(stripped ? stripped : ""));
    json_set(r, "provider", provider ? json_copy(provider) : json_null());
    json_set(r, "input_modalities", input_mods ? json_copy(input_mods) : json_null());
    json_set(r, "output_modalities", output_mods ? json_copy(output_mods) : json_null());
    json_set(r, "streaming", json_bool(true));
    free(stripped);
    return r;
}

/* PoP: bt2_list_foundation_models @ agent/bedrock_adapter.py:_list_foundation_models */
static json_t *bt2_list_foundation_models(bt2_ctx *ctx, const json_t *client, const json_t *filter_set, json_t *models) {
    (void)client;   /* null-client check happens in the harness-facing wrapper below */
    json_t *resp = bt2_call(ctx, "list_foundation_models", json_object());
    if (bt2_exc_pending(ctx)) { if (resp) json_free(resp); return NULL; }
    if (!bt2_is_obj(resp)) { bt2_raise_attr(ctx, resp, "get"); if (resp) json_free(resp); return NULL; }
    json_t *summaries = bt2_get(resp, "modelSummaries");
    if (!summaries) summaries = NULL;   /* .get default [] */
    if (summaries && !bt2_is_arr(summaries)) {
        bt2_raise_not_iterable(ctx, summaries);
        json_free(resp);
        return NULL;
    }
    bool has_filter = bt2_is_arr(filter_set) && filter_set->c.count > 0;
    for (size_t i = 0; summaries && i < summaries->c.count; i++) {
        json_t *summary = summaries->c.items[i];
        if (!bt2_is_obj(summary)) { bt2_raise_attr(ctx, summary, "get"); json_free(resp); return NULL; }
        json_t *mid_v = bt2_get(summary, "modelId");
        const json_t *mid_or = bt2_truthy(mid_v) ? mid_v : NULL;
        if (mid_or && !bt2_is_str(mid_or)) { bt2_raise_attr(ctx, mid_or, "strip"); json_free(resp); return NULL; }
        char *model_id = bt2_py_strip(mid_or ? bt2_jstr(mid_or) : "");
        if (!model_id || !model_id[0]) { free(model_id); continue; }
        json_t *pn_v = bt2_get(summary, "providerName");
        const json_t *pn_or = bt2_truthy(pn_v) ? pn_v : NULL;
        const char *provider_name = pn_or ? bt2_jstr(pn_or) : "";
        /* model_prefix = model_id.split(".")[0].lower() if "." in model_id else "" */
        char *model_prefix = bt2_dup_span("", 0);
        if (strchr(model_id, '.')) {
            const char *dot = strchr(model_id, '.');
            char *head = bt2_dup_span(model_id, (size_t)(dot - model_id));
            free(model_prefix);
            model_prefix = bt2_py_lower(head);
            free(head);
        }
        if (has_filter) {
            if (pn_or && !bt2_is_str(pn_or)) { bt2_raise_attr(ctx, pn_or, "lower"); free(model_id); free(model_prefix); json_free(resp); return NULL; }
            char *pn_lower = bt2_py_lower(provider_name);
            bool pn_in = false, mp_in = false;
            for (size_t k = 0; k < filter_set->c.count; k++) {
                const char *f = bt2_jstr(filter_set->c.items[k]);
                if (bt2_str_eq(pn_lower, f)) pn_in = true;
                if (bt2_str_eq(model_prefix, f)) mp_in = true;
            }
            free(pn_lower);
            if (!pn_in && !mp_in) { free(model_id); free(model_prefix); continue; }
        }
        free(model_prefix);
        json_t *output_mods = bt2_get(summary, "outputModalities");
        if (!output_mods) output_mods = NULL;   /* default [] */
        /* lifecycle status gate */
        json_t *lc = bt2_get(summary, "modelLifecycle");
        json_t *empty_lc = NULL;
        if (!lc) { empty_lc = json_object(); lc = empty_lc; }
        if (!bt2_is_obj(lc)) { bt2_raise_attr(ctx, lc, "get"); free(model_id); if (empty_lc) json_free(empty_lc); json_free(resp); return NULL; }
        json_t *status_v = bt2_get(lc, "status");
        const char *status = status_v ? bt2_jstr(status_v) : "";
        if (status_v && !bt2_is_str(status_v)) { bt2_raise_attr(ctx, status_v, "upper"); free(model_id); if (empty_lc) json_free(empty_lc); json_free(resp); return NULL; }
        char *status_up = bt2_py_upper(status);
        bool active = bt2_str_eq(status_up, "ACTIVE");
        free(status_up);
        if (empty_lc) json_free(empty_lc);
        if (!active) { free(model_id); continue; }
        if (!bt2_truthy(bt2_get(summary, "responseStreamingSupported"))) { free(model_id); continue; }
        /* "TEXT" in output_mods */
        bool text_in = false;
        if (bt2_is_arr(output_mods)) {
            for (size_t k = 0; k < output_mods->c.count; k++)
                if (bt2_is_str(output_mods->c.items[k]) && bt2_str_eq(bt2_jstr(output_mods->c.items[k]), "TEXT")) { text_in = true; break; }
        } else if (bt2_is_str(output_mods)) {
            text_in = bt2_str_has(bt2_jstr(output_mods), "TEXT");
        } else if (output_mods) {
            bt2_raise_not_container(ctx, output_mods);
            free(model_id); json_free(resp); return NULL;
        }
        if (!text_in) { free(model_id); continue; }
        /* models.append(_model_entry(model_id, modelName, provider_name.strip(), inputModalities, output_mods)) */
        if (pn_or && !bt2_is_str(pn_or)) { bt2_raise_attr(ctx, pn_or, "strip"); free(model_id); json_free(resp); return NULL; }
        char *pn_stripped = bt2_py_strip(provider_name);
        json_t *mid_node = json_string(model_id);
        json_t *pn_node = json_string(pn_stripped ? pn_stripped : "");
        json_t *in_mods = bt2_get(summary, "inputModalities");
        json_t *empty_mods = NULL;
        if (!in_mods) { empty_mods = json_array(); in_mods = empty_mods; }   /* .get default [] */
        json_t *entry = bt2_model_entry(ctx, mid_node, bt2_get(summary, "modelName"), pn_node,
                                        in_mods, output_mods);
        json_free(mid_node); json_free(pn_node); free(pn_stripped); free(model_id);
        if (empty_mods) json_free(empty_mods);
        if (bt2_exc_pending(ctx)) { if (entry) json_free(entry); json_free(resp); return NULL; }
        if (!bt2_is_arr(models)) { bt2_raise_attr(ctx, models, "append"); json_free(entry); json_free(resp); return NULL; }
        json_append(models, entry);
    }
    json_free(resp);
    return json_null();
}

/* PoP: bt2_list_inference_profiles @ agent/bedrock_adapter.py:_list_inference_profiles */
static json_t *bt2_list_inference_profiles(bt2_ctx *ctx, const json_t *client, const json_t *filter_set, json_t *models) {
    (void)client;
    json_t *profiles = json_array();
    json_t *next_token = NULL;   /* owned copy or NULL */
    for (;;) {
        json_t *a = json_object();
        if (bt2_truthy(next_token)) json_set(a, "nextToken", json_copy(next_token));
        json_t *resp = bt2_call(ctx, "list_inference_profiles", a);
        if (bt2_exc_pending(ctx)) { if (resp) json_free(resp); if (next_token) json_free(next_token); json_free(profiles); return NULL; }
        if (!bt2_is_obj(resp)) { bt2_raise_attr(ctx, resp, "get"); if (resp) json_free(resp); if (next_token) json_free(next_token); json_free(profiles); return NULL; }
        json_t *summaries = bt2_get(resp, "inferenceProfileSummaries");
        if (summaries && !bt2_is_arr(summaries)) { bt2_raise_not_iterable(ctx, summaries); json_free(resp); if (next_token) json_free(next_token); json_free(profiles); return NULL; }
        if (summaries)
            for (size_t i = 0; i < summaries->c.count; i++) json_append(profiles, json_copy(summaries->c.items[i]));
        json_t *nt = bt2_get(resp, "nextToken");
        if (next_token) json_free(next_token);
        next_token = bt2_truthy(nt) ? json_copy(nt) : NULL;
        json_free(resp);
        if (!next_token) break;
    }
    /* seen_ids = {m["id"].lower() for m in models} */
    json_t *seen = json_array();
    if (!bt2_is_arr(models)) { bt2_raise_not_iterable(ctx, models); if (next_token) json_free(next_token); json_free(profiles); json_free(seen); return NULL; }
    for (size_t i = 0; i < models->c.count; i++) {
        json_t *m = models->c.items[i];
        if (bt2_is_str(m)) { bt2_raise_msg(ctx, "TypeError", "string indices must be integers, not 'str'"); goto fail; }
        if (!bt2_is_obj(m)) {
            char msg[256];
            snprintf(msg, sizeof msg, "'%s' object is not subscriptable", bt2_typename(m));
            bt2_raise_msg(ctx, "TypeError", msg);
            goto fail;
        }
        json_t *id = bt2_get(m, "id");
        if (!id) { bt2_raise_key(ctx, "id"); goto fail; }
        if (!bt2_is_str(id)) { bt2_raise_attr(ctx, id, "lower"); goto fail; }
        char *low = bt2_py_lower(bt2_jstr(id));
        json_append(seen, json_string(low ? low : ""));
        free(low);
    }
    bool has_filter = bt2_is_arr(filter_set) && filter_set->c.count > 0;
    for (size_t i = 0; i < profiles->c.count; i++) {
        json_t *profile = profiles->c.items[i];
        if (!bt2_is_obj(profile)) { bt2_raise_attr(ctx, profile, "get"); goto fail; }
        json_t *pid_v = bt2_get(profile, "inferenceProfileId");
        const json_t *pid_or = bt2_truthy(pid_v) ? pid_v : NULL;
        if (pid_or && !bt2_is_str(pid_or)) { bt2_raise_attr(ctx, pid_or, "strip"); goto fail; }
        char *profile_id = bt2_py_strip(pid_or ? bt2_jstr(pid_or) : "");
        if (!profile_id || !profile_id[0]) { free(profile_id); continue; }
        json_t *status = bt2_get(profile, "status");
        if (!bt2_is_str(status) || !bt2_str_eq(bt2_jstr(status), "ACTIVE")) { free(profile_id); continue; }
        char *pid_lower = bt2_py_lower(profile_id);
        bool dup = false;
        for (size_t k = 0; k < seen->c.count; k++)
            if (bt2_str_eq(bt2_jstr(seen->c.items[k]), pid_lower)) { dup = true; break; }
        if (dup) { free(pid_lower); free(profile_id); continue; }
        if (has_filter) {
            json_t *pmodels = bt2_get(profile, "models");
            if (!pmodels) pmodels = NULL;   /* default [] */
            if (pmodels && !bt2_is_arr(pmodels)) { bt2_raise_not_iterable(ctx, pmodels); free(pid_lower); free(profile_id); goto fail; }
            bool any_match = false;
            for (size_t k = 0; pmodels && k < pmodels->c.count; k++) {
                json_t *m = pmodels->c.items[k];
                if (!bt2_is_obj(m)) { bt2_raise_attr(ctx, m, "get"); free(pid_lower); free(profile_id); goto fail; }
                json_t *arn = bt2_get(m, "modelArn");
                json_t *a = json_object();
                json_set(a, "arn", arn ? json_copy(arn) : json_string(""));
                json_t *prov = bt2_call(ctx, "_extract_provider_from_arn", a);
                if (bt2_exc_pending(ctx)) { if (prov) json_free(prov); free(pid_lower); free(profile_id); goto fail; }
                if (!bt2_is_str(prov)) { bt2_raise_attr(ctx, prov, "lower"); if (prov) json_free(prov); free(pid_lower); free(profile_id); goto fail; }
                char *prov_low = bt2_py_lower(bt2_jstr(prov));
                json_free(prov);
                for (size_t j = 0; j < filter_set->c.count; j++)
                    if (bt2_str_eq(prov_low, bt2_jstr(filter_set->c.items[j]))) { any_match = true; break; }
                free(prov_low);
                if (any_match) break;   /* any() short-circuit */
            }
            if (!any_match) { free(pid_lower); free(profile_id); continue; }
        }
        /* models.append(_model_entry(profile_id, name, "inference-profile", ["TEXT"], ["TEXT"])) */
        json_t *pid_node = json_string(profile_id);
        json_t *mods = json_array();
        json_append(mods, json_string("TEXT"));
        json_t *entry = bt2_model_entry(ctx, pid_node, bt2_get(profile, "inferenceProfileName"),
                                        json_string("inference-profile"), mods, mods);
        json_free(pid_node); json_free(mods);
        free(profile_id);
        if (bt2_exc_pending(ctx)) { if (entry) json_free(entry); goto fail; }
        json_append(models, entry);
        json_append(seen, json_string(pid_lower));
        free(pid_lower);
    }
    if (next_token) json_free(next_token);
    json_free(profiles);
    json_free(seen);
    return json_null();
fail:
    if (next_token) json_free(next_token);
    json_free(profiles);
    json_free(seen);
    return NULL;
}

/* ── Converse reasoning-config builders ──────────────────────────────────── */

/* PoP: bt2_nova_converse_reasoning @ agent/bedrock_adapter.py:_nova_converse_reasoning */
static json_t *bt2_nova_converse_reasoning(bt2_ctx *ctx, const json_t *model, const json_t *reasoning_config, const json_t *max_tokens) {
    (void)model; (void)max_tokens;
    if (!bt2_is_obj(reasoning_config)) { bt2_raise_attr(ctx, reasoning_config, "get"); return NULL; }
    json_t *pair = json_array();
    if (bt2_is_false(bt2_get(reasoning_config, "enabled"))) {
        json_append(pair, json_object());
        json_append(pair, json_object());
        return pair;
    }
    json_t *effort_v = bt2_get(reasoning_config, "effort");
    char *effort_str = effort_v ? bt2_pystr(effort_v) : bt2_dup_span("medium", 6);
    char *effort_low = bt2_py_lower(effort_str ? effort_str : "");
    free(effort_str);
    const char *effort = bt2_nova_effort(effort_low ? effort_low : "");
    free(effort_low);
    json_t *rc = json_object();
    json_set(rc, "type", json_string("enabled"));
    json_set(rc, "maxReasoningEffort", json_string(effort));
    json_t *fields = json_object();
    json_set(fields, "reasoningConfig", rc);
    json_append(pair, fields);
    json_t *inf = json_object();
    if (bt2_str_eq(effort, "high")) {
        json_set(inf, "temperature", json_null());
        json_set(inf, "topP", json_null());
        json_set(inf, "maxTokens", json_null());
    }
    json_append(pair, inf);
    return pair;
}

/* PoP: bt2_claude_converse_reasoning @ agent/bedrock_adapter.py:_claude_converse_reasoning */
static json_t *bt2_claude_converse_reasoning(bt2_ctx *ctx, const json_t *model, const json_t *reasoning_config, const json_t *max_tokens) {
    /* thinking = _thinking_kwargs(reasoning_config, model, max_tokens or 0) */
    json_t *a = json_object();
    bt2_arg_node(a, "reasoning_config", reasoning_config);
    bt2_arg_node(a, "model", model);
    json_set(a, "max_tokens", bt2_truthy(max_tokens) ? json_copy(max_tokens) : json_number(0));
    json_t *thinking = bt2_call(ctx, "_thinking_kwargs", a);
    if (bt2_exc_pending(ctx)) { if (thinking) json_free(thinking); return NULL; }
    /* fields = {k: thinking[k] for k in ("thinking","output_config") if k in thinking} */
    if (!bt2_is_obj(thinking)) {
        bt2_raise_not_container(ctx, thinking);
        if (thinking) json_free(thinking);
        return NULL;
    }
    json_t *fields = json_object();
    json_t *th = bt2_get(thinking, "thinking");
    if (th) json_set(fields, "thinking", json_copy(th));
    json_t *oc = bt2_get(thinking, "output_config");
    if (oc) json_set(fields, "output_config", json_copy(oc));
    json_t *inf = json_object();
    json_t *th_or = bt2_truthy(th) ? th : NULL;
    json_t *empty_th = NULL;
    if (!th_or) { empty_th = json_object(); th_or = empty_th; }
    if (!bt2_is_obj(th_or)) {
        bt2_raise_attr(ctx, th_or, "get");
        if (empty_th) json_free(empty_th);
        json_free(thinking); json_free(fields); json_free(inf);
        return NULL;
    }
    json_t *ty = bt2_get(th_or, "type");
    if (bt2_is_str(ty) && bt2_str_eq(bt2_jstr(ty), "enabled")) {
        if (!bt2_is_str(model)) {
            bt2_raise_attr(ctx, model, "lower");
            if (empty_th) json_free(empty_th);
            json_free(thinking); json_free(fields); json_free(inf);
            return NULL;
        }
        char *low = bt2_py_lower(bt2_jstr(model));
        if (!bt2_str_has(low, "claude-3")) {
            json_t *beta = json_array();
            json_append(beta, json_string("interleaved-thinking-2025-05-14"));
            json_set(fields, "anthropic_beta", beta);
        }
        free(low);
        json_t *temp = bt2_get(thinking, "temperature");
        json_set(inf, "temperature", temp ? json_copy(temp) : json_number(1));
        if (!bt2_is_none(max_tokens)) {
            json_t *mt = bt2_get(thinking, "max_tokens");
            json_set(inf, "maxTokens", mt ? json_copy(mt) : json_copy(max_tokens));
        }
    }
    if (empty_th) json_free(empty_th);
    json_free(thinking);
    json_t *pair = json_array();
    json_append(pair, fields);
    json_append(pair, inf);
    return pair;
}

/* PoP: bt2_apply_converse_reasoning @ agent/bedrock_adapter.py:_apply_converse_reasoning */
static json_t *bt2_apply_converse_reasoning(bt2_ctx *ctx, json_t *kwargs, json_t *inference_config,
                                            const json_t *model, const json_t *reasoning_config, const json_t *max_tokens) {
    /* builder = next((b for matches, b in _CONVERSE_REASONING_BUILDERS if matches(model or "")), None) */
    json_t *model_or = bt2_truthy(model) ? json_copy(model) : json_string("");
    json_t *a = json_object();
    json_set(a, "model_id", json_copy(model_or));
    json_t *m1 = bt2_call(ctx, "is_anthropic_bedrock_model", a);
    if (bt2_exc_pending(ctx)) { if (m1) json_free(m1); json_free(model_or); return NULL; }
    int builder = 0;   /* 1 claude, 2 nova */
    if (bt2_truthy(m1)) builder = 1;
    else {
        if (!bt2_is_str(model_or)) { bt2_raise_attr(ctx, model_or, "lower"); if (m1) json_free(m1); json_free(model_or); return NULL; }
        char *low = bt2_py_lower(bt2_jstr(model_or));
        if (bt2_str_has(low, "amazon.nova-2-lite")) builder = 2;
        free(low);
    }
    if (m1) json_free(m1);
    json_free(model_or);
    if (!builder) return json_null();
    json_t *pair = (builder == 1)
        ? bt2_claude_converse_reasoning(ctx, model, reasoning_config, max_tokens)
        : bt2_nova_converse_reasoning(ctx, model, reasoning_config, max_tokens);
    if (bt2_exc_pending(ctx)) { if (pair) json_free(pair); return NULL; }
    json_t *fields = json_get(pair, 0);
    json_t *inference = json_get(pair, 1);
    if (bt2_truthy(fields)) {
        if (!bt2_is_obj(kwargs)) { bt2_raise_assign(ctx, kwargs); json_free(pair); return NULL; }
        json_set(kwargs, "additionalModelRequestFields", json_copy(fields));
    }
    if (bt2_is_obj(inference)) {
        for (size_t i = 0; i < inference->c.count; i++) {
            const char *key = inference->c.keys[i];
            json_t *value = inference->c.items[i];
            if (bt2_is_none(value)) {
                if (!bt2_is_obj(inference_config)) { bt2_raise_attr(ctx, inference_config, "pop"); json_free(pair); return NULL; }
                json_obj_del(inference_config, key);
            } else {
                if (!bt2_is_obj(inference_config)) { bt2_raise_assign(ctx, inference_config); json_free(pair); return NULL; }
                json_set(inference_config, key, json_copy(value));
            }
        }
    }
    json_free(pair);
    return json_null();
}

/* ── Mantle model-id gate ────────────────────────────────────────────────── */

/* PoP: bt2_bedrock_openai_uses_mantle @ agent/bedrock_adapter.py:bedrock_openai_uses_mantle */
static json_t *bt2_bedrock_openai_uses_mantle(bt2_ctx *ctx, const json_t *model_id) {
    json_t *a = json_object();
    bt2_arg_node(a, "model_id", model_id);
    json_t *parsed = bt2_call(ctx, "parse_bedrock_openai_model_id", a);
    if (bt2_exc_pending(ctx)) { if (parsed) json_free(parsed); return NULL; }
    if (bt2_is_none(parsed)) { if (parsed) json_free(parsed); return json_bool(false); }
    if (!bt2_is_arr(parsed)) {
        /* outside the documented domain; tuple-index semantics */
        bt2_raise_msg(ctx, "IndexError", "tuple index out of range");
        json_free(parsed);
        return NULL;
    }
    if (parsed->c.count < 2) {
        bt2_raise_msg(ctx, "IndexError", "tuple index out of range");
        json_free(parsed);
        return NULL;
    }
    bool r = !bt2_truthy(parsed->c.items[1]);
    json_free(parsed);
    return json_bool(r);
}