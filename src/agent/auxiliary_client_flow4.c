/* auxiliary_client_flow4.c — FLOW port of the remaining UNCLAIMED deep
 * async / client-builder orchestrators of agent/auxiliary_client.py (beyond
 * auxiliary_client_flow3).
 *
 * Ported targets (all strict-PoP unclaimed at authoring time):
 *   _named_custom_openai_wire_client, _api_key_profile_supplied_client,
 *   _build_vertex_client, _build_bedrock_client, _wrap_transport,
 *   _project_provider_profile, _resolve_call_client, _acreate_with_progress,
 *   _try_discovery_chain, _translate_anthropic_response_format,
 *   _pool_credential_digest.
 *
 * Every collaborator rides the injected universal seam ac4_call; the harness
 * scripts results and records the ordered trace the oracle diffs against LIVE
 * Python under the same scripts. Import gates are driven by `in.gate`.
 *
 * Representation contract (shared with tests/oracle_auxiliary_client_flow4.py):
 *   - seam args are JSON objects; absent/None-valued keys are dropped on BOTH
 *     sides; no-arg seams either emit {} or {"args":[]} per their argmap.
 *   - {"$seq":[...]} per-call queue (exhaustion raises RuntimeError);
 *     {"__raise__":cls,"msg":m} raises a real exception of that class.
 *   - exception arg in a logger event projects to {"__raise__":cls,"msg":m}.
 *   - FULL builtin exception hierarchy: SystemExit/KeyboardInterrupt/
 *     GeneratorExit derive from BaseException ONLY (never match `except
 *     Exception`); every other builtin (and _ClassifiedAttempt,
 *     AuxiliaryClientUnavailable->RuntimeError) derives from Exception.
 *
 * libjson-only. acf_/ac2_/ac3_ prefixes are taken by flow1/2/3; this port
 * uses ac4_.
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>

/* ── context + seam plumbing ─────────────────────────────────────────────── */
typedef struct ac4_ctx {
    json_t *gate;
    json_t *io;
    json_t *trace;
    json_t *counters;
    json_t *exc;
} ac4_ctx;

static bool ac4_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool ac4_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool ac4_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool ac4_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool ac4_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool ac4_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool ac4_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static bool ac4_truthy(const json_t *v) {
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
static const char *ac4_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static double ac4_num(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
static json_t *ac4_get(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}
static const char *ac4_gets(const json_t *obj, const char *key) {
    return ac4_jstr(ac4_get(obj, key));
}
static bool ac4_getb(const json_t *obj, const char *key, bool def) {
    json_t *v = ac4_get(obj, key);
    if (!v || v->type == JSON_NULL) return def;
    if (v->type == JSON_BOOL) return v->bool_val;
    return ac4_truthy(v);
}

/* ── exception plumbing (FULL builtin hierarchy) ─────────────────────────── */
static bool ac4_exc_pending(const ac4_ctx *ctx) { return ctx && ctx->exc != NULL; }
static void ac4_exc_clear(ac4_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}
static void ac4_raise(ac4_ctx *ctx, const char *cls, const char *msg) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
    json_set(ctx->exc, "msg", json_string(msg ? msg : ""));
}
static const char *ac4_exc_class(const ac4_ctx *ctx) {
    return json_get_str(ctx->exc, "__raise__", "");
}
static const char *ac4_exc_msg(const ac4_ctx *ctx) {
    return json_get_str(ctx->exc, "msg", "");
}

/* Direct parent of every builtin/custom exception this port can script. */
static const char *ac4_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        /* BaseException-only family: MUST NOT match `except Exception`. */
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"},
        {"BaseExceptionGroup", "BaseException"},
        /* Exception itself */
        {"Exception", "BaseException"},
        {"ExceptionGroup", "Exception"},
        /* OSError family */
        {"OSError", "Exception"},
        {"FileNotFoundError", "OSError"}, {"PermissionError", "OSError"},
        {"IsADirectoryError", "OSError"}, {"NotADirectoryError", "OSError"},
        {"FileExistsError", "OSError"}, {"InterruptedError", "OSError"},
        {"BlockingIOError", "OSError"}, {"ChildProcessError", "OSError"},
        {"ProcessLookupError", "OSError"}, {"ConnectionError", "OSError"},
        {"BrokenPipeError", "ConnectionError"}, {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
        {"TimeoutError", "OSError"},
        /* Core Exception subclasses */
        {"ValueError", "Exception"}, {"RuntimeError", "Exception"},
        {"TypeError", "Exception"}, {"AttributeError", "Exception"},
        {"LookupError", "Exception"}, {"ArithmeticError", "Exception"},
        {"StopIteration", "Exception"}, {"StopAsyncIteration", "Exception"},
        {"AssertionError", "Exception"}, {"BufferError", "Exception"},
        {"EOFError", "Exception"}, {"ImportError", "Exception"},
        {"MemoryError", "Exception"}, {"NameError", "Exception"},
        {"ReferenceError", "Exception"}, {"SyntaxError", "Exception"},
        {"SystemError", "Exception"}, {"Warning", "Exception"},
        /* Second-level specializations */
        {"NotImplementedError", "RuntimeError"}, {"RecursionError", "RuntimeError"},
        {"ModuleNotFoundError", "ImportError"}, {"UnboundLocalError", "NameError"},
        {"IndentationError", "SyntaxError"}, {"TabError", "IndentationError"},
        {"UnicodeEncodeError", "UnicodeError"}, {"UnicodeTranslateError", "UnicodeError"},
        {"UnicodeDecodeError", "UnicodeError"}, {"UnicodeError", "ValueError"},
        {"JSONDecodeError", "ValueError"},
        {"FloatingPointError", "ArithmeticError"},
        {"ZeroDivisionError", "ArithmeticError"}, {"OverflowError", "ArithmeticError"},
        {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
        {"PythonFinalizationError", "RuntimeError"}, {"_IncompleteInputError", "SyntaxError"},
        /* custom */
        {"AuxiliaryClientUnavailable", "RuntimeError"},
        {"_ClassifiedAttempt", "Exception"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (ac4_str_eq(T[i].c, c)) return T[i].p;
    return NULL;
}
static bool ac4_exc_matches(const ac4_ctx *ctx, const char *const *classes, size_t n) {
    if (!ac4_exc_pending(ctx)) return false;
    const char *cls = ac4_exc_class(ctx);
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (ac4_str_eq(cls, classes[i])) return true;
        const char *p = ac4_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}
static bool ac4_exc_is_exception(const ac4_ctx *ctx) {
    static const char *E[] = {"Exception"};
    return ac4_exc_matches(ctx, E, 1);
}

/* ── universal seam ──────────────────────────────────────────────────────── */
static json_t *ac4_script_result(ac4_ctx *ctx, const json_t *script) {
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

/* args ownership TRANSFERRED (NULL -> empty object). Result is NEW. */
static json_t *ac4_call(ac4_ctx *ctx, const char *name, json_t *args) {
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
    if (chosen != ctx->exc) ret = ac4_script_result(ctx, chosen);
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

/* arg builders: None-valued keys dropped on BOTH sides (""/empty kept). */
static void ac4_arg(json_t *args, const char *key, const json_t *val) {
    if (!val || val->type == JSON_NULL) return;
    json_set(args, key, json_copy(val));
}
static void ac4_arg_str(json_t *args, const char *key, const char *val) {
    if (!val) return;
    json_set(args, key, json_string(val));
}
static void ac4_arg_bool(json_t *args, const char *key, bool val) {
    json_set(args, key, json_bool(val));
}
static json_t *ac4_args_emptyarr(void) {
    json_t *a = json_object();
    json_set(a, "args", json_array());
    return a;
}

/* logger seam: {"msg","args"[,"exc_info"]} -> logger.<level>. */
static json_t *ac4_log(ac4_ctx *ctx, const char *level, const char *msg,
                       json_t *args_arr, bool exc_info) {
    json_t *a = json_object();
    json_set(a, "msg", json_string(msg));
    json_set(a, "args", args_arr ? args_arr : json_array());
    if (exc_info) json_set(a, "exc_info", json_bool(true));
    char name[32];
    snprintf(name, sizeof name, "logger.%s", level);
    json_t *r = ac4_call(ctx, name, a);
    json_free(r);
    return NULL;
}
static json_t *ac4_exc_arg(const ac4_ctx *ctx) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(ac4_exc_class(ctx)));
    json_set(e, "msg", json_string(ac4_exc_msg(ctx)));
    return e;
}

/* helper: seam with one positional arg under `key`. */
static json_t *ac4_call1(ac4_ctx *ctx, const char *name, const char *key, const json_t *val) {
    json_t *a = json_object();
    ac4_arg(a, key, val);
    return ac4_call(ctx, name, a);
}


/* ── 1. _named_custom_openai_wire_client ─────────────────────────────────── */
/* PoP: ac4_named_custom_openai_wire_client @ agent/auxiliary_client.py:_named_custom_openai_wire_client */
static json_t *ac4_named_custom_openai_wire_client(ac4_ctx *ctx, const char *custom_base,
        const json_t *custom_key, const json_t *extra_headers) {
    json_t *a1 = json_object();
    ac4_arg_str(a1, "base", custom_base ? custom_base : "");
    json_t *u = ac4_call(ctx, "to_openai_base_url", a1);
    if (ac4_exc_pending(ctx)) { json_free(u); return NULL; }
    json_t *a2 = json_object();
    ac4_arg(a2, "url", u);
    json_t *p = ac4_call(ctx, "extract_url_query_params", a2);
    json_free(u);
    if (ac4_exc_pending(ctx)) { json_free(p); return NULL; }
    json_t *clean = (ac4_is_arr(p) && p->c.count > 0) ? json_copy(p->c.items[0]) : json_string("");
    json_t *dq = (ac4_is_arr(p) && p->c.count > 1 && !ac4_is_none(p->c.items[1]))
                 ? json_copy(p->c.items[1]) : NULL;
    json_free(p);
    json_t *h = ac4_call(ctx, "apply_user_default_headers", json_object());
    if (ac4_exc_pending(ctx)) { json_free(h); json_free(clean); json_free(dq); return NULL; }
    json_t *headers = ac4_is_obj(h) ? json_copy(h) : json_object();
    json_free(h);
    if (ac4_is_obj(extra_headers)) {
        for (size_t i = 0; i < extra_headers->c.count; i++) {
            const char *k = extra_headers->c.keys[i];
            json_set(headers, k, json_copy(extra_headers->c.items[i]));
        }
    }
    json_t *a4 = json_object();
    ac4_arg(a4, "api_key", custom_key);
    ac4_arg(a4, "base_url", clean);
    json_free(clean);
    if (ac4_truthy(dq)) json_set(a4, "default_query", dq); else json_free(dq);
    if (ac4_truthy(headers)) json_set(a4, "default_headers", headers); else json_free(headers);
    return ac4_call(ctx, "create_openai_client", a4);
}

/* ── 2. _api_key_profile_supplied_client ─────────────────────────────────── */
/* PoP: ac4_api_key_profile_supplied_client @ agent/auxiliary_client.py:_api_key_profile_supplied_client */
static json_t *ac4_api_key_profile_supplied_client(ac4_ctx *ctx, const char *provider,
        const json_t *client_kwargs) {
    json_t *a = json_object();
    ac4_arg_str(a, "provider", provider);
    json_t *p = ac4_call(ctx, "get_provider_profile", a);
    if (ac4_exc_pending(ctx)) {
        json_free(p);
        if (ac4_exc_is_exception(ctx)) ac4_exc_clear(ctx);
        return NULL;
    }
    if (ac4_is_none(p)) { json_free(p); return NULL; }
    json_t *a2 = json_object();
    if (client_kwargs) ac4_arg(a2, "kwargs", client_kwargs); else json_set(a2, "kwargs", json_object());
    json_t *r = ac4_call(ctx, "profile.create_client", a2);
    json_free(p);
    if (ac4_exc_pending(ctx)) {
        json_free(r);
        if (ac4_exc_is_exception(ctx)) {
            json_t *arr = json_array();
            json_append(arr, json_string(provider ? provider : ""));
            ac4_log(ctx, "warning",
                "resolve_provider_client: provider profile %r failed to create an "
                "auxiliary client; falling back to the standard client path",
                arr, true);
            ac4_exc_clear(ctx);
        }
        return NULL;
    }
    return r;
}

/* ── 3. _build_vertex_client ─────────────────────────────────────────────── */
/* PoP: ac4_build_vertex_client @ agent/auxiliary_client.py:_build_vertex_client */
static json_t *ac4_build_vertex_client(ac4_ctx *ctx, const char *provider, const json_t *model) {
    json_t *res = json_array();
    json_append(res, json_null());
    json_append(res, json_null());
    if (!ac4_getb(ctx->gate, "vertex", true)) {
        ac4_log(ctx, "warning",
            "resolve_provider_client: vertex requested but google-auth not installed",
            NULL, false);
        return res;
    }
    json_t *h = ac4_call(ctx, "has_vertex_credentials", ac4_args_emptyarr());
    if (ac4_exc_pending(ctx)) { json_free(h); return res; }
    bool have = ac4_truthy(h);
    json_free(h);
    if (!have) {
        ac4_log(ctx, "debug",
            "resolve_provider_client: vertex requested but no GCP credentials found",
            NULL, false);
        return res;
    }
    json_t *cfg = ac4_call(ctx, "get_vertex_config", ac4_args_emptyarr());
    if (ac4_exc_pending(ctx)) { json_free(cfg); return res; }
    json_t *token = (ac4_is_arr(cfg) && cfg->c.count > 0) ? json_copy(cfg->c.items[0]) : json_null();
    json_t *base = (ac4_is_arr(cfg) && cfg->c.count > 1) ? json_copy(cfg->c.items[1]) : json_null();
    json_free(cfg);
    if (!ac4_truthy(token) || !ac4_truthy(base)) {
        json_free(token); json_free(base);
        ac4_log(ctx, "warning",
            "resolve_provider_client: vertex requested but could not mint token / resolve project",
            NULL, false);
        return res;
    }
    json_t *a = json_object();
    ac4_arg(a, "model", ac4_truthy(model) ? model : NULL);
    if (!ac4_truthy(model)) json_set(a, "model", json_string("google/gemini-3-flash-preview"));
    ac4_arg_str(a, "provider", provider);
    json_t *fm = ac4_call(ctx, "normalize_resolved_model", a);
    if (ac4_exc_pending(ctx)) { json_free(fm); json_free(token); json_free(base); return res; }
    json_t *a2 = json_object();
    json_set(a2, "api_key", json_copy(token));
    json_set(a2, "base_url", json_copy(base));
    json_free(token); json_free(base);
    json_t *cl = ac4_call(ctx, "vertex_openai_ctor", a2);
    if (ac4_exc_pending(ctx)) {
        json_free(cl); json_free(fm);
        if (ac4_exc_is_exception(ctx)) {
            json_t *arr = json_array();
            json_append(arr, ac4_exc_arg(ctx));
            ac4_log(ctx, "warning", "resolve_provider_client: cannot create Vertex client: %s",
                    arr, false);
            ac4_exc_clear(ctx);
        }
        return res;
    }
    json_t *arr = json_array();
    json_append(arr, fm ? json_copy(fm) : json_null());
    ac4_log(ctx, "debug", "resolve_provider_client: vertex (%s)", arr, false);
    json_free(res);
    res = json_array();
    json_append(res, cl ? cl : json_null());
    json_append(res, fm ? fm : json_null());
    return res;
}

/* ── 4. _build_bedrock_client ────────────────────────────────────────────── */
/* PoP: ac4_build_bedrock_client @ agent/auxiliary_client.py:_build_bedrock_client */
static json_t *ac4_build_bedrock_client(ac4_ctx *ctx, const char *provider, const json_t *model,
        bool raw_codex) {
    json_t *res = json_array();
    json_append(res, json_null());
    json_append(res, json_null());
    if (!ac4_getb(ctx->gate, "bedrock", true)) {
        ac4_log(ctx, "warning",
            "resolve_provider_client: bedrock requested but boto3, httpx/openai, or anthropic SDK not installed",
            NULL, false);
        return res;
    }
    json_t *cred = ac4_call(ctx, "has_aws_credentials", ac4_args_emptyarr());
    if (ac4_exc_pending(ctx)) { json_free(cred); return res; }
    bool have = ac4_truthy(cred);
    json_free(cred);
    if (!have) {
        ac4_log(ctx, "debug",
            "resolve_provider_client: bedrock requested but no AWS credentials found", NULL, false);
        return res;
    }
    json_t *region = ac4_call(ctx, "resolve_bedrock_runtime_region", ac4_args_emptyarr());
    if (ac4_exc_pending(ctx)) { json_free(region); return res; }
    json_t *a = json_object();
    if (ac4_truthy(model)) json_set(a, "model", json_copy(model));
    else json_set(a, "model", json_string("anthropic.claude-haiku-4-5-20251001-v1:0"));
    ac4_arg_str(a, "provider", provider);
    json_t *nm = ac4_call(ctx, "normalize_resolved_model", a);
    if (ac4_exc_pending(ctx)) { json_free(nm); json_free(region); return res; }
    json_t *fm = ac4_truthy(nm) ? json_copy(nm) : json_string("anthropic.claude-haiku-4-5-20251001-v1:0");
    json_free(nm);
    json_t *a2 = json_object();
    ac4_arg(a2, "model", fm);
    json_t *mantle = ac4_call(ctx, "bedrock_openai_uses_mantle", a2);
    if (ac4_exc_pending(ctx)) { json_free(mantle); json_free(fm); json_free(region); return res; }
    bool is_mantle = ac4_truthy(mantle);
    json_free(mantle);
    json_t *cl = NULL;
    if (is_mantle) {
        json_t *bearer = ac4_call(ctx, "resolve_bedrock_bearer_token", ac4_args_emptyarr());
        if (ac4_exc_pending(ctx)) { json_free(bearer); json_free(fm); json_free(region); return res; }
        json_t *a3 = json_object();
        ac4_arg(a3, "region", region);
        json_t *bu = ac4_call(ctx, "bedrock_openai_base_url", a3);
        if (ac4_exc_pending(ctx)) { json_free(bu); json_free(bearer); json_free(fm); json_free(region); return res; }
        json_t *ck = json_object();
        json_set(ck, "api_key", ac4_truthy(bearer) ? json_copy(bearer) : json_string("aws-sdk"));
        json_set(ck, "base_url", bu ? json_copy(bu) : json_null());
        json_t *a4 = json_object();
        ac4_arg(a4, "kwargs", ck);
        json_t *cfg = ac4_call(ctx, "configure_bedrock_openai_client_kwargs", a4);
        json_free(cfg);
        if (ac4_exc_pending(ctx)) { json_free(ck); json_free(bearer); json_free(fm); json_free(region); return res; }
        json_free(bearer);
        json_t *raw = ac4_call(ctx, "openai_ctor", ck);
        if (ac4_exc_pending(ctx)) { json_free(raw); json_free(fm); json_free(region); return res; }
        json_t *arr = json_array();
        json_append(arr, json_copy(fm));
        json_append(arr, json_copy(region));
        ac4_log(ctx, "debug", "resolve_provider_client: bedrock-openai (%s, %s)", arr, false);
        if (!raw_codex) {
            json_t *a5 = json_object();
            ac4_arg(a5, "client", raw);
            ac4_arg(a5, "model", fm);
            cl = ac4_call(ctx, "codex_client_ctor", a5);
            if (ac4_exc_pending(ctx)) { json_free(cl); json_free(fm); json_free(region); return res; }
        } else cl = raw;
    } else {
        char base_url[256];
        snprintf(base_url, sizeof base_url, "https://bedrock-runtime.%s.amazonaws.com",
                 ac4_jstr(region));
        json_t *a3 = json_object();
        ac4_arg(a3, "model", fm);
        json_t *anth = ac4_call(ctx, "is_anthropic_bedrock_model", a3);
        if (ac4_exc_pending(ctx)) { json_free(anth); json_free(fm); json_free(region); return res; }
        bool is_anth = ac4_truthy(anth);
        json_free(anth);
        if (is_anth) {
            json_t *a4 = json_object();
            ac4_arg(a4, "region", region);
            json_t *real = ac4_call(ctx, "build_anthropic_bedrock_client", a4);
            if (ac4_exc_pending(ctx)) {
                json_free(real);
                if (ac4_exc_matches(ctx, (const char *[]){"ImportError"}, 1)) {
                    json_t *arr = json_array();
                    json_append(arr, ac4_exc_arg(ctx));
                    ac4_log(ctx, "warning", "resolve_provider_client: cannot create Bedrock client: %s",
                            arr, false);
                    ac4_exc_clear(ctx);
                }
                json_free(fm); json_free(region); return res;
            }
            json_t *a5 = json_object();
            ac4_arg(a5, "real_client", real);
            ac4_arg(a5, "model", fm);
            json_set(a5, "api_key", json_string("aws-sdk"));
            json_set(a5, "base_url", json_string(base_url));
            cl = ac4_call(ctx, "anthropic_client_ctor", a5);
            if (ac4_exc_pending(ctx)) { json_free(cl); json_free(fm); json_free(region); return res; }
            json_t *arr = json_array();
            json_append(arr, json_copy(fm));
            json_append(arr, json_copy(region));
            ac4_log(ctx, "debug", "resolve_provider_client: bedrock anthropic (%s, %s)", arr, false);
        } else {
            json_t *a4 = json_object();
            ac4_arg(a4, "region", region);
            ac4_arg(a4, "model", fm);
            cl = ac4_call(ctx, "bedrock_client_ctor", a4);
            if (ac4_exc_pending(ctx)) { json_free(cl); json_free(fm); json_free(region); return res; }
            json_t *arr = json_array();
            json_append(arr, json_copy(fm));
            json_append(arr, json_copy(region));
            ac4_log(ctx, "debug", "resolve_provider_client: bedrock converse (%s, %s)", arr, false);
        }
    }
    json_free(region);
    json_free(res);
    res = json_array();
    json_append(res, cl ? cl : json_null());
    json_append(res, fm ? fm : json_null());
    return res;
}

/* ── 5. _wrap_transport ──────────────────────────────────────────────────── */
/* PoP: ac4_wrap_transport @ agent/auxiliary_client.py:_wrap_transport */
static json_t *ac4_wrap_transport(ac4_ctx *ctx, const json_t *req, json_t *client,
        const char *final_model, const char *base_url, const char *api_key,
        json_t **state_out) {
    json_t *st = json_object();
    char *orig_mode = strdup(ac4_gets(req, "api_mode"));
    char *eff_mode = strdup(orig_mode);
    bool raw_codex = ac4_getb(req, "raw_codex", false);
    const char *eff_base = base_url ? base_url : "";
    json_t *oc_base = NULL;

    json_t *a = json_object();
    json_t *rj = json_object();
    ac4_arg_str(rj, "provider", ac4_gets(req, "provider"));
    ac4_arg_str(rj, "api_mode", orig_mode);
    ac4_arg_bool(rj, "raw_codex", raw_codex);
    json_set(a, "req", rj);
    ac4_arg_str(a, "base_url", base_url);
    json_t *actual = ac4_call(ctx, "is_actual_auxiliary_route", a);
    if (ac4_exc_pending(ctx)) { json_free(actual); free(orig_mode); free(eff_mode);
                                json_free(st); *state_out = st; return NULL; }
    bool is_actual = ac4_truthy(actual);
    json_free(actual);
    if (is_actual) {
        json_t *out = client;
        bool codex = ac4_str_eq(ac4_gets(client, "kind"), "codex");
        if (codex) out = json_obj_get(client, "real_client");
        if (out && out->type == JSON_OBJECT)
            json_set(out, "_hermes_aux_effective_provider", json_string("actual"));
        json_set(st, "api_mode", json_string(orig_mode));
        json_set(st, "base_url", json_string(base_url ? base_url : ""));
        json_set(st, "flag", codex ? json_null() : json_string("actual"));
        json_t *rf = (codex && out && out->type == JSON_OBJECT)
                     ? json_obj_get(out, "_hermes_aux_effective_provider") : NULL;
        json_set(st, "real_flag", rf ? json_copy(rf) : json_null());
        free(orig_mode); free(eff_mode); *state_out = st;
        return out ? json_copy(out) : NULL;
    }

    json_t *a2 = json_object();
    ac4_arg_str(a2, "provider", ac4_gets(req, "provider"));
    ac4_arg_str(a2, "model", final_model);
    ac4_arg_str(a2, "base_url", base_url);
    json_t *oc = ac4_call(ctx, "opencode_transport", a2);
    if (ac4_exc_pending(ctx)) { json_free(oc); free(orig_mode); free(eff_mode);
                                json_free(st); *state_out = st; return NULL; }
    json_t *oc_mode = (ac4_is_arr(oc) && oc->c.count > 0) ? json_copy(oc->c.items[0]) : NULL;
    oc_base = (ac4_is_arr(oc) && oc->c.count > 1) ? json_copy(oc->c.items[1]) : NULL;
    json_free(oc);
    if (ac4_truthy(oc_mode)) {
        free(eff_mode);
        eff_mode = strdup(ac4_jstr(oc_mode));
        if (oc_base && ac4_is_str(oc_base)) eff_base = oc_base->str_val;
    }
    json_free(oc_mode);

    bool client_codex = ac4_str_eq(ac4_gets(client, "kind"), "codex");
    bool host_is_openai = false;
    if (!(client_codex || raw_codex) && eff_mode[0] == '\0') {
        json_t *a3 = json_object();
        ac4_arg_str(a3, "base_url", eff_base);
        json_t *hnr = ac4_call(ctx, "base_url_hostname", a3);
        if (ac4_exc_pending(ctx)) { json_free(hnr); json_free(oc_base); free(orig_mode);
                                    free(eff_mode); json_free(st); *state_out = st; return NULL; }
        host_is_openai = ac4_str_eq(ac4_jstr(hnr), "api.openai.com");
        json_free(hnr);
    }
    bool has_codex_word = false;
    if (final_model) {
        char low[512]; size_t k = 0;
        for (const char *p = final_model; *p && k + 1 < sizeof low; p++)
            low[k++] = (char)tolower((unsigned char)*p);
        low[k] = '\0';
        has_codex_word = strstr(low, "codex") != NULL;
    }
    bool needs = (!(client_codex || raw_codex)) &&
        (ac4_str_eq(eff_mode, "codex_responses") ||
         (eff_mode[0] == '\0' && host_is_openai && has_codex_word));
    json_t *ret = NULL;
    if (needs) {
        json_t *arr = json_array();
        json_append(arr, json_string(eff_mode[0] ? eff_mode : "auto-detected"));
        json_append(arr, json_string(final_model ? final_model : ""));
        json_append(arr, json_string(eff_base));
        ac4_log(ctx, "debug",
            "resolve_provider_client: wrapping client in CodexAuxiliaryClient "
            "(api_mode=%s, model=%s, base_url=%s)", arr, false);
        json_t *a4 = json_object();
        ac4_arg(a4, "client", client);
        ac4_arg_str(a4, "model", final_model);
        ret = ac4_call(ctx, "codex_client_ctor", a4);
        if (ac4_exc_pending(ctx)) { json_free(ret); json_free(oc_base); free(orig_mode);
                                    free(eff_mode); json_free(st); *state_out = st; return NULL; }
    } else {
        char mode_buf[128];
        if (eff_mode[0]) snprintf(mode_buf, sizeof mode_buf, "%s", eff_mode);
        else {
            json_t *a5 = json_object();
            ac4_arg_str(a5, "provider", ac4_gets(req, "provider"));
            json_t *pd = ac4_call(ctx, "profile_declared_messages_wire", a5);
            if (ac4_exc_pending(ctx)) { json_free(pd); json_free(oc_base); free(orig_mode);
                                        free(eff_mode); json_free(st); *state_out = st; return NULL; }
            snprintf(mode_buf, sizeof mode_buf, "%s", ac4_jstr(pd));
            json_free(pd);
        }
        json_t *a6 = json_object();
        ac4_arg(a6, "client", client);
        ac4_arg_str(a6, "model", final_model);
        ac4_arg_str(a6, "api_key", api_key);
        ac4_arg_str(a6, "base_url", eff_base);
        ac4_arg_str(a6, "api_mode", mode_buf);
        ret = ac4_call(ctx, "maybe_wrap_anthropic", a6);
        if (ac4_exc_pending(ctx)) { json_free(ret); json_free(oc_base); free(orig_mode);
                                    free(eff_mode); json_free(st); *state_out = st; return NULL; }
    }
    json_set(st, "api_mode", json_string(orig_mode));
    json_set(st, "base_url", json_string(base_url ? base_url : ""));
    json_set(st, "flag", json_null());
    json_set(st, "real_flag", json_null());
    json_free(oc_base); free(orig_mode); free(eff_mode);
    *state_out = st;
    return ret;
}

/* ── 6. _project_provider_profile ────────────────────────────────────────── */
static json_t *ac4_contains_fields(ac4_ctx *ctx, const json_t *d);
/* PoP: ac4_project_provider_profile @ agent/auxiliary_client.py:_project_provider_profile */
static json_t *ac4_project_provider_profile(ac4_ctx *ctx, const char *provider,
        const char *provider_norm, const char *model, const char *base_url,
        const json_t *reasoning_config, const json_t *data_use) {
    json_t *body = json_object();
    json_t *rextra = json_object();
    json_t *top = json_object();
    bool handles = false, messages_wire = false;
    json_t *a = json_object();
    ac4_arg_str(a, "provider", provider_norm);
    json_t *p = ac4_call(ctx, "get_provider_profile", a);
    if (ac4_exc_pending(ctx)) goto done;
    if (ac4_is_none(p)) {
        json_free(p);
        json_t *a2 = json_object();
        ac4_arg_str(a2, "provider", provider_norm);
        json_t *rtc = ac4_call(ctx, "routes_to_custom_endpoint", a2);
        if (ac4_exc_pending(ctx)) { json_free(rtc); goto done; }
        bool route = ac4_truthy(rtc);
        json_free(rtc);
        if (route) {
            json_t *a3 = json_object();
            json_set(a3, "provider", json_string("custom"));
            json_free(p);
            p = ac4_call(ctx, "get_provider_profile", a3);
            if (ac4_exc_pending(ctx)) goto done;
        }
    }
    if (!ac4_is_none(p)) {
        messages_wire = ac4_str_eq(ac4_gets(p, "api_mode"), "anthropic_messages");
        json_t *a4 = json_object();
        ac4_arg_str(a4, "model", model);
        ac4_arg_str(a4, "base_url", base_url);
        ac4_arg(a4, "reasoning_config", reasoning_config);
        ac4_arg(a4, "data_use", data_use);
        json_t *be = ac4_call(ctx, "build_extra_body", a4);
        if (ac4_exc_pending(ctx)) { json_free(be); goto done; }
        if (ac4_is_obj(be)) { json_free(body); body = be; } else json_free(be);
        json_t *a5 = json_object();
        ac4_arg(a5, "reasoning_config", reasoning_config);
        ac4_arg_str(a5, "model", model);
        ac4_arg_str(a5, "base_url", base_url);
        json_t *ex = ac4_call(ctx, "build_api_kwargs_extras", a5);
        if (ac4_exc_pending(ctx)) { json_free(ex); goto done; }
        if (ac4_is_arr(ex) && ex->c.count >= 2) {
            json_t *r = ac4_truthy(ex->c.items[0]) ? json_copy(ex->c.items[0]) : json_object();
            json_t *t = ac4_truthy(ex->c.items[1]) ? json_copy(ex->c.items[1]) : json_object();
            json_free(rextra); json_free(top); rextra = r; top = t;
        } else if (ac4_is_obj(ex)) {
            json_free(rextra); rextra = json_copy(ex);
        }
        json_free(ex);
        bool overridden = ac4_getb(p, "__overridden__", false);
        handles = overridden;
        if (!handles) { json_t *cf = ac4_contains_fields(ctx, body); handles = ac4_truthy(cf); json_free(cf); }
        if (!handles) { json_t *cf = ac4_contains_fields(ctx, rextra); handles = ac4_truthy(cf); json_free(cf); }
        if (!handles) { json_t *cf = ac4_contains_fields(ctx, top); handles = ac4_truthy(cf); json_free(cf); }
    }
    json_free(p);
done:
    if (ac4_exc_pending(ctx)) {
        if (ac4_exc_is_exception(ctx)) {
            json_t *arr = json_array();
            json_append(arr, json_string(provider ? provider : ""));
            json_append(arr, ac4_exc_arg(ctx));
            ac4_log(ctx, "debug",
                "_build_call_kwargs: provider profile projection failed for %s: %s", arr, false);
            ac4_exc_clear(ctx);
        }
        json_t *r = json_array();
        json_append(r, body); json_append(r, rextra); json_append(r, top);
        json_append(r, json_bool(handles)); json_append(r, json_bool(messages_wire));
        return r;
    }
    {
        json_t *r = json_array();
        json_append(r, body); json_append(r, rextra); json_append(r, top);
        json_append(r, json_bool(handles)); json_append(r, json_bool(messages_wire));
        return r;
    }
}
static json_t *ac4_contains_fields(ac4_ctx *ctx, const json_t *d) {
    json_t *a = json_object();
    ac4_arg(a, "d", d);
    json_t *r = ac4_call(ctx, "contains_profile_reasoning_fields", a);
    if (ac4_exc_pending(ctx)) { json_free(r); return json_bool(false); }
    bool v = ac4_truthy(r);
    json_free(r);
    return json_bool(v);
}


/* ── 7. _resolve_call_client ─────────────────────────────────────────────── */
/* PoP: ac4_resolve_call_client @ agent/auxiliary_client.py:_resolve_call_client */
static json_t *ac4_resolve_call_client(ac4_ctx *ctx, const char *task,
        const json_t *provider, const json_t *model, const json_t *base_url,
        const json_t *api_key, const char *resolved_provider,
        const json_t *resolved_model, const json_t *resolved_base_url,
        const json_t *resolved_api_key, const json_t *resolved_api_mode,
        const json_t *main_runtime, bool async_mode) {
    json_t *effective = json_string(resolved_provider ? resolved_provider : "");
    const char *result_provider = resolved_provider ? resolved_provider : "";
    json_t *client = NULL, *final_model = NULL;
    bool vision = task && ac4_str_eq(task, "vision");
    if (vision) {
        json_t *a = json_object();
        const char *vp = (!ac4_str_eq(resolved_provider, "auto"))
                         ? resolved_provider : ac4_jstr(provider);
        ac4_arg_str(a, "provider", vp);
        ac4_arg(a, "model", ac4_truthy(resolved_model) ? resolved_model : model);
        ac4_arg(a, "base_url", ac4_truthy(resolved_base_url) ? resolved_base_url : base_url);
        ac4_arg(a, "api_key", ac4_truthy(resolved_api_key) ? resolved_api_key : api_key);
        ac4_arg_bool(a, "async_mode", async_mode);
        ac4_arg(a, "main_runtime", main_runtime);
        json_t *r = ac4_call(ctx, "resolve_vision_provider_client", a);
        if (ac4_exc_pending(ctx)) { json_free(r); json_free(effective); return NULL; }
        if (ac4_is_arr(r) && r->c.count >= 3) {
            json_free(effective); effective = json_copy(r->c.items[0]);
            result_provider = ac4_jstr(effective);
            client = json_copy(r->c.items[1]);
            final_model = json_copy(r->c.items[2]);
        }
        json_free(r);
        if (ac4_is_none(client) && !ac4_str_eq(resolved_provider, "auto") &&
            !ac4_truthy(resolved_base_url)) {
            json_t *arr = json_array();
            json_append(arr, json_string(resolved_provider ? resolved_provider : ""));
            ac4_log(ctx, "warning",
                "Vision provider %s unavailable, falling back to auto vision backends", arr, false);
            json_t *a2 = json_object();
            json_set(a2, "provider", json_string("auto"));
            ac4_arg(a2, "model", resolved_model);
            ac4_arg_bool(a2, "async_mode", async_mode);
            ac4_arg(a2, "main_runtime", main_runtime);
            json_t *r2 = ac4_call(ctx, "resolve_vision_provider_client", a2);
            if (ac4_exc_pending(ctx)) { json_free(r2); json_free(client); json_free(final_model);
                                        json_free(effective); return NULL; }
            if (ac4_is_arr(r2) && r2->c.count >= 3) {
                json_free(effective); effective = json_copy(r2->c.items[0]);
            result_provider = ac4_jstr(effective);
                json_free(client); client = json_copy(r2->c.items[1]);
                json_free(final_model); final_model = json_copy(r2->c.items[2]);
            }
            json_free(r2);
        }
    } else {
        json_t *a = json_object();
        ac4_arg_str(a, "provider", resolved_provider);
        ac4_arg(a, "model", resolved_model);
        ac4_arg_bool(a, "async_mode", async_mode);
        ac4_arg(a, "base_url", resolved_base_url);
        ac4_arg(a, "api_key", resolved_api_key);
        ac4_arg(a, "api_mode", resolved_api_mode);
        ac4_arg(a, "main_runtime", main_runtime);
        ac4_arg_str(a, "task", task);
        json_t *r = ac4_call(ctx, "get_cached_client", a);
        if (ac4_exc_pending(ctx)) { json_free(r); json_free(effective); return NULL; }
        if (ac4_is_arr(r) && r->c.count >= 2) {
            client = json_copy(r->c.items[0]);
            final_model = json_copy(r->c.items[1]);
        }
        json_free(r);
        json_t *a2 = json_object();
        ac4_arg(a2, "client", client);
        ac4_arg_str(a2, "provider", resolved_provider);
        json_t *ep = ac4_call(ctx, "effective_provider_for_client", a2);
        if (ac4_exc_pending(ctx)) { json_free(ep); json_free(client); json_free(final_model);
                                    json_free(effective); return NULL; }
        json_free(effective); effective = ep ? ep : json_null();
        if (ac4_is_none(client)) {
            char exp[128];
            snprintf(exp, sizeof exp, "%s", resolved_provider ? resolved_provider : "");
            /* lowercase */
            for (char *p = exp; *p; p++) *p = (char)tolower((unsigned char)*p);
            bool explicit_provider = exp[0] && !ac4_str_eq(exp, "auto") &&
                                     !ac4_str_eq(exp, "openrouter") && !ac4_str_eq(exp, "custom");
            if (explicit_provider) {
                json_t *a3 = json_object();
                ac4_arg_str(a3, "task", task);
                ac4_arg_str(a3, "provider", exp);
                json_t *fb = ac4_call(ctx, "try_configured_fallback_for_unavailable_client", a3);
                if (ac4_exc_pending(ctx)) { json_free(fb); json_free(effective); json_free(final_model); return NULL; }
                json_t *fb_client = (ac4_is_arr(fb) && fb->c.count > 0) ? json_copy(fb->c.items[0]) : json_null();
                json_t *fb_model = (ac4_is_arr(fb) && fb->c.count > 1) ? json_copy(fb->c.items[1]) : json_null();
                json_t *fb_label = (ac4_is_arr(fb) && fb->c.count > 2) ? json_copy(fb->c.items[2]) : json_null();
                json_free(fb);
                if (ac4_is_none(fb_client)) {
                    const char *detail = NULL;
                    if (ac4_str_eq(exp, "nous")) {
                        json_t *nd = ac4_call(ctx, "nous_credential_failure_detail", json_object());
                        if (ac4_exc_pending(ctx)) { json_free(nd); json_free(fb_client); json_free(fb_model);
                            json_free(fb_label); json_free(effective); json_free(final_model); return NULL; }
                        if (!ac4_is_none(nd)) detail = ac4_jstr(nd);
                        json_free(nd);
                    }
                    const char *msg = detail;
                    char *owned = NULL;
                    if (!msg) {
                        json_t *a4 = json_object();
                        ac4_arg_str(a4, "provider", exp);
                        json_t *mm = ac4_call(ctx, "missing_provider_credentials_message", a4);
                        if (ac4_exc_pending(ctx)) { json_free(mm); json_free(fb_client); json_free(fb_model);
                            json_free(fb_label); json_free(effective); json_free(final_model); return NULL; }
                        owned = strdup(ac4_jstr(mm));
                        json_free(mm); msg = owned;
                    }
                    ac4_raise(ctx, "AuxiliaryClientUnavailable", msg ? msg : "");
                    free(owned);
                    json_free(fb_client); json_free(fb_model); json_free(fb_label);
                    json_free(effective); json_free(final_model);
                    return NULL;
                }
                json_free(client); json_free(final_model);
                client = fb_client; final_model = fb_model;
                if (async_mode) {
                    json_t *a5 = json_object();
                    ac4_arg(a5, "client", client);
                    ac4_arg_str(a5, "model", ac4_jstr(final_model));
                    ac4_arg_bool(a5, "is_vision", vision);
                    json_t *ac = ac4_call(ctx, "to_async_client", a5);
                    if (ac4_exc_pending(ctx)) { json_free(ac); json_free(fb_label);
                        json_free(client); json_free(final_model); json_free(effective); return NULL; }
                    if (ac4_is_arr(ac) && ac->c.count >= 2) {
                        json_free(client); json_free(final_model);
                        client = json_copy(ac->c.items[0]);
                        final_model = json_copy(ac->c.items[1]);
                    }
                    json_free(ac);
                }
                if (!ac4_is_none(fb_label)) result_provider = ac4_jstr(fb_label);
                json_free(effective); effective = fb_label ? fb_label : json_string(resolved_provider);
            }
            if (ac4_is_none(client) && !ac4_truthy(resolved_base_url)) {
                json_t *arr = json_array();
                json_append(arr, json_string(task ? task : "call"));
                json_append(arr, json_string(resolved_provider ? resolved_provider : ""));
                ac4_log(ctx, "info",
                    "Auxiliary %s: provider %s unavailable, trying auto-detection chain", arr, false);
                json_t *a6 = json_object();
                json_set(a6, "provider", json_string("auto"));
                ac4_arg_bool(a6, "async_mode", async_mode);
                ac4_arg(a6, "main_runtime", main_runtime);
                ac4_arg_str(a6, "task", task);
                json_t *r2 = ac4_call(ctx, "get_cached_client", a6);
                if (ac4_exc_pending(ctx)) { json_free(r2); json_free(effective);
                    json_free(final_model); return NULL; }
                if (ac4_is_arr(r2) && r2->c.count >= 2) {
                    json_free(client); client = json_copy(r2->c.items[0]);
                    json_free(final_model); final_model = json_copy(r2->c.items[1]);
                }
                json_free(r2);
                json_t *a7 = json_object();
                ac4_arg(a7, "client", client);
                json_set(a7, "provider", json_string("auto"));
                json_t *ep2 = ac4_call(ctx, "effective_provider_for_client", a7);
                if (ac4_exc_pending(ctx)) { json_free(ep2); json_free(effective);
                    json_free(client); json_free(final_model); return NULL; }
                json_free(effective); effective = ep2 ? ep2 : json_null();
            }
        }
    }
    if (ac4_is_none(client)) {
        char buf[512];
        snprintf(buf, sizeof buf,
            "No LLM provider configured for task=%s provider=%s. Run: hermes setup",
            task ? task : "None", resolved_provider ? resolved_provider : "");
        ac4_raise(ctx, "AuxiliaryClientUnavailable", buf);
        json_free(effective); json_free(final_model); json_free(client);
        return NULL;
    }
    json_t *out = json_array();
    json_append(out, client ? client : json_null());
    json_append(out, final_model ? final_model : json_null());
    json_append(out, json_string(result_provider));
    json_append(out, effective ? effective : json_null());
    return out;
}

/* ── 8. _acreate_with_progress ───────────────────────────────────────────── */
/* PoP: ac4_acreate_with_progress @ agent/auxiliary_client.py:_acreate_with_progress */
static json_t *ac4_acreate_with_progress(ac4_ctx *ctx, const char *client_marker,
        const json_t *kwargs, const char *task, bool force_stream, const char *provider) {
    json_t *a1 = json_object();
    ac4_arg(a1, "kwargs", kwargs);
    ac4_arg_str(a1, "client", client_marker);
    json_t *kw = ac4_call(ctx, "bypass_chat_sdk_request_transform", a1);
    if (ac4_exc_pending(ctx)) { json_free(kw); return NULL; }
    if (!ac4_is_obj(kw)) { json_t *bad = kw; kw = json_object(); json_free(bad); }
    ac4_call(ctx, "notify_aux_dispatch", json_object());
    if (ac4_exc_pending(ctx)) { json_free(kw); return NULL; }
    json_t *act = ac4_call(ctx, "aux_progress_active", json_object());
    if (ac4_exc_pending(ctx)) { json_free(act); json_free(kw); return NULL; }
    bool active = ac4_truthy(act);
    json_free(act);
    /* plain path if (not active and not force_stream) OR streams internally */
    bool plain_first = (!active && !force_stream);
    bool streams = false;
    if (!plain_first) {
        json_t *a2 = json_object();
        ac4_arg_str(a2, "client", client_marker);
        json_t *si = ac4_call(ctx, "async_client_streams_internally", a2);
        if (ac4_exc_pending(ctx)) { json_free(si); json_free(kw); return NULL; }
        streams = ac4_truthy(si);
        json_free(si);
    }
    if (plain_first || streams) {
        json_t *resp = ac4_call(ctx, "create", json_copy(kw));
        json_t *orig_kwargs = json_copy(kw);
        if (ac4_exc_pending(ctx)) { json_free(resp); json_free(orig_kwargs); json_free(kw); return NULL; }
        {
            json_t *a2 = json_object();
            ac4_arg_str(a2, "client", client_marker);
            json_t *si = ac4_call(ctx, "async_client_streams_internally", a2);
            if (ac4_exc_pending(ctx)) { json_free(si); json_free(kw); return NULL; }
            bool internal = ac4_truthy(si);
            json_free(si);
            if (!internal) {
                ac4_call(ctx, "notify_aux_provider_response", json_object());
                if (ac4_exc_pending(ctx)) { json_free(resp); json_free(orig_kwargs); json_free(kw); return NULL; }
            }
        }
        json_free(orig_kwargs); json_free(kw);
        return resp;
    }
    /* stream path */
    json_t *a3 = json_object();
    ac4_arg(a3, "kwargs", kw);
    ac4_arg_str(a3, "client", client_marker);
    json_t *orig_kwargs = json_copy(kw);
    json_t *plan = ac4_call(ctx, "stream_request_plan", a3);
    if (ac4_exc_pending(ctx)) { json_free(plan); json_free(orig_kwargs); json_free(kw); return NULL; }
    json_t *stream_kwargs = (ac4_is_arr(plan) && plan->c.count > 0 && ac4_is_obj(plan->c.items[0]))
                            ? json_copy(plan->c.items[0]) : json_copy(kw);
    json_t *model = (ac4_is_arr(plan) && plan->c.count > 1) ? json_copy(plan->c.items[1]) : json_null();
    json_t *ceiling = (ac4_is_arr(plan) && plan->c.count > 2) ? json_copy(plan->c.items[2]) : json_null();
    json_free(plan); json_free(kw);
    json_t *chunks = ac4_call(ctx, "create", stream_kwargs);
    if (ac4_exc_pending(ctx)) {
        /* Negotiation failure handling. */
        if (!force_stream) {
            json_t *exc_arg = ac4_exc_arg(ctx);
            ac4_exc_clear(ctx); /* the exception is now a value for classify_failure */
            json_t *a4 = json_object();
            json_set(a4, "exc", json_copy(exc_arg));
            ac4_arg_str(a4, "provider", provider);
            ac4_arg_str(a4, "client", client_marker);
            ac4_arg(a4, "model", model);
            json_t *failure = ac4_call(ctx, "classify_failure", a4);
            if (ac4_exc_pending(ctx)) { json_free(failure); json_free(model); json_free(ceiling); json_free(orig_kwargs); return NULL; }
            json_t *verdict = ac4_get(failure, "verdict");
            json_t *a5 = json_object();
            ac4_arg(a5, "verdict", verdict);
            json_t *rr = ac4_call(ctx, "stream_negotiation_reraises", a5);
            if (ac4_exc_pending(ctx)) { json_free(rr); json_free(failure); json_free(model); json_free(ceiling); json_free(orig_kwargs); return NULL; }
            bool rethrow = ac4_truthy(rr);
            json_free(rr);
            if (rethrow) {
                const char *err = ac4_gets(failure, "error");
                json_free(failure); json_free(model); json_free(ceiling); json_free(orig_kwargs);
                ac4_raise(ctx, "_ClassifiedAttempt", err);
                return NULL;
            }
            json_free(failure);
            json_t *arr = json_array();
            json_append(arr, json_string(task ? task : "call"));
            json_append(arr, exc_arg);
            ac4_log(ctx, "debug",
                "Auxiliary %s: streamed async request failed (%s); retrying non-streaming", arr, false);
            ac4_call(ctx, "notify_aux_dispatch", json_object());
            if (ac4_exc_pending(ctx)) { json_free(model); json_free(ceiling); json_free(orig_kwargs); return NULL; }
            json_t *r = ac4_call(ctx, "create", json_copy(orig_kwargs));
            json_free(r);
            if (ac4_exc_pending(ctx)) { json_free(model); json_free(ceiling); json_free(orig_kwargs); return NULL; }
            ac4_call(ctx, "notify_aux_provider_response", json_object());
            if (ac4_exc_pending(ctx)) { json_free(model); json_free(ceiling); json_free(orig_kwargs); return NULL; }
            json_free(model); json_free(ceiling); json_free(orig_kwargs);
            return json_string("");
        }
        json_free(chunks); json_free(model); json_free(ceiling); json_free(orig_kwargs);
        return NULL;
    }
    /* A dict seam result has no attributes, so `hasattr(chunks, "choices")` is
     * false for every scripted response: transcribe to the aggregate branch. */
    json_t *a6 = json_object();
    json_set(a6, "chunks", chunks ? chunks : json_null());
    ac4_arg(a6, "model", model);
    ac4_arg(a6, "total_ceiling", ceiling);
    json_t *agg = ac4_call(ctx, "aggregate_chat_stream_async", a6);
    json_free(model); json_free(ceiling); json_free(orig_kwargs);
    return agg;
}

/* ── 9. _try_discovery_chain ─────────────────────────────────────────────── */
/* PoP: ac4_try_discovery_chain @ agent/auxiliary_client.py:_try_discovery_chain */
static json_t *ac4_try_discovery_chain(ac4_ctx *ctx) {
    json_t *chain = ac4_call(ctx, "get_provider_chain", json_object());
    if (ac4_exc_pending(ctx)) { json_free(chain); return NULL; }
    json_t *tried = json_array();
    json_t *result = NULL;
    if (ac4_is_arr(chain)) {
        for (size_t i = 0; i < chain->c.count && !result; i++) {
            json_t *row = chain->c.items[i];
            if (!ac4_is_arr(row) || row->c.count < 2) continue;
            const char *label = ac4_jstr(row->c.items[0]);
            const char *marker = ac4_jstr(row->c.items[1]);
            json_t *a1 = json_object();
            ac4_arg_str(a1, "label", label);
            json_t *bu = ac4_call(ctx, "custom_health_base_url", a1);
            if (ac4_exc_pending(ctx)) { json_free(bu); break; }
            json_t *a2 = json_object();
            ac4_arg_str(a2, "label", label);
            ac4_arg(a2, "base_url", bu);
            json_t *unhealthy = ac4_call(ctx, "is_provider_unhealthy", a2);
            json_t *bu_copy = bu ? json_copy(bu) : NULL;
            json_free(bu);
            if (ac4_exc_pending(ctx)) { json_free(unhealthy); break; }
            bool skip = ac4_truthy(unhealthy);
            json_free(unhealthy);
            if (skip) {
                { json_t *aa = json_object();
                  ac4_arg_str(aa, "label", label);
                  ac4_arg(aa, "base_url", bu_copy);
                  json_free(ac4_call(ctx, "log_skip_unhealthy", aa)); }
                if (ac4_exc_pending(ctx)) break;
                char t[256]; snprintf(t, sizeof t, "%s (unhealthy)", label);
                json_append(tried, json_string(t));
                json_free(bu_copy);
                continue;
            }
            char name[128]; snprintf(name, sizeof name, "try_fn.%s", marker);
            json_t *tf = ac4_call(ctx, name, json_object());
            if (ac4_exc_pending(ctx)) { json_free(tf); break; }
            json_t *cl = (ac4_is_arr(tf) && tf->c.count > 0) ? json_copy(tf->c.items[0]) : json_null();
            json_t *md = (ac4_is_arr(tf) && tf->c.count > 1) ? json_copy(tf->c.items[1]) : json_null();
            json_free(tf);
            if (!ac4_is_none(cl)) {
                json_t *arr = json_array();
                json_append(arr, json_string(label));
                json_append(arr, ac4_truthy(md) ? json_copy(md) : json_string("default"));
                if (tried->c.count > 0) {
                    char joined[1024]; size_t k = 0;
                    for (size_t j = 0; j < tried->c.count; j++) {
                        const char *s = ac4_jstr(tried->c.items[j]);
                        size_t n = strlen(s);
                        if (k + n + 2 < sizeof joined) {
                            if (j) joined[k++] = ',', joined[k++] = ' ';
                            memcpy(joined + k, s, n); k += n;
                        }
                    }
                    joined[k] = '\0';
                    json_append(arr, json_string(joined));
                    ac4_log(ctx, "info", "Auxiliary auto-detect: using %s (%s) — skipped: %s", arr, false);
                } else {
                    ac4_log(ctx, "info", "Auxiliary auto-detect: using %s (%s)", arr, false);
                }
                result = json_array();
                json_append(result, cl);
                json_append(result, md);
                json_append(result, json_string(label));
                break;
            }
            json_free(cl); json_free(md);
            json_append(tried, json_string(label));
        }
    }
    json_free(chain);
    if (ac4_exc_pending(ctx)) { json_free(tried); return NULL; }
    if (!result) {
        char joined[1024]; size_t k = 0;
        for (size_t j = 0; j < tried->c.count; j++) {
            const char *s = ac4_jstr(tried->c.items[j]);
            size_t n = strlen(s);
            if (k + n + 2 < sizeof joined) {
                if (j) joined[k++] = ',', joined[k++] = ' ';
                memcpy(joined + k, s, n); k += n;
            }
        }
        joined[k] = '\0';
        json_t *arr = json_array();
        json_append(arr, json_string(joined));
        ac4_log(ctx, "warning",
            "Auxiliary auto-detect: no provider available (tried: %s). "
            "Compression, summarization, and memory flush will not work. "
            "Set OPENROUTER_API_KEY or configure a local model in config.yaml.",
            arr, false);
        result = json_array();
        json_append(result, json_null());
        json_append(result, json_null());
        json_append(result, json_string(""));
    }
    json_free(tried);
    return result;
}

/* ── 10. _translate_anthropic_response_format ───────────────────────────── */
/* PoP: ac4_translate_anthropic_response_format @ agent/auxiliary_client.py:_translate_anthropic_response_format */
static void ac4_translate_anthropic_response_format(json_t *anthropic_kwargs,
        const json_t *response_format) {
    if (!ac4_is_obj(response_format)) return;
    const char *ft = ac4_gets(response_format, "type");
    json_t *schema = NULL;
    if (ac4_str_eq(ft, "json_schema")) {
        json_t *js = ac4_get(response_format, "json_schema");
        if (!ac4_is_obj(js) || !ac4_get(js, "schema")) return;
        schema = json_copy(ac4_get(js, "schema"));
    } else if (ac4_str_eq(ft, "json_object")) {
        schema = json_object();
        json_set(schema, "type", json_string("object"));
    } else return;
    json_t *oc = ac4_get(anthropic_kwargs, "output_config");
    if (!ac4_is_obj(oc)) {
        oc = json_object();
        json_set(anthropic_kwargs, "output_config", oc);
    }
    json_t *fmt = json_object();
    json_set(fmt, "type", json_string("json_schema"));
    json_set(fmt, "schema", schema);
    json_set(oc, "format", fmt);
}

/* ── 11. _pool_credential_digest ─────────────────────────────────────────── */
/* PoP: ac4_pool_credential_digest @ agent/auxiliary_client.py:_pool_credential_digest */
static json_t *ac4_pool_credential_digest(ac4_ctx *ctx, const json_t *pool, const json_t *entry) {
    json_t *keys = json_array();
    if (!ac4_is_none(entry)) {
        json_t *a = json_object();
        ac4_arg(a, "entry", entry);
        json_t *k = ac4_call(ctx, "pool_runtime_api_key", a);
        if (ac4_exc_pending(ctx)) { json_free(k); json_free(keys); return NULL; }
        if (ac4_truthy(k)) json_append(keys, json_copy(k));
        json_free(k);
    }
    if (keys->c.count == 0) {
        json_t *a = json_object();
        ac4_arg(a, "pool", pool);
        json_t *entries = ac4_call(ctx, "pool.entries", a);
        if (ac4_exc_pending(ctx)) {
            json_free(entries);
            if (ac4_exc_is_exception(ctx)) {
                json_t *arr = json_array();
                json_append(arr, ac4_exc_arg(ctx));
                ac4_log(ctx, "debug",
                    "Auxiliary client: could not list pool entries for digest: %s", arr, false);
                ac4_exc_clear(ctx);
            }
            json_free(keys);
            return json_string("");
        }
        if (ac4_is_arr(entries)) {
            for (size_t i = 0; i < entries->c.count; i++) {
                json_t *a2 = json_object();
                ac4_arg(a2, "entry", entries->c.items[i]);
                json_t *k = ac4_call(ctx, "pool_runtime_api_key", a2);
                if (ac4_exc_pending(ctx)) { json_free(k); json_free(entries); json_free(keys); return NULL; }
                if (ac4_truthy(k)) json_append(keys, json_copy(k));
                json_free(k);
            }
        }
        json_free(entries);
    }
    if (keys->c.count == 0) { json_free(keys); return json_string(""); }
    /* sorted(...) */
    for (size_t i = 0; i < keys->c.count; i++)
        for (size_t j = i + 1; j < keys->c.count; j++)
            if (strcmp(ac4_jstr(keys->c.items[i]), ac4_jstr(keys->c.items[j])) > 0) {
                json_t *tmp = keys->c.items[i]; keys->c.items[i] = keys->c.items[j]; keys->c.items[j] = tmp;
            }
    json_t *a = json_object();
    json_set(a, "keys", keys);
    return ac4_call(ctx, "blake2b_hexdigest", a);
}
