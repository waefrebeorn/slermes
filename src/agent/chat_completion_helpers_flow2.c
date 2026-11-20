/* chat_completion_helpers_flow2.c -- FLOW2 port of the UNCLAIMED remainder of
 * agent/chat_completion_helpers.py (the _StreamingCall shared small helpers).
 *
 * PoP ledger (all names verified UNCLAIMED by CRLF-aware PoP grep +
 * pop_claims_full.json before authoring; flow1 / cch2-wave claimed the rest):
 *   PoP: cch2_live_route                  @ agent/chat_completion_helpers.py:_live_route
 *   PoP: cch2_visible_text_delivered      @ agent/chat_completion_helpers.py:_visible_text_delivered
 *   PoP: cch2_route_switched_under_request @ agent/chat_completion_helpers.py:_route_switched_under_request
 *   PoP: cch2_set_managed_stream          @ agent/chat_completion_helpers.py:_set_managed_stream
 *   PoP: cch2_start_stream_attempt        @ agent/chat_completion_helpers.py:_start_stream_attempt
 *   PoP: cch2_cancel_current_stream_attempt @ agent/chat_completion_helpers.py:_cancel_current_stream_attempt
 *   PoP: cch2_stream_attempt_is_active    @ agent/chat_completion_helpers.py:_stream_attempt_is_active
 *   PoP: cch2_stream_attempt_was_cancelled @ agent/chat_completion_helpers.py:_stream_attempt_was_cancelled
 *   PoP: cch2_mark_finish_seen            @ agent/chat_completion_helpers.py:_mark_finish_seen
 *   PoP: cch2_quiet                       @ agent/chat_completion_helpers.py:_quiet
 *
 * Self-contained: depends only on libjson (json.o) + libc. Static helpers only.
 * Domain notes (shadow-diffed against the live CPython drive in the oracle):
 *  - JSON object absence == Python missing attribute; json_null == Python None.
 *  - str(x or "") / int(x) projection follows the cch2 ASCII conventions.
 *  - Full builtin exception hierarchy for contextlib.suppress(Exception): only
 *    BaseException / SystemExit / KeyboardInterrupt / GeneratorExit /
 *    asyncio.CancelledError escape; every other builtin is an Exception subclass.
 * =========================================================================== */

#include "hermes_cch_flow.h"  /* pulls in lib/libjson/json.h (json_t + API) */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

#ifndef CCH2_IO_T_DEFINED
#define CCH2_IO_T_DEFINED
typedef struct cch2_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} cch2_io_t;
#endif

/* ---- truthiness / accessors ---- */
static int cch2_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}
static const char *cch2_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static json_t *cch2_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get((json_t *)o, k) : NULL;
}
static double cch2_num(const json_t *v)
{
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
/* array element accessor (NULL when absent / not an array) */
static const json_t *cch2_aget(const json_t *a, size_t i)
{
    return (a && a->type == JSON_ARRAY && i < a->c.count) ? a->c.items[i] : NULL;
}

/* ---- raise specs / full builtin exception hierarchy ---- */
static int cch2_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get((json_t *)r, "__raise__") != NULL;
}
static const char *cch2_kind(const json_t *r)
{
    const char *k = cch2_str(cch2_get(r, "__raise__"));
    return k ? k : "";
}
/* BaseException-family names that are NOT subclasses of Exception: these are
 * the ONLY ones contextlib.suppress(Exception) does not catch. Every other
 * builtin exception name is an Exception subclass (and unknown names are
 * modelled by the oracle as subclasses of Exception). */
static int cch2_is_base(const char *kind)
{
    return strcmp(kind, "BaseException") == 0 || strcmp(kind, "SystemExit") == 0
        || strcmp(kind, "KeyboardInterrupt") == 0 || strcmp(kind, "GeneratorExit") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "asyncio.CancelledError") == 0;
}
static json_t *cch2_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}
static char *cch2_xfmt(const char *fmt, ...)
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

/* ---- universal seam call (args owned here) ---- */
static json_t *cch2_call(cch2_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = cch2_xfmt("no seam installed for %s", name);
        r = cch2_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

/* ---- string helpers (ASCII domain) ---- */
static int cch2_is_ws(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static char *cch2_strip(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && cch2_is_ws((unsigned char)s[a])) a++;
    while (b > a && cch2_is_ws((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}
/* Python str() projection for scalars; caller frees. */
static char *cch2_pystr(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return cch2_xfmt("None");
    if (v->type == JSON_BOOL) return cch2_xfmt(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d) return cch2_xfmt("%.0f", d);
        return cch2_xfmt("%.17g", d);
    }
    if (v->type == JSON_STRING) return cch2_xfmt("%s", v->str_val ? v->str_val : "");
    char *s = json_serialize((json_t *)v);
    return s ? s : cch2_xfmt("");
}

/* Python == for the scalar/sequence domain used here (bool is subclass of int). */
static int cch2_pyequal(const json_t *a, const json_t *b)
{
    if (a == b) return 1;
    if (!a || !b) return 0;
    int an = (a->type == JSON_NUMBER || a->type == JSON_BOOL);
    int bn = (b->type == JSON_NUMBER || b->type == JSON_BOOL);
    if (an && bn) {
        double av = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return av == bv;
    }
    if (a->type != b->type) return 0;
    switch (a->type) {
    case JSON_NULL: return 1;
    case JSON_STRING: return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    case JSON_ARRAY:
        if (a->c.count != b->c.count) return 0;
        for (size_t i = 0; i < a->c.count; i++)
            if (!cch2_pyequal(a->c.items[i], b->c.items[i])) return 0;
        return 1;
    default: return 0;
    }
}

/* logger seam: record fmt + args array verbatim. */
static json_t *cch2_log(cch2_io_t *io, const char *name, const char *fmt, json_t *args)
{
    json_t *a = json_object();
    json_set(a, "fmt", json_string(fmt));
    json_set(a, "args", args ? args : json_array());
    return cch2_call(io, name, a);
}

/* =========================================================================== */
/* PoP: cch2_live_route @ agent/chat_completion_helpers.py:_live_route */
/* tuple(str(getattr(agent, attr, "") or "") for the four route attrs) */
static json_t *cch2_route_tuple(const json_t *agent)
{
    static const char *attrs[4] = {"model", "provider", "base_url", "api_mode"};
    json_t *arr = json_array();
    for (int i = 0; i < 4; i++) {
        const json_t *v = cch2_get(agent, attrs[i]);
        if (cch2_truth(v)) {
            char *s = cch2_pystr(v);
            json_append(arr, json_string(s ? s : ""));
            free(s);
        } else {
            json_append(arr, json_string(""));
        }
    }
    return arr;
}
static json_t *cch2_live_route(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    return cch2_route_tuple(cch2_get(cch2_get(in, "self"), "agent"));
}

/* PoP: cch2_visible_text_delivered @ agent/chat_completion_helpers.py:_visible_text_delivered */
static json_t *cch2_visible_text_delivered(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *v = cch2_get(cch2_get(cch2_get(in, "self"), "agent"), "_current_streamed_assistant_text");
    char *s = cch2_truth(v) ? cch2_pystr(v) : cch2_xfmt("");
    char *t = cch2_strip(s ? s : "");
    int yes = t && t[0] != '\0';
    free(t); free(s);
    return json_bool(yes);
}

/* PoP: cch2_route_switched_under_request @ agent/chat_completion_helpers.py:_route_switched_under_request */
static json_t *cch2_route_switched_under_request(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *live = cch2_route_tuple(cch2_get(cch2_get(in, "self"), "agent"));
    const json_t *req = cch2_get(cch2_get(in, "self"), "_request_route");
    if (cch2_pyequal(live, req)) { json_free(live); return json_bool(0); }
    /* req[1] or req[2], live[0], live[1] or live[2] */
    const json_t *r0 = cch2_aget(req, 0), *r1 = cch2_aget(req, 1), *r2 = cch2_aget(req, 2);
    const json_t *l0 = cch2_aget(live, 0), *l1 = cch2_aget(live, 1), *l2 = cch2_aget(live, 2);
    const json_t *ra = cch2_truth(r1) ? r1 : r2;
    const json_t *la = cch2_truth(l1) ? l1 : l2;
    json_t *args = json_array();
    json_append(args, r0 ? json_copy(r0) : json_string(""));
    json_append(args, ra ? json_copy(ra) : json_string(""));
    json_append(args, l0 ? json_copy(l0) : json_string(""));
    json_append(args, la ? json_copy(la) : json_string(""));
    json_free(cch2_log(io, "logger.warning",
        "Stream retry skipped: model/provider switched mid-request (%s via %s -> %s via %s); "
        "handing back to the turn loop to rebuild the request for the current route.", args));
    json_free(live);
    return json_bool(1);
}

/* PoP: cch2_set_managed_stream @ agent/chat_completion_helpers.py:_set_managed_stream */
static json_t *cch2_set_managed_stream(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    const json_t *v = cch2_get(in, "stream");
    json_set(st, "stream", v ? json_copy((json_t *)v) : json_null());
    return v ? json_copy((json_t *)v) : json_null();
}

/* PoP: cch2_start_stream_attempt @ agent/chat_completion_helpers.py:_start_stream_attempt */
static json_t *cch2_start_stream_attempt(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    const json_t *state = cch2_get(cch2_get(in, "self"), "stream_attempt_state");
    long long cur = (long long)cch2_num(cch2_get(state, "current")) + 1;
    json_set(st, "current", json_number((double)cur));
    json_set(st, "provider_tool_in_flight", json_bool(0));
    json_set(st, "prompt_progress", json_null());
    json_set(st, "partial_tool_names", json_array());
    return json_number((double)cur);
}

/* PoP: cch2_cancel_current_stream_attempt @ agent/chat_completion_helpers.py:_cancel_current_stream_attempt */
static json_t *cch2_cancel_current_stream_attempt(cch2_io_t *io, const json_t *in, json_t *st)
{
    const json_t *state = cch2_get(cch2_get(in, "self"), "stream_attempt_state");
    long long cur = (long long)cch2_num(cch2_get(state, "current"));
    const json_t *cancelled = cch2_get(state, "cancelled");
    json_t *out = json_array();
    if (cancelled && cancelled->type == JSON_ARRAY)
        for (size_t i = 0; i < cancelled->c.count; i++)
            json_append(out, json_copy(cancelled->c.items[i]));
    if (cur != 0) {
        int found = 0;
        for (size_t i = 0; i < out->c.count && !found; i++)
            if (cch2_num(out->c.items[i]) == (double)cur && out->c.items[i]->type == JSON_NUMBER)
                found = 1;
        if (!found) json_append(out, json_number((double)cur));
    }
    /* Python's set state is projected with sorted(); keep the C state window
     * in the same ascending order so the differential comparison is stable. */
    for (size_t i = 0; i + 1 < out->c.count; i++) {
        size_t best = i;
        for (size_t j = i + 1; j < out->c.count; j++)
            if (cch2_num(out->c.items[j]) < cch2_num(out->c.items[best])) best = j;
        if (best != i) {
            json_t *tmp = out->c.items[i];
            out->c.items[i] = out->c.items[best];
            out->c.items[best] = tmp;
        }
    }
    json_set(st, "cancelled", out);
    if (cur != 0) {
        json_t *args = json_array();
        json_append(args, json_number((double)cur));
        const json_t *reason = cch2_get(in, "reason");
        json_append(args, reason ? json_copy((json_t *)reason) : json_null());
        json_free(cch2_log(io, "logger.debug", "Marked stream attempt %s cancelled: %s", args));
    }
    return NULL; /* None */
}

/* PoP: cch2_stream_attempt_is_active @ agent/chat_completion_helpers.py:_stream_attempt_is_active */
static json_t *cch2_stream_attempt_is_active(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *state = cch2_get(cch2_get(in, "self"), "stream_attempt_state");
    long long cur = (long long)cch2_num(cch2_get(state, "current"));
    long long sid = (long long)cch2_num(cch2_get(in, "stream_attempt_id"));
    const json_t *cancelled = cch2_get(state, "cancelled");
    int in_cancel = 0;
    if (cancelled && cancelled->type == JSON_ARRAY)
        for (size_t i = 0; i < cancelled->c.count; i++)
            if (cch2_pyequal(cancelled->c.items[i], cch2_get(in, "stream_attempt_id"))) { in_cancel = 1; break; }
    return json_bool(sid == cur && !in_cancel);
}

/* PoP: cch2_stream_attempt_was_cancelled @ agent/chat_completion_helpers.py:_stream_attempt_was_cancelled */
static json_t *cch2_stream_attempt_was_cancelled(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *state = cch2_get(cch2_get(in, "self"), "stream_attempt_state");
    const json_t *cancelled = cch2_get(state, "cancelled");
    int in_cancel = 0;
    if (cancelled && cancelled->type == JSON_ARRAY)
        for (size_t i = 0; i < cancelled->c.count; i++)
            if (cch2_pyequal(cancelled->c.items[i], cch2_get(in, "stream_attempt_id"))) { in_cancel = 1; break; }
    return json_bool(in_cancel);
}

/* PoP: cch2_mark_finish_seen @ agent/chat_completion_helpers.py:_mark_finish_seen */
static json_t *cch2_mark_finish_seen(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    const json_t *src = cch2_get(in, "diag");
    json_t *diag = src && src->type == JSON_OBJECT ? json_copy((json_t *)src)
                                                   : (src ? json_copy((json_t *)src) : json_null());
    const json_t *fr = cch2_get(in, "finish_reason");
    if (cch2_truth(fr) && diag && diag->type == JSON_OBJECT
        && !cch2_truth(cch2_get(diag, "finish_reason_seen")))
        json_set(diag, "finish_reason_seen", json_bool(1));
    json_set(st, "diag", diag ? diag : json_null());
    return NULL; /* None */
}

/* PoP: cch2_quiet @ agent/chat_completion_helpers.py:_quiet */
static json_t *cch2_quiet(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *fargs = cch2_get(in, "fn_args");
    json_t *args = json_array();
    if (fargs && fargs->type == JSON_ARRAY)
        for (size_t i = 0; i < fargs->c.count; i++)
            json_append(args, json_copy(fargs->c.items[i]));
    json_t *r = cch2_call(io, "fn", args);
    if (cch2_is_raise(r)) {
        if (cch2_is_base(cch2_kind(r))) return r; /* BaseException family propagates */
        json_free(r);                              /* Exception family suppressed */
    } else if (r) {
        json_free(r);
    }
    return NULL;
}
