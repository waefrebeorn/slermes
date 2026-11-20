/* ===========================================================================
 * error_classifier_flow2.c -- FLOW port of the NINE unclaimed leaves of
 * agent/error_classifier.py left over by error_classifier.c (cli) and
 * error_classifier_flow.c (flow1). Flow1's header records that these were
 * deliberately not ported: the regex-heavy leaves and their callers.
 *
 * Ported (PoP, live name -> entry):
 *   _is_byte_size_limit_message   ec2_is_byte_size_limit_message_entry
 *   is_reasoning_level_rejection  ec2_is_reasoning_level_rejection_entry
 *   is_reasoning_required_rejection ec2_is_reasoning_required_rejection_entry
 *   is_reasoning_field_rejection  ec2_is_reasoning_field_rejection_entry
 *   _account_gates_model          ec2_account_gates_model_entry
 *   _by_message                   ec2_by_message_entry
 *   _has_large_inline_image       ec2_has_large_inline_image_entry
 *   _oversized_message_content_rejection ec2_oversized_message_content_rejection_entry
 *   _classify_image_tool_422      ec2_classify_image_tool_422_entry
 *
 * Self-contained: libjson + libc only. Every live dependency that is NOT one
 * of the nine ported defs is injected through ONE universal seam:
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * args is borrowed by the dispatcher and freed by ec2_call after the call;
 * the result is NEW json_t owned by the caller (NULL == None; a
 * {"__raise__":kind,"message":msg} object == a raised exception). A scripted
 * JSON null result is delivered as a NULL pointer (falsy). Fail-closed: no
 * channel / missing script -> RuntimeError "no script for <name>".
 *
 * The compiled regex OBJECTS are treated as recordable collaborators (like
 * flow1's claimed leaves): the generator computes each pattern's answer from
 * the real quarry regex and scripts it into in.io. The port transcribes the
 * CONTROL FLOW around those patterns exactly (short-circuit order, near-window
 * slices, marker wordings, finditer-islice(4), detail traversal).
 *
 * Full builtin exception hierarchy (EC2_EXC_TABLE / ec2_exc_is_exception) is
 * included: a scripted raise from any seam is classified against it. None of
 * these nine defs has an `except` site, so every raise propagates verbatim;
 * the table is the single source of truth for the kind (BaseException-only
 * kinds pass through an `except Exception`, which none of these has).
 *
 * ENTRIES. json_t *ec2_<fn>_entry(ec2_io_t *io, const json_t *in, json_t *st):
 *   io  -- universal seam channel (NULL => fail-closed); io->data is in.io.
 *   in  -- borrowed case input; reads msg / body / content (never mutates).
 *   st  -- fresh object; stays {} on EVERY path (these mutate no caller state).
 *   Caller owns the returned json_t. None -> NULL; bool -> json_bool.
 *
 * Domain restrictions (documented, enforced by the generator):
 *   - ASCII inputs; lower()/slices are byte-exact ASCII.
 *   - re.split token set is scripted as the matched request words.
 *   - loc elements in _oversized_message_content_rejection are strings or
 *     integral numbers; str()/lower() transcribed for those.
 *   - verdict constants + _IMAGE_SHRINK_TARGET_BYTES are scripted in io.
 * =========================================================================== */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* libjson API (harness includes the real header before this file) */
#ifndef EC2_JSON_PREINCLUDED
#include "hermes_json.h"
#endif

typedef struct {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
    const json_t *data;   /* borrowed in.io: scripted constants */
} ec2_io_t;

/* ---- tiny helpers ---- */
static const char *ec2_s(const json_t *n)
{
    return (n && n->type == JSON_STRING) ? n->str_val : "";
}

static json_t *ec2_g(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static int ec2_t(const json_t *n)
{
    if (!n || n->type == JSON_NULL) return 0;
    if (n->type == JSON_BOOL) return n->bool_val ? 1 : 0;
    if (n->type == JSON_NUMBER) return n->num_val != 0.0;
    if (n->type == JSON_STRING) return n->str_val && n->str_val[0] ? 1 : 0;
    if (n->type == JSON_ARRAY) return n->c.count > 0;
    if (n->type == JSON_OBJECT) return n->c.count > 0;
    return 0;
}

static char *ec2_lower(const char *s)
{
    size_t n = strlen(s), i;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        out[i] = (char)((c >= 'A' && c <= 'Z') ? (c + 32) : c);
    }
    out[n] = '\0';
    return out;
}

static int ec2_has(const char *hay, const char *needle)
{
    return hay && needle && strstr(hay, needle) != NULL;
}

/* Python msg[max(0,start-back):end+fwd] on ASCII bytes. */
static char *ec2_near(const char *msg, int start, int end, int back, int fwd)
{
    int L = (int)strlen(msg);
    int s = start - back; if (s < 0) s = 0;
    int e = end + fwd;    if (e > L) e = L;
    if (s > e) s = e;
    char *out = (char *)malloc((size_t)(e - s) + 1);
    if (!out) return NULL;
    if (e > s) memcpy(out, msg + s, (size_t)(e - s));
    out[e - s] = '\0';
    return out;
}

/* int field of a match dict (scripted {"start":i,"end":j}). */
static int ec2_mfield(const json_t *m, const char *key, int dflt)
{
    const json_t *v = ec2_g(m, key);
    return (v && v->type == JSON_NUMBER) ? (int)v->num_val : dflt;
}

/* Python str(x) for the fixture domain (strings / numbers / bool / None). */
static char *ec2_pystr(const json_t *v)
{
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d)
            snprintf(buf, sizeof(buf), "%lld", (long long)d);
        else
            snprintf(buf, sizeof(buf), "%.17g", d);
        return strdup(buf);
    }
    return strdup("");
}

/* ---- FULL builtin exception hierarchy (CPython 3.14) ----
 * parent table; unknown kinds default to Exception (synthetic subclass). */
typedef struct { const char *name; const char *parent; } ec2_exc_row_t;
static const ec2_exc_row_t EC2_EXC_TABLE[] = {
    {"BaseException", NULL},
    {"BaseExceptionGroup", "BaseException"},
    {"Exception", "BaseException"},
    {"ExceptionGroup", "Exception"},
    {"ArithmeticError", "Exception"},
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ImportError", "Exception"},
    {"ModuleNotFoundError", "ImportError"},
    {"LookupError", "Exception"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"MemoryError", "Exception"},
    {"NameError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"OSError", "Exception"},
    {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"},
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"Warning", "Exception"},
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
    {"GeneratorExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"SystemExit", "BaseException"},
    {"CancelledError", "BaseException"},
    /* seeded stub lookalikes */
    {"MoAPresetNotFoundError", "ValueError"},
    {"InvalidProviderResponseError", "RuntimeError"},
    {"GeminiAPIError", "Exception"},
    {"APIError", "Exception"},
    {"RateLimitError", "Exception"},
    {"AuthenticationError", "Exception"},
    {NULL, NULL}
};

static const char *ec2_exc_parent(const char *kind)
{
    for (int i = 0; EC2_EXC_TABLE[i].name; i++)
        if (strcmp(EC2_EXC_TABLE[i].name, kind) == 0)
            return EC2_EXC_TABLE[i].parent;
    return "Exception";
}

static int ec2_exc_is_instance(const char *kind, const char *target)
{
    const char *cur = kind;
    for (int depth = 0; cur && depth < 64; depth++) {
        if (strcmp(cur, target) == 0) return 1;
        cur = ec2_exc_parent(cur);
    }
    return 0;
}

static int ec2_exc_is_exception(const char *kind)
{
    return ec2_exc_is_instance(kind, "Exception");
}

static int ec2_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && ec2_g(r, "__raise__") != NULL;
}

static json_t *ec2_mkraise(const char *kind, const char *msg)
{
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "message", json_string(msg ? msg : ""));
    return r;
}

/* ---- universal seam call ---- */
static json_t *ec2_call(ec2_io_t *io, const char *name, json_t *args)
{
    json_t *r = NULL;
    if (io && io->call) r = io->call(io->ctx, name, args);
    else {
        char m[160];
        snprintf(m, sizeof(m), "no seam installed for %s", name);
        r = ec2_mkraise("RuntimeError", m);
    }
    json_free(args);   /* dispatcher borrowed it */
    if (r && ec2_is_raise(r)) {
        /* classify against the full builtin hierarchy; no `except` here, so
         * every raise propagates. ec2_exc_is_exception keeps the table live. */
        const json_t *k = ec2_g(r, "__raise__");
        (void)ec2_exc_is_exception(ec2_s(k));
    }
    return r;   /* NULL == None */
}

/* read a scripted constant (no seam event) */
static json_t *ec2_const(ec2_io_t *io, const char *name)
{
    return (io && io->data) ? ec2_g(io->data, name) : NULL;
}

static json_t *ec2_const_copy(ec2_io_t *io, const char *name)
{
    const json_t *v = ec2_const(io, name);
    return v ? json_copy(v) : json_object();
}

/* ---- marker tables transcribed verbatim from the quarry ---- */
static const char *EC2_UNSUPPORTED_PARAM_MARKERS[] = {
    "unsupported parameter", "unsupported_parameter", "not supported",
    "does not support", "doesn't support", "is deprecated for this model",
    "unknown parameter", "unrecognized request argument", "unrecognized parameter",
    "invalid parameter", "extra inputs are not permitted",
    "invalid option: expected one of", NULL
};
static const char *EC2_REASONING_REQUIRED_MARKERS[] = {
    "mandatory", "cannot be disabled", "can't be disabled", "must be enabled",
    "is required", "always enabled", "cannot be turned off", NULL
};
static const char *EC2_REASONING_LEVEL_MARKERS[] = {
    "must be one of", "invalid value for", NULL
};

static int ec2_any_in(const char *hay, const char *const *tbl)
{
    for (int i = 0; tbl[i]; i++) if (ec2_has(hay, tbl[i])) return 1;
    return 0;
}

/* ============================================================
 * 1. _is_byte_size_limit_message
 * ============================================================ */
/* PoP: ec2_is_byte_size_limit_message @ agent/error_classifier.py:_is_byte_size_limit_message */
static json_t *ec2_is_byte_size_limit_message_main(ec2_io_t *io, const char *error_msg)
{
    char *msg = ec2_lower(error_msg ? error_msg : "");
    if (!msg) return ec2_mkraise("RuntimeError", "oom");

    json_t *a = json_object();
    json_set(a, "msg", json_string(msg));
    json_t *r = ec2_call(io, "byte_size_overflow", a);
    if (ec2_is_raise(r)) { free(msg); return r; }
    if (!ec2_t(r)) { json_free(r); free(msg); return json_bool(0); }
    json_free(r);

    a = json_object(); json_set(a, "msg", json_string(msg));
    r = ec2_call(io, "token_count", a);
    if (ec2_is_raise(r)) { free(msg); return r; }
    if (ec2_t(r)) { json_free(r); free(msg); return json_bool(0); }
    json_free(r);

    a = json_object(); json_set(a, "msg", json_string(msg));
    r = ec2_call(io, "byte_size_subject", a);
    if (ec2_is_raise(r)) { free(msg); return r; }
    if (ec2_t(r)) { json_free(r); free(msg); return json_bool(1); }
    json_free(r);

    int res = 0;
    if (ec2_has(msg, "content length") || ec2_has(msg, "content-length")) {
        a = json_object(); json_set(a, "msg", json_string(msg));
        r = ec2_call(io, "request_words", a);
        if (ec2_is_raise(r)) { free(msg); return r; }
        res = (r && r->type == JSON_ARRAY && r->c.count > 0) ? 1 : 0;
        json_free(r);
    }
    free(msg);
    return json_bool(res);
}

static json_t *ec2_is_byte_size_limit_message_entry(ec2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *n = ec2_g(in, "error_msg");
    if (!n) n = ec2_g(in, "msg");
    return ec2_is_byte_size_limit_message_main(io, ec2_s(n));
}

/* fetch reasoning field token match (seam), NULL/None -> no match */
static json_t *ec2_reasoning_token(ec2_io_t *io, const char *msg)
{
    json_t *a = json_object();
    json_set(a, "msg", json_string(msg));
    return ec2_call(io, "reasoning_field_token", a);
}

/* ============================================================
 * 2. is_reasoning_level_rejection
 * ============================================================ */
/* PoP: ec2_is_reasoning_level_rejection @ agent/error_classifier.py:is_reasoning_level_rejection */
static json_t *ec2_is_reasoning_level_rejection_main(ec2_io_t *io, const char *error_msg)
{
    char *msg = ec2_lower(error_msg ? error_msg : "");
    if (!msg) return ec2_mkraise("RuntimeError", "oom");
    json_t *m = ec2_reasoning_token(io, msg);
    if (ec2_is_raise(m)) { free(msg); return m; }
    if (!m || m->type == JSON_NULL) { json_free(m); free(msg); return json_bool(0); }
    int s = ec2_mfield(m, "start", 0), e = ec2_mfield(m, "end", s + 1);
    json_free(m);
    char *wnd = ec2_near(msg, s, e, 48, 96);
    int res = ec2_any_in(wnd, EC2_REASONING_LEVEL_MARKERS);
    free(wnd); free(msg);
    return json_bool(res);
}

static json_t *ec2_is_reasoning_level_rejection_entry(ec2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ec2_is_reasoning_level_rejection_main(io, ec2_s(ec2_g(in, "msg")));
}

/* ============================================================
 * 3. is_reasoning_required_rejection
 * ============================================================ */
/* PoP: ec2_is_reasoning_required_rejection @ agent/error_classifier.py:is_reasoning_required_rejection */
static json_t *ec2_is_reasoning_required_rejection_main(ec2_io_t *io, const char *error_msg)
{
    char *msg = ec2_lower(error_msg ? error_msg : "");
    if (!msg) return ec2_mkraise("RuntimeError", "oom");
    json_t *m = ec2_reasoning_token(io, msg);
    if (ec2_is_raise(m)) { free(msg); return m; }
    if (!m || m->type == JSON_NULL) { json_free(m); free(msg); return json_bool(0); }
    int s = ec2_mfield(m, "start", 0), e = ec2_mfield(m, "end", s + 1);
    json_free(m);
    char *wnd = ec2_near(msg, s, e, 48, 96);
    int res = ec2_any_in(wnd, EC2_REASONING_REQUIRED_MARKERS);
    free(wnd);
    if (!res) {
        /* live: `or is_reasoning_level_rejection(msg)` -- re-fires the token seam */
        json_t *v = ec2_is_reasoning_level_rejection_main(io, msg);
        if (ec2_is_raise(v)) { free(msg); return v; }
        res = ec2_t(v);
        json_free(v);
    }
    free(msg);
    return json_bool(res);
}

static json_t *ec2_is_reasoning_required_rejection_entry(ec2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ec2_is_reasoning_required_rejection_main(io, ec2_s(ec2_g(in, "msg")));
}

/* ============================================================
 * 4. is_reasoning_field_rejection
 * ============================================================ */
/* PoP: ec2_is_reasoning_field_rejection @ agent/error_classifier.py:is_reasoning_field_rejection */
static json_t *ec2_is_reasoning_field_rejection_main(ec2_io_t *io, const char *error_msg)
{
    char *msg = ec2_lower(error_msg ? error_msg : "");
    if (!msg) return ec2_mkraise("RuntimeError", "oom");

    json_t *a = json_object(); json_set(a, "msg", json_string(msg));
    json_t *r = ec2_call(io, "reasoning_param_rejection", a);
    if (ec2_is_raise(r)) { free(msg); return r; }
    if (ec2_t(r)) { json_free(r); free(msg); return json_bool(1); }
    json_free(r);

    json_t *m = ec2_reasoning_token(io, msg);
    if (ec2_is_raise(m)) { free(msg); return m; }
    if (!m || m->type == JSON_NULL) { json_free(m); free(msg); return json_bool(0); }
    int s = ec2_mfield(m, "start", 0), e = ec2_mfield(m, "end", s + 1);
    json_free(m);
    char *wnd = ec2_near(msg, s, e, 32, 32);
    int res = ec2_has(wnd, "unsupported") || ec2_any_in(msg, EC2_UNSUPPORTED_PARAM_MARKERS);
    free(wnd); free(msg);
    return json_bool(res);
}

static json_t *ec2_is_reasoning_field_rejection_entry(ec2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ec2_is_reasoning_field_rejection_main(io, ec2_s(ec2_g(in, "msg")));
}

/* ============================================================
 * 5. _account_gates_model
 * ============================================================ */
/* PoP: ec2_account_gates_model @ agent/error_classifier.py:_account_gates_model */
static json_t *ec2_account_gates_model_main(ec2_io_t *io, const char *msg)
{
    json_t *a = json_object(); json_set(a, "msg", json_string(msg));
    json_t *tails = ec2_call(io, "account_gating_tail", a);
    if (ec2_is_raise(tails)) return tails;
    if (!tails || tails->type != JSON_ARRAY) { json_free(tails); return json_bool(0); }
    size_t n = tails->c.count, i;
    if (n > 4) n = 4;   /* itertools.islice(..., 4) */
    for (i = 0; i < n; i++) {
        json_t *t = tails->c.items[i];
        int start = ec2_mfield(t, "start", 0);
        json_t *b = json_object();
        json_set(b, "msg", json_string(msg));
        json_set(b, "start", json_number((double)start));
        json_t *gated = ec2_call(io, "account_gated_model", b);
        if (ec2_is_raise(gated)) { json_free(tails); return gated; }
        int yes = ec2_t(gated);
        json_free(gated);
        if (yes) {
            json_t *c = json_object(); json_set(c, "msg", json_string(msg));
            json_t *wall = ec2_call(io, "wall_words", c);
            if (ec2_is_raise(wall)) { json_free(tails); return wall; }
            int w = ec2_t(wall);
            json_free(wall); json_free(tails);
            return json_bool(w ? 0 : 1);   /* `not any(p in msg ...)` */
        }
    }
    json_free(tails);
    return json_bool(0);
}

static json_t *ec2_account_gates_model_entry(ec2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ec2_account_gates_model_main(io, ec2_s(ec2_g(in, "msg")));
}

/* ============================================================
 * 6. _by_message
 * ============================================================ */
/* PoP: ec2_by_message @ agent/error_classifier.py:_by_message */
static json_t *ec2_by_message_entry(ec2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const char *msg = ec2_s(ec2_g(in, "msg"));
    const json_t *body = ec2_g(in, "body");
    const json_t *headers = ec2_g(in, "response_headers");

    /* head rules */
    json_t *a = json_object();
    json_set(a, "msg", json_string(msg));
    json_set(a, "rules", json_string("head"));
    json_t *r = ec2_call(io, "first_match", a);
    if (ec2_is_raise(r)) return r;
    if (r && r->type == JSON_OBJECT) return r;   /* non-None verdict */

    /* byte-size leaf (ported sibling, real composition) */
    r = ec2_is_byte_size_limit_message_main(io, msg);
    if (ec2_is_raise(r)) return r;
    int hit = ec2_t(r); json_free(r);
    if (hit) return ec2_const_copy(io, "V_PAYLOAD_TOO_LARGE");

    /* named quota window -> disambiguate */
    a = json_object(); json_set(a, "msg", json_string(msg));
    r = ec2_call(io, "long_quota_window", a);
    if (ec2_is_raise(r)) return r;
    hit = ec2_t(r); json_free(r);
    if (hit) {
        a = json_object();
        json_set(a, "error_msg", json_string(msg));
        json_set(a, "body", body ? json_copy(body) : json_null());
        json_set(a, "response_headers", headers ? json_copy(headers) : json_null());
        json_t *t = ec2_call(io, "has_usage_limit_transient_signal", a);
        if (ec2_is_raise(t)) return t;
        int tr = ec2_t(t); json_free(t);
        return ec2_const_copy(io, tr ? "V_RATE_LIMIT" : "V_BILLING");
    }

    /* account gating (ported sibling, real composition) */
    r = ec2_account_gates_model_main(io, msg);
    if (ec2_is_raise(r)) return r;
    hit = ec2_t(r); json_free(r);
    if (hit) return ec2_const_copy(io, "V_MODEL_ENTITLEMENT");

    /* usage limit -> classify_402, else tail rules */
    a = json_object(); json_set(a, "msg", json_string(msg));
    r = ec2_call(io, "usage_limit", a);
    if (ec2_is_raise(r)) return r;
    hit = ec2_t(r); json_free(r);
    if (hit) {
        a = json_object();
        json_set(a, "error_msg", json_string(msg));
        json_set(a, "code", json_string(""));
        /* live: _classify_402(c.msg, dict) -- body defaults to None */
        json_set(a, "body", json_null());
        return ec2_call(io, "classify_402", a);
    }
    a = json_object();
    json_set(a, "msg", json_string(msg));
    json_set(a, "rules", json_string("tail"));
    return ec2_call(io, "first_match", a);
}

/* ============================================================
 * 7. _has_large_inline_image  (ported sibling, real)
 * ============================================================ */
/* PoP: ec2_has_large_inline_image @ agent/error_classifier.py:_has_large_inline_image */
static json_t *ec2_has_large_inline_image_main(ec2_io_t *io, const json_t *content)
{
    const json_t *cap = ec2_const(io, "image_shrink_target_bytes");
    double target = (cap && cap->type == JSON_NUMBER) ? cap->num_val : 4194304.0;
    if (!content || content->type != JSON_ARRAY) return json_bool(0);
    for (size_t i = 0; i < content->c.count; i++) {
        const json_t *part = content->c.items[i];
        const json_t *image = (part && part->type == JSON_OBJECT)
                              ? ec2_g(part, "image_url") : NULL;
        const json_t *url = (image && image->type == JSON_OBJECT)
                            ? ec2_g(image, "url") : image;
        if (url && url->type == JSON_STRING && url->str_val &&
            strncmp(url->str_val, "data:image/", 11) == 0 &&
            (double)strlen(url->str_val) > target)
            return json_bool(1);
    }
    return json_bool(0);
}

static json_t *ec2_has_large_inline_image_entry(ec2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ec2_has_large_inline_image_main(io, ec2_g(in, "content"));
}

/* ============================================================
 * 8. _oversized_message_content_rejection  (ported sibling, real)
 * ============================================================ */
/* PoP: ec2_oversized_message_content_rejection @ agent/error_classifier.py:_oversized_message_content_rejection */
static json_t *ec2_oversized_message_content_rejection_main(ec2_io_t *io, const json_t *body)
{
    const json_t *details = (body && body->type == JSON_OBJECT)
                            ? ec2_g(body, "detail") : NULL;
    if (!details || details->type != JSON_ARRAY) return json_bool(0);
    for (size_t i = 0; i < details->c.count; i++) {
        const json_t *d = details->c.items[i];
        if (!d || d->type != JSON_OBJECT) continue;
        const json_t *ty = ec2_g(d, "type");
        if (!ty || ty->type != JSON_STRING || strcmp(ty->str_val, "string_type") != 0)
            continue;
        const json_t *loc = ec2_g(d, "loc");
        if (!loc || loc->type != JSON_ARRAY || loc->c.count < 2) continue;
        /* parts = [str(x).lower() for x in loc] */
        char **parts = (char **)calloc(loc->c.count, sizeof(char *));
        if (!parts) return ec2_mkraise("RuntimeError", "oom");
        for (size_t j = 0; j < loc->c.count; j++) {
            parts[j] = ec2_pystr(loc->c.items[j]);
            for (char *p = parts[j]; p && *p; p++)
                if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
        }
        size_t L = loc->c.count;
        int ok = strcmp(parts[0], "body") == 0 && strcmp(parts[1], "messages") == 0 &&
                 strcmp(parts[L - 2], "content") == 0 && strcmp(parts[L - 1], "str") == 0;
        if (ok) {
            for (size_t j = 0; j < L; j++)
                if (strncmp(parts[j], "tool", 4) == 0) { ok = 0; break; }
        }
        for (size_t j = 0; j < L; j++) free(parts[j]);
        free(parts);
        if (!ok) continue;
        json_t *r = ec2_has_large_inline_image_main(io, ec2_g(d, "input"));
        if (ec2_is_raise(r)) return r;
        int hit = ec2_t(r); json_free(r);
        if (hit) return json_bool(1);
    }
    return json_bool(0);
}

static json_t *ec2_oversized_message_content_rejection_entry(ec2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ec2_oversized_message_content_rejection_main(io, ec2_g(in, "body"));
}

/* ============================================================
 * 9. _classify_image_tool_422
 * ============================================================ */
/* PoP: ec2_classify_image_tool_422 @ agent/error_classifier.py:_classify_image_tool_422 */
static json_t *ec2_classify_image_tool_422_entry(ec2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const char *msg = ec2_s(ec2_g(in, "msg"));
    const json_t *body = ec2_g(in, "body");

    json_t *r = ec2_oversized_message_content_rejection_main(io, body);
    if (ec2_is_raise(r)) return r;
    int hit = ec2_t(r); json_free(r);
    if (hit) return ec2_const_copy(io, "V_IMAGE_TOO_LARGE");

    json_t *a = json_object();
    json_set(a, "msg", json_string(msg));
    json_set(a, "rules", json_string("image_tool"));
    r = ec2_call(io, "first_match", a);
    if (ec2_is_raise(r)) return r;
    if (r && r->type == JSON_OBJECT) return r;
    json_free(r);
    return ec2_const_copy(io, "V_FORMAT_ERROR");
}
