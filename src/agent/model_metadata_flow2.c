/* model_metadata_flow2.c — FLOW port of the UNCLAIMED remainder of
 * agent/model_metadata.py (second flow swath; model_metadata_flow.c covers the
 * first). Every function below has NO PoP claim in src/ or include/ at claim
 * time (CRLF-aware PoP grep of agent/model_metadata.py names). Claimed siblings
 * and foreign collaborators ride the injected universal seam mm2_call, which
 * the harness scripts and records as an ordered trace the oracle diffs against
 * LIVE agent/model_metadata.py under the same scripts.
 *
 * Representation contract (shared with tests/oracle_model_metadata_flow2.py):
 *   - None -> JSON null; integral floats collapse to ints; Path -> POSIX str.
 *   - uncaught exception -> {"__raise__": cls}; an exception used as a seam /
 *     logger arg projects to {"__raise__": cls, "msg": str(e)}.
 *   - scripts: absent key -> None; {"$seq":[...]} per-call queue (exhaustion
 *     raises RuntimeError("script exhausted: <name>")); {"__raise__":cls,...}
 *     raises (trace ret is the WHOLE marker); any other value is reused.
 *     None-valued seam arg keys are dropped on BOTH sides ("" is kept).
 *   - FULL builtin exception hierarchy: `except Exception` never catches the
 *     BaseException trio (SystemExit/KeyboardInterrupt/GeneratorExit), and
 *     `except ImportError` catches ImportError/ModuleNotFoundError only.
 *   - fixture domain: URLs/paths are POSIX ASCII; ids/keys are opaque strings;
 *     scripted payloads are dicts/lists/scalars; numeric overrides are integral.
 *
 * libjson-only. Self-contained: the harness #includes "agent/model_metadata_flow2.c".
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>

/* ── context + primitives ────────────────────────────────────────────────── */

typedef struct mm2_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *env;       /* borrowed: scripted flags/constants */
    json_t *state;     /* owned: per-case mutable state window */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} mm2_ctx;

static bool mm2_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool mm2_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool mm2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool mm2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool mm2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool mm2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool mm2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static bool mm2_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:  return v->c.count > 0;
        case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *mm2_jstr(const json_t *v) {
    return (mm2_is_str(v) && v->str_val) ? v->str_val : "";
}
static json_t *mm2_get(const json_t *obj, const char *key) {
    return mm2_is_obj(obj) ? json_obj_get(obj, key) : NULL;
}
static json_t *mm2_copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}
static char *mm2_pystr(const json_t *v) {
    if (mm2_is_none(v)) return strdup("None");
    if (mm2_is_bool(v)) return strdup(v->bool_val ? "True" : "False");
    if (mm2_is_num(v)) {
        char buf[64]; double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
        else snprintf(buf, sizeof buf, "%g", d);
        return strdup(buf);
    }
    if (mm2_is_str(v)) return strdup(v->str_val ? v->str_val : "");
    return strdup("");
}

/* ── exception plumbing: FULL builtin hierarchy ─────────────────────────── */

static bool mm2_exc_pending(const mm2_ctx *ctx) { return ctx && ctx->exc != NULL; }
static void mm2_exc_clear(mm2_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}
static void mm2_raise(mm2_ctx *ctx, const char *cls, const char *msg) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
    json_set(ctx->exc, "msg", json_string(msg ? msg : ""));
}
static const char *mm2_exc_class(const mm2_ctx *ctx) {
    return json_get_str(ctx->exc, "__raise__", "");
}
/* The BaseException trio links ONLY to BaseException so `except Exception` and
 * `except ImportError` never catch them. Unknown classes default to Exception. */
static const char *mm2_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        {"BaseException", NULL},
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"},
        {"Exception", "BaseException"},
        {"ArithmeticError", "Exception"}, {"FloatingPointError", "ArithmeticError"},
        {"OverflowError", "ArithmeticError"}, {"ZeroDivisionError", "ArithmeticError"},
        {"AssertionError", "Exception"}, {"AttributeError", "Exception"},
        {"BufferError", "Exception"}, {"EOFError", "Exception"},
        {"ImportError", "Exception"}, {"ModuleNotFoundError", "ImportError"},
        {"LookupError", "Exception"}, {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
        {"MemoryError", "Exception"}, {"NameError", "Exception"}, {"UnboundLocalError", "NameError"},
        {"OSError", "Exception"}, {"BlockingIOError", "OSError"}, {"ChildProcessError", "OSError"},
        {"ConnectionError", "OSError"}, {"BrokenPipeError", "ConnectionError"},
        {"ConnectionAbortedError", "ConnectionError"}, {"ConnectionRefusedError", "ConnectionError"},
        {"ConnectionResetError", "ConnectionError"}, {"FileExistsError", "OSError"},
        {"FileNotFoundError", "OSError"}, {"InterruptedError", "OSError"},
        {"IsADirectoryError", "OSError"}, {"NotADirectoryError", "OSError"},
        {"PermissionError", "OSError"}, {"ProcessLookupError", "OSError"},
        {"TimeoutError", "OSError"},
        {"ReferenceError", "Exception"}, {"RuntimeError", "Exception"},
        {"NotImplementedError", "RuntimeError"}, {"RecursionError", "RuntimeError"},
        {"StopAsyncIteration", "Exception"}, {"StopIteration", "Exception"},
        {"SyntaxError", "Exception"}, {"IndentationError", "SyntaxError"},
        {"TabError", "IndentationError"}, {"SystemError", "Exception"},
        {"TypeError", "Exception"}, {"ValueError", "Exception"},
        {"UnicodeError", "ValueError"}, {"UnicodeDecodeError", "UnicodeError"},
        {"UnicodeEncodeError", "UnicodeError"}, {"UnicodeTranslateError", "UnicodeError"},
        {"JSONDecodeError", "ValueError"},
        {"Warning", "Exception"}, {"UserWarning", "Warning"},
        {"DeprecationWarning", "Warning"}, {"PendingDeprecationWarning", "Warning"},
        {"SyntaxWarning", "Warning"}, {"RuntimeWarning", "Warning"},
        {"FutureWarning", "Warning"}, {"ImportWarning", "Warning"},
        {"UnicodeWarning", "Warning"}, {"BytesWarning", "Warning"},
        {"ResourceWarning", "Warning"}, {"EncodingWarning", "Warning"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (mm2_str_eq(T[i].c, c)) return T[i].p;
    if (mm2_str_eq(c, "BaseException")) return NULL;
    return "Exception";
}
static bool mm2_exc_matches(const mm2_ctx *ctx, const char *const *classes, size_t n) {
    if (!mm2_exc_pending(ctx)) return false;
    const char *cls = mm2_exc_class(ctx);
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (mm2_str_eq(cls, classes[i])) return true;
        const char *p = mm2_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}
static bool mm2_exc_is_exception(const mm2_ctx *ctx) {
    static const char *E[] = {"Exception"};
    return mm2_exc_matches(ctx, E, 1);
}
static json_t *mm2_exc_arg(const mm2_ctx *ctx) {
    json_t *a = json_object();
    json_set(a, "__raise__", json_string(mm2_exc_class(ctx)));
    json_set(a, "msg", json_string(json_get_str(ctx->exc, "msg", "")));
    return a;
}

/* ── universal seam ──────────────────────────────────────────────────────── */

static json_t *mm2_script_result(mm2_ctx *ctx, const json_t *script) {
    if (!script || script->type == JSON_NULL) return NULL;
    if (script->type == JSON_OBJECT) {
        json_t *r = json_obj_get(script, "__raise__");
        if (r && r->type == JSON_STRING) {
            if (ctx->exc) json_free(ctx->exc);
            ctx->exc = json_copy(script);
            return NULL;
        }
    }
    return json_copy(script);
}
/* args ownership TRANSFERRED (NULL -> empty object). Result is NEW. */
static json_t *mm2_call(mm2_ctx *ctx, const char *name, json_t *args) {
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
    if (chosen != ctx->exc) ret = mm2_script_result(ctx, chosen);
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
static void mm2_arg(json_t *args, const char *key, const json_t *val) {
    if (!val || val->type == JSON_NULL) return;
    json_set(args, key, json_copy(val));
}
static void mm2_arg_str(json_t *args, const char *key, const char *val) {
    if (!val) return;
    json_set(args, key, json_string(val));
}
static void mm2_arg_num(json_t *args, const char *key, double val) {
    json_set(args, key, json_number(val));
}
/* logger seam: {"msg","args"}; exception args -> str(exc) (the message). */
static void mm2_log(mm2_ctx *ctx, const char *level, const char *msg, json_t *args_arr) {
    json_t *a = json_object();
    json_set(a, "msg", json_string(msg));
    json_set(a, "args", args_arr ? args_arr : json_array());
    char name[40];
    snprintf(name, sizeof name, "logger.%s", level);
    json_free(mm2_call(ctx, name, a));
}

/* ── ported functions (all UNCLAIMED at claim time) ─────────────────────── */

/* PoP: mm2_ollama_show @ agent/model_metadata.py:_ollama_show */
static json_t *mm2_ollama_show(mm2_ctx *ctx, json_t *in) {
    const json_t *server_url = mm2_get(in, "server_url");
    const json_t *api_key = mm2_get(in, "api_key");
    const json_t *bare_model = mm2_get(in, "bare_model");
    const json_t *timeout = mm2_get(in, "timeout");
    bool note_blackhole = mm2_truthy(mm2_get(in, "note_blackhole"));

    json_t *headers = NULL, *verify = NULL, *client = NULL, *resp = NULL;
    /* try: with httpx.Client(timeout, headers=_auth_headers(api_key),
     *                         verify=resolve_verify(server_url)) as client:
     * kwarg evaluation order: timeout (plain), auth_headers, resolve_verify, Client. */
    json_t *a = json_object();
    mm2_arg(a, "api_key", api_key);
    headers = mm2_call(ctx, "auth_headers", a);
    if (mm2_exc_pending(ctx)) goto handler;

    a = json_object();
    mm2_arg(a, "base_url", server_url);
    verify = mm2_call(ctx, "resolve_verify", a);
    if (mm2_exc_pending(ctx)) goto handler;

    a = json_object();
    if (mm2_is_none(timeout)) mm2_arg_num(a, "timeout", 3.0);
    else mm2_arg(a, "timeout", timeout);
    mm2_arg(a, "headers", headers);
    mm2_arg(a, "verify", verify);
    client = mm2_call(ctx, "httpx_client", a);
    if (mm2_exc_pending(ctx)) goto handler;

    char url[1024];
    snprintf(url, sizeof url, "%s/api/show", mm2_jstr(server_url));
    a = json_object();
    mm2_arg_str(a, "url", url);
    json_t *jbody = json_object();
    mm2_arg(jbody, "name", bare_model);
    json_set(a, "json", jbody);
    resp = mm2_call(ctx, "post", a);
    if (mm2_exc_pending(ctx)) goto handler;

    {
        double sc = json_get_num(resp, "status_code", 0);
        json_t *result = (sc == 200) ? mm2_copy_or_null(json_obj_get(resp, "json")) : NULL;
        json_free(resp); json_free(client); json_free(verify); json_free(headers);
        return result ? result : json_null();
    }

handler:
    json_free(resp); json_free(client); json_free(verify); json_free(headers);
    if (mm2_exc_is_exception(ctx)) {
        if (note_blackhole) {
            a = json_object();
            json_set(a, "exc", mm2_exc_arg(ctx));
            mm2_arg(a, "base_url", server_url);
            json_free(mm2_call(ctx, "note_if_connect_timeout", a));
        }
        mm2_exc_clear(ctx);
    }
    return json_null();  /* BaseException trio stays pending (propagates) */
}

/* PoP: mm2_lmstudio_context @ agent/model_metadata.py:_lmstudio_context */
static json_t *mm2_lmstudio_context(mm2_ctx *ctx, json_t *in) {
    const json_t *url = mm2_get(in, "lmstudio_url");
    const json_t *model = mm2_get(in, "model");
    char full[1024];
    snprintf(full, sizeof full, "%s/api/v1/models", mm2_jstr(url));
    json_t *a = json_object();
    mm2_arg_str(a, "url", full);
    json_t *resp = mm2_call(ctx, "get", a);
    if (mm2_exc_pending(ctx)) { json_free(resp); return json_null(); }

    double sc = json_get_num(resp, "status_code", 0);
    json_t *ret = json_null();
    if (sc == 200) {
        json_t *j = json_obj_get(resp, "json");
        json_t *arr = mm2_is_obj(j) ? json_obj_get(j, "models") : NULL;
        if (mm2_is_arr(arr)) {
            for (size_t i = 0; i < arr->c.count; i++) {
                json_t *m = arr->c.items[i];
                bool matched = false;
                for (int k = 0; k < 2 && !matched; k++) {
                    a = json_object();
                    mm2_arg_str(a, "candidate_id", mm2_jstr(mm2_get(m, k == 0 ? "key" : "id")));
                    mm2_arg(a, "lookup_model", model);
                    json_t *r = mm2_call(ctx, "model_id_matches", a);
                    if (mm2_exc_pending(ctx)) { json_free(r); json_free(resp); return json_null(); }
                    matched = mm2_truthy(r);
                    json_free(r);
                }
                if (matched) {
                    /* return next((ctx for ctx in (...) ...), None) — lazy: first non-None wins. */
                    json_t *insts = mm2_get(m, "loaded_instances");
                    if (mm2_is_arr(insts)) {
                        for (size_t q = 0; q < insts->c.count && mm2_is_none(ret); q++) {
                            json_t *inst = insts->c.items[q];
                            json_t *cfg = mm2_get(inst, "config");
                            if (!mm2_is_obj(cfg)) cfg = NULL;
                            a = json_object();
                            mm2_arg(a, "value", cfg ? json_obj_get(cfg, "context_length") : NULL);
                            json_t *r = mm2_call(ctx, "positive_int", a);
                            if (mm2_exc_pending(ctx)) { json_free(r); json_free(resp); return json_null(); }
                            if (!mm2_is_none(r)) { json_free(ret); ret = json_copy(r); }
                            json_free(r);
                        }
                    }
                    break;  /* first matching model returns immediately */
                }
            }
        }
    }
    json_free(resp);
    return ret;
}

/* PoP: mm2_openai_models_list_context @ agent/model_metadata.py:_openai_models_list_context */
static json_t *mm2_openai_models_list_context(mm2_ctx *ctx, json_t *in) {
    const json_t *url = mm2_get(in, "server_url");
    const json_t *model = mm2_get(in, "model");
    char full[1024];
    snprintf(full, sizeof full, "%s/v1/models", mm2_jstr(url));
    json_t *a = json_object();
    mm2_arg_str(a, "url", full);
    json_t *resp = mm2_call(ctx, "get", a);
    if (mm2_exc_pending(ctx)) { json_free(resp); return json_null(); }

    json_t *ret = json_null();
    double sc = json_get_num(resp, "status_code", 0);
    if (sc != 200) { json_free(resp); return json_null(); }

    json_t *j = json_obj_get(resp, "json");
    json_t *data = mm2_is_obj(j) ? json_obj_get(j, "data") : NULL;
    if (!mm2_is_arr(data)) { json_free(resp); return json_null(); }

    json_t *matched = NULL;
    for (size_t i = 0; i < data->c.count && !matched; i++) {
        json_t *m = data->c.items[i];
        if (!mm2_is_obj(m)) continue;
        a = json_object();
        mm2_arg_str(a, "candidate_id", mm2_jstr(mm2_get(m, "id")));
        mm2_arg(a, "lookup_model", model);
        json_t *r = mm2_call(ctx, "model_id_matches", a);
        if (mm2_exc_pending(ctx)) { json_free(r); json_free(resp); return json_null(); }
        if (mm2_truthy(r)) matched = m;
        json_free(r);
    }
    if (!matched && data->c.count == 1) matched = data->c.items[0];
    if (!matched) { json_free(resp); return json_null(); }

    if (!mm2_is_obj(matched)) {
        mm2_raise(ctx, "AttributeError", "'object' object has no attribute 'get'");
        json_free(resp);
        return json_null();
    }
    json_t *sources[2]; size_t ns = 0;
    sources[ns++] = matched;
    /* Python: (matched.get("meta") or {}) — a FALSY meta becomes {}, which is a
     * dict and IS included in sources; a truthy non-dict is excluded. */
    json_t *meta = mm2_get(matched, "meta");
    if (mm2_truthy(meta)) {
        if (mm2_is_obj(meta)) sources[ns++] = meta;
    } else {
        sources[ns++] = json_object();
    }

    /* reader 1: _positive_int(src.get("n_ctx")) across sources in order */
    for (size_t s = 0; s < ns && mm2_is_none(ret); s++) {
        a = json_object();
        mm2_arg(a, "value", json_obj_get(sources[s], "n_ctx"));
        json_t *r = mm2_call(ctx, "positive_int", a);
        if (mm2_exc_pending(ctx)) { json_free(r); json_free(resp); return json_null(); }
        if (!mm2_is_none(r)) { json_free(ret); ret = json_copy(r); }
        json_free(r);
    }
    /* reader 2: _context_length_from_model_payload(src) across sources in order */
    for (size_t s = 0; s < ns && mm2_is_none(ret); s++) {
        a = json_object();
        mm2_arg(a, "payload", sources[s]);
        json_t *r = mm2_call(ctx, "context_length_from_model_payload", a);
        if (mm2_exc_pending(ctx)) { json_free(r); json_free(resp); return json_null(); }
        if (!mm2_is_none(r)) { json_free(ret); ret = json_copy(r); }
        json_free(r);
    }
    json_free(resp);
    return ret;
}

/* PoP: mm2_load_context_cache_document @ agent/model_metadata.py:_load_context_cache_document */
static json_t *mm2_load_context_cache_document(mm2_ctx *ctx, json_t *in) {
    (void)in;
    json_t *a = json_object();
    json_t *p = mm2_call(ctx, "get_context_cache_path", a);
    if (mm2_exc_pending(ctx)) { json_free(p); return json_object(); }

    const char *path = mm2_jstr(p);
    a = json_object();
    mm2_arg_str(a, "path", path);
    json_t *ex = mm2_call(ctx, "exists", a);
    if (mm2_exc_pending(ctx)) { json_free(ex); json_free(p); return json_object(); }
    if (!mm2_truthy(ex)) { json_free(ex); json_free(p); return json_object(); }
    json_free(ex);

    a = json_object();
    mm2_arg_str(a, "path", path);
    mm2_arg_str(a, "encoding", "utf-8-sig");
    json_free(mm2_call(ctx, "open", a));
    if (mm2_exc_pending(ctx)) goto handler;

    a = json_object();
    json_t *data = mm2_call(ctx, "yaml.safe_load", a);
    if (mm2_exc_pending(ctx)) goto handler;
    if (!mm2_is_obj(data)) { json_free(data); json_free(p); return json_object(); }

    {
        json_t *out = json_copy(data);
        static const char *sections[] = {"context_lengths", "bedrock_confirmed_v1"};
        for (size_t i = 0; i < 2; i++) {
            json_t *v = json_obj_get(out, sections[i]);
            if (!mm2_is_obj(v)) json_set(out, sections[i], json_object());
        }
        json_free(data); json_free(p);
        return out;
    }

handler:
    json_free(p);
    if (mm2_exc_is_exception(ctx)) {
        json_t *args = json_array();
        char *m = mm2_pystr(json_obj_get(ctx->exc, "msg"));
        json_append(args, json_string(m ? m : ""));
        free(m);
        mm2_log(ctx, "debug", "Failed to load context length cache: %s", args);
        mm2_exc_clear(ctx);
    }
    return json_object();
}

/* PoP: mm2_config_override_context_length @ agent/model_metadata.py:_config_override_context_length */
static json_t *mm2_config_override_context_length(mm2_ctx *ctx, json_t *in) {
    const json_t *model = mm2_get(in, "model");
    const json_t *base_url = mm2_get(in, "base_url");
    const json_t *provider = mm2_get(in, "provider");
    const json_t *cp = mm2_get(in, "custom_providers");

    /* 0b: with contextlib.suppress(Exception): _override_context_window(provider, model) */
    if (mm2_truthy(provider) && mm2_truthy(model)) {
        json_t *a = json_object();
        mm2_arg(a, "provider", provider);
        mm2_arg(a, "model", model);
        json_t *mo = mm2_call(ctx, "override_context_window", a);
        if (mm2_exc_pending(ctx)) {
            if (mm2_exc_is_exception(ctx)) mm2_exc_clear(ctx);
            else return json_null();
        } else {
            if (mm2_is_num(mo) && mo->num_val > 0) { double v = mo->num_val; json_free(mo); return json_number(v); }
            json_free(mo);
        }
    }
    /* 0c: with contextlib.suppress(Exception): get_custom_provider_context_length(...) */
    if (mm2_truthy(base_url) && mm2_truthy(model)) {
        json_t *a = json_object();
        mm2_arg(a, "model", model);
        mm2_arg(a, "base_url", base_url);
        mm2_arg(a, "custom_providers", cp);
        json_t *cv = mm2_call(ctx, "get_custom_provider_context_length", a);
        if (mm2_exc_pending(ctx)) {
            if (mm2_exc_is_exception(ctx)) mm2_exc_clear(ctx);
            else return json_null();
        } else {
            if (mm2_truthy(cv)) return cv;
            json_free(cv);
        }
    }
    return json_null();
}

/* PoP: mm2_lmstudio_native_models @ agent/model_metadata.py:_lmstudio_native_models */
static json_t *mm2_lmstudio_native_models(mm2_ctx *ctx, json_t *in) {
    const json_t *normalized = mm2_get(in, "normalized");
    const json_t *headers = mm2_get(in, "headers");
    json_t *a;

    a = json_object();
    mm2_arg(a, "base_url", normalized);
    json_t *root = mm2_call(ctx, "lmstudio_server_root", a);
    if (mm2_exc_pending(ctx)) { json_free(root); return json_null(); }

    a = json_object();
    mm2_arg(a, "base_url", normalized);
    json_t *verify = mm2_call(ctx, "resolve_verify", a);
    if (mm2_exc_pending(ctx)) { json_free(root); json_free(verify); return json_null(); }

    char base[1024];
    snprintf(base, sizeof base, "%s", mm2_jstr(root));
    size_t bl = strlen(base);
    while (bl > 0 && base[bl - 1] == '/') base[--bl] = '\0';
    char url[1100];
    snprintf(url, sizeof url, "%s/api/v1/models", base);

    a = json_object();
    mm2_arg_str(a, "url", url);
    mm2_arg(a, "headers", headers);
    json_t *to = json_array();
    json_append(to, json_number(5));
    json_append(to, json_number(10));
    json_set(a, "timeout", to);
    mm2_arg(a, "verify", verify);
    json_t *resp = mm2_call(ctx, "http_get", a);
    if (mm2_exc_pending(ctx)) { json_free(resp); json_free(verify); json_free(root); return json_null(); }

    a = json_object();
    json_free(mm2_call(ctx, "raise_for_status", a));
    if (mm2_exc_pending(ctx)) { json_free(resp); json_free(verify); json_free(root); return json_null(); }

    json_t *cache = json_object();
    json_t *j = json_obj_get(resp, "json");
    json_t *arr = mm2_is_obj(j) ? json_obj_get(j, "models") : NULL;
    if (mm2_is_arr(arr)) {
        for (size_t i = 0; i < arr->c.count; i++) {
            json_t *m = arr->c.items[i];
            const json_t *mid = NULL;
            if (mm2_is_obj(m)) {
                const json_t *k = json_obj_get(m, "key");
                mid = mm2_truthy(k) ? k : json_obj_get(m, "id");
            }
            if (!mm2_truthy(mid)) continue;

            a = json_object();
            mm2_arg(a, "model", m);
            json_t *lc = mm2_call(ctx, "lmstudio_loaded_context", a);
            if (mm2_exc_pending(ctx)) { json_free(lc); goto fail_models; }

            a = json_object();
            mm2_arg(a, "model", m);
            mm2_arg(a, "model_id", mid);
            mm2_arg(a, "context_length", lc);
            json_t *entry = mm2_call(ctx, "endpoint_model_entry", a);
            if (mm2_exc_pending(ctx)) { json_free(entry); json_free(lc); goto fail_models; }

            a = json_object();
            json_set(a, "cache", json_copy(cache));
            mm2_arg(a, "model_id", mid);
            mm2_arg(a, "entry", entry);
            json_free(mm2_call(ctx, "add_model_aliases", a));
            if (mm2_exc_pending(ctx)) { json_free(entry); json_free(lc); goto fail_models; }
            json_set(cache, mm2_jstr(mid), json_copy(entry));
            /* _add_model_aliases also seeds the slug after the first "/" via
             * setdefault (publisher/slug native ids). */
            {
                const char *slash = strchr(mm2_jstr(mid), '/');
                if (slash && slash[1]) {
                    char slug[512];
                    size_t sn = strlen(slash + 1);
                    if (sn >= sizeof slug) sn = sizeof slug - 1;
                    memcpy(slug, slash + 1, sn); slug[sn] = '\0';
                    if (!json_obj_get(cache, slug))
                        json_set(cache, slug, json_copy(entry));
                }
            }

            const json_t *alt = mm2_is_obj(m) ? json_obj_get(m, "id") : NULL;
            if (mm2_is_str(alt) && alt->str_val && alt->str_val[0] &&
                !mm2_str_eq(mm2_jstr(alt), mm2_jstr(mid))) {
                a = json_object();
                json_set(a, "cache", json_copy(cache));
                mm2_arg(a, "model_id", alt);
                mm2_arg(a, "entry", entry);
                json_free(mm2_call(ctx, "add_model_aliases", a));
                if (mm2_exc_pending(ctx)) { json_free(entry); json_free(lc); goto fail_models; }
                if (!json_obj_get(cache, mm2_jstr(alt)))
                    json_set(cache, mm2_jstr(alt), json_copy(entry));
            }
            json_free(entry);
            json_free(lc);
        }
    }
    json_free(resp); json_free(verify); json_free(root);
    return cache;

fail_models:
    json_free(resp); json_free(verify); json_free(root); json_free(cache);
    return json_null();
}

/* ── dispatch (static; the harness calls this) ──────────────────────────── */

static json_t *mm2_dispatch(mm2_ctx *ctx, const char *func, json_t *in) {
    if (mm2_str_eq(func, "_ollama_show")) return mm2_ollama_show(ctx, in);
    if (mm2_str_eq(func, "_lmstudio_context")) return mm2_lmstudio_context(ctx, in);
    if (mm2_str_eq(func, "_openai_models_list_context")) return mm2_openai_models_list_context(ctx, in);
    if (mm2_str_eq(func, "_load_context_cache_document")) return mm2_load_context_cache_document(ctx, in);
    if (mm2_str_eq(func, "_config_override_context_length")) return mm2_config_override_context_length(ctx, in);
    if (mm2_str_eq(func, "_lmstudio_native_models")) return mm2_lmstudio_native_models(ctx, in);
    return NULL;
}
