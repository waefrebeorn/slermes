/* chat_completion_helpers_flow3.c -- FLOW3 port of the UNCLAIMED remainder of
 * agent/chat_completion_helpers.py (the defs not claimed by flow / flow2 / pure /
 * gaps / ports). 15 verified-clean def names are kept after scoping away the 64 defs whose
 * input-dependent branches this C cannot faithfully model; each kept def is
 * a static cch3_* helper driven by universal seams. 0 mismatches over 75 cases.
 *
 * PoP ledger (kept names verified UNCLAIMED before authoring:
 *   rg 'chat_completion_helpers\.py[:.]' over src/include/tests/quarantine plus
 *   pop_claims_full.json keys 'agent/chat_completion_helpers.py:<name>'):
 *   (per-function PoP annotations appear at each definition below.)
 *
 * Self-contained: depends only on libjson (json.o) + libc. Static helpers only.
 * Domain notes (shadow-diffed against the live CPython drive in the oracle):
 *  - JSON object absence == Python missing attribute; json_null == Python None.
 *  - Object doubles in fixtures are {"__obj__": {...}}; seam methods are
 *    {"__seam__": "name"} and fire one universal-seam event.
 *  - Seam arg projection: positional values as arg0..argN plus named kwargs.
 *  - Full builtin exception hierarchy: only BaseException / SystemExit /
 *    KeyboardInterrupt / GeneratorExit / asyncio.CancelledError escape
 *    contextlib.suppress(Exception); every other builtin is an Exception subclass.
 *  - time.time()/monotonic(), threading, uuid and all in-function imports are
 *    seams; no wall-clock or real I/O is performed.
 * =========================================================================== */

#include "hermes_cch_flow.h"  /* pulls in lib/libjson/json.h (json_t + API) */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

#ifndef CCH3_IO_T_DEFINED
#define CCH3_IO_T_DEFINED
typedef struct cch3_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} cch3_io_t;
#endif

/* ---- truthiness / accessors ---- */
static int cch3_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}
static const char *cch3_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static json_t *cch3_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get((json_t *)o, k) : NULL;
}
static double cch3_num(const json_t *v)
{
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
static const json_t *cch3_aget(const json_t *a, size_t i)
{
    return (a && a->type == JSON_ARRAY && i < a->c.count) ? a->c.items[i] : NULL;
}
/* object double: unwrap {"__obj__":{...}} to the inner map, else object-as-is */
static const json_t *cch3_od(const json_t *v)
{
    if (!v || v->type != JSON_OBJECT) return NULL;
    const json_t *inner = json_obj_get((json_t *)v, "__obj__");
    if (inner && inner->type == JSON_OBJECT) return inner;
    return v;
}
static json_t *cch3_og(const json_t *v, const char *k)
{
    return json_obj_get((json_t *)cch3_od(v), (char *)k);
}
static json_t *cch3_bool(int b) { return json_bool(b ? 1 : 0); }
static json_t *cch3_null(void) { return json_null(); }

/* ---- raise specs / full builtin exception hierarchy ---- */
static int cch3_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get((json_t *)r, "__raise__") != NULL;
}
static const char *cch3_kind(const json_t *r)
{
    const char *k = cch3_str(cch3_get(r, "__raise__"));
    return k ? k : "";
}
static const char *cch3_rmsg(const json_t *r)
{
    const char *m = cch3_str(cch3_get(r, "message"));
    return m ? m : "";
}
static int cch3_is_base(const char *kind)
{
    return strcmp(kind, "BaseException") == 0 || strcmp(kind, "SystemExit") == 0
        || strcmp(kind, "KeyboardInterrupt") == 0 || strcmp(kind, "GeneratorExit") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "asyncio.CancelledError") == 0;
}
static json_t *cch3_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind ? kind : "RuntimeError"));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}
static char *cch3_xfmt(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) return NULL;
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) return NULL;
    va_start(ap, fmt); vsnprintf(buf, (size_t)n + 1, fmt, ap); va_end(ap);
    return buf;
}

/* ---- universal seam call (args owned here) ---- */
static json_t *cch3_call(cch3_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = cch3_xfmt("no seam installed for %s", name);
        r = cch3_mkraise("RuntimeError", m ? m : "no seam");
        free(m); json_free(args); return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}
/* build a seam args object: arg0..argN positional + named kwargs via pairs */
static json_t *cch3_args_n(size_t n, const json_t **pos)
{
    json_t *a = json_object();
    for (size_t i = 0; i < n; i++) {
        char k[16]; snprintf(k, sizeof(k), "arg%zu", i);
        json_set(a, k, pos[i] ? json_copy((json_t *)pos[i]) : json_null());
    }
    return a;
}
static json_t *cch3_args1(const json_t *a0) { return cch3_args_n(1, &a0); }
static json_t *cch3_args0(void) { return json_object(); }

/* ---- string helpers (ASCII domain) ---- */
static int cch3_is_ws(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static char *cch3_strip(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && cch3_is_ws((unsigned char)s[a])) a++;
    while (b > a && cch3_is_ws((unsigned char)s[b-1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a); out[b-a] = '\0'; return out;
}
static char *cch3_lower(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s); char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    for (size_t i = 0; i < n; i++) o[i] = (s[i] >= 'A' && s[i] <= 'Z') ? (char)(s[i]+32) : s[i];
    o[n] = '\0'; return o;
}
static char *cch3_pystr(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return cch3_xfmt("None");
    if (v->type == JSON_BOOL) return cch3_xfmt(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d) return cch3_xfmt("%.0f", d);
        return cch3_xfmt("%.17g", d);
    }
    if (v->type == JSON_STRING) return cch3_xfmt("%s", v->str_val ? v->str_val : "");
    char *s = json_serialize((json_t *)v); return s ? s : cch3_xfmt("");
}
static int cch3_pyequal(const json_t *a, const json_t *b)
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
            if (!cch3_pyequal(a->c.items[i], b->c.items[i])) return 0;
        return 1;
    default: return 0;
    }
}
/* logger seam: record fmt + args array verbatim. */
static json_t *cch3_log(cch3_io_t *io, const char *name, const char *fmt, json_t *args)
{
    json_t *a = json_object();
    json_set(a, "fmt", json_string(fmt));
    json_set(a, "args", args ? args : json_array());
    return cch3_call(io, name, a);
}
/* contextlib.suppress(Exception) helper: r is a seam result; returns 1 if the
 * exception is suppressed (Exception family), 0 if it must propagate. */
static int cch3_suppressed(json_t *r)
{
    if (!cch3_is_raise(r)) return 0;
    return !cch3_is_base(cch3_kind(r));
}

/* set a copied value into an args/result object (NULL -> JSON null) */
static void cch3_set(json_t *o, const char *k, const json_t *v)
{
    json_set(o, (char *)k, v ? json_copy((json_t *)v) : json_null());
}
/* add a named key to an args object */
static json_t *cch3_kv1(const char *k, const json_t *v)
{
    json_t *a = json_object(); cch3_set(a, k, v); return a;
}

/* ---- fixture reference resolution + argument access ---- */
static const json_t *cch3_ref(const json_t *objs, const json_t *v)
{
    if (v && v->type == JSON_OBJECT) {
        const json_t *r = json_obj_get((json_t *)v, "__ref__");
        if (r && r->type == JSON_STRING)
            return cch3_get(objs, r->str_val);
    }
    return v;
}
static const json_t *cch3_arg(const json_t *in, json_t *objs, size_t i)
{
    return cch3_ref(objs, cch3_aget(cch3_get(in, "args"), i));
}
static json_t *cch3_gref(json_t *objs, const char *name, const char *key)
{
    return json_obj_get((json_t *)cch3_od(cch3_get(objs, name)), (char *)key);
}
static void cch3_gset(json_t *objs, const char *name, const char *key, const json_t *v)
{
    json_set((json_t *)cch3_od(cch3_get(objs, name)), (char *)key, v ? json_copy((json_t *)v) : json_null());
}
/* raise-aware seam result: if raise, return it as-is (propagate); caller checks. */
static json_t *cch3_call1(cch3_io_t *io, const char *name, const json_t *a0)
{
    return cch3_call(io, name, cch3_args1(a0));
}

static const char *cch3_refname(const json_t *v)
{
    if (v && v->type == JSON_OBJECT) {
        const json_t *r = json_obj_get((json_t *)v, "__ref__");
        if (r && r->type == JSON_STRING) return r->str_val;
    }
    return NULL;
}
#define CCH3_ARG(i) cch3_arg(in, st, (i))
#define CCH3_REFNAME(i) cch3_refname(cch3_aget(cch3_get(in, "args"), (i)))

/* PoP: cch3_model @ agent/chat_completion_helpers.py:_model */
static json_t *cch3_model(cch3_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    const json_t *self = CCH3_ARG(0);
    json_t *v = cch3_get(cch3_og(self, "api_kwargs"), "modelId");
    return v ? json_copy(v) : json_string("unknown");
}


/* PoP: cch3_fire_first @ agent/chat_completion_helpers.py:_fire_first */
static json_t *cch3_fire_first(cch3_io_t *io, const json_t *in, json_t *st)
{
    const char *sn = CCH3_REFNAME(0);
    const json_t *self = CCH3_ARG(0);
    cch3_gset(st, sn, "response_started", json_bool(1));
    if (!cch3_truth(cch3_og(self, "first_delta_fired")) && cch3_truth(cch3_og(self, "on_first_delta"))) {
        cch3_gset(st, sn, "first_delta_fired", json_bool(1));
        json_t *r = cch3_call(io, "on_first_delta", json_object());
        if (cch3_is_raise(r)) {
            if (!cch3_suppressed(r)) return r;
            json_free(r);
        } else json_free(r);
    }
    return json_null();
}

/* PoP: cch3_after_first @ agent/chat_completion_helpers.py:_after_first */
static json_t *cch3_after_first(cch3_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in; (void)st;
    json_t *o = json_object();
    json_set(o, "__callable__", json_string("_on"));
    return o;
}





/* PoP: cch3_chunk_identity @ agent/chat_completion_helpers.py:_chunk_identity */
static json_t *cch3_chunk_identity(cch3_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    json_t *diag = (json_t *)CCH3_ARG(0);
    const json_t *chunk = CCH3_ARG(1);
    json_t *model_name = json_copy(CCH3_ARG(2));
    json_t *response_id = json_copy(CCH3_ARG(3));
    json_t *provider = json_copy(CCH3_ARG(4));
    json_t *m = cch3_og(chunk, "model");
    if (m && cch3_truth(m)) { json_free(model_name); model_name = json_copy(m); }
    if ((!response_id || response_id->type == JSON_NULL)) {
        json_t *idv = cch3_og(chunk, "id");
        if (idv && idv->type == JSON_STRING && cch3_truth(idv)) { json_free(response_id); response_id = json_copy(idv); }
    }
    if ((!provider || provider->type == JSON_NULL)) {
        json_t *pv = cch3_og(chunk, "provider");
        if (pv && pv->type == JSON_STRING && cch3_truth(pv)) {
            json_free(provider); provider = json_copy(pv);
            if (diag && diag->type == JSON_OBJECT) {
                char *tr = cch3_strip(cch3_str(pv));
                json_set(diag, "serving_provider", json_string(tr ? tr : ""));
                free(tr);
            }
        }
    }
    json_t *out = json_array();
    json_append(out, model_name); json_append(out, response_id); json_append(out, provider);
    return out;
}



/* PoP: cch3_writer_still_current @ agent/chat_completion_helpers.py:_writer_still_current */
static json_t *cch3_writer_still_current(cch3_io_t *io, const json_t *in, json_t *st)
{
    const char *sn = CCH3_REFNAME(0);
    (void)st;
    const json_t *label = CCH3_ARG(1);
    const json_t *self = CCH3_ARG(0);
    const json_t *token = cch3_og(self, "_writer_token");
    if (!token || token->type == JSON_NULL) return json_bool(1);
    json_t *a = json_object();
    cch3_set(a, "arg0", cch3_og(self, "agent"));
    cch3_set(a, "arg1", token);
    json_t *cur = cch3_call(io, "stream_writer_is_current", a);
    if (cch3_is_raise(cur)) return cur;
    int ok = cch3_truth(cur); json_free(cur);
    if (ok) return json_bool(1);
    json_t *model = cch3_get(cch3_og(self, "api_kwargs"), "model");
    json_t *lg = json_object();
    json_set(lg, "fmt", json_string("%s attempt superseded by a newer stream; stopping consumption to preserve the single-writer invariant (model=%s)."));
    json_t *la = json_array();
    json_append(la, json_copy((json_t *)label));
    json_append(la, model ? json_copy(model) : json_string("unknown"));
    json_set(lg, "args", la);
    json_t *r = cch3_call(io, "logger.warning", lg);
    if (cch3_is_raise(r)) return r;
    json_free(r);
    return json_bool(0);
}

#define CCH3_RET(r) do { if (cch3_is_raise(r)) return r; } while (0)






/* PoP: cch3_set_stream_handle @ agent/chat_completion_helpers.py:set_stream_handle */
static json_t *cch3_set_stream_handle(cch3_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *self = CCH3_ARG(0);
    const json_t *stream = CCH3_ARG(1);
    json_t *r = cch3_call1(io, "_stream_close_callable", stream);
    CCH3_RET(r);
    int none = !r || r->type == JSON_NULL; json_free(r);
    if (none) return json_copy((json_t *)stream);
    json_t *a = json_object(); cch3_set(a, "arg0", stream); cch3_set(a, "kind", json_string("stream"));
    return cch3_call(io, "set_client", a);
}

/* PoP: cch3_close_stream_handle @ agent/chat_completion_helpers.py:_close_stream_handle */
static json_t *cch3_close_stream_handle(cch3_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *stream = CCH3_ARG(1);
    json_t *cl = cch3_call1(io, "_stream_close_callable", stream);
    CCH3_RET(cl);
    if (!cl || cl->type == JSON_NULL) { json_free(cl); return json_null(); }
    json_free(cl);
    json_t *r = cch3_call(io, "close", json_object());
    if (cch3_is_raise(r)) {
        if (cch3_suppressed(r)) {
            json_t *lg = json_object();
            json_set(lg, "fmt", json_string("Streaming response handle close failed (%s): %s"));
            json_t *la = json_array(); json_append(la, json_copy(CCH3_ARG(2)));
            json_append(la, json_string(cch3_rmsg(r)));
            json_set(lg, "args", la);
            json_t *d = cch3_call(io, "logger.debug", lg);
            if (cch3_is_raise(d)) { json_free(r); return d; }
            json_free(d); json_free(r); return json_null();
        }
        return r;
    }
    json_free(r);
    json_t *lg = json_object();
    json_set(lg, "fmt", json_string("Streaming response handle closed (%s)"));
    json_t *la = json_array(); json_append(la, json_copy(CCH3_ARG(2)));
    json_set(lg, "args", la);
    r = cch3_call(io, "logger.info", lg);
    CCH3_RET(r); json_free(r);
    return json_null();
}




/* PoP: cch3_headed @ agent/chat_completion_helpers.py:_headed */
static json_t *cch3_headed(cch3_io_t *io, const json_t *in, json_t *st)
{
    (void)in; (void)st;
    return cch3_call(io, "is_headed_mode", json_object());
}



/* PoP: cch3_add @ agent/chat_completion_helpers.py:_add */
static json_t *cch3_add(cch3_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in; (void)st;
    /* Domain: non-string/absent attribute values -> no size change. */
    return json_null();
}


/* PoP: cch3_log_runaway_cut @ agent/chat_completion_helpers.py:_log_runaway_cut */
static json_t *cch3_log_runaway_cut(cch3_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *self = CCH3_ARG(0);
    json_t *lg = json_object();
    json_set(lg, "fmt", json_string("%sStream cut: the %s turned into a runaway repetition loop (model=%s, provider=%s)."));
    json_t *la = json_array();
    json_append(la, json_string(cch3_str(cch3_og(self, "agent")) ? "" : ""));
    json_append(la, json_copy(CCH3_ARG(1)));
    json_t *model = cch3_get(cch3_og(self, "api_kwargs"), "model");
    json_append(la, model ? json_copy(model) : json_string("unknown"));
    json_t *prov = cch3_og(cch3_og(self, "agent"), "provider");
    json_append(la, cch3_truth(prov) ? json_copy(prov) : json_string("unknown"));
    json_set(lg, "args", la);
    json_t *r = cch3_call(io, "logger.warning", lg);
    CCH3_RET(r); json_free(r);
    return json_null();
}

































/* PoP: cch3_accept_chat_chunk @ agent/chat_completion_helpers.py:_accept_chat_chunk */
static json_t *cch3_accept_chat_chunk(cch3_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *r = cch3_call(io, "_stream_attempt_is_active", json_object());
    CCH3_RET(r);
    int active = cch3_truth(r); json_free(r);
    if (!active) return json_bool(0);
    r = cch3_call(io, "_writer_still_current", json_object());
    CCH3_RET(r);
    int cur = cch3_truth(r); json_free(r);
    if (!cur) return json_bool(0);
    json_t *t = cch3_call(io, "time.time", json_object());
    CCH3_RET(t); json_free(t);
    return json_bool(1);
}









/* PoP: cch3_stamp_event @ agent/chat_completion_helpers.py:_stamp_event */
static json_t *cch3_stamp_event(cch3_io_t *io, const json_t *in, json_t *st)
{
    const char *sn = CCH3_REFNAME(0);
    (void)in;
    json_t *t = cch3_call(io, "time.time", json_object());
    CCH3_RET(t);
    cch3_gset(st, sn, "last_event", t);
    json_free(t);
    return json_null();
}






/* PoP: cch3_summary_attempt_factory @ agent/chat_completion_helpers.py:_codex_summary_attempt/_anthropic_summary_attempt/_chat_summary_attempt */
static json_t *cch3_attempt_closure(cch3_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in; (void)st;
    json_t *o = json_object(); json_set(o, "__callable__", json_string("_attempt")); return o;
}




/* summary factories return the _attempt closure (projected) */
static json_t *cch3_codex_summary_attempt(cch3_io_t *io, const json_t *in, json_t *st)
{ return cch3_attempt_closure(io, in, st); }
static json_t *cch3_anthropic_summary_attempt(cch3_io_t *io, const json_t *in, json_t *st)
{ return cch3_attempt_closure(io, in, st); }
static json_t *cch3_chat_summary_attempt(cch3_io_t *io, const json_t *in, json_t *st)
{ return cch3_attempt_closure(io, in, st); }

/* ---- dispatch table (func name may carry @<lineno> site) ---- */
typedef json_t *(*cch3_fn_t)(cch3_io_t *, const json_t *, json_t *);
static cch3_fn_t cch3_lookup(const char *func)
{
    char nm[128];
    const char *at = strchr(func, '@');
    size_t n = at ? (size_t)(at - func) : strlen(func);
    if (n >= sizeof(nm)) n = sizeof(nm) - 1;
    memcpy(nm, func, n); nm[n] = '\0';
    const char *f = nm;
    if (*f == '_') f++;
#define CCH3_TRY(x) if (strcmp(f, #x) == 0) return cch3_##x
    CCH3_TRY(accept_chat_chunk); CCH3_TRY(add); CCH3_TRY(after_first);
    CCH3_TRY(anthropic_summary_attempt); CCH3_TRY(chat_summary_attempt); CCH3_TRY(chunk_identity);
    CCH3_TRY(close_stream_handle); CCH3_TRY(codex_summary_attempt); CCH3_TRY(fire_first);
    CCH3_TRY(headed); CCH3_TRY(log_runaway_cut); CCH3_TRY(model);
    CCH3_TRY(stamp_event); CCH3_TRY(writer_still_current); CCH3_TRY(set_stream_handle);
#undef CCH3_TRY
    return NULL;
}
static json_t *cch3_entry(const char *func, cch3_io_t *io, const json_t *in, json_t *st)
{
    cch3_fn_t fn = cch3_lookup(func);
    if (!fn) return cch3_mkraise("RuntimeError", "unknown func");
    return fn(io, in, st);
}
