/* codex_transport_flow.c — FLOW + leaves port of the UNCLAIMED remainder of
 * agent/transports/codex.py (the claimed siblings live in codex_gaps.c /
 * codex_app_server_session.c / transports_codex_ports.c and are SEAMED here).
 *
 * Decision-sequence port of the Codex Responses transport's format/normalization
 * layer: the cache-key field bounder + header merge, the backend predicates
 * (openai origin / perplexity / azure / tool-search reservation / native-web-search
 * preference), the reserved-tool wire aliasing orchestrator, the reasoning-effort
 * resolution chain (route vocabulary / profile declaration / clamp), the Astra
 * request sanitizer, the reasoning-field builder, the newest-reasoning-only replay
 * trim, the timeout coercion, the all-turns gate, and the two ResponsesApiTransport
 * methods reject_all_turns / drop_unverified_replay.
 *
 * ALL collaborator I/O (claimed siblings, foreign-module imports, logger) rides the
 * injected universal seam cxt_call; the harness scripts results and records the
 * ordered trace the oracle diffs against LIVE agent/transports/codex.py under the
 * same scripts. Ported functions call each other REAL (internal cxt_* composition).
 *
 * Representation contract (shared with tests/oracle_codex_transport_flow.py):
 *   - scripts: MISSING key -> fail-closed RuntimeError("no script for <name>");
 *     null -> None; {"$seq":[...]} per-call queue (exhaustion raises
 *     RuntimeError("script exhausted: <name>")); {"__raise__":cls,"msg":m,...}
 *     raises (trace ret is the WHOLE marker); any other value reused (a bare ARRAY
 *     is a LIST RESULT, not a queue). None-valued seam arg keys dropped on BOTH sides.
 *   - get_active_search_provider results are attribute dicts (oracle materializes a
 *     SimpleNamespace; C reads the JSON "name" field). _alias_reserved_tools results
 *     are 2-element arrays [tools, alias_map] (tuple unpack). _profile_declared_efforts
 *     / codex_supported_efforts results are effort arrays (tuple). clamp_effort returns
 *     str-or-null. strip_unverified_reasoning_items returns an int count.
 *   - tuple -> array; set/frozenset -> sorted array; None -> JSON null; integral
 *     floats collapse to ints on both sides; uncaught exception -> {"__raise__":cls}.
 *   - except Exception sites (_is_perplexity_responses_backend / _reserves_tool_search
 *     / _openai_prefers_native_web_search) swallow the Exception family and return
 *     False; the BaseException trio (SystemExit/KeyboardInterrupt/GeneratorExit)
 *     passes through. Full builtin exception-parent table below.
 *
 * Fixture domain (ASCII scope):
 *   - strings ASCII (str.strip/lower transcribed byte-wise; strip set is White_Space
 *     incl \x1c-\x1f). base_url / provider / model are str-or-null.
 *   - str()-computed positions carry strings or integers only (no non-integral float
 *     repr); _coerce_timeout returns a number (integral floats collapse via norm).
 *   - include items / declared effort levels / header values are strings or integers.
 *   - error (reject_all_turns) is str / null / exception-marker {__raise__,msg,status_code}.
 *   - _all_turns_rejected is an array of strings; _last_issuer_model a string on the
 *     mutate path; message_lists an array; _alias_reserved_tools results 2-element arrays.
 *
 * libjson-only (default oracle deps suffice). Self-contained: the harness
 * #includes "agent/transports/codex_transport_flow.c".
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

/* ── context + seam plumbing ─────────────────────────────────────────────── */

typedef struct cxt_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} cxt_ctx;

static bool cxt_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool cxt_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool cxt_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool cxt_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool cxt_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool cxt_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool cxt_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
/* `x is True` identity gate (JSON_BOOL true only). */
static bool cxt_is_true(const json_t *v) { return v && v->type == JSON_BOOL && v->bool_val; }
/* `x is False` identity gate (JSON_BOOL false only). */
static bool cxt_is_false(const json_t *v) { return v && v->type == JSON_BOOL && !v->bool_val; }

static bool cxt_truthy(const json_t *v) {
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
static const char *cxt_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
/* borrowed object field or NULL */
static json_t *cxt_get(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}

/* ── string helpers (caller frees) ───────────────────────────────────────── */

static char *cxt_dup_span(const char *s, size_t len) {
    char *r = (char *)malloc(len + 1);
    if (!r) return NULL;
    memcpy(r, s, len);
    r[len] = '\0';
    return r;
}
static bool cxt_is_ws(char c) {
    unsigned char u = (unsigned char)c;
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r'
        || (u >= 0x1c && u <= 0x1f);
}
static char *cxt_py_strip(const char *s) {
    if (!s) return cxt_dup_span("", 0);
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && cxt_is_ws(s[a])) a++;
    while (b > a && cxt_is_ws(s[b - 1])) b--;
    return cxt_dup_span(s + a, b - a);
}
static char *cxt_py_lower(const char *s) {
    char *r = cxt_dup_span(s ? s : "", strlen(s ? s : ""));
    if (!r) return NULL;
    for (char *p = r; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    return r;
}
/* str(v) for a scalar (null -> "None"). Caller frees. */
static char *cxt_pystr(const json_t *v) {
    if (cxt_is_none(v)) return cxt_dup_span("None", 4);
    if (cxt_is_bool(v)) return cxt_dup_span(v->bool_val ? "True" : "False", v->bool_val ? 4 : 5);
    if (cxt_is_num(v)) {
        char buf[64]; double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
        else snprintf(buf, sizeof buf, "%g", d);
        return cxt_dup_span(buf, strlen(buf));
    }
    if (cxt_is_str(v)) return cxt_dup_span(cxt_jstr(v), strlen(cxt_jstr(v)));
    return cxt_dup_span("", 0);
}
/* str(v or "") for a scalar (falsy -> ""). Caller frees. */
static char *cxt_str_or_empty(const json_t *v) {
    if (!cxt_truthy(v)) return cxt_dup_span("", 0);
    return cxt_pystr(v);
}
/* s.rsplit("/", 1)[-1] — substring after the last '/'. Caller frees. */
static char *cxt_bare_slug(const char *s) {
    const char *p = strrchr(s ? s : "", '/');
    const char *tail = p ? p + 1 : (s ? s : "");
    return cxt_dup_span(tail, strlen(tail));
}
/* str(model or "").strip().lower().rsplit("/",1)[-1]. Caller frees. */
static char *cxt_bare_model(const json_t *model) {
    char *s = cxt_str_or_empty(model);
    char *st = cxt_py_strip(s);
    char *lo = cxt_py_lower(st);
    char *bare = cxt_bare_slug(lo);
    free(s); free(st); free(lo);
    return bare;
}

/* ── exception plumbing (full builtin hierarchy) ─────────────────────────── */

static bool cxt_exc_pending(const cxt_ctx *ctx) { return ctx && ctx->exc != NULL; }
static void cxt_exc_clear(cxt_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}
static void cxt_raise_msg(cxt_ctx *ctx, const char *cls, const char *msg) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
    json_set(ctx->exc, "msg", json_string(msg ? msg : ""));
}
static const char *cxt_exc_class(const cxt_ctx *ctx) {
    return json_get_str(ctx->exc, "__raise__", "");
}
/* FULL builtin exception subclass chain (parent lookup). */
static const char *cxt_exc_parent(const char *c) {
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
        if (cxt_str_eq(T[i].c, c)) return T[i].p;
    return NULL;
}
static bool cxt_exc_matches(const cxt_ctx *ctx, const char *const *classes, size_t n) {
    if (!cxt_exc_pending(ctx)) return false;
    const char *cls = cxt_exc_class(ctx);
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (cxt_str_eq(cls, classes[i])) return true;
        const char *p = cxt_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}
/* except Exception (NOT the BaseException trio). */
static bool cxt_exc_is_exception(const cxt_ctx *ctx) {
    static const char *E[] = {"Exception"};
    return cxt_exc_matches(ctx, E, 1);
}

/* ── universal seam (fail-closed) ────────────────────────────────────────── */

/* args ownership is TRANSFERRED (NULL -> empty object). Result is NEW. */
static json_t *cxt_call(cxt_ctx *ctx, const char *name, json_t *args) {
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
static void cxt_arg_str(json_t *args, const char *key, const char *val) {
    if (!val) return;
    json_set(args, key, json_string(val));
}
static void cxt_arg_node(json_t *args, const char *key, const json_t *val) {
    if (!cxt_is_none(val)) json_set(args, key, json_copy(val));
}
/* logger.warning seam: {"msg","args"}. args_arr ownership transferred. */
static void cxt_log_warning(cxt_ctx *ctx, const char *msg, json_t *args_arr) {
    json_t *a = json_object();
    json_set(a, "msg", json_string(msg));
    json_set(a, "args", args_arr ? args_arr : json_array());
    json_t *r = cxt_call(ctx, "logger.warning", a);
    if (r) json_free(r);
}

/* tuple result helper: [a, b] (ownership transferred). */
static json_t *cxt_pair(json_t *a, json_t *b) {
    json_t *r = json_array();
    json_append(r, a ? a : json_null());
    json_append(r, b ? b : json_null());
    return r;
}
/* dict.update: copy every src key into dest. */
static void cxt_obj_update(json_t *dest, const json_t *src) {
    if (!cxt_is_obj(dest) || !cxt_is_obj(src)) return;
    for (size_t i = 0; i < src->c.count; i++)
        json_set(dest, src->c.keys[i], json_copy(src->c.items[i]));
}
/* getattr(obj, key, None) — non-object / absent reads NULL. Borrowed. */
static json_t *cxt_getattr_def(const json_t *obj, const char *key) {
    if (!cxt_is_obj(obj)) return NULL;
    return json_obj_get(obj, key);
}

/* ── live constant tables ────────────────────────────────────────────────── */

static json_t *cxt_str_array(const char *const *v, size_t n) {
    json_t *a = json_array();
    for (size_t i = 0; i < n; i++) json_append(a, json_string(v[i]));
    return a;
}
static json_t *cxt_xai_grok46_efforts(void) {
    static const char *V[] = {"low", "medium", "high", "xhigh"};
    return cxt_str_array(V, 4);
}
static json_t *cxt_xai_legacy_efforts(void) {
    static const char *V[] = {"low", "medium", "high"};
    return cxt_str_array(V, 3);
}
static json_t *cxt_codex_legacy_efforts(void) {
    static const char *V[] = {"none", "low", "medium", "high", "xhigh"};
    return cxt_str_array(V, 5);
}
static json_t *cxt_codex_astra_efforts(void) {
    static const char *V[] = {"low", "medium", "high", "xhigh", "max"};
    return cxt_str_array(V, 5);
}
static json_t *cxt_opencode_reserved(void) {
    static const char *V[] = {"web_search", "search_files"};
    return cxt_str_array(V, 2);
}
static json_t *cxt_perplexity_reserved(void) {
    static const char *V[] = {"web_search", "search_files", "fetch_url", "people_search", "finance_search"};
    return cxt_str_array(V, 5);
}
static json_t *cxt_xai_reserved(void) {
    static const char *V[] = {"tool_search"};
    return cxt_str_array(V, 1);
}
static bool cxt_in_openai_responses_hosts(const char *s) {
    return cxt_str_eq(s, "api.openai.com") || cxt_str_eq(s, "chatgpt.com");
}
static bool cxt_in_gpt62(const char *s) {
    return cxt_str_eq(s, "gpt-6.2-astra") || cxt_str_eq(s, "gpt-6.2-sol");
}

/* ── _coerce_timeout (pure leaf) ─────────────────────────────────────────── */

/* PoP: cxt_coerce_timeout @ agent/transports/codex.py:_coerce_timeout */
static json_t *cxt_coerce_timeout(const json_t *timeout) {
    /* isinstance(timeout,(int,float)) and not isinstance(timeout,bool) and 0<float<inf */
    if (cxt_is_num(timeout) && timeout->num_val > 0.0 && isfinite(timeout->num_val))
        return json_number(timeout->num_val);
    return json_null();
}

/* ── backend predicates ──────────────────────────────────────────────────── */

/* PoP: cxt_is_openai_api_origin @ agent/transports/codex.py:_is_openai_api_origin */
static json_t *cxt_is_openai_api_origin(cxt_ctx *ctx, const json_t *base_url) {
    char *s = cxt_str_or_empty(base_url);
    json_t *a = json_object();
    cxt_arg_str(a, "base_url", s);
    free(s);
    json_t *hn = cxt_call(ctx, "base_url_hostname", a);
    if (cxt_exc_pending(ctx)) { if (hn) json_free(hn); return NULL; }
    char *lo = cxt_py_lower(cxt_jstr(hn));
    if (hn) json_free(hn);
    bool eq = cxt_str_eq(lo, "api.openai.com");
    free(lo);
    return json_bool(eq);
}

/* PoP: cxt_is_perplexity_responses_backend @ agent/transports/codex.py:_is_perplexity_responses_backend */
static json_t *cxt_is_perplexity_responses_backend(cxt_ctx *ctx, json_t *params) {
    char *s = cxt_str_or_empty(cxt_get(params, "base_url"));
    json_t *a = json_object();
    cxt_arg_str(a, "base_url", s);
    free(s);
    json_t *hn = cxt_call(ctx, "base_url_hostname", a);
    if (cxt_exc_pending(ctx)) {
        if (hn) json_free(hn);
        if (cxt_exc_is_exception(ctx)) { cxt_exc_clear(ctx); return json_bool(false); }
        return NULL;   /* BaseException passthrough */
    }
    char *lo = cxt_py_lower(cxt_jstr(hn));
    if (hn) json_free(hn);
    bool eq = cxt_str_eq(lo, "api.perplexity.ai");
    free(lo);
    return json_bool(eq);
}

/* PoP: cxt_is_azure_responses @ agent/transports/codex.py:_is_azure_responses */
static json_t *cxt_is_azure_responses(cxt_ctx *ctx, json_t *params) {
    char *p = cxt_str_or_empty(cxt_get(params, "provider"));
    char *pst = cxt_py_strip(p);
    char *plo = cxt_py_lower(pst);
    bool foundry = cxt_str_eq(plo, "azure-foundry");
    free(p); free(pst); free(plo);
    if (foundry) return json_bool(true);
    char *base_url = cxt_str_or_empty(cxt_get(params, "base_url"));
    json_t *a1 = json_object();
    cxt_arg_str(a1, "base_url", base_url);
    cxt_arg_str(a1, "host", "openai.azure.com");
    json_t *m1 = cxt_call(ctx, "base_url_host_matches", a1);
    if (cxt_exc_pending(ctx)) { free(base_url); if (m1) json_free(m1); return NULL; }
    bool b1 = cxt_truthy(m1);
    if (m1) json_free(m1);
    if (b1) { free(base_url); return json_bool(true); }
    json_t *a2 = json_object();
    cxt_arg_str(a2, "base_url", base_url);
    cxt_arg_str(a2, "host", "services.ai.azure.com");
    free(base_url);
    json_t *m2 = cxt_call(ctx, "base_url_host_matches", a2);
    if (cxt_exc_pending(ctx)) { if (m2) json_free(m2); return NULL; }
    bool b2 = cxt_truthy(m2);
    if (m2) json_free(m2);
    return json_bool(b2);
}

/* PoP: cxt_reserves_tool_search @ agent/transports/codex.py:_reserves_tool_search */
static json_t *cxt_reserves_tool_search(cxt_ctx *ctx, json_t *params, const json_t *is_xai_responses) {
    if (cxt_truthy(is_xai_responses) || cxt_is_true(cxt_get(params, "is_codex_backend")))
        return json_bool(true);
    char *s = cxt_str_or_empty(cxt_get(params, "base_url"));
    json_t *a = json_object();
    cxt_arg_str(a, "base_url", s);
    free(s);
    json_t *hn = cxt_call(ctx, "base_url_hostname", a);
    if (cxt_exc_pending(ctx)) {
        if (hn) json_free(hn);
        if (cxt_exc_is_exception(ctx)) { cxt_exc_clear(ctx); return json_bool(false); }
        return NULL;
    }
    char *lo = cxt_py_lower(cxt_jstr(hn));
    if (hn) json_free(hn);
    bool in = cxt_in_openai_responses_hosts(lo);
    free(lo);
    return json_bool(in);
}

/* PoP: cxt_openai_prefers_native_web_search @ agent/transports/codex.py:_openai_prefers_native_web_search */
static json_t *cxt_openai_prefers_native_web_search(cxt_ctx *ctx) {
    json_t *provider = cxt_call(ctx, "get_active_search_provider", json_object());
    if (cxt_exc_pending(ctx)) {
        if (provider) json_free(provider);
        if (cxt_exc_is_exception(ctx)) { cxt_exc_clear(ctx); return json_bool(false); }
        return NULL;
    }
    if (!cxt_is_none(provider)) {
        json_t *name = cxt_getattr_def(provider, "name");
        bool eq = cxt_str_eq(cxt_jstr(name), "openai-native");
        json_free(provider);
        return json_bool(eq);
    }
    if (provider) json_free(provider);
    json_t *backend = cxt_call(ctx, "_get_search_backend", json_object());
    if (cxt_exc_pending(ctx)) {
        if (backend) json_free(backend);
        if (cxt_exc_is_exception(ctx)) { cxt_exc_clear(ctx); return json_bool(false); }
        return NULL;
    }
    char *s = cxt_str_or_empty(backend);
    char *st = cxt_py_strip(s);
    char *lo = cxt_py_lower(st);
    bool eq = cxt_str_eq(lo, "openai-native");
    free(s); free(st); free(lo);
    if (backend) json_free(backend);
    return json_bool(eq);
}

/* PoP: cxt_is_official_openai_responses_route @ agent/transports/codex.py:_is_official_openai_responses_route */
static json_t *cxt_is_official_openai_responses_route(cxt_ctx *ctx, json_t *model, json_t *base_url) {
    char *bare = cxt_bare_model(model);
    bool gpt62 = cxt_in_gpt62(bare);
    free(bare);
    if (gpt62) {
        json_t *o = cxt_is_openai_api_origin(ctx, base_url);
        if (cxt_exc_pending(ctx)) { if (o) json_free(o); return NULL; }
        bool ob = cxt_truthy(o);
        if (o) json_free(o);
        if (ob) return json_bool(true);
        char *s = cxt_str_or_empty(base_url);
        json_t *a = json_object();
        cxt_arg_str(a, "base_url", s);
        free(s);
        json_t *codex = cxt_call(ctx, "is_official_codex_base_url", a);
        if (cxt_exc_pending(ctx)) { if (codex) json_free(codex); return NULL; }
        bool cb = cxt_truthy(codex);
        if (codex) json_free(codex);
        if (cb) return json_bool(true);
        char *s2 = cxt_str_or_empty(base_url);
        json_t *a2 = json_object();
        cxt_arg_str(a2, "base_url", s2);
        free(s2);
        json_t *hn = cxt_call(ctx, "base_url_hostname", a2);
        if (cxt_exc_pending(ctx)) { if (hn) json_free(hn); return NULL; }
        /* NO .lower() here: case-sensitive membership in {us,eu}.api.openai.com */
        bool in = cxt_str_eq(cxt_jstr(hn), "us.api.openai.com")
               || cxt_str_eq(cxt_jstr(hn), "eu.api.openai.com");
        if (hn) json_free(hn);
        return json_bool(in);
    }
    json_t *a = json_object();
    cxt_arg_node(a, "model", model);
    json_t *astra = cxt_call(ctx, "is_astra_model", a);
    if (cxt_exc_pending(ctx)) { if (astra) json_free(astra); return NULL; }
    bool ab = cxt_truthy(astra);
    if (astra) json_free(astra);
    if (!ab) return json_bool(false);
    json_t *o = cxt_is_openai_api_origin(ctx, base_url);
    if (cxt_exc_pending(ctx)) { if (o) json_free(o); return NULL; }
    bool ob = cxt_truthy(o);
    if (o) json_free(o);
    return json_bool(ob);
}

/* PoP: cxt_codex_efforts_for_route @ agent/transports/codex.py:_codex_efforts_for_route */
static json_t *cxt_codex_efforts_for_route(cxt_ctx *ctx, json_t *model, json_t *base_url, bool is_codex_backend) {
    if (!is_codex_backend) {
        json_t *o = cxt_is_openai_api_origin(ctx, base_url);
        if (cxt_exc_pending(ctx)) { if (o) json_free(o); return NULL; }
        bool ob = cxt_truthy(o);
        if (o) json_free(o);
        if (ob) {
            char *s = cxt_str_or_empty(model);
            json_t *a = json_object();
            cxt_arg_str(a, "model", s);
            free(s);
            json_t *rej = cxt_call(ctx, "openai_model_rejects_reasoning", a);
            if (cxt_exc_pending(ctx)) { if (rej) json_free(rej); return NULL; }
            bool rb = cxt_truthy(rej);
            if (rej) json_free(rej);
            if (rb) return json_array();   /* () */
        }
    }
    json_t *a = json_object();
    cxt_arg_node(a, "model", model);
    json_t *astra = cxt_call(ctx, "is_astra_model", a);
    if (cxt_exc_pending(ctx)) { if (astra) json_free(astra); return NULL; }
    bool ab = cxt_truthy(astra);
    if (astra) json_free(astra);
    if (ab) {
        bool inner = is_codex_backend;
        if (!inner) {
            json_t *off = cxt_is_official_openai_responses_route(ctx, model, base_url);
            if (cxt_exc_pending(ctx)) { if (off) json_free(off); return NULL; }
            inner = cxt_truthy(off);
            if (off) json_free(off);
        }
        if (!inner) return cxt_codex_legacy_efforts();
    }
    char *s = cxt_str_or_empty(model);
    json_t *a2 = json_object();
    cxt_arg_str(a2, "model", s);
    free(s);
    json_t *sup = cxt_call(ctx, "codex_supported_efforts", a2);
    if (cxt_exc_pending(ctx)) { if (sup) json_free(sup); return NULL; }
    return sup ? sup : json_null();
}

/* PoP: cxt_sends_all_turns @ agent/transports/codex.py:_sends_all_turns */
static json_t *cxt_sends_all_turns(cxt_ctx *ctx, json_t *model, json_t *params) {
    if (cxt_is_true(cxt_get(params, "is_codex_backend")))
        return json_bool(false);   /* `is not True` fails */
    json_t *o = cxt_is_openai_api_origin(ctx, cxt_get(params, "base_url"));
    if (cxt_exc_pending(ctx)) { if (o) json_free(o); return NULL; }
    bool ob = cxt_truthy(o);
    if (o) json_free(o);
    if (!ob) return json_bool(false);
    json_t *a = json_object();
    cxt_arg_node(a, "model", model);
    json_t *fam = cxt_call(ctx, "openai_reasoning_family", a);
    if (cxt_exc_pending(ctx)) { if (fam) json_free(fam); return NULL; }
    bool notnone = !cxt_is_none(fam);
    if (fam) json_free(fam);
    return json_bool(notnone);
}

/* ── _reasoning_fields (pure dict builder) ───────────────────────────────── */

/* PoP: cxt_reasoning_fields @ agent/transports/codex.py:_reasoning_fields */
static json_t *cxt_reasoning_fields(cxt_ctx *ctx, json_t *model, json_t *params,
                                    const json_t *effort, bool enabled,
                                    bool replay_encrypted_reasoning, bool is_xai_responses,
                                    bool profile_reasoning, bool all_turns) {
    json_t *include = json_array();
    if (replay_encrypted_reasoning)
        json_append(include, json_string("reasoning.encrypted_content"));
    json_t *fields = json_object();
    if (enabled && is_xai_responses) {
        json_set(fields, "include", include);   /* include owned, transferred */
        json_t *a = json_object();
        cxt_arg_node(a, "model", model);
        json_t *g = cxt_call(ctx, "grok_supports_reasoning_effort", a);
        if (cxt_exc_pending(ctx)) { if (g) json_free(g); json_free(fields); return NULL; }
        bool gb = cxt_truthy(g);
        if (g) json_free(g);
        if (gb) {
            json_t *r = json_object();
            json_set(r, "effort", effort ? json_copy(effort) : json_null());
            json_set(fields, "reasoning", r);
        }
        return fields;
    }
    if (enabled) {
        if (profile_reasoning) {
            json_t *pr = cxt_get(params, "profile_reasoning");
            if (!cxt_is_none(pr)) {
                json_t *r = json_object();
                cxt_obj_update(r, pr);
                json_set(r, "summary", json_string("auto"));
                json_set(fields, "reasoning", r);
            }
        } else {
            json_t *r = json_object();
            json_set(r, "effort", effort ? json_copy(effort) : json_null());
            json_set(r, "summary", json_string("auto"));
            if (all_turns) json_set(r, "context", json_string("all_turns"));
            json_set(fields, "reasoning", r);
            json_set(fields, "include", include);
            return fields;
        }
        json_free(include);
        return fields;
    }
    /* disabled */
    if (!profile_reasoning && !is_xai_responses) {
        json_set(fields, "include", json_array());
        if (cxt_is_str(effort) && cxt_str_eq(cxt_jstr(effort), "none")) {
            json_t *r = json_object();
            json_set(r, "effort", json_string("none"));
            json_set(fields, "reasoning", r);
        }
    }
    json_free(include);
    return fields;
}

/* ── _bound_prompt_cache_key_field (claimed sibling seamed) ──────────────── */

/* PoP: cxt_bound_prompt_cache_key_field @ agent/transports/codex.py:_bound_prompt_cache_key_field */
static void cxt_bound_prompt_cache_key_field(cxt_ctx *ctx, json_t *container) {
    if (!cxt_is_obj(container) || !json_has(container, "prompt_cache_key")) return;
    json_t *value = json_obj_get(container, "prompt_cache_key");
    json_t *a = json_object();
    cxt_arg_node(a, "value", value);
    json_t *bounded = cxt_call(ctx, "_bounded_prompt_cache_key", a);
    if (cxt_exc_pending(ctx)) { if (bounded) json_free(bounded); return; }
    if (cxt_truthy(bounded)) {
        json_set(container, "prompt_cache_key", bounded);   /* bounded owned, transferred */
    } else {
        if (bounded) json_free(bounded);
        json_obj_del(container, "prompt_cache_key");
    }
}

/* ── _merge_extra_headers (pure dict mutation) ───────────────────────────── */

/* PoP: cxt_merge_extra_headers @ agent/transports/codex.py:_merge_extra_headers */
static void cxt_merge_extra_headers(json_t *kwargs, json_t *headers) {
    json_t *existing = cxt_get(kwargs, "extra_headers");
    json_t *merged = json_object();
    if (cxt_is_obj(existing)) {
        for (size_t i = 0; i < existing->c.count; i++) {
            const char *k = existing->c.keys[i];
            json_t *v = existing->c.items[i];
            if (k && k[0] != '\0' && !cxt_is_none(v)) {   /* if k and v is not None */
                char *vs = cxt_pystr(v);
                json_set(merged, k, json_string(vs));
                free(vs);
            }
        }
    }
    cxt_obj_update(merged, headers);
    json_set(kwargs, "extra_headers", merged);
}

/* ── _newest_reasoning_only (pure list flow) ─────────────────────────────── */

static bool cxt_is_client_reasoning_item(const json_t *i) {
    return cxt_is_obj(i) && !cxt_str_eq(cxt_jstr(cxt_get(i, "type")), "compaction");
}
static bool cxt_has_noncompaction(const json_t *items) {
    if (!cxt_is_arr(items)) return false;
    for (size_t i = 0; i < items->c.count; i++)
        if (cxt_is_client_reasoning_item(items->c.items[i])) return true;
    return false;
}

/* PoP: cxt_newest_reasoning_only @ agent/transports/codex.py:_newest_reasoning_only */
static json_t *cxt_newest_reasoning_only(json_t *messages) {
    json_t *out = json_array();
    bool newest_kept = false;
    size_t n = cxt_is_arr(messages) ? messages->c.count : 0;
    for (size_t ri = 0; ri < n; ri++) {
        json_t *msg = messages->c.items[n - 1 - ri];   /* reversed(messages) */
        json_t *items = NULL;
        if (cxt_is_obj(msg) && cxt_str_eq(cxt_jstr(cxt_get(msg, "role")), "assistant"))
            items = cxt_get(msg, "codex_reasoning_items");
        if (cxt_is_arr(items) && cxt_has_noncompaction(items)) {
            if (newest_kept) {
                json_t *checkpoints = json_array();
                for (size_t i = 0; i < items->c.count; i++) {
                    json_t *it = items->c.items[i];
                    if (cxt_is_obj(it) && cxt_str_eq(cxt_jstr(cxt_get(it, "type")), "compaction"))
                        json_append(checkpoints, json_copy(it));
                }
                json_t *nm = json_copy(msg);
                json_set(nm, "codex_reasoning_trimmed", json_bool(true));
                if (checkpoints->c.count > 0) {
                    json_set(nm, "codex_reasoning_items", checkpoints);
                } else {
                    json_free(checkpoints);
                    json_obj_del(nm, "codex_reasoning_items");
                }
                json_append(out, nm);
                newest_kept = true;
                continue;
            }
            newest_kept = true;
        }
        json_append(out, json_copy(msg));
    }
    /* out.reverse() */
    for (size_t i = 0; i < out->c.count / 2; i++) {
        json_t *t = out->c.items[i];
        out->c.items[i] = out->c.items[out->c.count - 1 - i];
        out->c.items[out->c.count - 1 - i] = t;
    }
    return out;
}

/* ── _sanitize_astra_request_kwargs (mutation flow) ──────────────────────── */

/* PoP: cxt_sanitize_astra_request_kwargs @ agent/transports/codex.py:_sanitize_astra_request_kwargs */
static void cxt_sanitize_astra_request_kwargs(cxt_ctx *ctx, json_t *kwargs, json_t *model, json_t *base_url) {
    json_t *off = cxt_is_official_openai_responses_route(ctx, model, base_url);
    if (cxt_exc_pending(ctx)) { if (off) json_free(off); return; }
    bool official = cxt_truthy(off);
    if (off) json_free(off);
    if (!official) return;
    json_t *reasoning = cxt_get(kwargs, "reasoning");
    if (cxt_is_obj(reasoning)) {
        char *req = cxt_str_or_empty(cxt_get(reasoning, "effort"));
        char *st = cxt_py_strip(req);
        char *lo = cxt_py_lower(st);
        bool requested = lo && lo[0] != '\0';
        if (requested) {
            json_t *a = json_object();
            cxt_arg_str(a, "effort", lo);
            json_t *sup = cxt_codex_astra_efforts();
            json_set(a, "supported", sup);
            json_t *clamped = cxt_call(ctx, "clamp_effort", a);
            free(req); free(st); free(lo);
            if (cxt_exc_pending(ctx)) { if (clamped) json_free(clamped); return; }
            json_set(reasoning, "effort", clamped ? clamped : json_null());
        } else {
            free(req); free(st); free(lo);
            json_set(reasoning, "effort", json_string("low"));
        }
    }
    static const char *DROP[] = {"temperature", "top_p", "top_logprobs", "logprobs", "prompt_cache_retention"};
    for (size_t i = 0; i < 5; i++) json_obj_del(kwargs, DROP[i]);
    json_t *include = cxt_get(kwargs, "include");
    if (cxt_is_arr(include)) {
        json_t *kept = json_array();
        for (size_t i = 0; i < include->c.count; i++) {
            char *s = cxt_pystr(include->c.items[i]);
            char *lo = cxt_py_lower(s);
            if (!strstr(lo, "logprob")) json_append(kept, json_copy(include->c.items[i]));
            free(s); free(lo);
        }
        json_set(kwargs, "include", kept);
    }
}

/* ── _resolve_reasoning (orchestrator + module-global warned set) ────────── */

static bool cxt_set_has(const json_t *set, const char *key) {
    return cxt_is_obj(set) && json_has(set, key);
}
static void cxt_set_add(json_t *set, const char *key) {
    if (cxt_is_obj(set)) json_set(set, key, json_bool(true));
}
/* ", ".join(str(level) for level in supported). Caller frees. */
static char *cxt_join_levels(const json_t *supported) {
    size_t cap = 64, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    buf[0] = '\0';
    size_t n = cxt_is_arr(supported) ? supported->c.count : 0;
    for (size_t i = 0; i < n; i++) {
        char *s = cxt_pystr(supported->c.items[i]);
        size_t sl = strlen(s);
        size_t need = len + sl + (i ? 2 : 0) + 1;
        if (need > cap) {
            while (cap < need) cap *= 2;
            char *nb = (char *)realloc(buf, cap);
            if (!nb) { free(s); free(buf); return NULL; }
            buf = nb;
        }
        if (i) { memcpy(buf + len, ", ", 2); len += 2; }
        memcpy(buf + len, s, sl); len += sl; buf[len] = '\0';
        free(s);
    }
    return buf;
}

/* PoP: cxt_resolve_reasoning @ agent/transports/codex.py:_resolve_reasoning */
static json_t *cxt_resolve_reasoning(cxt_ctx *ctx, json_t *model, json_t *params, json_t *warned) {
    json_t *effort_node = json_string("medium");
    bool enabled = true;
    json_t *rc = cxt_get(params, "reasoning_config");
    if (cxt_truthy(rc) && cxt_is_obj(rc)) {
        json_t *en = cxt_get(rc, "enabled");
        if (cxt_is_false(en)) {
            enabled = false;
        } else {
            json_t *ef = cxt_get(rc, "effort");
            if (cxt_truthy(ef)) {
                json_free(effort_node);
                effort_node = json_copy(ef);
            }
        }
    }
    json_t *supported = NULL;
    if (cxt_truthy(cxt_get(params, "is_xai_responses"))) {
        json_t *a = json_object();
        cxt_arg_node(a, "model", model);
        json_t *g46 = cxt_call(ctx, "is_grok_46_family", a);
        if (cxt_exc_pending(ctx)) { json_free(effort_node); if (g46) json_free(g46); return NULL; }
        bool g = cxt_truthy(g46);
        if (g46) json_free(g46);
        supported = g ? cxt_xai_grok46_efforts() : cxt_xai_legacy_efforts();
    } else {
        json_t *base_url = cxt_get(params, "base_url");
        bool is_codex = cxt_is_true(cxt_get(params, "is_codex_backend"));
        bool gate = is_codex;
        if (!gate) {
            json_t *o = cxt_is_openai_api_origin(ctx, base_url);
            if (cxt_exc_pending(ctx)) { json_free(effort_node); if (o) json_free(o); return NULL; }
            gate = cxt_truthy(o);
            if (o) json_free(o);
        }
        json_t *declared = NULL;
        if (!gate) {
            json_t *a = json_object();
            cxt_arg_node(a, "provider", cxt_get(params, "provider"));
            cxt_arg_node(a, "model", model);
            cxt_arg_node(a, "base_url", base_url);
            declared = cxt_call(ctx, "_profile_declared_efforts", a);
            if (cxt_exc_pending(ctx)) { json_free(effort_node); if (declared) json_free(declared); return NULL; }
        }
        if (!cxt_is_none(declared)) {
            supported = declared;
        } else {
            if (declared) json_free(declared);
            supported = cxt_codex_efforts_for_route(ctx, model, base_url, is_codex);
            if (cxt_exc_pending(ctx)) { json_free(effort_node); if (supported) json_free(supported); return NULL; }
        }
        if (!cxt_truthy(supported)) {
            json_free(effort_node);
            if (supported) json_free(supported);
            return cxt_pair(json_null(), json_bool(false));
        }
    }
    if (!enabled) {
        bool has_none = false;
        size_t n = cxt_is_arr(supported) ? supported->c.count : 0;
        for (size_t i = 0; i < n && !has_none; i++) {
            char *s = cxt_pystr(supported->c.items[i]);
            char *st = cxt_py_strip(s);
            char *lo = cxt_py_lower(st);
            if (cxt_str_eq(lo, "none")) has_none = true;
            free(s); free(st); free(lo);
        }
        const char *mkey = cxt_jstr(model);
        if (!has_none && !cxt_set_has(warned, mkey)) {
            cxt_set_add(warned, mkey);
            char *joined = cxt_join_levels(supported);
            json_t *aa = json_array();
            json_append(aa, model ? json_copy(model) : json_null());
            json_append(aa, json_string(joined ? joined : ""));
            free(joined);
            /* "reasoning_effort: none cannot be sent for %s — its route accepts only %s, ..." */
            cxt_log_warning(ctx,
                "reasoning_effort: none cannot be sent for %s \xe2\x80\x94 its route accepts only %s, "
                "so the model's default effort stays on (an omitted reasoning field does not disable it).",
                aa);
            if (cxt_exc_pending(ctx)) { json_free(effort_node); if (supported) json_free(supported); return NULL; }
        }
        json_t *ret_effort = has_none ? json_string("none") : json_null();
        json_free(effort_node);
        if (supported) json_free(supported);
        return cxt_pair(ret_effort, json_bool(false));
    }
    json_t *a = json_object();
    cxt_arg_node(a, "effort", effort_node);
    cxt_arg_node(a, "supported", supported);
    json_t *clamped = cxt_call(ctx, "clamp_effort", a);
    json_free(effort_node);
    if (supported) json_free(supported);
    if (cxt_exc_pending(ctx)) { if (clamped) json_free(clamped); return NULL; }
    return cxt_pair(clamped ? clamped : json_null(), json_bool(true));
}

/* ── _alias_wire_tools (reserved-tool aliasing orchestrator) ─────────────── */

static bool cxt_is_client_web_search(const json_t *t) {
    return cxt_is_obj(t) && cxt_str_eq(cxt_jstr(cxt_get(t, "name")), "web_search");
}
static bool cxt_any_client_web_search(const json_t *tools) {
    if (!cxt_is_arr(tools)) return false;
    for (size_t i = 0; i < tools->c.count; i++)
        if (cxt_is_client_web_search(tools->c.items[i])) return true;
    return false;
}
static json_t *cxt_web_search_builtin(void) {
    json_t *o = json_object();
    json_set(o, "type", json_string("web_search"));
    return o;
}
static json_t *cxt_filter_not_web_search(const json_t *tools) {
    json_t *nt = json_array();
    for (size_t i = 0; i < tools->c.count; i++)
        if (!cxt_is_client_web_search(tools->c.items[i]))
            json_append(nt, json_copy(tools->c.items[i]));
    return nt;
}
static json_t *cxt_rename_web_search(const json_t *tools) {
    json_t *nt = json_array();
    for (size_t i = 0; i < tools->c.count; i++) {
        json_t *t = tools->c.items[i];
        json_t *cp = json_copy(t);
        if (cxt_is_client_web_search(t))
            json_set(cp, "name", json_string("hermes_web_search"));
        json_append(nt, cp);
    }
    return nt;
}
/* _alias_reserved_tools(tools, reserved) seam -> [tools, alias_map]. Owned. */
static json_t *cxt_alias_reserved(cxt_ctx *ctx, json_t *tools, json_t *reserved) {
    json_t *a = json_object();
    cxt_arg_node(a, "response_tools", tools);
    json_set(a, "reserved_names", reserved);   /* reserved owned, transferred */
    return cxt_call(ctx, "_alias_reserved_tools", a);
}
/* Apply a reserved-tool seam result: tools <- res[0], wire_aliases.update(res[1]).
 * Returns the new tools (owned) or NULL on raise. */
static json_t *cxt_apply_alias_result(cxt_ctx *ctx, json_t *res, json_t *wire_aliases) {
    if (cxt_is_none(res)) {
        cxt_raise_msg(ctx, "TypeError", "cannot unpack non-iterable NoneType object");
        return NULL;
    }
    if (!cxt_is_arr(res) || res->c.count != 2) {
        cxt_raise_msg(ctx, "ValueError", "not enough values to unpack (expected 2)");
        return NULL;
    }
    json_t *al = json_get(res, 1);
    cxt_obj_update(wire_aliases, al);
    return json_copy(json_get(res, 0));
}

/* PoP: cxt_alias_wire_tools @ agent/transports/codex.py:_alias_wire_tools */
static json_t *cxt_alias_wire_tools(cxt_ctx *ctx, json_t *response_tools, json_t *params,
                                    const json_t *is_xai_responses, const json_t *is_codex_backend) {
    json_t *tools = response_tools ? json_copy(response_tools) : json_null();
    json_t *wire_aliases = json_object();
    bool xai = cxt_truthy(is_xai_responses);
    bool codex = cxt_truthy(is_codex_backend);

    if (xai && cxt_truthy(tools) && cxt_any_client_web_search(tools)) {
        json_t *pref = cxt_call(ctx, "_xai_prefers_native_web_search", json_object());
        if (cxt_exc_pending(ctx)) { json_free(tools); json_free(wire_aliases); if (pref) json_free(pref); return NULL; }
        bool native = cxt_truthy(pref);
        if (pref) json_free(pref);
        if (native) {
            json_t *nt = cxt_filter_not_web_search(tools);
            json_append(nt, cxt_web_search_builtin());
            json_free(tools); tools = nt;
        } else {
            json_t *nt = cxt_rename_web_search(tools);
            json_free(tools); tools = nt;
            json_set(wire_aliases, "hermes_web_search", json_string("web_search"));
        }
    }
    if (codex && cxt_truthy(tools) && cxt_any_client_web_search(tools)) {
        json_t *pref = cxt_openai_prefers_native_web_search(ctx);
        if (cxt_exc_pending(ctx)) { json_free(tools); json_free(wire_aliases); if (pref) json_free(pref); return NULL; }
        bool native = cxt_truthy(pref);
        if (pref) json_free(pref);
        if (native) {
            json_t *nt = cxt_filter_not_web_search(tools);
            json_append(nt, cxt_web_search_builtin());
            json_free(tools); tools = nt;
        }
    }
    if (cxt_truthy(tools)) {
        json_t *a = json_object();
        cxt_arg_node(a, "params", params);
        json_t *oc = cxt_call(ctx, "_is_opencode_responses_backend", a);
        if (cxt_exc_pending(ctx)) { json_free(tools); json_free(wire_aliases); if (oc) json_free(oc); return NULL; }
        bool ocb = cxt_truthy(oc);
        if (oc) json_free(oc);
        if (ocb) {
            json_t *res = cxt_alias_reserved(ctx, tools, cxt_opencode_reserved());
            if (cxt_exc_pending(ctx)) { json_free(tools); json_free(wire_aliases); if (res) json_free(res); return NULL; }
            json_t *nt = cxt_apply_alias_result(ctx, res, wire_aliases);
            json_free(res);
            if (cxt_exc_pending(ctx)) { json_free(tools); json_free(wire_aliases); if (nt) json_free(nt); return NULL; }
            json_free(tools); tools = nt;
        }
    }
    if (cxt_truthy(tools)) {
        json_t *pp = cxt_is_perplexity_responses_backend(ctx, params);
        if (cxt_exc_pending(ctx)) { json_free(tools); json_free(wire_aliases); if (pp) json_free(pp); return NULL; }
        bool ppb = cxt_truthy(pp);
        if (pp) json_free(pp);
        if (ppb) {
            json_t *res = cxt_alias_reserved(ctx, tools, cxt_perplexity_reserved());
            if (cxt_exc_pending(ctx)) { json_free(tools); json_free(wire_aliases); if (res) json_free(res); return NULL; }
            json_t *nt = cxt_apply_alias_result(ctx, res, wire_aliases);
            json_free(res);
            if (cxt_exc_pending(ctx)) { json_free(tools); json_free(wire_aliases); if (nt) json_free(nt); return NULL; }
            json_free(tools); tools = nt;
        }
    }
    if (cxt_truthy(tools)) {
        json_t *rs = cxt_reserves_tool_search(ctx, params, is_xai_responses);
        if (cxt_exc_pending(ctx)) { json_free(tools); json_free(wire_aliases); if (rs) json_free(rs); return NULL; }
        bool rsb = cxt_truthy(rs);
        if (rs) json_free(rs);
        if (rsb) {
            json_t *res = cxt_alias_reserved(ctx, tools, cxt_xai_reserved());
            if (cxt_exc_pending(ctx)) { json_free(tools); json_free(wire_aliases); if (res) json_free(res); return NULL; }
            json_t *nt = cxt_apply_alias_result(ctx, res, wire_aliases);
            json_free(res);
            if (cxt_exc_pending(ctx)) { json_free(tools); json_free(wire_aliases); if (nt) json_free(nt); return NULL; }
            json_free(tools); tools = nt;
        }
    }
    return cxt_pair(tools, wire_aliases);
}

/* ── ResponsesApiTransport methods ───────────────────────────────────────── */

/* str(error).lower() for str / null / exception-marker / scalar error. Caller frees. */
static char *cxt_error_text(const json_t *error) {
    char *s;
    if (cxt_is_obj(error)) {
        const char *m = cxt_jstr(cxt_get(error, "msg"));   /* absent msg -> "" (str(exc)) */
        s = cxt_dup_span(m, strlen(m));
    } else {
        s = cxt_pystr(error);
    }
    char *lo = cxt_py_lower(s);
    free(s);
    return lo;
}
/* getattr(error, "status_code", None). Borrowed (NULL when absent / non-obj). */
static json_t *cxt_error_status(const json_t *error) {
    if (!cxt_is_obj(error)) return NULL;
    return json_obj_get(error, "status_code");
}
/* set.add for an array-backed set (dedup, append). */
static void cxt_array_set_add(json_t *arr, const char *key) {
    if (!cxt_is_arr(arr)) return;
    for (size_t i = 0; i < arr->c.count; i++)
        if (cxt_is_str(arr->c.items[i]) && cxt_str_eq(arr->c.items[i]->str_val, key)) return;
    json_append(arr, json_string(key));
}

/* PoP: cxt_reject_all_turns @ agent/transports/codex.py:reject_all_turns */
static json_t *cxt_reject_all_turns(cxt_ctx *ctx, json_t *slf, json_t *error) {
    char *text = cxt_error_text(error);
    bool last_sent = cxt_truthy(cxt_get(slf, "_last_sent_all_turns"));
    json_t *sc = cxt_error_status(error);
    bool sc_ok = cxt_is_none(sc) || (cxt_is_num(sc) && sc->num_val == 400.0);
    bool marker = strstr(text, "reasoning.context") != NULL || strstr(text, "all_turns") != NULL;
    free(text);
    if (!last_sent || !sc_ok || !marker) return json_bool(false);
    json_t *rej = cxt_get(slf, "_all_turns_rejected");
    json_t *model = cxt_get(slf, "_last_issuer_model");
    cxt_array_set_add(rej, cxt_jstr(model));
    json_set(slf, "_last_sent_all_turns", json_bool(false));
    json_t *aa = json_array();
    json_append(aa, model ? json_copy(model) : json_null());
    cxt_log_warning(ctx, "reasoning.context rejected by %s; omitting it for this session", aa);
    if (cxt_exc_pending(ctx)) return NULL;
    return json_bool(true);
}

/* PoP: cxt_drop_unverified_replay @ agent/transports/codex.py:drop_unverified_replay */
static json_t *cxt_drop_unverified_replay(cxt_ctx *ctx, json_t *slf, json_t *message_lists) {
    json_t *kind = cxt_get(slf, "_last_issuer_kind");
    json_t *model = cxt_get(slf, "_last_issuer_model");
    if (cxt_is_none(kind) || cxt_is_none(model)) return json_number(0);
    json_t *removed = json_array();
    if (cxt_is_arr(message_lists)) {
        for (size_t i = 0; i < message_lists->c.count; i++) {
            json_t *a = json_object();
            cxt_arg_node(a, "msgs", message_lists->c.items[i]);
            cxt_arg_node(a, "issuer_kind", kind);
            cxt_arg_node(a, "issuer_model", model);
            json_t *r = cxt_call(ctx, "strip_unverified_reasoning_items", a);
            if (cxt_exc_pending(ctx)) { json_free(removed); if (r) json_free(r); return NULL; }
            json_append(removed, r ? r : json_null());
        }
    }
    json_t *ret = removed->c.count > 0 ? json_copy(removed->c.items[0]) : json_number(0);
    json_free(removed);
    return ret;
}