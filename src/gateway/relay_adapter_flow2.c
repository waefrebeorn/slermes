/* relay_adapter_flow2.c — FLOW port of the UNCLAIMED remainder of
 * gateway/relay/adapter.py (LIVE authority). The claimed siblings live in
 * relay_adapter*.c / relay_adapter_flow.c and are SEAMED here through the
 * injected universal seam ra2_io_t; the pure egress-decline leaves
 * (is_egress_decline / decline_error / log_decline, gateway/relay/egress.py)
 * are transcribed inline WITHOUT re-claiming their PoP (the oracle drives the
 * LIVE egress defs, so any drift surfaces as a trace mismatch).
 *
 * Ported functions call each other REAL (internal composition):
 *   warning_notifications_enabled -> _chat_platform
 *   _text_metadata -> (seams _with_format_hints_for_chat / _with_scope)
 *   _draft_frame -> _text_metadata
 *   send_draft -> _draft_frame
 *   native_task_card_destination_supported -> _chat_platform, _task_card_metadata
 *   _prepare_slack_egress -> _chat_platform, _stamp_slack_unfurl
 *   _send_exec_approval_prompt -> _mint_and_send_prompt
 *
 * Representation contract (shared with tests/oracle_relay_adapter_flow2.py):
 *   - raise specs are VALUES {"__raise__": kind, "message": msg} threaded
 *     through every helper; an uncaught one becomes the case ret. The
 *     BaseException trio (SystemExit / KeyboardInterrupt / GeneratorExit) plus
 *     asyncio.CancelledError passes through every `except Exception` guard.
 *   - fail-closed: an absent / exhausted seam script returns a RuntimeError
 *     raise spec ("no script for <name>"), recorded as the event ret BEFORE
 *     propagation.
 *   - SendResult doubles are tagged {"__send_result__": true, ...fields};
 *     _MediaNotDelivered is {"__not_delivered__": true, "reason": ...}; a
 *     callable double is {"__callable__": "<seam>"}. Returned SendResults are
 *     projected to the FULL dataclass field set (defaults included).
 *   - scripts: a bare JSON array is a per-call QUEUE; a list RESULT must be
 *     wrapped {"__script__": true, "ret": [...]}. A scripted null returns None.
 *     None-valued seam-arg OBJECT keys are dropped on both sides.
 *   - state window (st): a copy of in.state with the draft/prompt maps
 *     materialized to {} when absent (mirroring RelayAdapter.__init__) and
 *     mutated in place; _stamp_slack_unfurl / _prepare_slack_egress also carry
 *     st.metadata (the mutated metadata argument).
 *   - fixture domain (ASCII scope): str()/strip()/lower() operands are ASCII;
 *     chat_id/draft_id/reply_to str()-operands are scalars; metadata / fields /
 *     state maps are dict-or-falsy; supported_ops is list-or-falsy (str/dict
 *     membership is modelled but integral-FLOAT type-name positions are
 *     excluded — libjson has no int/float identity); descriptor / config /
 *     supported_ops attribute doubles carry the fields the live body reads.
 *     EXCLUDED: integral-FLOAT literals in str() positions (7.0 reads as 7).
 *
 * libjson-only (default oracle deps suffice). Self-contained: the harness
 * #includes "gateway/relay_adapter_flow2.c". ra2_ prefix (radapter_flow_ is
 * taken by flow1).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>

/* ── universal seam ── */
typedef struct ra2_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} ra2_io_t;

/* ── truthiness / accessors ── */
static bool ra2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool ra2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool ra2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool ra2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool ra2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool ra2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static bool ra2_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return false;
    if (v->type == JSON_BOOL) return v->bool_val;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0;
    return false;
}

static const char *ra2_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *ra2_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static bool ra2_bool(const json_t *o, const char *k, bool def)
{
    const json_t *v = ra2_get(o, k);
    return (v && v->type == JSON_BOOL) ? v->bool_val : def;
}

static bool ra2_str_eq(const char *a, const char *b)
{
    return strcmp(a ? a : "", b ? b : "") == 0;
}

static json_t *ra2_copy_or_null(const json_t *v) { return v ? json_copy(v) : NULL; }

static const char *ra2_type_name(const json_t *v)
{
    if (ra2_is_null(v)) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == (double)(long long)d && fabs(d) < 9.2e18) return "int";
            return "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* type-sensitive JSON equality (draft_id == stored value). */
static bool ra2_json_eq(const json_t *a, const json_t *b)
{
    if (ra2_is_null(a) && ra2_is_null(b)) return true;
    if (ra2_is_null(a) || ra2_is_null(b)) return false;
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return ra2_str_eq(a->str_val, b->str_val);
        case JSON_ARRAY: {
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!ra2_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        }
        case JSON_OBJECT: {
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                const json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !ra2_json_eq(a->c.items[i], bv)) return false;
            }
            return true;
        }
        default: return false;
    }
}

/* ── raise specs + the FULL builtin exception hierarchy ── */
static bool ra2_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}
static const char *ra2_kind(const json_t *r)
{
    const char *k = ra2_str(ra2_get(r, "__raise__"));
    return k ? k : "";
}
static const char *ra2_msg(const json_t *r)
{
    const char *m = ra2_str(ra2_get(r, "message"));
    return m ? m : "";
}
static json_t *ra2_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}
static char *ra2_xfmt(const char *fmt, ...)
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

/* parent table: every builtin Exception subclass walks to Exception; the
 * BaseException trio (and CancelledError) walk to BaseException ONLY. */
typedef struct { const char *child; const char *parent; } ra2_exc_rel_t;
static const ra2_exc_rel_t RA2_EXC_PARENTS[] = {
    {"Exception", "BaseException"},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"CancelledError", "BaseException"},
    {"RuntimeError", "Exception"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"ValueError", "Exception"},
    {"UnicodeError", "ValueError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"TypeError", "Exception"},
    {"AttributeError", "Exception"},
    {"NameError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"LookupError", "Exception"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"ArithmeticError", "Exception"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"FloatingPointError", "ArithmeticError"},
    {"OSError", "Exception"},
    {"IOError", "OSError"},
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
    {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"StopIteration", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"AssertionError", "Exception"},
    {"SyntaxError", "Exception"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"SystemError", "Exception"},
    {"ImportError", "Exception"},
    {"ModuleNotFoundError", "ImportError"},
    {"MemoryError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ReferenceError", "Exception"},
    {"Warning", "Exception"},
    {"UserWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"BytesWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"ExceptionGroup", "Exception"},
    {"BaseExceptionGroup", "BaseException"},
};
static const char *ra2_exc_parent(const char *kind)
{
    for (size_t i = 0; i < sizeof(RA2_EXC_PARENTS) / sizeof(RA2_EXC_PARENTS[0]); i++)
        if (ra2_str_eq(RA2_EXC_PARENTS[i].child, kind))
            return RA2_EXC_PARENTS[i].parent;
    /* unknown scripted kind: the oracle builds type(kind, (Exception,), {}) */
    return "Exception";
}
static bool ra2_is_exception(const char *kind)
{
    const char *k = kind;
    for (int depth = 0; k && depth < 16; depth++) {
        if (ra2_str_eq(k, "Exception")) return true;
        if (ra2_str_eq(k, "BaseException")) return false;
        k = ra2_exc_parent(k);
    }
    return false;
}
static bool ra2_is_base(const char *kind)
{
    return ra2_str_eq(kind, "SystemExit") || ra2_str_eq(kind, "KeyboardInterrupt")
        || ra2_str_eq(kind, "GeneratorExit") || ra2_str_eq(kind, "CancelledError");
}
/* true when a raise spec must pass through an `except Exception` guard. */
static bool ra2_passthrough(const json_t *r)
{
    return ra2_is_base(ra2_kind(r)) || !ra2_is_exception(ra2_kind(r));
}

/* ── universal seam call: args OWNED by caller, freed here; result caller-owned
 * (NULL == None, or a raise spec). Fail-closed when no channel installed. ── */
static json_t *ra2_call(ra2_io_t *io, const char *name, json_t *args)
{
    if (!io || !io->call) {
        char *m = ra2_xfmt("no seam installed for %s", name);
        json_t *r = ra2_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    json_t *r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

/* arg builders: None-valued keys dropped on both sides ("" kept). */
static void ra2_arg_s(json_t *a, const char *k, const char *v)
{
    if (v) json_set(a, k, json_string(v));
}
static void ra2_arg_j(json_t *a, const char *k, const json_t *v)
{
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void ra2_arg_b(json_t *a, const char *k, bool v)
{
    json_set(a, k, json_bool(v));
}

/* logger seam: {msg, args:[...], exc_info?}. args_arr ownership consumed. */
static json_t *ra2_log_arr(ra2_io_t *io, const char *level, const char *msg,
                           json_t *args_arr, bool exc_info)
{
    json_t *a = json_object();
    ra2_arg_s(a, "msg", msg);
    json_set(a, "args", args_arr ? args_arr : json_array());
    if (exc_info) ra2_arg_b(a, "exc_info", true);
    char name[32];
    snprintf(name, sizeof name, "logger.%s", level);
    return ra2_call(io, name, a);
}
static json_t *ra2_log(ra2_io_t *io, const char *level, const char *msg, bool exc_info)
{
    return ra2_log_arr(io, level, msg, json_array(), exc_info);
}

/* invoke a callable double {"__callable__": seam}; args/kwargs consumed. */
static json_t *ra2_invoke_callable(ra2_io_t *io, const json_t *cb,
                                   json_t *args_arr, json_t *kwargs_obj)
{
    const char *name = ra2_str(ra2_get(cb, "__callable__"));
    if (!name) {
        json_free(args_arr);
        json_free(kwargs_obj);
        char *m = ra2_xfmt("'%s' object is not callable", ra2_type_name(cb));
        json_t *r = ra2_mkraise("TypeError", m ? m : "object is not callable");
        free(m);
        return r;
    }
    json_t *a = json_object();
    json_set(a, "args", args_arr ? args_arr : json_array());
    json_set(a, "kwargs", kwargs_obj ? kwargs_obj : json_object());
    return ra2_call(io, name, a);
}

/* ── str() leaf (ASCII scalar domain) ── */
static void ra2_float_repr(double v, char *out, size_t cap)
{
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(out, cap, "%.*g", prec, v);
        if (strtod(out, NULL) == v) return;
    }
    snprintf(out, cap, "%.17g", v);
}
static char *ra2_pystr(const json_t *v)
{
    if (ra2_is_null(v)) return strdup("None");
    if (ra2_is_bool(v)) return strdup(v->bool_val ? "True" : "False");
    if (ra2_is_num(v)) {
        char buf[64];
        double d = v->num_val;
        if (d == (double)(long long)d && fabs(d) < 9.2e18)
            snprintf(buf, sizeof buf, "%lld", (long long)d);
        else
            ra2_float_repr(d, buf, sizeof buf);
        return strdup(buf);
    }
    if (ra2_is_str(v)) return strdup(v->str_val ? v->str_val : "");
    return json_serialize(v);   /* containers: outside the str() fixture domain */
}
static char *ra2_pystr_or_empty(const json_t *v)
{
    if (!ra2_truth(v)) return strdup("");
    return ra2_pystr(v);
}
static char *ra2_str_lower(const char *s)
{
    char *out = strdup(s ? s : "");
    if (!out) return NULL;
    for (char *p = out; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + ('a' - 'A'));
    return out;
}

/* ── SendResult projection (full dataclass field set, defaults included) ── */
static json_t *ra2_mk_send_result(bool success, const json_t *message_id,
                                  const char *error, const json_t *raw_response)
{
    json_t *o = json_object();
    json_set(o, "success", json_bool(success));
    json_set(o, "message_id", message_id ? json_copy(message_id) : json_null());
    json_set(o, "error", error ? json_string(error) : json_null());
    json_set(o, "raw_response", raw_response ? json_copy(raw_response) : json_null());
    json_set(o, "retryable", json_bool(false));
    json_set(o, "retry_after", json_null());
    json_set(o, "continuation_message_ids", json_array());
    json_set(o, "error_kind", json_null());
    return o;
}
/* project a tagged SendResult double to the full field set. */
static json_t *ra2_project_send_result(const json_t *sr)
{
    json_t *o = json_object();
    json_set(o, "success", json_bool(ra2_bool(sr, "success", false)));
    json_set(o, "message_id", ra2_copy_or_null(ra2_get(sr, "message_id")));
    json_set(o, "error", ra2_copy_or_null(ra2_get(sr, "error")));
    json_set(o, "raw_response", ra2_copy_or_null(ra2_get(sr, "raw_response")));
    json_set(o, "retryable", json_bool(ra2_bool(sr, "retryable", false)));
    json_set(o, "retry_after", ra2_copy_or_null(ra2_get(sr, "retry_after")));
    const json_t *cmi = ra2_get(sr, "continuation_message_ids");
    json_set(o, "continuation_message_ids", cmi ? json_copy(cmi) : json_array());
    json_set(o, "error_kind", ra2_copy_or_null(ra2_get(sr, "error_kind")));
    return o;
}

/* ── egress-decline mirror (gateway/relay/egress.py; oracle drives LIVE) ── */
static bool ra2_is_egress_decline(const json_t *result)
{
    if (!ra2_is_obj(result)) return false;
    if (ra2_truth(ra2_get(result, "success"))) return false;
    if (ra2_truth(ra2_get(result, "ambiguous"))) return false;
    char *code = ra2_pystr_or_empty(ra2_get(result, "code"));
    bool by_code = ra2_str_eq(code, "egress_declined");
    free(code);
    if (by_code) return true;
    char *err = ra2_pystr_or_empty(ra2_get(result, "error"));
    char *low = ra2_str_lower(err);
    free(err);
    bool by_text = low && strstr(low, "egress declined:") != NULL;
    free(low);
    return by_text;
}
static char *ra2_decline_error(const json_t *result)
{
    if (ra2_is_obj(result)) {
        const json_t *error = ra2_get(result, "error");
        if (ra2_truth(error)) return ra2_pystr(error);
    }
    return strdup("relay egress declined");
}
/* log_decline: WARNING "relay %s DECLINED for %s: %s" (op, chat, decline_error).
 * Returns the logger seam result (raise spec or NULL). */
static json_t *ra2_log_decline(ra2_io_t *io, const char *op,
                               const json_t *chat_id, const json_t *result)
{
    char *de = ra2_decline_error(result);
    json_t *args = json_array();
    json_append(args, json_string(op));
    json_append(args, chat_id ? json_copy(chat_id) : json_null());
    json_append(args, json_string(de));
    free(de);
    return ra2_log_arr(io, "warning", "relay %s DECLINED for %s: %s", args, false);
}

/* ── state map ensure (mirrors __init__ default {}; mutates st in place) ── */
static json_t *ra2_ensure_map(json_t *st, const char *key)
{
    json_t *m = json_obj_get(st, key);
    if (!m || m->type != JSON_OBJECT) {
        m = json_object();
        json_set(st, key, m);
    }
    return m;
}

/* ════════════════════════ ported cores ════════════════════════ */

/* ── _chat_platform ── */
/* PoP: ra2_chat_platform @ gateway/relay/adapter.py:_chat_platform */
static json_t *ra2_chat_platform(ra2_io_t *io, const json_t *chat_id, json_t *st)
{
    (void)io;
    char *key = ra2_pystr(chat_id);
    const json_t *map = ra2_get(st, "platform_by_chat");
    const json_t *v = map ? json_obj_get(map, key) : NULL;
    free(key);
    if (ra2_truth(v)) return json_copy(v);
    const json_t *desc = ra2_get(st, "descriptor");
    const json_t *plat = desc ? json_obj_get(desc, "platform") : NULL;
    return plat ? json_copy(plat) : json_null();
}

/* ── warning_notifications_enabled ── */
/* PoP: ra2_warning_notifications_enabled @ gateway/relay/adapter.py:warning_notifications_enabled */
static json_t *ra2_warning_notifications_enabled(ra2_io_t *io, const json_t *in, json_t *st)
{
    const json_t *lp = ra2_get(in, "logical_platform");
    const json_t *platform = NULL;
    json_t *cp_owned = NULL;
    if (ra2_truth(lp)) {
        platform = lp;
    } else {
        const json_t *md = ra2_get(in, "metadata");
        const json_t *got = NULL;
        if (ra2_truth(md)) {
            if (!ra2_is_obj(md)) {
                char *m = ra2_xfmt("'%s' object has no attribute 'get'", ra2_type_name(md));
                json_t *r = ra2_mkraise("AttributeError", m ? m : "");
                free(m);
                return r;
            }
            got = json_obj_get(md, "_relay_logical_platform");
        }
        if (ra2_truth(got)) {
            platform = got;
        } else {
            cp_owned = ra2_chat_platform(io, ra2_get(in, "chat_id"), st);
            if (ra2_is_raise(cp_owned)) return cp_owned;
            platform = cp_owned;
        }
    }
    json_t *a = json_object();
    ra2_arg_j(a, "logical_platform", platform);
    json_t *r = ra2_call(io, "super.warning_notifications_enabled", a);
    json_free(cp_owned);
    return r;
}

/* ── _text_metadata ── */
/* PoP: ra2_text_metadata @ gateway/relay/adapter.py:_text_metadata */
static json_t *ra2_text_metadata(ra2_io_t *io, const json_t *chat_id, const json_t *metadata)
{
    json_t *a1 = json_object();
    ra2_arg_j(a1, "chat_id", chat_id);
    ra2_arg_j(a1, "metadata", metadata);
    json_t *r1 = ra2_call(io, "_with_format_hints_for_chat", a1);
    if (ra2_is_raise(r1)) return r1;
    json_t *a2 = json_object();
    ra2_arg_j(a2, "chat_id", chat_id);
    ra2_arg_j(a2, "metadata", r1);
    json_t *r2 = ra2_call(io, "_with_scope", a2);
    json_free(r1);
    return r2;
}

/* ── _draft_frame ── */
/* PoP: ra2_draft_frame @ gateway/relay/adapter.py:_draft_frame */
static json_t *ra2_draft_frame(ra2_io_t *io, const json_t *chat_id, const json_t *draft_id,
                               const json_t *content, const json_t *final, const json_t *metadata)
{
    /* dict(metadata or {}) — dict-or-falsy domain. */
    json_t *mdcopy = (metadata && metadata->type == JSON_OBJECT)
                         ? json_copy(metadata) : json_object();
    json_t *tm = ra2_text_metadata(io, chat_id, mdcopy);
    json_free(mdcopy);
    if (ra2_is_raise(tm)) return tm;
    json_t *frame = json_object();
    json_set(frame, "op", json_string("draft"));
    json_set(frame, "chat_id", ra2_copy_or_null(chat_id));
    json_set(frame, "draft_id", ra2_copy_or_null(draft_id));
    json_set(frame, "content", ra2_copy_or_null(content));
    json_set(frame, "final", final ? json_copy(final) : json_bool(false));
    json_set(frame, "metadata", tm ? tm : json_null());
    return frame;
}

/* ── send_draft ── */
/* PoP: ra2_send_draft @ gateway/relay/adapter.py:send_draft */
static json_t *ra2_send_draft(ra2_io_t *io, const json_t *in, json_t *st)
{
    const json_t *chat_id = ra2_get(in, "chat_id");
    const json_t *draft_id = ra2_get(in, "draft_id");
    const json_t *content = ra2_get(in, "content");
    const json_t *metadata = ra2_get(in, "metadata");
    char *chat_str = ra2_pystr(chat_id);
    char *chat_key = NULL;
    json_t *ret = NULL;

    /* supports_draft_streaming(chat_id=str(chat_id)) */
    json_t *a = json_object();
    ra2_arg_s(a, "chat_id", chat_str);
    json_t *r = ra2_call(io, "supports_draft_streaming", a);
    if (ra2_is_raise(r)) { ret = r; goto done; }
    bool sup = ra2_truth(r);
    json_free(r);
    if (!sup) {
        ret = ra2_mkraise("NotImplementedError",
                          "connector does not advertise the 'draft' relay op");
        goto done;
    }
    /* transport is None */
    if (ra2_is_null(ra2_get(st, "transport"))) {
        ret = ra2_mk_send_result(false, NULL, "no transport", NULL);
        goto done;
    }
    /* chat_key = _draft_key(str(chat_id), metadata) */
    a = json_object();
    ra2_arg_s(a, "chat_id", chat_str);
    ra2_arg_j(a, "metadata", metadata);
    json_t *key_r = ra2_call(io, "_draft_key", a);
    if (ra2_is_raise(key_r)) { ret = key_r; goto done; }
    chat_key = strdup(ra2_str(key_r) ? ra2_str(key_r) : "");
    json_free(key_r);

    /* post-seal straggler: sealed_draft_by_chat.get(chat_key) == draft_id */
    json_t *sealed = ra2_ensure_map(st, "sealed_draft_by_chat");
    if (ra2_json_eq(json_obj_get(sealed, chat_key), draft_id)) {
        json_t *logged = ra2_ensure_map(st, "tombstone_swallow_logged");
        if (!json_obj_get(logged, chat_key)) {
            json_set(logged, chat_key, ra2_copy_or_null(draft_id));
            a = json_object();
            ra2_arg_j(a, "d", logged);
            json_t *er = ra2_call(io, "_evict_oldest", a);
            if (ra2_is_raise(er)) { ret = er; goto done; }
            json_free(er);
            json_t *args = json_array();
            json_append(args, json_string(chat_key));
            json_append(args, ra2_copy_or_null(draft_id));
            json_t *lr = ra2_log_arr(io, "warning",
                "relay: draft frame for %s swallowed by post-seal "
                "tombstone (draft_id=%s) \xe2\x80\x94 expected for a straggler; "
                "a live stream freezing NOW means something sealed it "
                "mid-flight", args, false);
            if (ra2_is_raise(lr)) { ret = lr; goto done; }
            json_free(lr);
        }
        ret = ra2_mk_send_result(true, NULL, NULL, NULL);
        goto done;
    }
    /* arm interception only for stream-is-the-message chats */
    a = json_object();
    ra2_arg_s(a, "chat_id", chat_str);
    json_t *sm = ra2_call(io, "stream_is_message_for_chat", a);
    if (ra2_is_raise(sm)) { ret = sm; goto done; }
    bool sim = ra2_truth(sm);
    json_free(sm);
    if (sim) {
        json_t *open = ra2_ensure_map(st, "open_draft_by_chat");
        json_set(open, chat_key, ra2_copy_or_null(draft_id));
        a = json_object();
        ra2_arg_j(a, "d", open);
        json_t *er = ra2_call(io, "_evict_oldest", a);
        if (ra2_is_raise(er)) { ret = er; goto done; }
        json_free(er);
    }
    /* try: result = await _outbound(chat_id, _draft_frame(...)) */
    json_t *frame = ra2_draft_frame(io, chat_id, draft_id, content, json_bool(false), metadata);
    if (ra2_is_raise(frame)) {
        if (ra2_passthrough(frame)) { ret = frame; goto done; }
        char *m = ra2_xfmt("draft transport error: %s", ra2_msg(frame));
        ret = ra2_mk_send_result(false, NULL, m ? m : "", NULL);
        free(m);
        json_free(frame);
        goto done;
    }
    a = json_object();
    ra2_arg_j(a, "chat_id", chat_id);
    ra2_arg_j(a, "action", frame);
    json_t *result = ra2_call(io, "_outbound", a);
    json_free(frame);
    if (ra2_is_raise(result)) {
        if (ra2_passthrough(result)) { ret = result; goto done; }
        char *m = ra2_xfmt("draft transport error: %s", ra2_msg(result));
        ret = ra2_mk_send_result(false, NULL, m ? m : "", NULL);
        free(m);
        json_free(result);
        goto done;
    }
    /* result.get(...) — outside the try; a non-dict raises uncaught. */
    if (!ra2_is_obj(result)) {
        char *m = ra2_xfmt("'%s' object has no attribute 'get'", ra2_type_name(result));
        ret = ra2_mkraise("AttributeError", m ? m : "");
        free(m);
        json_free(result);
        goto done;
    }
    if (ra2_truth(ra2_get(result, "success"))) {
        ret = ra2_mk_send_result(true, NULL, NULL, NULL);
        json_free(result);
        goto done;
    }
    if (ra2_truth(ra2_get(result, "ambiguous"))) {
        const json_t *err = ra2_get(result, "error");
        char *errs = ra2_truth(err) ? ra2_pystr(err) : strdup("draft ack lost");
        ret = ra2_mk_send_result(false, NULL, errs, result);
        free(errs);
        json_free(result);
        goto done;
    }
    /* DEFINITE rejection: disarm */
    json_t *open = ra2_ensure_map(st, "open_draft_by_chat");
    if (ra2_json_eq(json_obj_get(open, chat_key), draft_id))
        json_obj_del(open, chat_key);
    if (ra2_is_egress_decline(result)) {
        json_t *lr = ra2_log_decline(io, "draft", chat_id, result);
        if (ra2_is_raise(lr)) { ret = lr; json_free(result); goto done; }
        json_free(lr);
    }
    const json_t *err = ra2_get(result, "error");
    char *errs;
    if (ra2_truth(err)) errs = ra2_pystr(err);
    else errs = ra2_decline_error(result);   /* never falsy */
    ret = ra2_mk_send_result(false, NULL, errs, result);
    free(errs);
    json_free(result);

done:
    free(chat_str);
    free(chat_key);
    return ret;
}

/* ── _absorb_into_open_draft ── */
/* PoP: ra2_absorb_into_open_draft @ gateway/relay/adapter.py:_absorb_into_open_draft */
static json_t *ra2_absorb_into_open_draft(ra2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *chat_id = ra2_get(in, "chat_id");
    const json_t *content = ra2_get(in, "content");
    const json_t *metadata = ra2_get(in, "metadata");
    const json_t *interim = ra2_get(in, "interim");
    if (ra2_truth(interim)) return json_null();

    char *chat_str = ra2_pystr(chat_id);
    json_t *a = json_object();
    ra2_arg_s(a, "chat_id", chat_str);
    ra2_arg_j(a, "metadata", metadata);
    json_t *key_r = ra2_call(io, "_match_open_draft", a);
    free(chat_str);
    if (ra2_is_raise(key_r)) return key_r;
    if (ra2_is_null(key_r)) { json_free(key_r); return json_null(); }

    a = json_object();
    ra2_arg_j(a, "chat_id", chat_id);
    ra2_arg_j(a, "content", content);
    ra2_arg_j(a, "metadata", metadata);
    ra2_arg_j(a, "draft_key", key_r);
    json_t *seal = ra2_call(io, "_seal_open_draft", a);
    json_free(key_r);
    if (ra2_is_raise(seal)) return seal;
    if (!ra2_is_obj(seal)) {
        char *m = ra2_xfmt("'%s' object has no attribute 'success'", ra2_type_name(seal));
        json_t *r = ra2_mkraise("AttributeError", m ? m : "");
        free(m);
        json_free(seal);
        return r;
    }
    if (ra2_bool(seal, "success", false)) {
        json_t *p = ra2_project_send_result(seal);
        json_free(seal);
        return p;
    }
    if (ra2_is_egress_decline(ra2_get(seal, "raw_response"))) {
        json_t *args = json_array();
        json_append(args, ra2_copy_or_null(chat_id));
        json_t *lr = ra2_log_arr(io, "warning",
            "relay draft seal DECLINED for %s \xe2\x80\x94 not falling back to a plain "
            "send (the destination is not approved for this connection)", args, false);
        if (ra2_is_raise(lr)) { json_free(seal); return lr; }
        json_free(lr);
        json_t *p = ra2_project_send_result(seal);
        json_free(seal);
        return p;
    }
    json_t *args = json_array();
    json_append(args, ra2_copy_or_null(ra2_get(seal, "error")));
    json_t *lr = ra2_log_arr(io, "warning",
        "relay seal failed (%s); delivering turn-final as plain send", args, false);
    if (ra2_is_raise(lr)) { json_free(seal); return lr; }
    json_free(lr);
    json_free(seal);
    return json_null();
}

/* ── _card_frame ── */
/* PoP: ra2_card_frame @ gateway/relay/adapter.py:_card_frame */
static json_t *ra2_card_frame(ra2_io_t *io, const json_t *in, json_t *st)
{
    const json_t *chat_id = ra2_get(in, "chat_id");
    const json_t *op = ra2_get(in, "op");
    const json_t *reply_to = ra2_get(in, "reply_to");
    const json_t *metadata = ra2_get(in, "metadata");
    const json_t *fields = ra2_get(in, "fields");

    json_t *a = json_object();
    json_t *r = ra2_call(io, "supports_native_task_cards", a);
    if (ra2_is_raise(r)) return r;
    bool sup = ra2_truth(r);
    json_free(r);
    if (!sup) return ra2_mk_send_result(false, NULL, "connector does not advertise task_card", NULL);
    if (ra2_is_null(ra2_get(st, "transport")))
        return ra2_mk_send_result(false, NULL, "no transport", NULL);

    json_t *frame = json_object();
    json_set(frame, "op", ra2_copy_or_null(op));
    json_set(frame, "chat_id", ra2_copy_or_null(chat_id));
    a = json_object();
    ra2_arg_j(a, "reply_to", reply_to);
    ra2_arg_j(a, "metadata", metadata);
    json_t *ck = ra2_call(io, "_card_key", a);
    if (ra2_is_raise(ck)) { json_free(frame); return ck; }
    json_set(frame, "card_id", ck ? ck : json_null());
    /* **fields splat (dict display: later keys overwrite, position kept). */
    if (fields && fields->type == JSON_OBJECT) {
        for (size_t i = 0; i < fields->c.count; i++)
            json_set(frame, fields->c.keys[i], json_copy(fields->c.items[i]));
    }
    a = json_object();
    ra2_arg_j(a, "chat_id", chat_id);
    ra2_arg_j(a, "metadata", metadata);
    json_t *ws = ra2_call(io, "_with_scope", a);
    if (ra2_is_raise(ws)) { json_free(frame); return ws; }
    json_set(frame, "metadata", ws ? ws : json_null());

    /* try: return await _outbound(chat_id, frame) */
    a = json_object();
    ra2_arg_j(a, "chat_id", chat_id);
    ra2_arg_j(a, "action", frame);
    json_t *result = ra2_call(io, "_outbound", a);
    json_free(frame);
    if (ra2_is_raise(result)) {
        if (ra2_passthrough(result)) return result;
        char *opstr = ra2_pystr(op);
        char *m = ra2_xfmt("%s transport error: %s", opstr, ra2_msg(result));
        free(opstr);
        json_t *sr = ra2_mk_send_result(false, NULL, m ? m : "", NULL);
        free(m);
        json_free(result);
        return sr;
    }
    return result ? result : json_null();
}

/* ── _task_card_metadata (staticmethod) ── */
/* PoP: ra2_task_card_metadata @ gateway/relay/adapter.py:_task_card_metadata */
static json_t *ra2_task_card_metadata(ra2_io_t *io, const json_t *reply_to, const json_t *metadata)
{
    (void)io;
    json_t *merged = (metadata && metadata->type == JSON_OBJECT)
                         ? json_copy(metadata) : json_object();
    if (ra2_truth(reply_to) && !json_obj_get(merged, "thread_ts")) {
        char *rs = ra2_pystr(reply_to);
        json_set(merged, "thread_ts", json_string(rs));
        free(rs);
    }
    return merged;
}

/* ── native_task_card_destination_supported ── */
/* PoP: ra2_native_task_card_destination_supported @ gateway/relay/adapter.py:native_task_card_destination_supported */
static json_t *ra2_native_task_card_destination_supported(ra2_io_t *io, const json_t *in, json_t *st)
{
    const json_t *chat_id = ra2_get(in, "chat_id");
    json_t *cp = ra2_chat_platform(io, chat_id, st);
    if (ra2_is_raise(cp)) return cp;
    bool is_slack = ra2_is_str(cp) && ra2_str_eq(ra2_str(cp), "slack");
    json_free(cp);
    if (!is_slack) return json_bool(true);

    json_t *md = ra2_task_card_metadata(io, ra2_get(in, "reply_to"), ra2_get(in, "metadata"));
    if (ra2_is_raise(md)) return md;
    const json_t *thread = json_obj_get(md, "thread_id");
    if (ra2_is_null(thread)) thread = json_obj_get(md, "thread_ts");
    bool r = ra2_is_str(thread);
    json_free(md);
    return json_bool(r);
}

/* ── _relay_platform_extra ── */
/* PoP: ra2_relay_platform_extra @ gateway/relay/adapter.py:_relay_platform_extra */
static json_t *ra2_relay_platform_extra(ra2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    const json_t *platform = ra2_get(in, "platform");
    const json_t *config = ra2_get(st, "config");
    const json_t *extra = config ? ra2_get(config, "extra") : NULL;
    json_t *empty_owned = NULL;
    const json_t *src;
    if (!ra2_truth(extra)) {
        empty_owned = json_object();
        src = empty_owned;
    } else if (ra2_is_obj(extra)) {
        src = extra;
    } else {
        char *m = ra2_xfmt("'%s' object has no attribute 'get'", ra2_type_name(extra));
        json_t *r = ra2_mkraise("AttributeError", m ? m : "");
        free(m);
        return r;
    }
    const char *ps = ra2_str(platform);
    const json_t *sub = json_obj_get(src, ps ? ps : "");
    json_t *ret = (sub && sub->type == JSON_OBJECT) ? json_copy(sub) : json_copy(src);
    json_free(empty_owned);
    return ret;
}

/* ── _slack_flag ── */
/* PoP: ra2_slack_flag @ gateway/relay/adapter.py:_slack_flag */
static json_t *ra2_slack_flag(ra2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *knob = ra2_get(in, "knob");
    bool dflt = ra2_bool(in, "default", false);
    /* try: _coerce_flag(_relay_slack_extra().get(knob), default) */
    json_t *a = json_object();
    json_t *extra_r = ra2_call(io, "_relay_slack_extra", a);
    if (ra2_is_raise(extra_r)) {
        if (ra2_passthrough(extra_r)) return extra_r;
        json_free(extra_r);
        return json_bool(dflt);
    }
    if (!ra2_is_obj(extra_r)) {   /* None / non-dict .get -> AttributeError caught */
        json_free(extra_r);
        return json_bool(dflt);
    }
    const char *knobs = ra2_str(knob);
    const json_t *raw = json_obj_get(extra_r, knobs ? knobs : "");
    a = json_object();
    ra2_arg_j(a, "raw", raw);
    ra2_arg_b(a, "default", dflt);
    json_t *cr = ra2_call(io, "_coerce_flag", a);
    json_free(extra_r);
    if (ra2_is_raise(cr)) {
        if (ra2_passthrough(cr)) return cr;
        json_free(cr);
        return json_bool(dflt);
    }
    return cr ? cr : json_bool(dflt);
}

/* ── _stamp_slack_unfurl (mutates st.metadata) ── */
/* PoP: ra2_stamp_slack_unfurl @ gateway/relay/adapter.py:_stamp_slack_unfurl */
static json_t *ra2_stamp_slack_unfurl(ra2_io_t *io, const json_t *platform, json_t *md)
{
    json_t *a = json_object();
    ra2_arg_j(a, "platform", platform);
    json_t *unfurl = ra2_call(io, "_slack_unfurl_hints", a);
    if (ra2_is_raise(unfurl)) return unfurl;
    if (ra2_truth(unfurl)) {
        if (!ra2_is_obj(md)) {
            char *m = ra2_xfmt("'%s' object has no attribute 'update'", ra2_type_name(md));
            json_t *r = ra2_mkraise("AttributeError", m ? m : "");
            free(m);
            json_free(unfurl);
            return r;
        }
        if (!ra2_is_obj(unfurl)) {
            json_free(unfurl);
            return ra2_mkraise("TypeError", "object is not iterable");
        }
        for (size_t i = 0; i < unfurl->c.count; i++)
            json_set(md, unfurl->c.keys[i], json_copy(unfurl->c.items[i]));
    }
    json_free(unfurl);
    return json_null();
}

/* ── hold_redial ── */
/* PoP: ra2_hold_redial @ gateway/relay/adapter.py:hold_redial */
static json_t *ra2_hold_redial(ra2_io_t *io, const json_t *in, json_t *st)
{
    (void)in; (void)st;
    json_t *a = json_object();
    ra2_arg_s(a, "method_name", "hold_redial");
    return ra2_call(io, "_toggle_transport_redial", a);
}

/* ── release_redial ── */
/* PoP: ra2_release_redial @ gateway/relay/adapter.py:release_redial */
static json_t *ra2_release_redial(ra2_io_t *io, const json_t *in, json_t *st)
{
    (void)in; (void)st;
    json_t *a = json_object();
    ra2_arg_s(a, "method_name", "release_redial");
    return ra2_call(io, "_toggle_transport_redial", a);
}

/* ── _prepare_slack_egress (mutates st.metadata) ── */
/* PoP: ra2_prepare_slack_egress @ gateway/relay/adapter.py:_prepare_slack_egress */
static json_t *ra2_prepare_slack_egress(ra2_io_t *io, const json_t *in, json_t *st)
{
    const json_t *chat_id = ra2_get(in, "chat_id");
    const json_t *reply_to = ra2_get(in, "reply_to");
    json_t *md = json_obj_get(st, "metadata");
    json_t *a = json_object();
    ra2_arg_j(a, "chat_id", chat_id);
    ra2_arg_j(a, "reply_to", reply_to);
    ra2_arg_j(a, "metadata", md);
    json_t *eff = ra2_call(io, "_apply_slack_thread_anchor", a);
    if (ra2_is_raise(eff)) return eff;

    json_t *cp = ra2_chat_platform(io, chat_id, st);
    if (ra2_is_raise(cp)) { json_free(eff); return cp; }
    json_t *sr = ra2_stamp_slack_unfurl(io, cp, md);
    json_free(cp);
    if (ra2_is_raise(sr)) { json_free(eff); return sr; }
    json_free(sr);
    return eff ? eff : json_null();
}

/* ── status_metadata_for_turn ── */
/* PoP: ra2_status_metadata_for_turn @ gateway/relay/adapter.py:status_metadata_for_turn */
static json_t *ra2_status_metadata_for_turn(ra2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *a = json_object();
    ra2_arg_j(a, "chat_id", ra2_get(in, "chat_id"));
    ra2_arg_j(a, "metadata", ra2_get(in, "metadata"));
    ra2_arg_j(a, "anchor", ra2_get(in, "anchor"));
    return ra2_call(io, "_with_status_thread_anchor", a);
}

/* ── delete_message ── */
/* PoP: ra2_delete_message @ gateway/relay/adapter.py:delete_message */
static json_t *ra2_delete_message(ra2_io_t *io, const json_t *in, json_t *st)
{
    const json_t *chat_id = ra2_get(in, "chat_id");
    const json_t *message_id = ra2_get(in, "message_id");
    if (ra2_is_null(ra2_get(st, "transport"))) return json_bool(false);

    char *cs = ra2_pystr(chat_id);
    json_t *a = json_object();
    ra2_arg_s(a, "chat_id", cs);
    json_t *desc_r = ra2_call(io, "_descriptor_for_chat", a);
    free(cs);
    if (ra2_is_raise(desc_r)) return desc_r;
    if (!ra2_is_obj(desc_r)) {
        char *m = ra2_xfmt("'%s' object has no attribute 'supported_ops'", ra2_type_name(desc_r));
        json_t *r = ra2_mkraise("AttributeError", m ? m : "");
        free(m);
        json_free(desc_r);
        return r;
    }
    const json_t *ops = ra2_get(desc_r, "supported_ops");
    if (!ops) {   /* absent attribute on the SimpleNamespace double */
        json_free(desc_r);
        return ra2_mkraise("AttributeError",
                           "'types.SimpleNamespace' object has no attribute 'supported_ops'");
    }
    /* "delete" not in (supported_ops or ()) */
    bool found = false;
    if (ra2_truth(ops)) {
        if (ra2_is_arr(ops)) {
            for (size_t i = 0; i < ops->c.count; i++) {
                const json_t *o = ops->c.items[i];
                if (ra2_is_str(o) && ra2_str_eq(o->str_val, "delete")) { found = true; break; }
            }
        } else if (ra2_is_str(ops)) {
            found = strstr(ops->str_val, "delete") != NULL;
        } else if (ra2_is_obj(ops)) {
            found = json_obj_get(ops, "delete") != NULL;
        } else {
            char *m = ra2_xfmt("argument of type '%s' is not a container or iterable",
                               ra2_type_name(ops));
            json_t *r = ra2_mkraise("TypeError", m ? m : "");
            free(m);
            json_free(desc_r);
            return r;
        }
    }
    json_free(desc_r);
    if (!found) return json_bool(false);

    /* try: frame metadata = _with_scope(chat_id, {}); _outbound(...) */
    json_t *empty = json_object();
    a = json_object();
    ra2_arg_j(a, "chat_id", chat_id);
    ra2_arg_j(a, "metadata", empty);
    json_free(empty);
    json_t *ws = ra2_call(io, "_with_scope", a);
    bool failed = false;
    json_t *result = NULL;
    if (ra2_is_raise(ws)) {
        if (ra2_passthrough(ws)) return ws;
        json_free(ws);
        failed = true;
    } else {
        json_t *frame = json_object();
        json_set(frame, "op", json_string("delete"));
        json_set(frame, "chat_id", ra2_copy_or_null(chat_id));
        json_set(frame, "message_id", ra2_copy_or_null(message_id));
        json_set(frame, "metadata", ws ? ws : json_null());
        a = json_object();
        ra2_arg_j(a, "chat_id", chat_id);
        ra2_arg_j(a, "action", frame);
        result = ra2_call(io, "_outbound", a);
        json_free(frame);
        if (ra2_is_raise(result)) {
            if (ra2_passthrough(result)) return result;
            json_free(result);
            result = NULL;
            failed = true;
        }
    }
    if (failed) {
        json_t *lr = ra2_log(io, "debug", "relay delete_message failed", true);
        if (ra2_is_raise(lr)) return lr;
        json_free(lr);
        return json_bool(false);
    }
    /* return bool(result.get("success")) — outside try. */
    if (!ra2_is_obj(result)) {
        char *m = ra2_xfmt("'%s' object has no attribute 'get'", ra2_type_name(result));
        json_t *r = ra2_mkraise("AttributeError", m ? m : "");
        free(m);
        json_free(result);
        return r;
    }
    bool ok = ra2_truth(ra2_get(result, "success"));
    json_free(result);
    return json_bool(ok);
}

/* ── _send_local_media ── */
/* PoP: ra2_send_local_media @ gateway/relay/adapter.py:_send_local_media */
static json_t *ra2_send_local_media(ra2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *chat_id = ra2_get(in, "chat_id");
    const json_t *fallback = ra2_get(in, "fallback");
    json_t *a = json_object();
    ra2_arg_j(a, "chat_id", chat_id);
    ra2_arg_j(a, "media_kind", ra2_get(in, "media_kind"));
    ra2_arg_j(a, "source", ra2_get(in, "path"));
    ra2_arg_b(a, "source_is_path", true);
    ra2_arg_j(a, "caption", ra2_get(in, "caption"));
    ra2_arg_j(a, "filename", ra2_get(in, "filename"));
    ra2_arg_j(a, "reply_to", ra2_get(in, "reply_to"));
    ra2_arg_j(a, "metadata", ra2_get(in, "metadata"));
    json_t *result = ra2_call(io, "_send_media", a);
    if (ra2_is_raise(result)) return result;

    /* isinstance(result, SendResult): not None and not _MediaNotDelivered. */
    if (!ra2_is_null(result) && !ra2_truth(ra2_get(result, "__not_delivered__"))) {
        json_t *p = ra2_project_send_result(result);
        json_free(result);
        return p;
    }
    /* shown = await fallback() */
    json_t *shown = ra2_invoke_callable(io, fallback, json_array(), json_object());
    if (ra2_is_raise(shown)) { json_free(result); return shown; }
    if (ra2_is_null(result)) {
        json_free(result);
        json_t *p = ra2_project_send_result(shown);
        json_free(shown);
        return p;
    }
    /* _MediaNotDelivered: failed send carrying the fallback notice. */
    char *reasons = ra2_pystr(ra2_get(result, "reason"));
    char *errs = ra2_xfmt("attachment not delivered: %s", reasons);
    free(reasons);
    json_t *raw = json_object();
    json_set(raw, "failure_notice_sent", json_bool(ra2_bool(shown, "success", false)));
    json_set(raw, "fallback", ra2_copy_or_null(ra2_get(shown, "raw_response")));
    json_t *sr = ra2_mk_send_result(false, ra2_get(shown, "message_id"), errs, raw);
    free(errs);
    json_free(raw);
    json_free(result);
    json_free(shown);
    return sr;
}

/* ── _mint_and_send_prompt ── */
/* PoP: ra2_mint_and_send_prompt @ gateway/relay/adapter.py:_mint_and_send_prompt */
static json_t *ra2_mint_and_send_prompt(ra2_io_t *io, const json_t *kind, const json_t *state_node,
                                        const json_t *chat_id, const json_t *prompt_kind,
                                        const json_t *text, const json_t *options,
                                        const json_t *metadata, json_t *st)
{
    /* {**state, "chat_id": str(chat_id)} — dict-or-null domain. */
    json_t *merged = (state_node && state_node->type == JSON_OBJECT)
                         ? json_copy(state_node) : json_object();
    char *cs = ra2_pystr(chat_id);
    json_set(merged, "chat_id", json_string(cs));
    free(cs);
    json_t *a = json_object();
    ra2_arg_j(a, "kind", kind);
    ra2_arg_j(a, "state", merged);
    json_t *pid_r = ra2_call(io, "_mint_prompt", a);
    json_free(merged);
    if (ra2_is_raise(pid_r)) return pid_r;

    a = json_object();
    ra2_arg_j(a, "chat_id", chat_id);
    ra2_arg_j(a, "prompt_kind", prompt_kind);
    ra2_arg_j(a, "text", text);
    ra2_arg_j(a, "prompt_id", pid_r);
    ra2_arg_j(a, "options", options);
    ra2_arg_j(a, "metadata", metadata);
    json_t *result = ra2_call(io, "_send_prompt", a);
    if (ra2_is_raise(result)) { json_free(pid_r); return result; }

    /* if result is None or not getattr(result, "success", False): pop */
    bool do_pop = ra2_is_null(result) || !ra2_bool(result, "success", false);
    if (do_pop) {
        json_t *pp = ra2_ensure_map(st, "pending_prompts");
        const char *pid = ra2_str(pid_r);
        if (pid) json_obj_del(pp, pid);
    }
    json_free(pid_r);
    if (ra2_is_null(result)) return json_null();
    json_t *p = ra2_project_send_result(result);
    json_free(result);
    return p;
}

/* ── _send_exec_approval_prompt ── */
/* PoP: ra2_send_exec_approval_prompt @ gateway/relay/adapter.py:_send_exec_approval_prompt */
static json_t *ra2_send_exec_approval_prompt(ra2_io_t *io, const json_t *in, json_t *st)
{
    const json_t *prompt = ra2_get(in, "prompt");
    const json_t *actions = ra2_get(prompt, "actions");
    /* options comprehension is ATOMIC: a bad row discards all partial results
     * and fires no seam. */
    json_t *options = json_array();
    if (!ra2_is_arr(actions)) {
        char *m = ra2_xfmt("'%s' object is not iterable", ra2_type_name(actions));
        json_t *r = ra2_mkraise("TypeError", m ? m : "");
        free(m);
        json_free(options);
        return r;
    }
    for (size_t i = 0; i < actions->c.count; i++) {
        const json_t *row = actions->c.items[i];
        if (!ra2_is_arr(row)) {
            char *m = ra2_xfmt("cannot unpack non-iterable %s object", ra2_type_name(row));
            json_t *r = ra2_mkraise("TypeError", m ? m : "");
            free(m);
            json_free(options);
            return r;
        }
        if (row->c.count != 3) {
            char *m = (row->c.count < 3)
                ? ra2_xfmt("not enough values to unpack (expected 3, got %zu)", row->c.count)
                : ra2_xfmt("too many values to unpack (expected 3, got %zu)", row->c.count);
            json_t *r = ra2_mkraise("ValueError", m ? m : "");
            free(m);
            json_free(options);
            return r;
        }
        const json_t *label = row->c.items[0];
        const json_t *choice = row->c.items[1];
        const json_t *style = row->c.items[2];
        json_t *opt = json_object();
        json_set(opt, "id", ra2_copy_or_null(choice));
        json_set(opt, "label", ra2_copy_or_null(label));
        if (ra2_truth(style)) json_set(opt, "style", json_copy(style));
        json_append(options, opt);
    }

    json_t *state = json_object();
    json_set(state, "session_key", ra2_copy_or_null(ra2_get(prompt, "session_key")));
    json_set(state, "request_id", ra2_copy_or_null(ra2_get(prompt, "request_id")));
    json_t *kind = json_string("exec_approval");
    json_t *pk = json_string("approval");
    json_t *result = ra2_mint_and_send_prompt(io, kind, state, ra2_get(prompt, "chat_id"),
                                              pk, ra2_get(prompt, "text"), options,
                                              ra2_get(prompt, "metadata"), st);
    json_free(state);
    json_free(kind);
    json_free(pk);
    json_free(options);
    if (ra2_is_raise(result)) return result;
    if (!ra2_is_null(result)) return result;
    /* self._PROMPT_UNAVAILABLE */
    return ra2_mk_send_result(false, NULL, "relay prompt op unavailable", NULL);
}

/* ════════════════════════ dispatch wrappers ════════════════════════ */
static json_t *ra2_w_chat_platform(ra2_io_t *io, const json_t *in, json_t *st)
{ return ra2_chat_platform(io, ra2_get(in, "chat_id"), st); }

static json_t *ra2_w_text_metadata(ra2_io_t *io, const json_t *in, json_t *st)
{ (void)st; return ra2_text_metadata(io, ra2_get(in, "chat_id"), ra2_get(in, "metadata")); }

static json_t *ra2_w_draft_frame(ra2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ra2_draft_frame(io, ra2_get(in, "chat_id"), ra2_get(in, "draft_id"),
                           ra2_get(in, "content"), ra2_get(in, "final"), ra2_get(in, "metadata"));
}

static json_t *ra2_w_task_card_metadata(ra2_io_t *io, const json_t *in, json_t *st)
{ (void)st; return ra2_task_card_metadata(io, ra2_get(in, "reply_to"), ra2_get(in, "metadata")); }

static json_t *ra2_w_stamp_slack_unfurl(ra2_io_t *io, const json_t *in, json_t *st)
{ return ra2_stamp_slack_unfurl(io, ra2_get(in, "platform"), json_obj_get(st, "metadata")); }

static json_t *ra2_w_mint_and_send_prompt(ra2_io_t *io, const json_t *in, json_t *st)
{
    return ra2_mint_and_send_prompt(io, ra2_get(in, "kind"), ra2_get(in, "state"),
                                    ra2_get(in, "chat_id"), ra2_get(in, "prompt_kind"),
                                    ra2_get(in, "text"), ra2_get(in, "options"),
                                    ra2_get(in, "metadata"), st);
}

typedef json_t *(*ra2_fn_t)(ra2_io_t *, const json_t *, json_t *);
typedef struct { const char *func; ra2_fn_t fn; } ra2_dispatch_t;
static const ra2_dispatch_t RA2_DISPATCH[] = {
    { "_chat_platform", ra2_w_chat_platform },
    { "warning_notifications_enabled", ra2_warning_notifications_enabled },
    { "_text_metadata", ra2_w_text_metadata },
    { "_draft_frame", ra2_w_draft_frame },
    { "send_draft", ra2_send_draft },
    { "_absorb_into_open_draft", ra2_absorb_into_open_draft },
    { "_card_frame", ra2_card_frame },
    { "_task_card_metadata", ra2_w_task_card_metadata },
    { "native_task_card_destination_supported", ra2_native_task_card_destination_supported },
    { "_relay_platform_extra", ra2_relay_platform_extra },
    { "_slack_flag", ra2_slack_flag },
    { "_stamp_slack_unfurl", ra2_w_stamp_slack_unfurl },
    { "hold_redial", ra2_hold_redial },
    { "release_redial", ra2_release_redial },
    { "_prepare_slack_egress", ra2_prepare_slack_egress },
    { "status_metadata_for_turn", ra2_status_metadata_for_turn },
    { "delete_message", ra2_delete_message },
    { "_send_local_media", ra2_send_local_media },
    { "_mint_and_send_prompt", ra2_w_mint_and_send_prompt },
    { "_send_exec_approval_prompt", ra2_send_exec_approval_prompt },
};
#define RA2_DISPATCH_N (sizeof(RA2_DISPATCH) / sizeof(RA2_DISPATCH[0]))