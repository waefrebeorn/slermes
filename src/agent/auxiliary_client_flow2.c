/* auxiliary_client_flow2.c — FLOW + leaves port of the UNCLAIMED remainder of
 * agent/auxiliary_client.py (the claimed siblings live in auxiliary_client_flow.c
 * / _helpers / _gaps / _runtime and are SEAMED here).
 *
 * Decision-sequence port: notify hooks, header builders, auth/path/env reads,
 * pool/close/socket leaves, the per-provider resolve branches, discovery/main
 * routing predicates, main-config field readers, credential refresh, vision
 * routing and the semaphore cache. ALL collaborator I/O (claimed siblings,
 * foreign-module imports, config/registry/pool/clock reads, logger) rides the
 * injected universal seam ac2_call; the harness scripts results and records the
 * ordered trace the oracle diffs against LIVE agent/auxiliary_client.py under the
 * same scripts. Ported functions call each other REAL (internal composition).
 *
 * Representation contract (shared with tests/oracle_auxiliary_client_flow2.py):
 *   - object doubles (pool/profile/client/refreshed) are attribute dicts read via
 *     json_obj_get; tuple/set -> array (sets sorted); None -> JSON null; integral
 *     floats collapse to ints on both sides; Path -> str.
 *   - uncaught exception -> {"__raise__": cls}; an exception passed as a logger
 *     arg projects to {"__raise__": cls, "msg": str(e)}.
 *   - scripts: absent key -> None; {"$seq":[...]} per-call queue (exhaustion
 *     raises RuntimeError("script exhausted: <name>")); {"__raise__":cls,"msg":m}
 *     raises (trace ret is the WHOLE marker); any other value reused. None-valued
 *     seam arg keys dropped on BOTH sides.
 *   - object-valued seam results are null or a NON-EMPTY record (empty {} is
 *     outside the live domain for truthiness-gated pool getters); profile {} is
 *     used only for the `is not None` -> .api_mode AttributeError branch.
 *   - fixture domain: str()/int()/float() operands are ASCII strings, numbers,
 *     bools or null; .lower()/.strip() use ASCII semantics.
 *
 * libjson-only (default oracle deps suffice). Self-contained: the harness
 * #includes "agent/auxiliary_client_flow2.c". acf_ prefix is taken by flow1; this
 * port uses ac2_.
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>

/* ── context + seam plumbing ─────────────────────────────────────────────── */

typedef struct ac2_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *env;       /* borrowed: scripted constants/flags */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} ac2_ctx;

static bool ac2_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool ac2_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool ac2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool ac2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool ac2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool ac2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool ac2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static bool ac2_truthy(const json_t *v) {
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

static const char *ac2_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static double ac2_num(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}

/* borrowed object field or NULL */
static json_t *ac2_get(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}

/* dict.get(key, default): present-null keeps the null node; absent -> default.
 * Returns a BORROWED node (the default node is borrowed too). */
static json_t *ac2_get_def(const json_t *obj, const char *key, json_t *def) {
    if (obj && obj->type == JSON_OBJECT) {
        json_t *v = json_obj_get(obj, key);
        if (v) return v;
    }
    return def;
}

static json_t *ac2_copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}

/* Deep Python-style equality; NULL pointer and JSON_NULL both read None. */
static bool ac2_json_eq(const json_t *a, const json_t *b) {
    if (ac2_is_none(a) && ac2_is_none(b)) return true;
    if (ac2_is_none(a) || ac2_is_none(b)) return false;
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return ac2_str_eq(a->str_val, b->str_val);
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!ac2_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !ac2_json_eq(a->c.items[i], bv)) return false;
            }
            return true;
        default: return true;
    }
}

/* ── string helpers (caller frees) ───────────────────────────────────────── */

static bool ac2_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static char *ac2_strip(const char *s) {
    if (!s) return strdup("");
    const char *b = s;
    while (*b && ac2_is_space(*b)) b++;
    const char *e = b + strlen(b);
    while (e > b && ac2_is_space(e[-1])) e--;
    size_t n = (size_t)(e - b);
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, b, n); r[n] = '\0';
    return r;
}
static char *ac2_lower(const char *s) {
    char *r = strdup(s ? s : "");
    if (!r) return NULL;
    for (char *p = r; *p; p++) *p = (char)tolower((unsigned char)*p);
    return r;
}
static char *ac2_rstrip(const char *s, const char *chars) {
    if (!s) return strdup("");
    size_t n = strlen(s);
    while (n > 0 && strchr(chars, s[n - 1])) n--;
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, s, n); r[n] = '\0';
    return r;
}
/* str(v) for a scalar (null -> "None"). Caller frees. */
static char *ac2_pystr(const json_t *v) {
    if (ac2_is_none(v)) return strdup("None");
    if (ac2_is_bool(v)) return strdup(v->bool_val ? "True" : "False");
    if (ac2_is_num(v)) {
        char buf[64]; double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
        else snprintf(buf, sizeof buf, "%g", d);
        return strdup(buf);
    }
    if (ac2_is_str(v)) return strdup(v->str_val ? v->str_val : "");
    return strdup("");
}
/* str(v or "") for a scalar (falsy -> ""). Caller frees. */
static char *ac2_str_or_empty(const json_t *v) {
    if (!ac2_truthy(v)) return strdup("");
    return ac2_pystr(v);
}

/* ── exception plumbing ──────────────────────────────────────────────────── */

static bool ac2_exc_pending(const ac2_ctx *ctx) { return ctx && ctx->exc != NULL; }
static void ac2_exc_clear(ac2_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}
static void ac2_raise(ac2_ctx *ctx, const char *cls, const char *msg) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
    json_set(ctx->exc, "msg", json_string(msg ? msg : ""));
}
static const char *ac2_exc_class(const ac2_ctx *ctx) {
    return json_get_str(ctx->exc, "__raise__", "");
}
/* Builtin exception subclass chain (parent lookup). */
static const char *ac2_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        {"FileNotFoundError", "OSError"}, {"PermissionError", "OSError"},
        {"IsADirectoryError", "OSError"}, {"NotADirectoryError", "OSError"},
        {"FileExistsError", "OSError"}, {"InterruptedError", "OSError"},
        {"BlockingIOError", "OSError"}, {"ChildProcessError", "OSError"},
        {"ProcessLookupError", "OSError"}, {"ConnectionError", "OSError"},
        {"BrokenPipeError", "ConnectionError"}, {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
        {"TimeoutError", "OSError"},
        {"JSONDecodeError", "ValueError"}, {"UnicodeDecodeError", "UnicodeError"},
        {"UnicodeError", "ValueError"},
        {"ZeroDivisionError", "ArithmeticError"}, {"OverflowError", "ArithmeticError"},
        {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (ac2_str_eq(T[i].c, c)) return T[i].p;
    return NULL;
}
static bool ac2_exc_matches(const ac2_ctx *ctx, const char *const *classes, size_t n) {
    if (!ac2_exc_pending(ctx)) return false;
    const char *cls = ac2_exc_class(ctx);
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (ac2_str_eq(cls, classes[i])) return true;
        const char *p = ac2_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}
/* except Exception (NOT BaseException trio). */
static bool ac2_exc_is_exception(const ac2_ctx *ctx) {
    static const char *E[] = {"Exception"};
    return ac2_exc_matches(ctx, E, 1);
}

/* ── universal seam ──────────────────────────────────────────────────────── */

static json_t *ac2_script_result(ac2_ctx *ctx, const json_t *script) {
    if (!script || script->type == JSON_NULL) return NULL;
    if (script->type == JSON_OBJECT) {
        json_t *raise = json_obj_get(script, "__raise__");
        if (raise && raise->type == JSON_STRING) {
            if (ctx->exc) json_free(ctx->exc);
            ctx->exc = json_copy(script);
            return NULL;
        }
    }
    return json_copy(script);
}

/* args ownership is TRANSFERRED (NULL -> empty object). Result is NEW. */
static json_t *ac2_call(ac2_ctx *ctx, const char *name, json_t *args) {
    if (!args) args = json_object();
    json_t *ret = NULL;
    json_t *script = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *chosen = script;
    if (script && script->type == JSON_OBJECT) {
        json_t *seq = json_obj_get(script, "$seq");
        if (seq && seq->type == JSON_ARRAY) {
            double cur = json_get_num(ctx->counters, name, 0);
            size_t idx = (size_t)cur;
            if (idx >= seq->c.count) {
                if (ctx->exc) json_free(ctx->exc);
                ctx->exc = json_object();
                json_set(ctx->exc, "__raise__", json_string("RuntimeError"));
                char msg[256];
                snprintf(msg, sizeof msg, "script exhausted: %s", name);
                json_set(ctx->exc, "msg", json_string(msg));
                chosen = ctx->exc;
            } else {
                json_set(ctx->counters, name, json_number(cur + 1));
                chosen = seq->c.items[idx];
            }
        }
    }
    if (chosen != ctx->exc) ret = ac2_script_result(ctx, chosen);
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args);
    if (ctx->exc && chosen == ctx->exc) json_set(ev, "ret", json_copy(ctx->exc));
    else if (ctx->exc && chosen && chosen->type == JSON_OBJECT && json_obj_get(chosen, "__raise__"))
        json_set(ev, "ret", json_copy(ctx->exc));
    else json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(ctx->trace, ev);
    return ret;
}

/* Arg builders: None-valued keys dropped on BOTH sides ("" kept). Borrowed val
 * is copied; the caller keeps ownership of the source node. */
static void ac2_arg(json_t *args, const char *key, const json_t *val) {
    if (!val || val->type == JSON_NULL) return;
    json_set(args, key, json_copy(val));
}
static void ac2_arg_str(json_t *args, const char *key, const char *val) {
    if (!val) return;
    json_set(args, key, json_string(val));
}
static void ac2_arg_bool(json_t *args, const char *key, bool val) {
    json_set(args, key, json_bool(val));
}
/* logger seam: {"msg","args"[,"exc_info"]}. args_arr ownership transferred. */
static json_t *ac2_log(ac2_ctx *ctx, const char *level, const char *msg,
                       json_t *args_arr, bool exc_info) {
    json_t *a = json_object();
    json_set(a, "msg", json_string(msg));
    json_set(a, "args", args_arr ? args_arr : json_array());
    if (exc_info) json_set(a, "exc_info", json_bool(true));
    char name[32];
    snprintf(name, sizeof name, "logger.%s", level);
    json_t *r = ac2_call(ctx, name, a);
    json_free(r);
    return NULL;
}
/* project the pending exception as a logger arg node {"__raise__","msg"}. */
static json_t *ac2_exc_arg(const ac2_ctx *ctx) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(ac2_exc_class(ctx)));
    json_set(e, "msg", json_string(json_get_str(ctx->exc, "msg", "")));
    return e;
}

/* tuple result helpers */
static json_t *ac2_t0(const json_t *r) { return ac2_is_arr(r) ? json_get(r, 0) : NULL; }
static json_t *ac2_t1(const json_t *r) { return ac2_is_arr(r) ? json_get(r, 1) : NULL; }
static json_t *ac2_pair(json_t *a, json_t *b) {
    json_t *r = json_array();
    json_append(r, a ? a : json_null());
    json_append(r, b ? b : json_null());
    return r;
}
static json_t *ac2_triple(const char *s, json_t *b, json_t *c) {
    json_t *r = json_array();
    json_append(r, s ? json_string(s) : json_null());
    json_append(r, b ? json_copy(b) : json_null());
    json_append(r, c ? json_copy(c) : json_null());
    return r;
}

/* ── notify hooks ────────────────────────────────────────────────────────── */

/* PoP: ac2_tick_hook @ agent/auxiliary_client.py:_tick_hook */
static json_t *ac2_tick_hook(ac2_ctx *ctx, json_t *local, const char *label) {
    json_t *hook = ac2_get(local, "hook");
    if (ac2_is_none(hook)) return json_null();
    json_t *r = ac2_call(ctx, "hook", json_object());
    json_free(r);
    if (ac2_exc_pending(ctx)) {
        if (ac2_exc_is_exception(ctx)) {
            ac2_exc_clear(ctx);
            json_t *aa = json_array();
            json_append(aa, json_string(label));
            ac2_log(ctx, "debug", "aux %s hook failed", aa, true);
        }
        /* BaseException propagates (exc stays pending) */
    }
    return json_null();
}

/* PoP: ac2_notify_aux_dispatch @ agent/auxiliary_client.py:_notify_aux_dispatch */
static json_t *ac2_notify_aux_dispatch(ac2_ctx *ctx, json_t *in) {
    return ac2_tick_hook(ctx, ac2_get(in, "local"), "dispatch");
}
/* PoP: ac2_notify_aux_timing_response @ agent/auxiliary_client.py:_notify_aux_timing_response */
static json_t *ac2_notify_aux_timing_response(ac2_ctx *ctx, json_t *in) {
    return ac2_tick_hook(ctx, ac2_get(in, "local"), "provider response");
}
/* PoP: ac2_notify_aux_provider_response @ agent/auxiliary_client.py:_notify_aux_provider_response */
static json_t *ac2_notify_aux_provider_response(ac2_ctx *ctx, json_t *in) {
    json_t *r = ac2_notify_aux_timing_response(ctx, in);
    json_free(r);
    if (ac2_exc_pending(ctx)) return json_null();
    json_t *p = ac2_call(ctx, "_notify_aux_progress", json_object());
    json_free(p);
    return json_null();
}

/* ── headers / path / env ────────────────────────────────────────────────── */

/* PoP: ac2_ai_gateway_headers @ agent/auxiliary_client.py:_ai_gateway_headers */
static json_t *ac2_ai_gateway_headers(ac2_ctx *ctx, json_t *in) {
    (void)in;
    json_t *a = json_object();
    ac2_arg_str(a, "product", "HermesAgent");
    json_t *ua = ac2_call(ctx, "hermes_user_agent", a);
    if (ac2_exc_pending(ctx)) { json_free(ua); return NULL; }
    json_t *out = json_object();
    json_set(out, "HTTP-Referer", json_string("https://hermes-agent.nousresearch.com"));
    json_set(out, "X-Title", json_string("Hermes Agent"));
    json_set(out, "User-Agent", ua ? ua : json_null());
    return out;
}

/* PoP: ac2_auth_json_path @ agent/auxiliary_client.py:_auth_json_path */
static json_t *ac2_auth_json_path(ac2_ctx *ctx, json_t *in) {
    (void)in;
    const char *p = json_get_str(ctx->env, "auth_json_path", "/default/auth.json");
    const char *pi = json_get_str(ctx->env, "auth_json_path_at_import", "/default/auth.json");
    if (!ac2_str_eq(p, pi)) return json_string(p);
    json_t *r = ac2_call(ctx, "_auth_file_path", json_object());
    if (ac2_exc_pending(ctx)) { json_free(r); return NULL; }
    return r ? r : json_null();
}

/* PoP: ac2_codex_base_url_override @ agent/auxiliary_client.py:_codex_base_url_override */
static json_t *ac2_codex_base_url_override(ac2_ctx *ctx, json_t *in) {
    (void)in;
    json_t *a = json_object();
    ac2_arg_str(a, "name", "HERMES_CODEX_BASE_URL");
    json_t *r = ac2_call(ctx, "_scoped_key_env", a);
    if (ac2_exc_pending(ctx)) { json_free(r); return NULL; }
    if (ac2_is_none(r)) { json_free(r); ac2_raise(ctx, "AttributeError", ""); return NULL; }
    char *s = ac2_rstrip(ac2_jstr(r), "/");
    json_free(r);
    json_t *out = json_string(s);
    free(s);
    return out;
}

/* PoP: ac2_current_event_loop @ agent/auxiliary_client.py:_current_event_loop */
static json_t *ac2_current_event_loop(ac2_ctx *ctx, json_t *in) {
    (void)in;
    json_t *r = ac2_call(ctx, "asyncio.get_event_loop", json_object());
    if (ac2_exc_pending(ctx)) {
        static const char *RE[] = {"RuntimeError"};
        if (ac2_exc_matches(ctx, RE, 1)) { ac2_exc_clear(ctx); json_free(r); return json_null(); }
        json_free(r); return NULL;
    }
    return r ? r : json_null();
}

/* ── pool / close / socket leaves ────────────────────────────────────────── */

/* PoP: ac2_load_pool_with_credentials @ agent/auxiliary_client.py:_load_pool_with_credentials */
static json_t *ac2_load_pool_with_credentials(ac2_ctx *ctx, json_t *in) {
    const char *provider = json_get_str(in, "provider", "");
    const char *note = json_get_str(in, "note", "");
    json_t *a = json_object();
    ac2_arg_str(a, "provider", provider);
    json_t *pool = ac2_call(ctx, "load_pool", a);
    if (ac2_exc_pending(ctx)) {
        if (ac2_exc_is_exception(ctx)) {
            json_t *aa = json_array();
            json_append(aa, json_string(provider));
            json_append(aa, json_string(note));
            json_append(aa, ac2_exc_arg(ctx));
            ac2_exc_clear(ctx);
            ac2_log(ctx, "debug", "Auxiliary client: could not load pool for %s%s: %s", aa, false);
            json_free(pool);
            return json_null();
        }
        json_free(pool); return NULL;
    }
    if (ac2_truthy(pool)) {
        json_t *hc = ac2_call(ctx, "pool.has_credentials", json_object());
        if (ac2_exc_pending(ctx)) { json_free(hc); json_free(pool); return NULL; }
        bool has = ac2_truthy(hc);
        json_free(hc);
        if (has) return pool;
    }
    json_free(pool);
    return json_null();
}

/* PoP: ac2_close_quietly @ agent/auxiliary_client.py:_close_quietly */
static json_t *ac2_close_quietly(ac2_ctx *ctx, json_t *in) {
    json_t *target = ac2_get(in, "target");
    json_t *failure_note = ac2_get(in, "failure_note");
    json_t *close = ac2_get(target, "close");
    /* callable(close): present non-null models a bound method; null/absent skips */
    if (!ac2_is_none(close)) {
        json_t *r = ac2_call(ctx, "close", json_object());
        json_free(r);
        if (ac2_exc_pending(ctx)) {
            if (ac2_exc_is_exception(ctx)) {
                ac2_exc_clear(ctx);
                if (ac2_truthy(failure_note)) {
                    json_t *aa = json_array();
                    char *s = ac2_pystr(failure_note);
                    json_append(aa, json_string(s));
                    free(s);
                    ac2_log(ctx, "debug", "Codex auxiliary: %s", aa, true);
                }
            }
        }
    }
    return json_null();
}

/* PoP: ac2_attempt_stream_socket @ agent/auxiliary_client.py:_attempt_stream_socket */
static json_t *ac2_attempt_stream_socket(ac2_ctx *ctx, json_t *in) {
    json_t *stream = ac2_get(in, "stream");
    json_t *response = ac2_get(stream, "response");
    json_t *extensions = ac2_get(response, "extensions");
    json_t *network_stream = NULL;
    if (ac2_is_obj(extensions)) network_stream = json_obj_get(extensions, "network_stream");
    if (!ac2_is_none(network_stream)) {
        json_t *a = json_object();
        ac2_arg(a, "network_stream", network_stream);
        json_t *r = ac2_call(ctx, "_socket_from_stream", a);
        if (ac2_exc_pending(ctx)) { json_free(r); return NULL; }
        return r ? r : json_null();
    }
    return json_null();
}

/* ── header builders ─────────────────────────────────────────────────────── */

/* PoP: ac2_route_client_headers @ agent/auxiliary_client.py:_route_client_headers */
static json_t *ac2_route_client_headers(ac2_ctx *ctx, const char *base_url,
                                        const char *provider, bool is_vision) {
    json_t *a = json_object();
    ac2_arg_str(a, "kind", "client_headers");
    ac2_arg_str(a, "provider", provider ? provider : "");
    ac2_arg_str(a, "base_url", base_url);
    ac2_arg_bool(a, "has_images", is_vision);
    json_t *headers = ac2_call(ctx, "route_answer", a);
    if (ac2_exc_pending(ctx)) { json_free(headers); return NULL; }
    if (ac2_truthy(headers)) return headers;  /* dict(headers) copy == itself */
    json_free(headers);
    return json_null();
}

/* PoP: ac2_profile_default_headers @ agent/auxiliary_client.py:_profile_default_headers */
static json_t *ac2_profile_default_headers(ac2_ctx *ctx, const char *provider) {
    if (!provider || !provider[0]) return json_null();
    json_t *a = json_object();
    ac2_arg_str(a, "provider", provider);
    json_t *profile = ac2_call(ctx, "get_provider_profile", a);
    if (ac2_exc_pending(ctx)) goto suppress;
    if (ac2_truthy(profile)) {
        json_t *dh = ac2_get(profile, "default_headers");
        if (!dh) { ac2_raise(ctx, "AttributeError", ""); goto suppress; }
        if (ac2_truthy(dh)) { json_t *out = json_copy(dh); json_free(profile); return out; }
    }
    json_free(profile);
    return json_null();
suppress:
    json_free(profile);
    if (ac2_exc_is_exception(ctx)) ac2_exc_clear(ctx);
    return json_null();
}

/* PoP: ac2_endpoint_default_headers @ agent/auxiliary_client.py:_endpoint_default_headers */
static json_t *ac2_endpoint_default_headers(ac2_ctx *ctx, json_t *in) {
    const char *base_url = json_get_str(in, "base_url", "");
    const char *provider = json_get_str(in, "provider", "");
    bool is_vision = json_get_bool(in, "is_vision", false);
    bool xai = json_get_bool(in, "xai", false);
    json_t *headers = NULL;
    json_t *a;
    json_t *m;

    a = json_object();
    ac2_arg_str(a, "base_url", base_url);
    ac2_arg_str(a, "host", "api.kimi.com");
    m = ac2_call(ctx, "base_url_host_matches", a);
    if (ac2_exc_pending(ctx)) { json_free(m); return NULL; }
    bool kimi = ac2_truthy(m); json_free(m);
    if (kimi) {
        headers = json_object();
        json_set(headers, "User-Agent", json_string("claude-code/0.1.0"));
    } else {
        json_t *route = ac2_route_client_headers(ctx, base_url, provider, is_vision);
        if (ac2_exc_pending(ctx)) { json_free(route); return NULL; }
        if (ac2_truthy(route)) {
            headers = route;
        } else {
            json_free(route);
            a = json_object();
            ac2_arg_str(a, "base_url", base_url);
            ac2_arg_str(a, "host", "integrate.api.nvidia.com");
            m = ac2_call(ctx, "base_url_host_matches", a);
            if (ac2_exc_pending(ctx)) { json_free(m); return NULL; }
            bool nvidia = ac2_truthy(m); json_free(m);
            if (nvidia) {
                a = json_object();
                ac2_arg_str(a, "base_url", base_url);
                json_t *nh = ac2_call(ctx, "build_nvidia_nim_headers", a);
                if (ac2_exc_pending(ctx)) { json_free(nh); return NULL; }
                headers = nh ? nh : json_object();
            } else if (xai) {
                a = json_object();
                ac2_arg_str(a, "base_url", base_url);
                ac2_arg_str(a, "host", "x.ai");
                m = ac2_call(ctx, "base_url_host_matches", a);
                if (ac2_exc_pending(ctx)) { json_free(m); return NULL; }
                bool xaim = ac2_truthy(m); json_free(m);
                if (xaim) {
                    json_t *xh = ac2_call(ctx, "hermes_xai_default_headers", json_object());
                    if (ac2_exc_pending(ctx)) { json_free(xh); return NULL; }
                    headers = xh ? xh : json_object();
                }
            }
            if (!headers && !ac2_exc_pending(ctx)) {
                /* else branch: _profile_default_headers(provider) or {} */
                json_t *ph = ac2_profile_default_headers(ctx, provider);
                if (ac2_exc_pending(ctx)) { json_free(ph); return NULL; }
                headers = ac2_truthy(ph) ? ph : json_object();
                if (headers != ph) json_free(ph);
            }
        }
    }
    if (ac2_exc_pending(ctx)) { json_free(headers); return NULL; }
    /* return _apply_user_default_headers(headers or None) or None */
    json_t *arg = ac2_truthy(headers) ? json_copy(headers) : json_null();
    json_free(headers);
    a = json_object();
    ac2_arg(a, "headers", arg);
    json_free(arg);
    json_t *res = ac2_call(ctx, "_apply_user_default_headers", a);
    if (ac2_exc_pending(ctx)) { json_free(res); return NULL; }
    if (ac2_truthy(res)) return res;
    json_free(res);
    return json_null();
}

/* PoP: ac2_profile_declared_messages_wire @ agent/auxiliary_client.py:_profile_declared_messages_wire */
static json_t *ac2_profile_declared_messages_wire(ac2_ctx *ctx, json_t *in) {
    json_t *provider = ac2_get(in, "provider");
    char *s = ac2_str_or_empty(provider);
    char *st = ac2_strip(s); free(s);
    char *lo = ac2_lower(st); free(st);
    json_t *a = json_object();
    ac2_arg_str(a, "provider", lo);
    free(lo);
    json_t *profile = ac2_call(ctx, "get_provider_profile", a);
    if (ac2_exc_pending(ctx)) { json_free(profile); return NULL; }
    if (!ac2_is_none(profile)) {
        json_t *am = ac2_get(profile, "api_mode");
        if (!am) { json_free(profile); ac2_raise(ctx, "AttributeError", ""); return NULL; }
        if (ac2_is_str(am) && ac2_str_eq(am->str_val, "anthropic_messages")) {
            json_free(profile);
            return json_string("anthropic_messages");
        }
    }
    json_free(profile);
    return json_null();
}

/* ── routing core ────────────────────────────────────────────────────────── */

/* PoP: ac2_route_client @ agent/auxiliary_client.py:_route_client */
static json_t *ac2_route_client(ac2_ctx *ctx, json_t *req, json_t *client_obj,
                                json_t *final_model_str) {
    bool async_mode = json_get_bool(req, "async_mode", false);
    if (async_mode) {
        bool is_vision = json_get_bool(req, "is_vision", false);
        json_t *a = json_object();
        ac2_arg(a, "client", client_obj);
        ac2_arg(a, "model", final_model_str);
        ac2_arg_bool(a, "is_vision", is_vision);
        json_t *r = ac2_call(ctx, "_to_async_client", a);
        if (ac2_exc_pending(ctx)) { json_free(r); return NULL; }
        return r ? r : json_null();
    }
    return ac2_pair(ac2_copy_or_null(client_obj), ac2_copy_or_null(final_model_str));
}

/* PoP: ac2_route_or_warn @ agent/auxiliary_client.py:_route_or_warn */
static json_t *ac2_route_or_warn(ac2_ctx *ctx, json_t *req, json_t *client,
                                 json_t *def, const char *unavailable_msg, json_t *args) {
    if (ac2_is_none(client)) {
        ac2_log(ctx, "warning", unavailable_msg, args ? json_copy(args) : json_array(), false);
        return ac2_pair(NULL, NULL);
    }
    /* _normalize_resolved_model(req.model or default, req.provider) */
    json_t *model = ac2_get(req, "model");
    json_t *m = ac2_truthy(model) ? model : def;
    const char *provider = json_get_str(req, "provider", "");
    json_t *a = json_object();
    ac2_arg(a, "model", m);
    ac2_arg_str(a, "provider", provider);
    json_t *nm = ac2_call(ctx, "_normalize_resolved_model", a);
    if (ac2_exc_pending(ctx)) { json_free(nm); return NULL; }
    json_t *r = ac2_route_client(ctx, req, client, nm);
    json_free(nm);
    return r;
}

/* PoP: ac2_resolve_openrouter_branch @ agent/auxiliary_client.py:_resolve_openrouter_branch */
static json_t *ac2_resolve_openrouter_branch(ac2_ctx *ctx, json_t *in) {
    json_t *req = ac2_get(in, "req");
    json_t *a = json_object();
    ac2_arg(a, "explicit_api_key", ac2_get(req, "explicit_api_key"));
    ac2_arg(a, "model", ac2_get(req, "model"));
    ac2_arg(a, "explicit_base_url", ac2_get(req, "explicit_base_url"));
    json_t *r = ac2_call(ctx, "_try_openrouter", a);
    if (ac2_exc_pending(ctx)) { json_free(r); return NULL; }
    json_t *client = ac2_t0(r);
    json_t *def = ac2_t1(r);
    if (ac2_is_none(client)) {
        json_t *da = json_object();
        ac2_arg(da, "model", ac2_get(req, "model"));
        json_t *desc = ac2_call(ctx, "_describe_openrouter_unavailable", da);
        if (ac2_exc_pending(ctx)) { json_free(desc); json_free(r); return NULL; }
        json_t *aa = json_array();
        json_append(aa, desc ? desc : json_null());
        ac2_log(ctx, "warning", "resolve_provider_client: openrouter requested but %s", aa, false);
        json_free(r);
        return ac2_pair(NULL, NULL);
    }
    json_t *model = ac2_get(req, "model");
    json_t *m = ac2_truthy(model) ? model : def;
    const char *provider = json_get_str(req, "provider", "");
    a = json_object();
    ac2_arg(a, "model", m);
    ac2_arg_str(a, "provider", provider);
    json_t *nm = ac2_call(ctx, "_normalize_resolved_model", a);
    if (ac2_exc_pending(ctx)) { json_free(nm); json_free(r); return NULL; }
    json_t *out = ac2_route_client(ctx, req, client, nm);
    json_free(nm); json_free(r);
    return out;
}

/* PoP: ac2_resolve_nous_branch @ agent/auxiliary_client.py:_resolve_nous_branch */
static json_t *ac2_resolve_nous_branch(ac2_ctx *ctx, json_t *in) {
    json_t *req = ac2_get(in, "req");
    json_t *model = ac2_get(req, "model");
    bool is_vision = json_get_bool(req, "is_vision", false);
    /* vision = is_vision or model in _PROVIDER_VISION_MODELS.values() or
     *          (model or "").strip().lower() == "mimo-v2-omni" */
    bool vision = is_vision;
    if (!vision) {
        static const char *VALUES[] = {"mimo-v2.5", "glm-5.3-flash"};
        const char *ms = ac2_jstr(model);
        for (size_t i = 0; i < 2; i++)
            if (ac2_is_str(model) && ac2_str_eq(ms, VALUES[i])) { vision = true; break; }
    }
    if (!vision) {
        char *s = ac2_str_or_empty(model);
        char *st = ac2_strip(s); free(s);
        char *lo = ac2_lower(st); free(st);
        if (ac2_str_eq(lo, "mimo-v2-omni")) vision = true;
        free(lo);
    }
    json_t *a = json_object();
    ac2_arg_bool(a, "vision", vision);
    json_t *r = ac2_call(ctx, "_try_nous", a);
    if (ac2_exc_pending(ctx)) { json_free(r); return NULL; }
    json_t *client = ac2_t0(r);
    json_t *def = ac2_t1(r);
    if (ac2_is_none(client)) {
        ac2_log(ctx, "warning",
                "resolve_provider_client: nous requested but Nous Portal not configured (run: hermes auth)",
                json_array(), false);
        json_free(r);
        return ac2_pair(NULL, NULL);
    }
    json_t *m = ac2_truthy(model) ? model : def;
    const char *provider = json_get_str(req, "provider", "");
    a = json_object();
    ac2_arg(a, "model", m);
    ac2_arg_str(a, "provider", provider);
    json_t *final_model = ac2_call(ctx, "_normalize_resolved_model", a);
    if (ac2_exc_pending(ctx)) { json_free(final_model); json_free(r); return NULL; }
    /* nous_api_mode(final_model) */
    a = json_object();
    ac2_arg(a, "model", final_model);
    json_t *wire = ac2_call(ctx, "nous_api_mode", a);
    if (ac2_exc_pending(ctx)) { json_free(wire); json_free(final_model); json_free(r); return NULL; }
    /* _maybe_wrap_anthropic(client, final_model, str(api_key or ""), str(base_url or ""), wire) */
    char *ak = ac2_str_or_empty(ac2_get(client, "api_key"));
    char *bu = ac2_str_or_empty(ac2_get(client, "base_url"));
    a = json_object();
    ac2_arg(a, "client", client);
    ac2_arg(a, "model", final_model);
    ac2_arg_str(a, "api_key", ak);
    ac2_arg_str(a, "base_url", bu);
    ac2_arg(a, "api_mode", wire);
    free(ak); free(bu);
    json_t *wrapped = ac2_call(ctx, "_maybe_wrap_anthropic", a);
    json_free(wire);
    if (ac2_exc_pending(ctx)) { json_free(wrapped); json_free(final_model); json_free(r); return NULL; }
    json_t *out = ac2_route_client(ctx, req, wrapped, final_model);
    json_free(wrapped); json_free(final_model); json_free(r);
    return out;
}

/* PoP: ac2_resolve_xai_oauth_branch @ agent/auxiliary_client.py:_resolve_xai_oauth_branch */
static json_t *ac2_resolve_xai_oauth_branch(ac2_ctx *ctx, json_t *in) {
    json_t *req = ac2_get(in, "req");
    json_t *a = json_object();
    ac2_arg(a, "model", ac2_get(req, "model"));
    json_t *r = ac2_call(ctx, "_build_xai_oauth_aux_client", a);
    if (ac2_exc_pending(ctx)) { json_free(r); return NULL; }
    json_t *client = ac2_t0(r);
    json_t *def = ac2_t1(r);
    /* em dash U+2014 kept as UTF-8 bytes (ASCII source). */
    json_t *out = ac2_route_or_warn(ctx, req, client, def,
        "resolve_provider_client: xai-oauth requested but no xAI "
        "OAuth token found (run: hermes model -> xAI Grok OAuth \xe2\x80\x94 SuperGrok / Premium+)",
        NULL);
    json_free(r);
    return out;
}

/* PoP: ac2_resolve_azure_foundry_branch @ agent/auxiliary_client.py:_resolve_azure_foundry_branch */
static json_t *ac2_resolve_azure_foundry_branch(ac2_ctx *ctx, json_t *in) {
    json_t *req = ac2_get(in, "req");
    json_t *a = json_object();
    ac2_arg(a, "model", ac2_get(req, "model"));
    ac2_arg(a, "explicit_api_key", ac2_get(req, "explicit_api_key"));
    ac2_arg(a, "explicit_base_url", ac2_get(req, "explicit_base_url"));
    ac2_arg(a, "api_mode", ac2_get(req, "api_mode"));
    json_t *r = ac2_call(ctx, "_try_azure_foundry", a);
    if (ac2_exc_pending(ctx)) { json_free(r); return NULL; }
    json_t *client = ac2_t0(r);
    json_t *def = ac2_t1(r);
    json_t *out = ac2_route_or_warn(ctx, req, client, def,
        "resolve_provider_client: azure-foundry requested but "
        "runtime resolution failed (run: hermes doctor for diagnostics)", NULL);
    json_free(r);
    return out;
}

/* PoP: ac2_resolve_openai_codex_branch @ agent/auxiliary_client.py:_resolve_openai_codex_branch */
static json_t *ac2_resolve_openai_codex_branch(ac2_ctx *ctx, json_t *in) {
    json_t *req = ac2_get(in, "req");
    json_t *model = ac2_get(req, "model");
    const char *no_token_msg =
        "resolve_provider_client: openai-codex requested but no Codex OAuth token found (run: hermes model)";
    if (!ac2_truthy(model)) {
        ac2_log(ctx, "warning",
                "resolve_provider_client: openai-codex requested without a "
                "model; pass model explicitly (e.g. model.model in config.yaml "
                "or auxiliary.<task>.model for per-task aux routing).", json_array(), false);
        return ac2_pair(NULL, NULL);
    }
    bool raw_codex = json_get_bool(req, "raw_codex", false);
    const char *provider = json_get_str(req, "provider", "");
    if (raw_codex) {
        json_t *a = json_object();
        ac2_arg(a, "model", model);
        json_t *cb = ac2_call(ctx, "_resolve_codex_credential_and_base", a);
        if (ac2_exc_pending(ctx)) { json_free(cb); return NULL; }
        json_t *codex_token = ac2_t0(cb);
        json_t *base_url = ac2_t1(cb);
        if (!ac2_truthy(codex_token)) {
            ac2_log(ctx, "warning", no_token_msg, json_array(), false);
            json_free(cb);
            return ac2_pair(NULL, NULL);
        }
        a = json_object();
        ac2_arg(a, "token", codex_token);
        ac2_arg(a, "base_url", base_url);
        json_t *cf = ac2_call(ctx, "_codex_cloudflare_headers", a);
        if (ac2_exc_pending(ctx)) { json_free(cf); json_free(cb); return NULL; }
        a = json_object();
        ac2_arg(a, "api_key", codex_token);
        ac2_arg(a, "base_url", base_url);
        ac2_arg(a, "default_headers", cf);
        json_t *raw_client = ac2_call(ctx, "_create_openai_client", a);
        json_free(cf);
        if (ac2_exc_pending(ctx)) { json_free(raw_client); json_free(cb); return NULL; }
        a = json_object();
        ac2_arg(a, "model", model);
        ac2_arg_str(a, "provider", provider);
        json_t *nm = ac2_call(ctx, "_normalize_resolved_model", a);
        if (ac2_exc_pending(ctx)) { json_free(nm); json_free(raw_client); json_free(cb); return NULL; }
        json_t *out = ac2_pair(raw_client, nm);
        json_free(cb);
        return out;
    }
    json_t *a = json_object();
    ac2_arg(a, "model", model);
    json_t *r = ac2_call(ctx, "_build_codex_client", a);
    if (ac2_exc_pending(ctx)) { json_free(r); return NULL; }
    json_t *client = ac2_t0(r);
    json_t *def = ac2_t1(r);
    json_t *out = ac2_route_or_warn(ctx, req, client, def, no_token_msg, NULL);
    json_free(r);
    return out;
}

/* ── predicates / logging / state ────────────────────────────────────────── */

/* PoP: ac2_discovery_chain_allowed @ agent/auxiliary_client.py:_discovery_chain_allowed */
static json_t *ac2_discovery_chain_allowed(ac2_ctx *ctx, json_t *in) {
    json_t *main_provider = ac2_get(in, "main_provider");
    json_t *task = ac2_get(in, "task");
    char *s = ac2_str_or_empty(main_provider);
    char *st = ac2_strip(s); free(s);
    char *lo = ac2_lower(st); free(st);
    bool allowed = ac2_str_eq(lo, "") || ac2_str_eq(lo, "auto");
    free(lo);
    if (allowed) return json_bool(true);
    char *ts = ac2_str_or_empty(task);
    const char *tlabel = (task && ac2_truthy(task)) ? ts : "call";
    json_t *aa = json_array();
    json_append(aa, json_string(tlabel));
    char *ps = ac2_pystr(main_provider);
    json_append(aa, json_string(ps));
    free(ps);
    ac2_log(ctx, "warning",
            "Auxiliary %s: main provider %s is unavailable and no fallback_chain / fallback_providers is "
            "configured \xe2\x80\x94 refusing to guess another logged-in provider. Re-authenticate (`hermes model`) "
            "or declare a fallback.", aa, false);
    free(ts);
    return json_bool(false);
}

/* PoP: ac2_log_once_debug @ agent/auxiliary_client.py:_log_once_debug */
static json_t *ac2_log_once_debug(ac2_ctx *ctx, json_t *in, json_t *seen) {
    json_t *key = ac2_get(in, "key");
    const char *msg = json_get_str(in, "msg", "");
    json_t *args = ac2_get(in, "args");
    const char *ks = ac2_jstr(key);
    bool present = ac2_is_obj(seen) && json_has(seen, ks);
    if (!present) {
        json_set(seen, ks, json_bool(true));
        json_t *aa = json_array();
        if (ac2_is_arr(args))
            for (size_t i = 0; i < args->c.count; i++) json_append(aa, json_copy(args->c.items[i]));
        ac2_log(ctx, "debug", msg, aa, false);
    }
    return json_null();
}

/* PoP: ac2_warn_stale_openai_base_url @ agent/auxiliary_client.py:_warn_stale_openai_base_url */
static json_t *ac2_warn_stale_openai_base_url(ac2_ctx *ctx, json_t *in, bool *warned) {
    if (*warned) return json_null();
    json_t *runtime_provider = ac2_get(in, "runtime_provider");
    json_t *a = json_object();
    ac2_arg_str(a, "name", "OPENAI_BASE_URL");
    ac2_arg_str(a, "default", "");
    json_t *envb = ac2_call(ctx, "os.getenv", a);
    if (ac2_exc_pending(ctx)) { json_free(envb); return NULL; }
    if (ac2_is_none(envb)) { json_free(envb); ac2_raise(ctx, "AttributeError", ""); return NULL; }
    char *env_base = ac2_strip(ac2_jstr(envb));
    json_free(envb);
    /* _cfg_provider = runtime_provider or _read_main_provider() */
    json_t *cfg_provider = runtime_provider;
    json_t *rmp = NULL;
    if (!ac2_truthy(runtime_provider)) {
        rmp = ac2_call(ctx, "_read_main_provider", json_object());
        if (ac2_exc_pending(ctx)) { free(env_base); json_free(rmp); return NULL; }
        cfg_provider = rmp;
    }
    const char *cp = ac2_jstr(cfg_provider);
    bool cond = env_base[0] != '\0' && ac2_truthy(cfg_provider)
                && !ac2_str_eq(cp, "custom") && strncmp(cp, "custom:", 7) != 0;
    if (cond) {
        json_t *aa = json_array();
        json_append(aa, json_string(env_base));
        json_append(aa, json_string(cp));
        ac2_log(ctx, "warning",
                "OPENAI_BASE_URL is set (%s) but model.provider is '%s'. "
                "Auxiliary clients may route to the wrong endpoint. "
                "Run: hermes model to reconfigure, or remove "
                "OPENAI_BASE_URL from ~/.hermes/.env", aa, false);
        *warned = true;
    }
    free(env_base);
    json_free(rmp);
    return json_null();
}

/* PoP: ac2_is_actual_auxiliary_route @ agent/auxiliary_client.py:_is_actual_auxiliary_route */
static json_t *ac2_is_actual_auxiliary_route(ac2_ctx *ctx, json_t *in) {
    json_t *req = ac2_get(in, "req");
    const char *base_url = json_get_str(in, "base_url", "");
    const char *provider = json_get_str(req, "provider", "");
    json_t *a = json_object();
    ac2_arg_str(a, "provider", provider);
    ac2_arg_str(a, "base_url", base_url);
    json_t *ar = ac2_call(ctx, "is_actual_route", a);
    if (ac2_exc_pending(ctx)) { json_free(ar); return NULL; }
    if (ac2_truthy(ar)) { json_free(ar); return json_bool(true); }
    json_free(ar);
    json_t *nra = json_object();
    ac2_arg(nra, "main_runtime", ac2_get(req, "main_runtime"));
    json_t *runtime = ac2_call(ctx, "_normalize_main_runtime", nra);
    if (ac2_exc_pending(ctx)) { json_free(runtime); return NULL; }
    if (ac2_is_none(runtime)) { json_free(runtime); ac2_raise(ctx, "AttributeError", ""); return NULL; }
    /* bool(base_url and is_actual_route(runtime.provider, runtime.base_url) and norm==norm) */
    if (!base_url[0]) { json_free(runtime); return json_bool(false); }
    json_t *rt_provider = ac2_get_def(runtime, "provider", NULL);
    json_t *rt_base = ac2_get_def(runtime, "base_url", NULL);
    a = json_object();
    ac2_arg_str(a, "provider", ac2_jstr(rt_provider));
    ac2_arg_str(a, "base_url", ac2_jstr(rt_base));
    json_t *ar2 = ac2_call(ctx, "is_actual_route", a);
    if (ac2_exc_pending(ctx)) { json_free(ar2); json_free(runtime); return NULL; }
    if (!ac2_truthy(ar2)) { json_free(ar2); json_free(runtime); return json_bool(false); }
    json_free(ar2);
    /* normalize_route_base_url(normalize_actual_base_url(base_url)) */
    a = json_object(); ac2_arg_str(a, "base_url", base_url);
    json_t *n1 = ac2_call(ctx, "normalize_actual_base_url", a);
    if (ac2_exc_pending(ctx)) { json_free(n1); json_free(runtime); return NULL; }
    a = json_object(); ac2_arg(a, "base_url", n1);
    json_t *r1 = ac2_call(ctx, "normalize_route_base_url", a);
    json_free(n1);
    if (ac2_exc_pending(ctx)) { json_free(r1); json_free(runtime); return NULL; }
    a = json_object(); ac2_arg_str(a, "base_url", ac2_jstr(rt_base));
    json_t *n2 = ac2_call(ctx, "normalize_actual_base_url", a);
    if (ac2_exc_pending(ctx)) { json_free(n2); json_free(r1); json_free(runtime); return NULL; }
    a = json_object(); ac2_arg(a, "base_url", n2);
    json_t *r2 = ac2_call(ctx, "normalize_route_base_url", a);
    json_free(n2);
    if (ac2_exc_pending(ctx)) { json_free(r2); json_free(r1); json_free(runtime); return NULL; }
    bool eq = ac2_json_eq(r1, r2);
    json_free(r1); json_free(r2); json_free(runtime);
    return json_bool(eq);
}

/* ── main runtime / config readers ───────────────────────────────────────── */

/* PoP: ac2_main_route_target @ agent/auxiliary_client.py:_main_route_target */
static json_t *ac2_main_route_target(ac2_ctx *ctx, json_t *in) {
    json_t *runtime = ac2_get(in, "runtime");
    json_t *task = ac2_get(in, "task");
    if (ac2_is_none(runtime)) { ac2_raise(ctx, "AttributeError", ""); return NULL; }
    json_t *nullnode = json_null();
    json_t *emptystr = json_string("");
    /* main_provider = str(runtime.get("provider","") or _read_main_provider() or "") */
    json_t *rp = ac2_get_def(runtime, "provider", emptystr);
    json_t *rmp = NULL;
    json_t *prov_node = rp;
    if (!ac2_truthy(rp)) {
        rmp = ac2_call(ctx, "_read_main_provider", json_object());
        if (ac2_exc_pending(ctx)) { json_free(rmp); json_free(nullnode); json_free(emptystr); return NULL; }
        prov_node = ac2_truthy(rmp) ? rmp : emptystr;
    }
    char *main_provider = ac2_pystr(prov_node);
    /* main_model = str(runtime.get("model") or _read_main_model() or "") */
    json_t *rm = ac2_get_def(runtime, "model", nullnode);
    json_t *rmm = NULL;
    json_t *model_node = rm;
    if (!ac2_truthy(rm)) {
        rmm = ac2_call(ctx, "_read_main_model", json_object());
        if (ac2_exc_pending(ctx)) { free(main_provider); json_free(rmm); json_free(rmp); json_free(nullnode); json_free(emptystr); return NULL; }
        model_node = ac2_truthy(rmm) ? rmm : emptystr;
    }
    char *main_model = ac2_pystr(model_node);
    char *runtime_base_url = ac2_pystr(ac2_truthy(ac2_get_def(runtime, "base_url", nullnode))
                                       ? ac2_get_def(runtime, "base_url", nullnode) : emptystr);
    json_t *runtime_api_key = ac2_copy_or_null(ac2_get_def(runtime, "api_key", emptystr));
    char *runtime_api_mode = ac2_pystr(ac2_truthy(ac2_get_def(runtime, "api_mode", nullnode))
                                       ? ac2_get_def(runtime, "api_mode", nullnode) : emptystr);
    json_free(rmp); json_free(rmm);
    /* fast-model opt-in */
    json_t *tpa = json_object();
    ac2_arg(tpa, "task", task);
    json_t *tp = ac2_call(ctx, "_task_prefers_fast_model", tpa);
    if (ac2_exc_pending(ctx)) { goto fail; }
    bool prefers = ac2_truthy(tp); json_free(tp);
    if (prefers && main_provider[0] && !ac2_str_eq(main_provider, "auto")) {
        json_t *a = json_object();
        ac2_arg(a, "runtime", runtime);
        ac2_arg_str(a, "provider", main_provider);
        json_t *cat = ac2_call(ctx, "fast_catalog_provider", a);
        if (ac2_exc_pending(ctx)) { json_free(cat); goto fail; }
        a = json_object();
        ac2_arg(a, "provider", cat);
        ac2_arg_bool(a, "prefer_fast", true);
        json_t *fm = ac2_call(ctx, "_get_aux_model_for_provider", a);
        json_free(cat);
        if (ac2_exc_pending(ctx)) { json_free(fm); goto fail; }
        if (ac2_truthy(fm) && !ac2_str_eq(ac2_jstr(fm), main_model)) {
            json_t *aa = json_array();
            char *ts = ac2_pystr(task); json_append(aa, json_string(ts)); free(ts);
            char *fs = ac2_pystr(fm); json_append(aa, json_string(fs)); free(fs);
            json_append(aa, json_string(main_model));
            ac2_log(ctx, "debug", "Auxiliary task %s: preferring fast model %s over main model %s", aa, false);
            free(main_model);
            main_model = ac2_pystr(fm);
        }
        json_free(fm);
    }
    /* MoA substitution */
    if (ac2_str_eq(main_provider, "moa")) {
        json_t *a = json_object();
        ac2_arg_str(a, "preset_name", main_model);
        json_t *agg = ac2_call(ctx, "_resolve_moa_aggregator", a);
        if (ac2_exc_pending(ctx)) { json_free(agg); goto fail; }
        json_t *ap = ac2_t0(agg);
        json_t *am = ac2_t1(agg);
        if (ac2_truthy(ap) && ac2_truthy(am)) {
            free(main_provider); main_provider = ac2_pystr(ap);
            free(main_model); main_model = ac2_pystr(am);
            free(runtime_base_url); runtime_base_url = strdup("");
            json_free(runtime_api_key); runtime_api_key = json_string("");
            free(runtime_api_mode); runtime_api_mode = strdup("");
        }
        json_free(agg);
    }
    json_free(nullnode); json_free(emptystr);
    json_t *out = json_array();
    json_append(out, json_string(main_provider));
    json_append(out, json_string(main_model));
    json_append(out, json_string(runtime_base_url));
    json_append(out, runtime_api_key);
    json_append(out, json_string(runtime_api_mode));
    free(main_provider); free(main_model); free(runtime_base_url); free(runtime_api_mode);
    return out;
fail:
    free(main_provider); free(main_model); free(runtime_base_url); free(runtime_api_mode);
    json_free(runtime_api_key); json_free(nullnode); json_free(emptystr);
    return NULL;
}

/* PoP: ac2_read_main_field @ agent/auxiliary_client.py:_read_main_field */
static json_t *ac2_read_main_field(ac2_ctx *ctx, json_t *in) {
    const char *field = json_get_str(in, "field", "");
    bool readonly = json_get_bool(in, "readonly", false);
    bool lower = json_get_bool(in, "lower", false);
    json_t *a = json_object();
    ac2_arg_str(a, "field", field);
    json_t *override = ac2_call(ctx, "_runtime_main_value", a);
    if (ac2_exc_pending(ctx)) { json_free(override); return NULL; }
    if (ac2_is_str(override)) {
        char *st = ac2_strip(override->str_val);
        if (st[0]) {
            char *val = lower ? ac2_lower(st) : strdup(st);
            free(st); json_free(override);
            json_t *out = json_string(val); free(val);
            return out;
        }
        free(st);
    }
    json_free(override);
    /* with suppress(Exception): import config; cfg = loader(); ... */
    if (json_get_bool(ctx->env, "config_import_fails", false)) {
        ac2_raise(ctx, "ImportError", "");
        goto suppress;
    }
    json_t *cfg = ac2_call(ctx, readonly ? "load_config_readonly" : "load_config", json_object());
    if (ac2_exc_pending(ctx)) { json_free(cfg); goto suppress; }
    if (ac2_is_none(cfg)) { json_free(cfg); ac2_raise(ctx, "AttributeError", ""); goto suppress; }
    json_t *model_cfg = ac2_get_def(cfg, "model", NULL);
    if (!model_cfg) model_cfg = json_object();  /* default {} */
    bool mc_owned = !ac2_get(cfg, "model");
    if (ac2_str_eq(field, "model") && ac2_is_str(model_cfg)) {
        char *st = ac2_strip(model_cfg->str_val);
        if (st[0]) {
            json_t *out = json_string(st); free(st);
            if (mc_owned) json_free(model_cfg);
            json_free(cfg);
            return out;
        }
        free(st);
    }
    if (ac2_is_obj(model_cfg)) {
        const char *key = ac2_str_eq(field, "model") ? "default" : field;
        json_t *value = ac2_get_def(model_cfg, key, NULL);
        if (ac2_is_str(value)) {
            char *st = ac2_strip(value->str_val);
            if (st[0]) {
                char *val = lower ? ac2_lower(st) : strdup(st);
                free(st);
                json_t *out = json_string(val); free(val);
                if (mc_owned) json_free(model_cfg);
                json_free(cfg);
                return out;
            }
            free(st);
        }
    }
    if (mc_owned) json_free(model_cfg);
    json_free(cfg);
    return json_string("");
suppress:
    if (ac2_exc_is_exception(ctx)) ac2_exc_clear(ctx);
    return json_string("");
}

/* PoP: ac2_read_main_api_key_if_same_origin @ agent/auxiliary_client.py:_read_main_api_key_if_same_origin */
static json_t *ac2_read_main_api_key_if_same_origin(ac2_ctx *ctx, const char *aux_base_url) {
    json_t *a = json_object();
    ac2_arg_str(a, "base_url", aux_base_url);
    json_t *aux_origin = ac2_call(ctx, "base_url_origin", a);
    if (ac2_exc_pending(ctx)) { json_free(aux_origin); return NULL; }
    json_t *host = ac2_t1(aux_origin);
    if (!ac2_truthy(host)) { json_free(aux_origin); return json_string(""); }
    /* _normalize_main_runtime(None): None arg dropped -> {} */
    json_t *live = ac2_call(ctx, "_normalize_main_runtime", json_object());
    if (ac2_exc_pending(ctx)) { json_free(live); json_free(aux_origin); return NULL; }
    if (ac2_is_none(live)) { json_free(live); json_free(aux_origin); ac2_raise(ctx, "AttributeError", ""); return NULL; }
    json_t *emptystr = json_string("");
    json_t *live_base = ac2_get_def(live, "base_url", emptystr);
    json_t *live_key = ac2_get_def(live, "api_key", emptystr);
    if (ac2_truthy(live_base) || ac2_truthy(live_key)) {
        a = json_object();
        ac2_arg_str(a, "base_url", ac2_jstr(live_base));
        json_t *lo = ac2_call(ctx, "base_url_origin", a);
        if (ac2_exc_pending(ctx)) { json_free(lo); json_free(live); json_free(aux_origin); json_free(emptystr); return NULL; }
        bool eq = ac2_json_eq(aux_origin, lo);
        json_free(lo);
        json_free(aux_origin); json_free(emptystr);
        if (!eq) { json_free(live); return json_string(""); }
        json_t *out = ac2_copy_or_null(live_key);
        json_free(live);
        return out;
    }
    json_t *rmb = ac2_call(ctx, "_read_main_base_url", json_object());
    if (ac2_exc_pending(ctx)) { json_free(rmb); json_free(live); json_free(aux_origin); json_free(emptystr); return NULL; }
    a = json_object();
    ac2_arg_str(a, "base_url", ac2_jstr(rmb));
    json_t *co = ac2_call(ctx, "base_url_origin", a);
    json_free(rmb);
    if (ac2_exc_pending(ctx)) { json_free(co); json_free(live); json_free(aux_origin); json_free(emptystr); return NULL; }
    bool eq = ac2_json_eq(aux_origin, co);
    json_free(co); json_free(aux_origin); json_free(emptystr); json_free(live);
    if (!eq) return json_string("");
    json_t *key = ac2_call(ctx, "_read_main_api_key", json_object());
    if (ac2_exc_pending(ctx)) { json_free(key); return NULL; }
    return key ? key : json_null();
}

/* PoP: ac2_borrowed_main_credential_key @ agent/auxiliary_client.py:_borrowed_main_credential_key */
static json_t *ac2_borrowed_main_credential_key(ac2_ctx *ctx, json_t *in) {
    const char *provider = json_get_str(in, "provider", "");
    json_t *base_url = ac2_get(in, "base_url");
    json_t *api_key = ac2_get(in, "api_key");
    json_t *runtime = ac2_get(in, "runtime");
    if (!runtime) runtime = json_object();
    bool rt_owned = !ac2_get(in, "runtime");
    json_t *a = json_object();
    ac2_arg_str(a, "provider", provider);
    json_t *np = ac2_call(ctx, "_normalize_aux_provider", a);
    if (ac2_exc_pending(ctx)) { json_free(np); if (rt_owned) json_free(runtime); return NULL; }
    bool is_custom = ac2_is_str(np) && ac2_str_eq(np->str_val, "custom");
    json_free(np);
    if (!is_custom) { if (rt_owned) json_free(runtime); return json_array(); }
    json_t *emptystr = json_string("");
    if (!ac2_truthy(base_url)) {
        json_t *rb = ac2_get_def(runtime, "base_url", emptystr);
        json_t *rk = ac2_get_def(runtime, "api_key", emptystr);
        a = json_object();
        ac2_arg_str(a, "field", "api_key");
        ac2_arg(a, "value", rk);
        json_t *disc = ac2_call(ctx, "_runtime_cache_discriminator", a);
        if (ac2_exc_pending(ctx)) { json_free(disc); json_free(emptystr); if (rt_owned) json_free(runtime); return NULL; }
        json_t *out = ac2_pair(ac2_copy_or_null(rb), disc);
        json_free(emptystr); if (rt_owned) json_free(runtime);
        return out;
    }
    a = json_object();
    ac2_arg(a, "api_key", api_key);
    json_t *nak = ac2_call(ctx, "_normalize_api_key", a);
    if (ac2_exc_pending(ctx)) { json_free(nak); json_free(emptystr); if (rt_owned) json_free(runtime); return NULL; }
    if (ac2_truthy(nak)) { json_free(nak); json_free(emptystr); if (rt_owned) json_free(runtime); return json_array(); }
    json_free(nak);
    a = json_object();
    ac2_arg(a, "base_url", base_url);
    json_t *tob = ac2_call(ctx, "_to_openai_base_url", a);
    if (ac2_exc_pending(ctx)) { json_free(tob); json_free(emptystr); if (rt_owned) json_free(runtime); return NULL; }
    char *custom_base = ac2_strip(ac2_jstr(tob));
    json_free(tob);
    /* borrowed = _read_main_api_key_if_same_origin(custom_base) or
     *            declared_custom_credential_identity(base_url, provider) or
     *            declared_custom_credential_identity(custom_base, provider) */
    json_t *borrowed = ac2_read_main_api_key_if_same_origin(ctx, custom_base);
    if (ac2_exc_pending(ctx)) { free(custom_base); json_free(borrowed); json_free(emptystr); if (rt_owned) json_free(runtime); return NULL; }
    if (!ac2_truthy(borrowed)) {
        json_free(borrowed);
        a = json_object();
        ac2_arg(a, "base_url", base_url);
        ac2_arg_str(a, "provider", provider);
        borrowed = ac2_call(ctx, "declared_custom_credential_identity", a);
        if (ac2_exc_pending(ctx)) { free(custom_base); json_free(borrowed); json_free(emptystr); if (rt_owned) json_free(runtime); return NULL; }
    }
    if (!ac2_truthy(borrowed)) {
        json_free(borrowed);
        a = json_object();
        ac2_arg_str(a, "base_url", custom_base);
        ac2_arg_str(a, "provider", provider);
        borrowed = ac2_call(ctx, "declared_custom_credential_identity", a);
        if (ac2_exc_pending(ctx)) { free(custom_base); json_free(borrowed); json_free(emptystr); if (rt_owned) json_free(runtime); return NULL; }
    }
    a = json_object();
    ac2_arg_str(a, "field", "api_key");
    ac2_arg(a, "value", borrowed);
    json_t *disc = ac2_call(ctx, "_runtime_cache_discriminator", a);
    json_free(borrowed);
    if (ac2_exc_pending(ctx)) { free(custom_base); json_free(disc); json_free(emptystr); if (rt_owned) json_free(runtime); return NULL; }
    json_t *out = json_array();
    json_append(out, disc ? disc : json_null());
    free(custom_base); json_free(emptystr); if (rt_owned) json_free(runtime);
    return out;
}

/* ── semaphore cache ─────────────────────────────────────────────────────── */

/* PoP: ac2_cached_semaphore @ agent/auxiliary_client.py:_cached_semaphore */
static json_t *ac2_cached_semaphore(ac2_ctx *ctx, json_t *in, json_t *store) {
    const char *key = json_get_str(in, "key", "");
    double limit = json_get_num(in, "limit", 0);
    json_t *entry = ac2_get(store, key);
    bool rebuild = ac2_is_none(entry);
    if (!rebuild) {
        json_t *e0 = ac2_t0(entry);
        rebuild = !(ac2_is_num(e0) && e0->num_val == limit);
    }
    if (rebuild) {
        json_t *a = json_object();
        json_set(a, "limit", json_number(limit));
        json_t *sem = ac2_call(ctx, "factory", a);
        if (ac2_exc_pending(ctx)) { json_free(sem); return NULL; }
        json_t *ne = ac2_pair(json_number(limit), sem);
        json_set(store, key, ne);
        entry = ac2_get(store, key);
    }
    json_t *e1 = ac2_t1(entry);
    return ac2_copy_or_null(e1);
}

/* ── isinstance predicate ────────────────────────────────────────────────── */

/* PoP: ac2_async_client_streams_internally @ agent/auxiliary_client.py:_async_client_streams_internally */
static json_t *ac2_async_client_streams_internally(ac2_ctx *ctx, json_t *in) {
    (void)ctx;
    json_t *client = ac2_get(in, "client");
    if (ac2_is_none(client)) return json_bool(false);
    const char *kind = json_get_str(client, "kind", "");
    static const char *K[] = {"AsyncCodexAuxiliaryClient", "AsyncAnthropicAuxiliaryClient",
                              "AsyncBedrockAuxiliaryClient", "SubAsyncCodex"};
    for (size_t i = 0; i < sizeof(K) / sizeof(K[0]); i++)
        if (ac2_str_eq(kind, K[i])) return json_bool(true);
    return json_bool(false);
}

/* ── credential refresh ──────────────────────────────────────────────────── */

/* PoP: ac2_refresh_codex_credentials @ agent/auxiliary_client.py:_refresh_codex_credentials */
static json_t *ac2_refresh_codex_credentials(ac2_ctx *ctx, json_t *in) {
    (void)in;
    json_t *a = json_object();
    ac2_arg_bool(a, "force_refresh", true);
    json_t *creds = ac2_call(ctx, "resolve_codex_runtime_credentials", a);
    if (ac2_exc_pending(ctx)) { json_free(creds); return NULL; }
    a = json_object();
    ac2_arg(a, "creds", creds);
    json_t *r = ac2_call(ctx, "_creds_have_api_key", a);
    json_free(creds);
    if (ac2_exc_pending(ctx)) { json_free(r); return NULL; }
    return r ? r : json_null();
}

/* PoP: ac2_refresh_nous_credentials @ agent/auxiliary_client.py:_refresh_nous_credentials */
static json_t *ac2_refresh_nous_credentials(ac2_ctx *ctx, json_t *in) {
    (void)in;
    json_t *a = json_object();
    ac2_arg_str(a, "name", "HERMES_NOUS_TIMEOUT_SECONDS");
    json_set(a, "default", json_number(15));
    json_t *timeout = ac2_call(ctx, "env_float", a);
    if (ac2_exc_pending(ctx)) { json_free(timeout); return NULL; }
    a = json_object();
    ac2_arg(a, "timeout_seconds", timeout);
    ac2_arg_bool(a, "force_refresh", true);
    json_t *creds = ac2_call(ctx, "resolve_nous_runtime_credentials", a);
    json_free(timeout);
    if (ac2_exc_pending(ctx)) { json_free(creds); return NULL; }
    a = json_object();
    ac2_arg(a, "creds", creds);
    json_t *r = ac2_call(ctx, "_creds_have_api_key", a);
    json_free(creds);
    if (ac2_exc_pending(ctx)) { json_free(r); return NULL; }
    return r ? r : json_null();
}

/* PoP: ac2_refresh_anthropic_credentials @ agent/auxiliary_client.py:_refresh_anthropic_credentials */
static json_t *ac2_refresh_anthropic_credentials(ac2_ctx *ctx, json_t *in) {
    const char *token = json_get_str(in, "failed_api_key", "");
    if (!token[0]) return json_bool(false);
    json_t *a = json_object();
    ac2_arg_str(a, "provider", "anthropic");
    json_t *pool = ac2_call(ctx, "load_pool", a);
    if (ac2_exc_pending(ctx)) { json_free(pool); return NULL; }
    if (ac2_is_none(pool)) { json_free(pool); ac2_raise(ctx, "AttributeError", ""); return NULL; }
    a = json_object();
    ac2_arg_str(a, "api_key", token);
    json_t *eid = ac2_call(ctx, "pool.entry_id_for_api_key", a);
    if (ac2_exc_pending(ctx)) { json_free(eid); json_free(pool); return NULL; }
    if (ac2_truthy(eid)) {
        json_free(eid);
        a = json_object();
        ac2_arg_str(a, "api_key_hint", token);
        json_t *ref = ac2_call(ctx, "pool.try_refresh_matching", a);
        if (ac2_exc_pending(ctx)) { json_free(ref); json_free(pool); return NULL; }
        bool ok = !ac2_is_none(ref);
        json_free(ref); json_free(pool);
        return json_bool(ok);
    }
    json_free(eid); json_free(pool);
    json_t *creds = ac2_call(ctx, "read_claude_code_credentials", json_object());
    if (ac2_exc_pending(ctx)) { json_free(creds); return NULL; }
    if (ac2_is_obj(creds)) {
        json_t *at = ac2_get(creds, "accessToken");
        json_t *rt = ac2_get(creds, "refreshToken");
        if (ac2_is_str(at) && ac2_str_eq(at->str_val, token) && ac2_truthy(rt)) {
            a = json_object();
            ac2_arg(a, "creds", creds);
            json_t *ref = ac2_call(ctx, "_refresh_oauth_token", a);
            json_free(creds);
            if (ac2_exc_pending(ctx)) { json_free(ref); return NULL; }
            bool ok = ac2_truthy(ref);
            json_free(ref);
            return json_bool(ok);
        }
    }
    json_free(creds);
    return json_bool(false);
}

/* PoP: ac2_refresh_xai_oauth_credentials @ agent/auxiliary_client.py:_refresh_xai_oauth_credentials */
static json_t *ac2_refresh_xai_oauth_credentials(ac2_ctx *ctx, json_t *in) {
    (void)in;
    json_t *a = json_object();
    ac2_arg_str(a, "provider", "xai-oauth");
    json_t *pool = ac2_call(ctx, "load_pool", a);
    if (ac2_exc_pending(ctx)) { json_free(pool); return NULL; }
    if (ac2_truthy(pool)) {
        json_t *hc = ac2_call(ctx, "pool.has_credentials", json_object());
        if (ac2_exc_pending(ctx)) { json_free(hc); json_free(pool); return NULL; }
        bool has = ac2_truthy(hc); json_free(hc);
        if (has) {
            json_t *sel = ac2_call(ctx, "pool.select", json_object());
            json_free(sel);
            if (ac2_exc_pending(ctx)) { json_free(pool); return NULL; }
            json_t *ref = ac2_call(ctx, "pool.try_refresh_current", json_object());
            if (ac2_exc_pending(ctx)) { json_free(ref); json_free(pool); return NULL; }
            if (!ac2_is_none(ref)) {
                char *rk = ac2_str_or_empty(ac2_get(ref, "runtime_api_key"));
                char *st = ac2_strip(rk); free(rk);
                bool ok = st[0] != '\0';
                free(st);
                if (ok) { json_free(ref); json_free(pool); return json_bool(true); }
            }
            json_free(ref);
        }
    }
    json_free(pool);
    a = json_object();
    ac2_arg_bool(a, "force_refresh", true);
    json_t *creds = ac2_call(ctx, "resolve_xai_oauth_runtime_credentials", a);
    if (ac2_exc_pending(ctx)) { json_free(creds); return NULL; }
    a = json_object();
    ac2_arg(a, "creds", creds);
    json_t *r = ac2_call(ctx, "_creds_have_api_key", a);
    json_free(creds);
    if (ac2_exc_pending(ctx)) { json_free(r); return NULL; }
    return r ? r : json_null();
}

/* PoP: ac2_refresh_vertex_credentials @ agent/auxiliary_client.py:_refresh_vertex_credentials */
static json_t *ac2_refresh_vertex_credentials(ac2_ctx *ctx, json_t *in) {
    (void)in;
    json_t *cfg = ac2_call(ctx, "get_vertex_config", json_object());
    if (ac2_exc_pending(ctx)) { json_free(cfg); return NULL; }
    json_t *token = ac2_t0(cfg);
    json_t *base_url = ac2_t1(cfg);
    bool ok = false;
    if (ac2_is_str(token)) {
        char *st = ac2_strip(token->str_val);
        if (st[0] && ac2_is_str(base_url)) {
            char *sb = ac2_strip(base_url->str_val);
            ok = sb[0] != '\0';
            free(sb);
        }
        free(st);
    }
    json_free(cfg);
    return json_bool(ok);
}

/* ── vision routing ──────────────────────────────────────────────────────── */

/* PoP: ac2_deepinfra_strict_vision_backend @ agent/auxiliary_client.py:_deepinfra_strict_vision_backend */
static json_t *ac2_deepinfra_strict_vision_backend(ac2_ctx *ctx, json_t *in) {
    json_t *model = ac2_get(in, "model");
    json_t *vision_model = model;
    json_t *vd = NULL;
    if (!ac2_truthy(model)) {
        json_t *a = json_object();
        ac2_arg_str(a, "provider", "deepinfra");
        vd = ac2_call(ctx, "_resolve_provider_vision_default", a);
        if (ac2_exc_pending(ctx)) { json_free(vd); return NULL; }
        vision_model = vd;
    }
    if (!ac2_truthy(vision_model)) {
        ac2_log(ctx, "debug",
                "Vision auto-detect: deepinfra catalog unreachable or returned no vision-tagged models \xe2\x80\x94 skipping",
                json_array(), false);
        json_free(vd);
        return ac2_pair(NULL, NULL);
    }
    json_t *a = json_object();
    ac2_arg_str(a, "provider", "deepinfra");
    ac2_arg(a, "model", vision_model);
    ac2_arg_bool(a, "is_vision", true);
    json_t *r = ac2_call(ctx, "resolve_provider_client", a);
    json_free(vd);
    if (ac2_exc_pending(ctx)) { json_free(r); return NULL; }
    return r ? r : json_null();
}

/* PoP: ac2_finalize_vision_client @ agent/auxiliary_client.py:_finalize_vision_client */
static json_t *ac2_finalize_vision_client(ac2_ctx *ctx, const char *resolved_provider,
                                          json_t *sync_client, json_t *default_model,
                                          json_t *resolved_model, bool async_mode) {
    if (ac2_is_none(sync_client))
        return ac2_triple(resolved_provider, NULL, NULL);
    json_t *final_model = ac2_truthy(resolved_model) ? resolved_model : default_model;
    if (async_mode) {
        json_t *a = json_object();
        ac2_arg(a, "client", sync_client);
        ac2_arg(a, "model", final_model);
        ac2_arg_bool(a, "is_vision", true);
        json_t *r = ac2_call(ctx, "_to_async_client", a);
        if (ac2_exc_pending(ctx)) { json_free(r); return NULL; }
        json_t *ac = ac2_t0(r);
        json_t *am = ac2_t1(r);
        json_t *out = ac2_triple(resolved_provider, ac, am);
        json_free(r);
        return out;
    }
    return ac2_triple(resolved_provider, sync_client, final_model);
}

/* PoP: ac2_vision_auto_route @ agent/auxiliary_client.py:_vision_auto_route */
static json_t *ac2_vision_auto_route(ac2_ctx *ctx, json_t *in) {
    json_t *runtime = ac2_get(in, "runtime");
    if (!runtime) runtime = json_object();
    bool rt_owned = !ac2_get(in, "runtime");
    json_t *resolved_model = ac2_get(in, "resolved_model");
    json_t *resolved_api_mode = ac2_get(in, "resolved_api_mode");
    bool async_mode = json_get_bool(in, "async_mode", false);
    json_t *emptystr = json_string("");
    json_t *nullnode = json_null();
    /* main_provider = str(runtime.get("provider") or _read_main_provider()) */
    json_t *rp = ac2_get_def(runtime, "provider", nullnode);
    json_t *rmp = NULL;
    json_t *prov_node = rp;
    if (!ac2_truthy(rp)) {
        rmp = ac2_call(ctx, "_read_main_provider", json_object());
        if (ac2_exc_pending(ctx)) { json_free(rmp); goto fail0; }
        prov_node = rmp;
    }
    char *main_provider = ac2_pystr(prov_node);
    json_t *rm = ac2_get_def(runtime, "model", nullnode);
    json_t *rmm = NULL;
    json_t *model_node = rm;
    if (!ac2_truthy(rm)) {
        rmm = ac2_call(ctx, "_read_main_model", json_object());
        if (ac2_exc_pending(ctx)) { free(main_provider); json_free(rmm); goto fail0; }
        model_node = rmm;
    }
    char *main_model = ac2_pystr(model_node);
    json_free(rmp); json_free(rmm);
    /* moa unwrap */
    {
        char *st = ac2_strip(main_provider);
        char *lo = ac2_lower(st); free(st);
        bool is_moa = ac2_str_eq(lo, "moa");
        free(lo);
        if (is_moa) {
            json_t *a = json_object();
            ac2_arg_str(a, "preset_name", main_model);
            json_t *agg = ac2_call(ctx, "_resolve_moa_aggregator", a);
            if (ac2_exc_pending(ctx)) { json_free(agg); goto fail; }
            json_t *ap = ac2_t0(agg);
            json_t *am = ac2_t1(agg);
            if (ac2_truthy(ap) && ac2_truthy(am)) {
                free(main_provider); main_provider = ac2_pystr(ap);
                free(main_model); main_model = ac2_pystr(am);
                json_t *nr = json_copy(runtime);
                json_set(nr, "base_url", json_string(""));
                json_set(nr, "api_key", json_string(""));
                json_set(nr, "api_mode", json_string(""));
                if (rt_owned) json_free(runtime);
                runtime = nr; rt_owned = true;
            }
            json_free(agg);
        }
    }
    json_t *out = NULL;
    if (main_provider[0] && !ac2_str_eq(main_provider, "auto") && !ac2_str_eq(main_provider, "moa")) {
        json_t *a = json_object();
        ac2_arg_str(a, "main_provider", main_provider);
        ac2_arg_str(a, "main_model", main_model);
        ac2_arg(a, "runtime", runtime);
        ac2_arg(a, "resolved_model", resolved_model);
        ac2_arg(a, "resolved_api_mode", resolved_api_mode);
        json_t *r = ac2_call(ctx, "_vision_main_provider_client", a);
        if (ac2_exc_pending(ctx)) { json_free(r); goto fail; }
        json_t *client = ac2_t0(r);
        json_t *default_model = ac2_t1(r);
        if (!ac2_is_none(client)) {
            out = ac2_finalize_vision_client(ctx, main_provider, client, default_model,
                                             resolved_model, async_mode);
            json_free(r);
            goto done;
        }
        json_free(r);
    }
    /* aggregator chain */
    {
        static const char *ORDER[] = {"openrouter", "nous", "deepinfra"};
        for (size_t i = 0; i < 3; i++) {
            if (ac2_str_eq(ORDER[i], main_provider)) continue;
            json_t *a = json_object();
            ac2_arg_str(a, "provider", ORDER[i]);
            json_t *r = ac2_call(ctx, "_resolve_strict_vision_backend", a);
            if (ac2_exc_pending(ctx)) { json_free(r); goto fail; }
            json_t *client = ac2_t0(r);
            json_t *default_model = ac2_t1(r);
            if (!ac2_is_none(client)) {
                out = ac2_finalize_vision_client(ctx, ORDER[i], client, default_model,
                                                 resolved_model, async_mode);
                json_free(r);
                goto done;
            }
            json_free(r);
        }
    }
    ac2_log(ctx, "debug", "Auxiliary vision client: none available", json_array(), false);
    out = ac2_triple(NULL, NULL, NULL);
done:
    free(main_provider); free(main_model);
    json_free(emptystr); json_free(nullnode);
    if (rt_owned) json_free(runtime);
    return out;
fail:
    free(main_provider); free(main_model);
fail0:
    json_free(emptystr); json_free(nullnode);
    if (rt_owned) json_free(runtime);
    return NULL;
}