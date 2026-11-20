/* chat_completions_flow2.c — FLOW + leaves port of the UNCLAIMED remainder of
 * agent/transports/chat_completions.py (quarry: C:/Users/eman5/hermes-agent,
 * LIVE authority). Wave 2 after openai_transport_flow.c (flow1, 15 claims).
 *
 * Claim landscape (grep PoP by PYTHON name first):
 *   - flow1 (openai_transport_flow.c) faithfully ports 15 transport leaves/flows;
 *     chat_completion_helpers_flow.c ports is_router_timeout_shim /
 *     router_timeout_shim_may_follow / _has_positive_completion_tokens;
 *     provider_profile.c ports _reasoning_config_for_model. Those claimed
 *     siblings are SEAMED here (never re-ported).
 *   - 16 defs are claimed ONLY by the printf-stub / strstr-era fakes in
 *     chat_completions_ports.c (cct_*: no live behaviour, never gated — see the
 *     skill's stub-port-audit / stub-purge doctrine: a real port of the same
 *     (module,func) pair is the sanctioned revival that lets the stub die):
 *     _build_gemini_thinking_config, _snake_case_gemini_thinking_config,
 *     _is_gemini_openai_compat_base_url, _model_consumes_thought_signature,
 *     api_mode, convert_messages, convert_tools, build_kwargs,
 *     _build_kwargs_from_profile, normalize_response, validate_response,
 *     extract_cache_stats, _static_prompt_instructions, _add_prompt_cache_key,
 *     _is_openai_api_base_url, _raise_gemini_thinking_max_tokens.
 *   16 targets total. Ported functions compose REAL (internal cc3_* composition);
 *   every claimed sibling / foreign-module import / profile-method / self-method
 *   double rides the injected universal seam cc3_call.
 *
 * ── Universal seam contract (shared with flow1) ──────────────────────────────
 *   json_t *cc3_call(cc3_ctx *ctx, const char *name, json_t *args)
 * args ownership is TRANSFERRED (freed after the call; NULL -> empty object).
 * Result is a NEW json_t owned by the caller: NULL or a JSON_NULL node == Python
 * None; an object carrying a string "__raise__" key ({"__raise__":cls,"msg":m})
 * == an exception of that class with str(exc)==msg, propagated or caught exactly
 * where the live code does (BaseException-only kinds pass through every
 * `except Exception` / `except (TypeError, ValueError)` site). The harness
 * records one {ev,args,ret} event per call (ret = RAW scripted value) in order.
 *
 * Script shapes (fixture "io" object, per seam name):
 *   MISSING key        -> fail-closed RuntimeError("no script for <name>")
 *   null               -> None on every call
 *   {"$seq":[v1,v2..]} -> vi on the i-th call (exhaustion raises
 *                         RuntimeError("script exhausted: <name>"))
 *   {"__raise__":cls,"msg":m,...} -> raises (trace ret is the WHOLE marker)
 *   any other value    -> reused (a bare ARRAY is a LIST RESULT, not a queue)
 * None-valued seam arg keys dropped on BOTH sides.
 *
 * ── Seam name table (name -> live call) ──────────────────────────────────────
 *  claimed siblings (flow1 / helpers / provider_profile):
 *   _profile_replays_tool_call_signatures  {provider}
 *   _wire_reasoning_carriers               {provider, base_url}
 *   _route_requires_mistral_tool_ids       {provider, model, base_url}
 *   _sanitize_message                      {msg, strip_extra_content, strip_reasoning,
 *                                           native_reasoning_details_type, mistral_tool_ids}
 *   _swap_developer_role                   {sanitized, model_lower}
 *   _base_kwargs                           {model, sanitized, tools, params, profile?}
 *   _apply_max_tokens                      {api_kwargs, model, reasoning_config, params,
 *                                           profile_max?}  (void: {"__update__":{}} merges)
 *   _pareto_score                          {raw}
 *   _finish_kwargs                         {api_kwargs, sanitized, params,
 *                                           supports_prompt_cache_key}
 *   _normalize_tool_call                   {tc}  (self-method, self stripped)
 *   _attr_or_model_extra                   {obj, name}
 *   _dump_extra_content                    {extra}
 *   usage_from_openai                      {u}   (Usage.from_openai, types.py)
 *   is_router_timeout_shim                 {response}
 *   _reasoning_config_for_model            {model, reasoning_config, base_url}
 *  foreign-module imports / profile-method / self-method doubles:
 *   gemini_uses_thinking_level             {model}
 *   clamp_effort                           {effort, supported, overrides?}
 *   _effective_gemini_max_output_tokens    {requested, thinking_config}
 *   base_url_host_matches                  {base_url, host}
 *   _bound_prompt_cache_key_field          {container}  (void: {"__update__":{}} /
 *                                           {"__pop__":[keys]} mutate the container)
 *   _cache_scope_from_session_id           {session_id}
 *   _content_cache_key                     {instructions, tools, scope_id}
 *   data_use_requirement                   {messages}
 *   requested_effort                       {reasoning_config}
 *   kimi_supported_efforts                 {model}  (result "__KIMI_K3_EFFORTS__" marker
 *                                           -> the real K3 constant for the `is` test)
 *   tokenhub_effort                        {requested}
 *   prepare_messages                       {sanitized}
 *   get_max_tokens                         {model}
 *   build_api_kwargs_extras                {reasoning_config, supports_reasoning,
 *                                           qwen_session_metadata, model, base_url,
 *                                           ollama_num_ctx, session_id, cache_scope_id,
 *                                           lmstudio_reasoning_options?}  (2-tuple result)
 *   build_extra_body                       {session_id, provider_preferences, model,
 *                                           base_url, reasoning_config,
 *                                           openrouter_min_coding_score, data_use}
 *   is_native_gemini_base_url              {base_url}
 *   _normalize_finish_reason               {finish_reason}
 *   declared_carrier_keys                  {}  (2-tuple [opaque_keys, text_keys])
 *
 * ── Value model ──────────────────────────────────────────────────────────────
 * None -> JSON null (pointer NULL and JSON_NULL node both read None).
 * attribute object (SimpleNamespace double) -> a plain JSON object in attribute
 *   position; getattr reads its fields. isinstance(x, dict) is true ONLY for a
 *   plain object (no "__attr__" tag is used here — position decides dict vs attr).
 * tuple -> JSON array; set/frozenset -> SORTED string array; integral floats
 *   collapse to ints on both sides (norm).
 * profile.fixed_temperature OMIT_TEMPERATURE sentinel -> the string marker
 *   "__OMIT_TEMPERATURE__" rides VERBATIM (the `is` identity test lives inside the
 *   seamed _base_kwargs, so it is never evaluated here).
 * kimi_supported_efforts K3 identity -> the string marker "__KIMI_K3_EFFORTS__"
 *   materializes to the real KIMI_K3_EFFORTS tuple on the PY side; C reads the
 *   marker for `is KIMI_K3_EFFORTS` and projects ["low","high","max"] into args.
 * finish_reason int (Poolside) -> {"__int__": n} marker (JSON numbers lose
 *   int/float identity); C str()'s it for the isinstance(int) branch.
 * _static_prompt_instructions non-serializable content -> {"__pyobj__": s}
 *   (truthy opaque object, str()==s) / {"__pyobj_falsy__": s} (falsy, str()==s);
 *   json.dumps raises TypeError -> str(content or ""). TOP-LEVEL content only.
 *
 * ── Fixture domain (ASCII scope) ─────────────────────────────────────────────
 * str.strip/lower transcribed byte-wise (White_Space incl \x1c-\x1f). base_url /
 * provider / model are str-or-null (scalars str()'d where the live str()s them).
 * str()-computed positions carry strings/integers/bools only (no non-integral
 * float repr). json.dumps content numbers are INTEGERS only (no float repr).
 * thinkingBudget numbers |x| < 2^53 (int() truncation). _base_kwargs /
 * _apply_max_tokens / build_api_kwargs_extras / build_extra_body script results
 * are dicts; _finish_kwargs result is a dict; profile is null-or-dict (a
 * SimpleNamespace double, ALWAYS truthy). messages is list-or-null.
 * _is_openai_api_base_url: bracketed netlocs never yield api.openai.com (they
 *   raise ValueError or return an IP literal) -> C returns false for any '[' / ']'
 *   in the netloc; the NFKC netloc check only fires on non-ASCII (out of scope).
 *
 * Self-contained: libjson only (gate links lib/libjson/json.o). The harness
 * #includes "agent/transports/chat_completions_flow2.c". All symbols static cc3_*.
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

/* ── context + seam plumbing ─────────────────────────────────────────────── */

typedef struct cc3_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} cc3_ctx;

static bool cc3_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool cc3_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool cc3_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool cc3_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool cc3_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool cc3_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool cc3_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static bool cc3_is_false(const json_t *v) { return v && v->type == JSON_BOOL && !v->bool_val; }

/* opaque-object markers (_static_prompt_instructions content position) */
static bool cc3_is_pyobj(const json_t *v) {
    return cc3_is_obj(v) && cc3_is_str(json_obj_get(v, "__pyobj__"));
}
static bool cc3_is_pyobj_falsy(const json_t *v) {
    return cc3_is_obj(v) && cc3_is_str(json_obj_get(v, "__pyobj_falsy__"));
}
static bool cc3_is_marker(const json_t *v) { return cc3_is_pyobj(v) || cc3_is_pyobj_falsy(v); }

/* Python truthiness (NULL pointer == None). A __pyobj__ double is truthy; a
 * __pyobj_falsy__ double is falsy; any other object is truthy by container count. */
static bool cc3_truthy(const json_t *v) {
    if (!v) return false;
    if (cc3_is_pyobj(v)) return true;
    if (cc3_is_pyobj_falsy(v)) return false;
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
/* An attribute-double (SimpleNamespace) is ALWAYS truthy, even when empty. */
static bool cc3_double_truthy(const json_t *v) {
    if (cc3_is_obj(v)) return true;
    return cc3_truthy(v);
}
static const char *cc3_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static json_t *cc3_get(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}
static json_t *cc3_getattr_def(const json_t *obj, const char *key) {
    if (!cc3_is_obj(obj)) return NULL;
    return json_obj_get(obj, key);
}

/* ── string helpers (caller frees) ───────────────────────────────────────── */

static char *cc3_dup_span(const char *s, size_t len) {
    char *r = (char *)malloc(len + 1);
    if (!r) return NULL;
    memcpy(r, s, len);
    r[len] = '\0';
    return r;
}
static bool cc3_is_ws(char c) {
    unsigned char u = (unsigned char)c;
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r'
        || (u >= 0x1c && u <= 0x1f);
}
static char *cc3_py_strip(const char *s) {
    if (!s) return cc3_dup_span("", 0);
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && cc3_is_ws(s[a])) a++;
    while (b > a && cc3_is_ws(s[b - 1])) b--;
    return cc3_dup_span(s + a, b - a);
}
static char *cc3_py_lower(const char *s) {
    char *r = cc3_dup_span(s ? s : "", strlen(s ? s : ""));
    if (!r) return NULL;
    for (char *p = r; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    return r;
}
/* str(v) for a scalar (null -> "None"). Marker-aware. Caller frees. */
static char *cc3_pystr(const json_t *v) {
    if (cc3_is_pyobj(v)) return cc3_dup_span(cc3_jstr(json_obj_get(v, "__pyobj__")),
                                             strlen(cc3_jstr(json_obj_get(v, "__pyobj__"))));
    if (cc3_is_pyobj_falsy(v)) return cc3_dup_span(cc3_jstr(json_obj_get(v, "__pyobj_falsy__")),
                                                   strlen(cc3_jstr(json_obj_get(v, "__pyobj_falsy__"))));
    if (cc3_is_none(v)) return cc3_dup_span("None", 4);
    if (cc3_is_bool(v)) return cc3_dup_span(v->bool_val ? "True" : "False", v->bool_val ? 4 : 5);
    if (cc3_is_num(v)) {
        char buf[64]; double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
        else snprintf(buf, sizeof buf, "%g", d);
        return cc3_dup_span(buf, strlen(buf));
    }
    if (cc3_is_str(v)) return cc3_dup_span(cc3_jstr(v), strlen(cc3_jstr(v)));
    return cc3_dup_span("", 0);
}
/* str(v or "") for a scalar (falsy -> ""). Caller frees. */
static char *cc3_str_or_empty(const json_t *v) {
    if (!cc3_truthy(v)) return cc3_dup_span("", 0);
    return cc3_pystr(v);
}
/* decimal string of an __int__ marker value (finish_reason int branch) */
static char *cc3_int_marker_str(const json_t *v) {
    double d = json_get_num(v, "__int__", 0);
    char buf[64];
    snprintf(buf, sizeof buf, "%.0f", d);
    return cc3_dup_span(buf, strlen(buf));
}

/* ── exception plumbing (FULL builtin hierarchy) ─────────────────────────── */

static bool cc3_exc_pending(const cc3_ctx *ctx) { return ctx && ctx->exc != NULL; }
static void cc3_exc_clear(cc3_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}
static void cc3_raise_msg(cc3_ctx *ctx, const char *cls, const char *msg) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
    json_set(ctx->exc, "msg", json_string(msg ? msg : ""));
}
static const char *cc3_exc_class(const cc3_ctx *ctx) {
    return json_get_str(ctx->exc, "__raise__", "");
}
static const char *cc3_exc_parent(const char *c) {
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
        if (cc3_str_eq(T[i].c, c)) return T[i].p;
    return NULL;
}
static bool cc3_exc_matches(const cc3_ctx *ctx, const char *const *classes, size_t n) {
    if (!cc3_exc_pending(ctx)) return false;
    const char *cls = cc3_exc_class(ctx);
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (cc3_str_eq(cls, classes[i])) return true;
        const char *p = cc3_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}
static bool cc3_exc_is_exception(const cc3_ctx *ctx) {
    static const char *E[] = {"Exception"};
    return cc3_exc_matches(ctx, E, 1);
}
static bool cc3_exc_is_typeerror(const cc3_ctx *ctx) {
    static const char *E[] = {"TypeError"};
    return cc3_exc_matches(ctx, E, 1);
}
static bool cc3_exc_is_valueerror(const cc3_ctx *ctx) {
    static const char *E[] = {"ValueError"};
    return cc3_exc_matches(ctx, E, 1);
}

/* ── universal seam (fail-closed; args ownership TRANSFERRED) ────────────── */

static json_t *cc3_call(cc3_ctx *ctx, const char *name, json_t *args) {
    if (!args) args = json_object();
    json_t *script = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    json_t *chosen = NULL;
    json_t *fc_marker = NULL;
    if (!script) {
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
    json_set(ev, "args", args);   /* ownership transferred into the event */
    if (raises) json_set(ev, "ret", json_copy(ctx->exc));
    else json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(ctx->trace, ev);
    if (fc_marker) json_free(fc_marker);
    return ret;
}

/* Arg builders: None-valued keys dropped on BOTH sides ("" kept). */
static void cc3_arg_str(json_t *args, const char *key, const char *val) {
    if (!val) return;
    json_set(args, key, json_string(val));
}
static void cc3_arg_node(json_t *args, const char *key, const json_t *val) {
    if (!cc3_is_none(val)) json_set(args, key, json_copy(val));
}
static void cc3_obj_update(json_t *dest, const json_t *src) {
    if (!cc3_is_obj(dest) || !cc3_is_obj(src)) return;
    for (size_t i = 0; i < src->c.count; i++)
        json_set(dest, src->c.keys[i], json_copy(src->c.items[i]));
}
static int cc3_cmp_jstr(const void *a, const void *b) {
    const json_t *ja = *(const json_t *const *)a;
    const json_t *jb = *(const json_t *const *)b;
    return strcmp(cc3_jstr(ja), cc3_jstr(jb));
}
static void cc3_sort_str_array(json_t *arr) {
    if (cc3_is_arr(arr) && arr->c.count > 1)
        qsort(arr->c.items, arr->c.count, sizeof(json_t *), cc3_cmp_jstr);
}
static bool cc3_strarr_has(const json_t *arr, const char *s) {
    if (!cc3_is_arr(arr)) return false;
    for (size_t i = 0; i < arr->c.count; i++)
        if (cc3_is_str(arr->c.items[i]) && cc3_str_eq(arr->c.items[i]->str_val, s)) return true;
    return false;
}

/* ── json.dumps(content, sort_keys=True, ensure_ascii=False, separators=(",",":"))
 * ASCII scope, INTEGER numbers only. A __pyobj__ marker raises TypeError. ── */

static void cc3_dump_str(const char *s, char **buf, size_t *len, size_t *cap) {
    /* append helper via realloc-growing buffer */
    #define CC3_PUT(ch) do { \
        if (*len + 1 >= *cap) { *cap = (*cap ? *cap * 2 : 64); *buf = (char *)realloc(*buf, *cap); } \
        (*buf)[(*len)++] = (ch); \
    } while (0)
    CC3_PUT('"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        switch (c) {
            case '"': CC3_PUT('\\'); CC3_PUT('"'); break;
            case '\\': CC3_PUT('\\'); CC3_PUT('\\'); break;
            case '\n': CC3_PUT('\\'); CC3_PUT('n'); break;
            case '\r': CC3_PUT('\\'); CC3_PUT('r'); break;
            case '\t': CC3_PUT('\\'); CC3_PUT('t'); break;
            case '\b': CC3_PUT('\\'); CC3_PUT('b'); break;
            case '\f': CC3_PUT('\\'); CC3_PUT('f'); break;
            default:
                if (c < 0x20) {
                    char hex[7];
                    snprintf(hex, sizeof hex, "\\u%04x", (unsigned)c);
                    for (int i = 0; i < 6; i++) CC3_PUT(hex[i]);
                } else {
                    CC3_PUT((char)c);
                }
        }
    }
    CC3_PUT('"');
    #undef CC3_PUT
}
/* recursive emitter; returns false (and parks TypeError) on a marker. */
static bool cc3_dump_node(const json_t *v, char **buf, size_t *len, size_t *cap, cc3_ctx *ctx) {
    #define CC3_PUTS(s) do { \
        const char *_s = (s); size_t _n = strlen(_s); \
        while (*len + _n + 1 >= *cap) { *cap = (*cap ? *cap * 2 : 64); *buf = (char *)realloc(*buf, *cap); } \
        memcpy(*buf + *len, _s, _n); *len += _n; \
    } while (0)
    if (cc3_is_marker(v)) {
        cc3_raise_msg(ctx, "TypeError", "Object of type _PyObj is not JSON serializable");
        return false;
    }
    if (cc3_is_none(v)) { CC3_PUTS("null"); return true; }
    if (cc3_is_bool(v)) { CC3_PUTS(v->bool_val ? "true" : "false"); return true; }
    if (cc3_is_num(v)) {
        char nb[64];
        snprintf(nb, sizeof nb, "%.0f", v->num_val);   /* integer domain */
        CC3_PUTS(nb);
        return true;
    }
    if (cc3_is_str(v)) { cc3_dump_str(cc3_jstr(v), buf, len, cap); return true; }
    if (cc3_is_arr(v)) {
        CC3_PUTS("[");
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) CC3_PUTS(",");
            if (!cc3_dump_node(v->c.items[i], buf, len, cap, ctx)) return false;
        }
        CC3_PUTS("]");
        return true;
    }
    if (cc3_is_obj(v)) {
        /* sort_keys=True: collect keys, sort, emit in sorted order */
        size_t n = v->c.count;
        const char **keys = (const char **)malloc((n ? n : 1) * sizeof(char *));
        for (size_t i = 0; i < n; i++) keys[i] = v->c.keys[i];
        for (size_t i = 1; i < n; i++) {
            const char *k = keys[i]; size_t j = i;
            while (j > 0 && strcmp(keys[j - 1], k) > 0) { keys[j] = keys[j - 1]; j--; }
            keys[j] = k;
        }
        CC3_PUTS("{");
        for (size_t i = 0; i < n; i++) {
            if (i) CC3_PUTS(",");
            cc3_dump_str(keys[i], buf, len, cap);
            CC3_PUTS(":");
            json_t *val = json_obj_get(v, keys[i]);
            if (!cc3_dump_node(val, buf, len, cap, ctx)) { free(keys); return false; }
        }
        CC3_PUTS("}");
        free(keys);
        return true;
    }
    CC3_PUTS("null");
    return true;
    #undef CC3_PUTS
}
/* returns malloc'd compact JSON, or NULL with a parked TypeError on a marker. */
static char *cc3_json_dumps(const json_t *v, cc3_ctx *ctx) {
    char *buf = NULL; size_t len = 0, cap = 0;
    if (!cc3_dump_node(v, &buf, &len, &cap, ctx)) { free(buf); return NULL; }
    if (!buf) { buf = cc3_dup_span("", 0); len = 0; }
    buf[len] = '\0';
    return buf;
}

/* ── urlparse(s).hostname transcription (ASCII scope) ──────────────────────
 * Mirrors CPython _urlsplit + _NetlocResultMixinStr._hostinfo + hostname.
 * Bracketed netlocs never yield api.openai.com (ValueError or IP literal) so
 * the predicate returns false for any '[' / ']' in the netloc. ── */

static bool cc3_scheme_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.';
}
/* returns malloc'd lowercase hostname or NULL (None). Parks ValueError on a
 * bracketed netloc (caught by the caller's except Exception -> false). */
static char *cc3_urlparse_hostname(const char *s, cc3_ctx *ctx) {
    /* lstrip C0-control-or-space (0x00..0x1f, 0x20) — NOT rstrip */
    size_t n = strlen(s), a = 0;
    while (a < n && ((unsigned char)s[a] <= 0x20)) a++;
    /* remove \t \r \n anywhere */
    char *url = (char *)malloc(n + 1);
    size_t ul = 0;
    for (size_t i = a; i < n; i++) {
        char c = s[i];
        if (c == '\t' || c == '\r' || c == '\n') continue;
        url[ul++] = c;
    }
    url[ul] = '\0';
    /* scheme detection: first ':' at i>0, url[0] ascii alpha, all scheme_chars */
    char *colon = strchr(url, ':');
    size_t rest = 0;
    if (colon && (colon - url) > 0 && url[0] &&
        ((url[0] >= 'a' && url[0] <= 'z') || (url[0] >= 'A' && url[0] <= 'Z'))) {
        bool ok = true;
        for (char *p = url; p < colon; p++) if (!cc3_scheme_char(*p)) { ok = false; break; }
        if (ok) rest = (size_t)(colon - url) + 1;
    }
    char *u = url + rest;
    char *netloc = NULL;
    if (u[0] == '/' && u[1] == '/') {
        /* _splitnetloc from pos 2: up to first of '/?#' */
        size_t start = 2, delim = strlen(u);
        for (const char *d = "/?#"; *d; d++) {
            char *w = strchr(u + start, *d);
            if (w && (size_t)(w - u) < delim) delim = (size_t)(w - u);
        }
        netloc = cc3_dup_span(u + start, delim - start);
        if (strchr(netloc, '[') || strchr(netloc, ']')) {
            /* bracketed: ValueError (Invalid IPv6 / IP literal) -> never api.openai.com */
            cc3_raise_msg(ctx, "ValueError", "Invalid IPv6 URL");
            free(netloc); free(url);
            return NULL;
        }
    }
    free(url);
    if (!netloc) return NULL;
    /* hostinfo: rpartition('@')[2], then partition(':')[0] */
    char *at = strrchr(netloc, '@');
    char *hostinfo = at ? at + 1 : netloc;
    char *hc = strchr(hostinfo, ':');
    size_t hlen = hc ? (size_t)(hc - hostinfo) : strlen(hostinfo);
    if (hlen == 0) { free(netloc); return NULL; }
    char *host = cc3_dup_span(hostinfo, hlen);
    free(netloc);
    /* hostname.lower() (the '%' zone partition only appends; never equals target) */
    char *low = cc3_py_lower(host);
    free(host);
    return low;
}
/* ── Gemini thinking-config cluster ──────────────────────────────────────── */

/* PoP: cc3_build_gemini_thinking_config @ agent/transports/chat_completions.py:_build_gemini_thinking_config */
static json_t *cc3_build_gemini_thinking_config(cc3_ctx *ctx, const json_t *model,
                                                const json_t *reasoning_config) {
    if (!cc3_is_obj(reasoning_config)) return json_null();
    char *ms = cc3_str_or_empty(model);
    char *st = cc3_py_strip(ms);
    char *lo = cc3_py_lower(st);
    free(ms); free(st);
    const char *nm = lo;
    if (strncmp(nm, "google/", 7) == 0) nm = lo + 7;   /* removeprefix("google/") */
    char *normalized = cc3_dup_span(nm, strlen(nm));
    free(lo);
    if (strncmp(normalized, "gemini", 6) != 0) { free(normalized); return json_null(); }
    /* effort = str(reasoning_config.get("effort", "medium") or "medium").strip().lower() */
    char *efftmp = NULL; const char *effsrc = "medium";
    json_t *effv = cc3_get(reasoning_config, "effort");
    if (cc3_truthy(effv)) { efftmp = cc3_pystr(effv); effsrc = efftmp; }
    char *effst = cc3_py_strip(effsrc);
    char *effort = cc3_py_lower(effst);
    free(effst); free(efftmp);
    json_t *enabled = cc3_get(reasoning_config, "enabled");
    bool disabled = cc3_is_false(enabled) || cc3_str_eq(effort, "none");
    json_t *out = NULL;
    if (strncmp(normalized, "gemini-4", 8) == 0) {
        const char *level = "low";
        json_t *clamped = NULL;
        if (!disabled) {
            json_t *a = json_object();
            cc3_arg_str(a, "effort", effort);
            json_t *levels = json_array();
            json_append(levels, json_string("low"));
            json_append(levels, json_string("medium"));
            json_append(levels, json_string("high"));
            cc3_arg_node(a, "supported", levels);
            clamped = cc3_call(ctx, "clamp_effort", a);
            if (cc3_exc_pending(ctx)) { if (clamped) json_free(clamped); goto done; }
            level = cc3_jstr(clamped);
        }
        bool in_levels = cc3_str_eq(level, "low") || cc3_str_eq(level, "medium") || cc3_str_eq(level, "high");
        out = json_object();
        json_set(out, "includeThoughts", json_bool(!disabled));
        json_set(out, "thinkingLevel", json_string(in_levels ? level : "medium"));
        if (clamped) json_free(clamped);
        goto done;
    }
    if (disabled) {
        out = json_object();
        json_set(out, "includeThoughts", json_bool(false));
        if (strncmp(normalized, "gemini-2.5-", 11) == 0) {
            json_set(out, "thinkingBudget", json_number(0));
        } else {
            json_t *a = json_object();
            cc3_arg_str(a, "model", normalized);
            json_t *uses = cc3_call(ctx, "gemini_uses_thinking_level", a);
            if (cc3_exc_pending(ctx)) { if (uses) json_free(uses); json_free(out); out = NULL; goto done; }
            if (cc3_truthy(uses)) json_set(out, "thinkingLevel", json_string("low"));
            if (uses) json_free(uses);
        }
        goto done;
    }
    out = json_object();
    json_set(out, "includeThoughts", json_bool(true));
    if (strncmp(normalized, "gemini-2.5-", 11) == 0) goto done;
    {
        bool in_white = cc3_str_eq(effort, "minimal") || cc3_str_eq(effort, "low")
            || cc3_str_eq(effort, "medium") || cc3_str_eq(effort, "high")
            || cc3_str_eq(effort, "xhigh") || cc3_str_eq(effort, "max") || cc3_str_eq(effort, "ultra");
        if (!in_white) { free(effort); effort = cc3_dup_span("medium", 6); }
        json_t *a = json_object();
        cc3_arg_str(a, "model", normalized);
        json_t *uses = cc3_call(ctx, "gemini_uses_thinking_level", a);
        if (cc3_exc_pending(ctx)) { if (uses) json_free(uses); json_free(out); out = NULL; goto done; }
        if (cc3_truthy(uses)) {
            bool high_eff = cc3_str_eq(effort, "high") || cc3_str_eq(effort, "xhigh")
                || cc3_str_eq(effort, "max") || cc3_str_eq(effort, "ultra");   /* _HIGH_EFFORTS */
            if (strstr(normalized, "pro")) {
                json_set(out, "thinkingLevel", json_string(high_eff ? "high" : "low"));
            } else {
                bool low_eff = cc3_str_eq(effort, "minimal") || cc3_str_eq(effort, "low");
                const char *lv = low_eff ? "low" : (high_eff ? "high" : "medium");
                json_set(out, "thinkingLevel", json_string(lv));
            }
        }
        if (uses) json_free(uses);
    }
done:
    free(normalized); free(effort);
    return out;
}

/* PoP: cc3_snake_case_gemini_thinking_config @ agent/transports/chat_completions.py:_snake_case_gemini_thinking_config */
static json_t *cc3_snake_case_gemini_thinking_config(const json_t *config) {
    if (!cc3_is_obj(config) || config->c.count == 0) return json_null();
    json_t *out = json_object();
    json_t *include = cc3_get(config, "includeThoughts");
    json_t *level = cc3_get(config, "thinkingLevel");
    json_t *budget = cc3_get(config, "thinkingBudget");
    if (cc3_is_bool(include)) json_set(out, "include_thoughts", json_bool(include->bool_val));
    if (cc3_is_str(level)) {
        char *st = cc3_py_strip(cc3_jstr(level));
        if (st && st[0]) {
            char *lo = cc3_py_lower(st);
            json_set(out, "thinking_level", json_string(lo));
            free(lo);
        }
        free(st);
    }
    if (cc3_is_num(budget) || cc3_is_bool(budget)) {
        double d = cc3_is_bool(budget) ? (budget->bool_val ? 1.0 : 0.0) : budget->num_val;
        json_set(out, "thinking_budget", json_number(trunc(d)));
    }
    if (out->c.count == 0) { json_free(out); return json_null(); }
    return out;
}

/* PoP: cc3_raise_gemini_thinking_max_tokens @ agent/transports/chat_completions.py:_raise_gemini_thinking_max_tokens */
static json_t *cc3_raise_gemini_thinking_max_tokens(cc3_ctx *ctx, const json_t *model,
                                                    const json_t *reasoning_config,
                                                    const json_t *requested) {
    json_t *tc = cc3_build_gemini_thinking_config(ctx, model, reasoning_config);
    if (cc3_exc_pending(ctx)) { if (tc) json_free(tc); return NULL; }
    if (!cc3_truthy(tc)) { if (tc) json_free(tc); return requested ? json_copy(requested) : json_null(); }
    json_t *a = json_object();
    cc3_arg_node(a, "requested", requested);
    cc3_arg_node(a, "thinking_config", tc);
    json_t *r = cc3_call(ctx, "_effective_gemini_max_output_tokens", a);
    json_free(tc);
    if (cc3_exc_pending(ctx)) { if (r) json_free(r); return NULL; }
    return r ? r : json_null();
}

/* PoP: cc3_is_gemini_openai_compat_base_url @ agent/transports/chat_completions.py:_is_gemini_openai_compat_base_url */
static json_t *cc3_is_gemini_openai_compat_base_url(const json_t *base_url) {
    char *s = cc3_str_or_empty(base_url);
    char *st = cc3_py_strip(s);
    free(s);
    size_t n = strlen(st);
    while (n > 0 && st[n - 1] == '/') st[--n] = '\0';   /* rstrip("/") */
    char *lo = cc3_py_lower(st);
    free(st);
    size_t ll = strlen(lo);
    bool hit = lo[0] && strstr(lo, "generativelanguage.googleapis.com")
        && ll >= 7 && cc3_str_eq(lo + ll - 7, "/openai");
    free(lo);
    return json_bool(hit);
}

/* PoP: cc3_is_openai_api_base_url @ agent/transports/chat_completions.py:_is_openai_api_base_url */
static json_t *cc3_is_openai_api_base_url(cc3_ctx *ctx, const json_t *base_url) {
    char *s = cc3_str_or_empty(base_url);
    char *st = cc3_py_strip(s);
    free(s);
    char *host = cc3_urlparse_hostname(st, ctx);
    free(st);
    if (cc3_exc_pending(ctx)) {
        if (cc3_exc_is_exception(ctx)) { cc3_exc_clear(ctx); free(host); return json_bool(false); }
        free(host);
        return NULL;   /* BaseException passthrough */
    }
    bool hit = host && cc3_str_eq(host, "api.openai.com");
    free(host);
    return json_bool(hit);
}

/* PoP: cc3_model_consumes_thought_signature @ agent/transports/chat_completions.py:_model_consumes_thought_signature */
static json_t *cc3_model_consumes_thought_signature(cc3_ctx *ctx, const json_t *model,
                                                    const json_t *base_url, const json_t *provider) {
    char *ms = cc3_str_or_empty(model);
    char *m = cc3_py_lower(ms);   /* str(model or "").lower() — NO strip */
    free(ms);
    if (strstr(m, "gemini") || strstr(m, "gemma")) { free(m); return json_bool(true); }
    free(m);
    char *ps = cc3_str_or_empty(provider);
    char *pst = cc3_py_strip(ps);
    char *pl = cc3_py_lower(pst);
    free(ps); free(pst);
    bool prov = cc3_str_eq(pl, "gemini") || cc3_str_eq(pl, "vertex");   /* _GEMINI_SIGNATURE_PROVIDERS */
    free(pl);
    if (prov) return json_bool(true);
    json_t *a = json_object();
    cc3_arg_node(a, "provider", provider);
    json_t *rep = cc3_call(ctx, "_profile_replays_tool_call_signatures", a);
    if (cc3_exc_pending(ctx)) { if (rep) json_free(rep); return NULL; }
    if (cc3_truthy(rep)) return rep;   /* or-chain leaks the raw truthy value */
    if (rep) json_free(rep);
    static const char *HOSTS[] = {"generativelanguage.googleapis.com", "aiplatform.googleapis.com"};
    for (int i = 0; i < 2; i++) {
        json_t *ha = json_object();
        cc3_arg_node(ha, "base_url", base_url);
        cc3_arg_str(ha, "host", HOSTS[i]);
        json_t *hm = cc3_call(ctx, "base_url_host_matches", ha);
        if (cc3_exc_pending(ctx)) { if (hm) json_free(hm); return NULL; }
        bool hit = cc3_truthy(hm);
        if (hm) json_free(hm);
        if (hit) return json_bool(true);
    }
    return json_bool(false);
}

/* ── prompt-cache cluster ────────────────────────────────────────────────── */

/* PoP: cc3_static_prompt_instructions @ agent/transports/chat_completions.py:_static_prompt_instructions */
static json_t *cc3_static_prompt_instructions(cc3_ctx *ctx, const json_t *messages) {
    json_t *first = NULL;
    if (cc3_truthy(messages) && cc3_is_arr(messages) && messages->c.count > 0) {
        json_t *m0 = messages->c.items[0];
        if (cc3_is_obj(m0)) first = m0;
    }
    json_t *role = first ? cc3_get(first, "role") : NULL;
    if (cc3_is_arr(role) || cc3_is_obj(role)) {
        cc3_raise_msg(ctx, "TypeError",
                      cc3_is_arr(role) ? "unhashable type: 'list'" : "unhashable type: 'dict'");
        return NULL;
    }
    const char *r = cc3_jstr(role);
    if (!(cc3_str_eq(r, "system") || cc3_str_eq(r, "developer"))) return json_string("");
    json_t *content = cc3_get(first, "content");
    if (cc3_is_str(content)) return json_copy(content);
    char *dumps = cc3_json_dumps(content, ctx);
    if (cc3_exc_pending(ctx)) {
        if (cc3_exc_is_typeerror(ctx) || cc3_exc_is_valueerror(ctx)) {
            cc3_exc_clear(ctx);
            char *s = cc3_str_or_empty(content);
            json_t *out = json_string(s);
            free(s);
            return out;
        }
        free(dumps);
        return NULL;   /* BaseException passthrough */
    }
    json_t *out = json_string(dumps ? dumps : "");
    free(dumps);
    return out;
}

/* PoP: cc3_add_prompt_cache_key @ agent/transports/chat_completions.py:_add_prompt_cache_key */
static void cc3_add_prompt_cache_key(cc3_ctx *ctx, json_t *api_kwargs, const json_t *messages,
                                     const json_t *tools, const json_t *supports,
                                     const json_t *session_id, const json_t *cache_scope_id) {
    if (!cc3_is_obj(api_kwargs)) {
        const char *tn = cc3_is_none(api_kwargs) ? "NoneType"
            : cc3_is_arr(api_kwargs) ? "list"
            : cc3_is_str(api_kwargs) ? "str"
            : cc3_is_bool(api_kwargs) ? "bool"
            : cc3_is_num(api_kwargs) ? (api_kwargs->num_val == floor(api_kwargs->num_val) ? "int" : "float")
            : "object";
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", tn);
        cc3_raise_msg(ctx, "AttributeError", msg);
        return;
    }
    json_t *containers[2]; int nc = 0;
    if (json_has(api_kwargs, "prompt_cache_key")) containers[nc++] = api_kwargs;
    json_t *eb = cc3_get(api_kwargs, "extra_body");
    if (cc3_is_obj(eb) && json_has(eb, "prompt_cache_key")) containers[nc++] = eb;
    if (nc > 0) {
        for (int i = 0; i < nc; i++) {
            json_t *a = json_object();
            cc3_arg_node(a, "container", containers[i]);
            json_t *r = cc3_call(ctx, "_bound_prompt_cache_key_field", a);
            if (cc3_exc_pending(ctx)) { if (r) json_free(r); return; }
            if (cc3_is_obj(r)) {
                json_t *upd = json_obj_get(r, "__update__");
                if (cc3_is_obj(upd)) cc3_obj_update(containers[i], upd);
                json_t *pop = json_obj_get(r, "__pop__");
                if (cc3_is_arr(pop))
                    for (size_t j = 0; j < pop->c.count; j++)
                        if (cc3_is_str(pop->c.items[j]))
                            json_obj_del(containers[i], pop->c.items[j]->str_val);
            }
            if (r) json_free(r);
        }
        return;
    }
    if (!cc3_truthy(supports)) return;
    json_t *instr = cc3_static_prompt_instructions(ctx, messages);
    if (cc3_exc_pending(ctx)) { if (instr) json_free(instr); return; }
    const json_t *scope_src = cc3_truthy(cache_scope_id) ? cache_scope_id : session_id;
    json_t *sa = json_object();
    cc3_arg_node(sa, "session_id", scope_src);
    json_t *scope = cc3_call(ctx, "_cache_scope_from_session_id", sa);
    if (cc3_exc_pending(ctx)) { if (instr) json_free(instr); if (scope) json_free(scope); return; }
    json_t *ca = json_object();
    cc3_arg_node(ca, "instructions", instr);
    cc3_arg_node(ca, "tools", tools);
    cc3_arg_node(ca, "scope_id", scope);
    json_t *cache_key = cc3_call(ctx, "_content_cache_key", ca);
    if (instr) json_free(instr);
    if (scope) json_free(scope);
    if (cc3_exc_pending(ctx)) { if (cache_key) json_free(cache_key); return; }
    if (cc3_truthy(cache_key)) json_set(api_kwargs, "prompt_cache_key", json_copy(cache_key));
    if (cache_key) json_free(cache_key);
}

/* ── transport methods: convert_messages / convert_tools / api_mode ──────── */

/* PoP: cc3_convert_messages @ agent/transports/chat_completions.py:convert_messages */
static json_t *cc3_convert_messages(cc3_ctx *ctx, const json_t *messages, const json_t *kwargs) {
    json_t *profile = cc3_get(kwargs, "provider_profile");
    json_t *pname = cc3_getattr_def(profile, "name");
    const json_t *provider = cc3_truthy(pname) ? pname : cc3_get(kwargs, "provider");
    json_t *consumes = cc3_model_consumes_thought_signature(ctx, cc3_get(kwargs, "model"),
                                                            cc3_get(kwargs, "base_url"), provider);
    if (cc3_exc_pending(ctx)) { if (consumes) json_free(consumes); return NULL; }
    bool strip_extra = !cc3_truthy(consumes);
    if (consumes) json_free(consumes);
    json_t *nt = cc3_getattr_def(profile, "native_reasoning_details_type");
    const json_t *native_type = cc3_truthy(nt) ? nt : NULL;
    json_t *wa = json_object();
    cc3_arg_node(wa, "provider", provider);
    cc3_arg_node(wa, "base_url", cc3_get(kwargs, "base_url"));
    json_t *carriers = cc3_call(ctx, "_wire_reasoning_carriers", wa);
    if (cc3_exc_pending(ctx)) { if (carriers) json_free(carriers); return NULL; }
    if (native_type && cc3_truthy(carriers)) {
        if (!cc3_strarr_has(carriers, "reasoning_details"))
            json_append(carriers, json_string("reasoning_details"));
    }
    static const char *RK[] = {"reasoning_content", "reasoning", "reasoning_details"};
    json_t *strip_reasoning = json_array();
    for (int i = 0; i < 3; i++)
        if (!cc3_strarr_has(carriers, RK[i])) json_append(strip_reasoning, json_string(RK[i]));
    cc3_sort_str_array(strip_reasoning);
    json_t *ma = json_object();
    cc3_arg_node(ma, "provider", provider);
    cc3_arg_node(ma, "model", cc3_get(kwargs, "model"));
    cc3_arg_node(ma, "base_url", cc3_get(kwargs, "base_url"));
    json_t *mistral_ids = cc3_call(ctx, "_route_requires_mistral_tool_ids", ma);
    if (cc3_exc_pending(ctx)) {
        if (carriers) json_free(carriers);
        json_free(strip_reasoning);
        if (mistral_ids) json_free(mistral_ids);
        return NULL;
    }
    if (!cc3_is_arr(messages)) {
        const char *tn = cc3_is_none(messages) ? "NoneType"
            : cc3_is_obj(messages) ? "dict"
            : cc3_is_bool(messages) ? "bool"
            : cc3_is_num(messages) ? (messages->num_val == floor(messages->num_val) ? "int" : "float")
            : "str";
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", tn);
        cc3_raise_msg(ctx, "TypeError", msg);
        if (carriers) json_free(carriers);
        json_free(strip_reasoning);
        if (mistral_ids) json_free(mistral_ids);
        return NULL;
    }
    json_t *pairs = json_array();
    for (size_t i = 0; i < messages->c.count; i++) {
        json_t *m = messages->c.items[i];
        json_t *sa = json_object();
        cc3_arg_node(sa, "msg", m);
        json_set(sa, "strip_extra_content", json_bool(strip_extra));
        cc3_arg_node(sa, "strip_reasoning", strip_reasoning);
        cc3_arg_node(sa, "native_reasoning_details_type", native_type);
        cc3_arg_node(sa, "mistral_tool_ids", mistral_ids);
        json_t *s = cc3_call(ctx, "_sanitize_message", sa);
        if (cc3_exc_pending(ctx)) {
            if (s) json_free(s);
            json_free(pairs);
            if (carriers) json_free(carriers);
            json_free(strip_reasoning);
            if (mistral_ids) json_free(mistral_ids);
            return NULL;
        }
        json_t *pair = json_array();
        json_append(pair, json_copy(m));
        json_append(pair, s ? s : json_null());
        json_append(pairs, pair);
    }
    if (carriers) json_free(carriers);
    json_free(strip_reasoning);
    if (mistral_ids) json_free(mistral_ids);
    bool all_none = true;
    for (size_t i = 0; i < pairs->c.count; i++) {
        if (!cc3_is_none(pairs->c.items[i]->c.items[1])) { all_none = false; break; }
    }
    if (all_none) { json_free(pairs); return messages ? json_copy(messages) : json_null(); }
    json_t *out = json_array();
    for (size_t i = 0; i < pairs->c.count; i++) {
        json_t *m = pairs->c.items[i]->c.items[0];
        json_t *s = pairs->c.items[i]->c.items[1];
        json_append(out, cc3_is_none(s) ? json_copy(m) : json_copy(s));
    }
    json_free(pairs);
    return out;
}

/* PoP: cc3_convert_tools @ agent/transports/chat_completions.py:convert_tools */
static json_t *cc3_convert_tools(const json_t *tools) {
    return tools ? json_copy(tools) : json_null();
}

/* PoP: cc3_api_mode @ agent/transports/chat_completions.py:api_mode */
static json_t *cc3_api_mode(void) {
    return json_string("chat_completions");
}
/* ── seamed flow1 leaf wrappers (claimed siblings) ───────────────────────── */

static json_t *cc3_attr_or_model_extra_seam(cc3_ctx *ctx, const json_t *obj, const char *name) {
    json_t *a = json_object();
    cc3_arg_node(a, "obj", obj);
    cc3_arg_str(a, "name", name);
    json_t *r = cc3_call(ctx, "_attr_or_model_extra", a);
    if (cc3_exc_pending(ctx)) { if (r) json_free(r); return NULL; }
    return r ? r : json_null();
}
static json_t *cc3_dump_extra_content_seam(cc3_ctx *ctx, const json_t *extra) {
    json_t *a = json_object();
    cc3_arg_node(a, "extra", extra);
    json_t *r = cc3_call(ctx, "_dump_extra_content", a);
    if (cc3_exc_pending(ctx)) { if (r) json_free(r); return NULL; }
    return r ? r : json_null();
}

/* ── build_kwargs (legacy flag path + profile dispatch) ──────────────────── */

static json_t *cc3_build_kwargs_from_profile(cc3_ctx *ctx, const json_t *profile, const json_t *model,
                                             const json_t *sanitized_in, const json_t *tools, json_t *params);

/* PoP: cc3_build_kwargs @ agent/transports/chat_completions.py:build_kwargs */
static json_t *cc3_build_kwargs(cc3_ctx *ctx, const json_t *model, const json_t *messages,
                                const json_t *tools, json_t *params) {
    json_t *sanitized = NULL, *api_kwargs = NULL, *reasoning_config = NULL, *e = NULL;
    json_t *extra_body = NULL, *final = NULL;
    json_t *profile = cc3_get(params, "provider_profile");
    {   /* params["data_use"] = data_use_requirement(messages) */
        json_t *da = json_object();
        cc3_arg_node(da, "messages", messages);
        json_t *data_use = cc3_call(ctx, "data_use_requirement", da);
        if (cc3_exc_pending(ctx)) { if (data_use) json_free(data_use); goto cleanup; }
        json_set(params, "data_use", data_use ? data_use : json_null());
    }
    {   /* sanitized = self.convert_messages(messages, model=, base_url=, provider_profile=, provider=) */
        json_t *ckwargs = json_object();
        cc3_arg_node(ckwargs, "model", model);
        cc3_arg_node(ckwargs, "base_url", cc3_get(params, "base_url"));
        cc3_arg_node(ckwargs, "provider_profile", profile);
        cc3_arg_node(ckwargs, "provider", cc3_get(params, "provider_name"));
        sanitized = cc3_convert_messages(ctx, messages, ckwargs);
        json_free(ckwargs);
        if (cc3_exc_pending(ctx)) goto cleanup;
    }
    if (cc3_double_truthy(profile)) {
        final = cc3_build_kwargs_from_profile(ctx, profile, model, sanitized, tools, params);
        goto cleanup;
    }
    {   /* sanitized = _swap_developer_role(sanitized, params.get("model_lower", (model or "").lower())) */
        json_t *ml_node;
        if (json_has(params, "model_lower")) ml_node = json_copy(cc3_get(params, "model_lower"));
        else {
            char *ms = cc3_str_or_empty(model);
            char *ml = cc3_py_lower(ms);
            free(ms);
            ml_node = json_string(ml);
            free(ml);
        }
        json_t *swa = json_object();
        cc3_arg_node(swa, "sanitized", sanitized);
        cc3_arg_node(swa, "model_lower", ml_node);
        json_t *swapped = cc3_call(ctx, "_swap_developer_role", swa);
        json_free(ml_node);
        if (cc3_exc_pending(ctx)) { if (swapped) json_free(swapped); goto cleanup; }
        if (sanitized) json_free(sanitized);
        sanitized = swapped ? swapped : json_null();
    }
    {   /* api_kwargs = _base_kwargs(model, sanitized, tools, params) */
        json_t *ba = json_object();
        cc3_arg_node(ba, "model", model);
        cc3_arg_node(ba, "sanitized", sanitized);
        cc3_arg_node(ba, "tools", tools);
        cc3_arg_node(ba, "params", params);
        api_kwargs = cc3_call(ctx, "_base_kwargs", ba);
        if (cc3_exc_pending(ctx)) goto cleanup;
        if (!api_kwargs) api_kwargs = json_object();
    }
    {   /* reasoning_config = _reasoning_config_for_model(model, params.get("reasoning_config"), params.get("base_url")) */
        json_t *ra = json_object();
        cc3_arg_node(ra, "model", model);
        cc3_arg_node(ra, "reasoning_config", cc3_get(params, "reasoning_config"));
        cc3_arg_node(ra, "base_url", cc3_get(params, "base_url"));
        reasoning_config = cc3_call(ctx, "_reasoning_config_for_model", ra);
        if (cc3_exc_pending(ctx)) goto cleanup;
    }
    {   /* _apply_max_tokens(api_kwargs, model, reasoning_config, params) — void mutator */
        json_t *aa = json_object();
        cc3_arg_node(aa, "api_kwargs", api_kwargs);
        cc3_arg_node(aa, "model", model);
        cc3_arg_node(aa, "reasoning_config", reasoning_config);
        cc3_arg_node(aa, "params", params);
        json_t *amr = cc3_call(ctx, "_apply_max_tokens", aa);
        if (cc3_exc_pending(ctx)) { if (amr) json_free(amr); goto cleanup; }
        if (cc3_is_obj(amr)) {
            json_t *upd = json_obj_get(amr, "__update__");
            if (cc3_is_obj(upd)) cc3_obj_update(api_kwargs, upd);
        }
        if (amr) json_free(amr);
    }
    bool thinking_off = cc3_is_obj(reasoning_config) && cc3_is_false(cc3_get(reasoning_config, "enabled"));
    bool is_kimi = cc3_truthy(cc3_get(params, "is_kimi"));
    bool supports_reasoning = cc3_truthy(cc3_get(params, "supports_reasoning"));
    {   /* _e = requested_effort(reasoning_config) */
        json_t *ea = json_object();
        cc3_arg_node(ea, "reasoning_config", reasoning_config);
        e = cc3_call(ctx, "requested_effort", ea);
        if (cc3_exc_pending(ctx)) goto cleanup;
    }
    if (is_kimi && !thinking_off) {
        json_t *ka = json_object();
        cc3_arg_node(ka, "model", model);
        json_t *supported = cc3_call(ctx, "kimi_supported_efforts", ka);
        if (cc3_exc_pending(ctx)) { if (supported) json_free(supported); goto cleanup; }
        bool is_k3 = cc3_is_str(supported) && cc3_str_eq(cc3_jstr(supported), "__KIMI_K3_EFFORTS__");
        json_t *effort_val = NULL;
        if (cc3_is_none(e)) {
            effort_val = json_string(is_k3 ? "high" : "medium");
        } else {
            json_t *sup_proj;
            if (is_k3) {
                sup_proj = json_array();
                json_append(sup_proj, json_string("low"));
                json_append(sup_proj, json_string("high"));
                json_append(sup_proj, json_string("max"));
            } else {
                sup_proj = supported ? json_copy(supported) : json_null();
            }
            json_t *cla = json_object();
            cc3_arg_node(cla, "effort", e);
            cc3_arg_node(cla, "supported", sup_proj);
            if (is_k3) {
                json_t *ov = json_object();
                json_set(ov, "medium", json_string("high"));
                json_set(ov, "xhigh", json_string("max"));
                cc3_arg_node(cla, "overrides", ov);
            }
            json_free(sup_proj);
            effort_val = cc3_call(ctx, "clamp_effort", cla);
            if (cc3_exc_pending(ctx)) {
                if (effort_val) json_free(effort_val);
                if (supported) json_free(supported);
                goto cleanup;
            }
            if (!effort_val) effort_val = json_null();
        }
        json_set(api_kwargs, "reasoning_effort", effort_val);
        if (supported) json_free(supported);
    }
    if (cc3_truthy(cc3_get(params, "is_tokenhub")) && !thinking_off) {
        json_t *ta = json_object();
        cc3_arg_node(ta, "requested", e);
        json_t *te = cc3_call(ctx, "tokenhub_effort", ta);
        if (cc3_exc_pending(ctx)) { if (te) json_free(te); goto cleanup; }
        json_set(api_kwargs, "reasoning_effort", te ? te : json_null());
    }
    extra_body = json_object();
    bool is_openrouter = cc3_truthy(cc3_get(params, "is_openrouter"));
    const json_t *base_url = cc3_get(params, "base_url");
    if (is_openrouter && cc3_truthy(cc3_get(params, "provider_preferences")))
        json_set(extra_body, "provider", json_copy(cc3_get(params, "provider_preferences")));
    if (is_openrouter && cc3_is_str(model) && cc3_str_eq(cc3_jstr(model), "openrouter/pareto-code")) {
        json_t *pa = json_object();
        cc3_arg_node(pa, "raw", cc3_get(params, "openrouter_min_coding_score"));
        json_t *pf = cc3_call(ctx, "_pareto_score", pa);
        if (cc3_exc_pending(ctx)) { if (pf) json_free(pf); goto cleanup; }
        if (!cc3_is_none(pf)) {
            json_t *plugins = json_array();
            json_t *pl = json_object();
            json_set(pl, "id", json_string("pareto-router"));
            json_set(pl, "min_coding_score", json_copy(pf));
            json_append(plugins, pl);
            json_set(extra_body, "plugins", plugins);
        }
        if (pf) json_free(pf);
    }
    if (is_kimi) {
        json_t *th = json_object();
        json_set(th, "type", json_string(thinking_off ? "disabled" : "enabled"));
        json_set(extra_body, "thinking", th);
    }
    if (supports_reasoning) {
        if (cc3_truthy(cc3_get(params, "profile_endpoint"))) {
            json_t *er = cc3_get(params, "endpoint_reasoning");
            if (!cc3_is_none(er)) json_set(extra_body, "reasoning", json_copy(er));
        } else {
            const char *effsrc = "medium";
            char *efftmp = NULL;
            if (cc3_truthy(reasoning_config) && cc3_is_obj(reasoning_config)) {
                json_t *ev = cc3_get(reasoning_config, "effort");
                if (cc3_truthy(ev)) { efftmp = cc3_pystr(ev); effsrc = efftmp; }
            }
            bool off = thinking_off || cc3_str_eq(effsrc, "none");
            json_t *rz = json_object();
            json_set(rz, "enabled", json_bool(!off));
            json_set(rz, "effort", json_string(off ? "none" : effsrc));
            json_set(extra_body, "reasoning", rz);
            free(efftmp);
        }
    }
    {   /* gemini provider branch */
        char *pns = cc3_str_or_empty(cc3_get(params, "provider_name"));
        char *pnst = cc3_py_strip(pns);
        char *pnlo = cc3_py_lower(pnst);
        free(pns); free(pnst);
        bool is_gemini = cc3_str_eq(pnlo, "gemini");
        free(pnlo);
        if (is_gemini) {
            json_t *raw_tc = cc3_build_gemini_thinking_config(ctx, model, reasoning_config);
            if (cc3_exc_pending(ctx)) { if (raw_tc) json_free(raw_tc); goto cleanup; }
            json_t *compat = cc3_is_gemini_openai_compat_base_url(base_url);
            if (cc3_truthy(compat)) {
                json_t *snake = cc3_snake_case_gemini_thinking_config(raw_tc);
                if (cc3_truthy(snake)) {
                    json_t *oce = cc3_get(extra_body, "extra_body");
                    oce = cc3_is_obj(oce) ? json_copy(oce) : json_object();
                    json_t *google = json_object();
                    json_t *gexisting = cc3_get(oce, "google");
                    if (cc3_is_obj(gexisting)) cc3_obj_update(google, gexisting);
                    json_set(google, "thinking_config", json_copy(snake));
                    json_set(oce, "google", google);
                    json_set(extra_body, "extra_body", oce);
                }
                if (snake) json_free(snake);
            } else if (cc3_truthy(raw_tc)) {
                json_set(extra_body, "thinking_config", json_copy(raw_tc));
            }
            if (compat) json_free(compat);
            if (raw_tc) json_free(raw_tc);
        }
    }
    if (cc3_truthy(cc3_get(params, "extra_body_additions")))
        cc3_obj_update(extra_body, cc3_get(params, "extra_body_additions"));
    if (cc3_truthy(extra_body)) json_set(api_kwargs, "extra_body", json_copy(extra_body));
    json_free(extra_body); extra_body = NULL;
    if (cc3_truthy(cc3_get(params, "request_overrides")))
        cc3_obj_update(api_kwargs, cc3_get(params, "request_overrides"));
    {   /* return _finish_kwargs(..., supports_prompt_cache_key=bool(params.get(...)) or _is_openai_api_base_url(base_url)) */
        bool supports = cc3_truthy(cc3_get(params, "supports_prompt_cache_key"));
        if (!supports) {
            json_t *is_oai = cc3_is_openai_api_base_url(ctx, base_url);
            if (cc3_exc_pending(ctx)) { if (is_oai) json_free(is_oai); goto cleanup; }
            supports = cc3_truthy(is_oai);
            if (is_oai) json_free(is_oai);
        }
        json_t *fa = json_object();
        cc3_arg_node(fa, "api_kwargs", api_kwargs);
        cc3_arg_node(fa, "sanitized", sanitized);
        cc3_arg_node(fa, "params", params);
        json_set(fa, "supports_prompt_cache_key", json_bool(supports));
        final = cc3_call(ctx, "_finish_kwargs", fa);
        if (cc3_exc_pending(ctx)) goto cleanup;
    }
cleanup:
    if (sanitized) json_free(sanitized);
    if (api_kwargs) json_free(api_kwargs);
    if (reasoning_config) json_free(reasoning_config);
    if (e) json_free(e);
    if (extra_body) json_free(extra_body);
    return final;
}

/* ── _build_kwargs_from_profile (profile-object path) ────────────────────── */

/* PoP: cc3_build_kwargs_from_profile @ agent/transports/chat_completions.py:_build_kwargs_from_profile */
static json_t *cc3_build_kwargs_from_profile(cc3_ctx *ctx, const json_t *profile, const json_t *model,
                                             const json_t *sanitized_in, const json_t *tools, json_t *params) {
    json_t *sanitized = NULL, *api_kwargs = NULL, *reasoning_config = NULL, *extra_body = NULL, *final = NULL;
    json_t *top_level = NULL, *profile_body = NULL, *extras_body = NULL;
    {   /* sanitized = _swap_developer_role(profile.prepare_messages(sanitized), (model or "").lower()) */
        json_t *pa = json_object();
        cc3_arg_node(pa, "sanitized", sanitized_in);
        json_t *prep = cc3_call(ctx, "prepare_messages", pa);
        if (cc3_exc_pending(ctx)) { if (prep) json_free(prep); goto cleanup; }
        char *ms = cc3_str_or_empty(model);
        char *ml = cc3_py_lower(ms);
        free(ms);
        json_t *swa = json_object();
        cc3_arg_node(swa, "sanitized", prep);
        cc3_arg_str(swa, "model_lower", ml);
        free(ml);
        sanitized = cc3_call(ctx, "_swap_developer_role", swa);
        if (prep) json_free(prep);
        if (cc3_exc_pending(ctx)) goto cleanup;
        if (!sanitized) sanitized = json_null();
    }
    {   /* api_kwargs = _base_kwargs(model, sanitized, tools, params, profile=profile) */
        json_t *ba = json_object();
        cc3_arg_node(ba, "model", model);
        cc3_arg_node(ba, "sanitized", sanitized);
        cc3_arg_node(ba, "tools", tools);
        cc3_arg_node(ba, "params", params);
        cc3_arg_node(ba, "profile", profile);
        api_kwargs = cc3_call(ctx, "_base_kwargs", ba);
        if (cc3_exc_pending(ctx)) goto cleanup;
        if (!api_kwargs) api_kwargs = json_object();
    }
    {   /* reasoning_config = _reasoning_config_for_model(...) */
        json_t *ra = json_object();
        cc3_arg_node(ra, "model", model);
        cc3_arg_node(ra, "reasoning_config", cc3_get(params, "reasoning_config"));
        cc3_arg_node(ra, "base_url", cc3_get(params, "base_url"));
        reasoning_config = cc3_call(ctx, "_reasoning_config_for_model", ra);
        if (cc3_exc_pending(ctx)) goto cleanup;
    }
    {   /* _apply_max_tokens(..., profile_max=profile.get_max_tokens(model)) */
        json_t *ga = json_object();
        cc3_arg_node(ga, "model", model);
        json_t *profile_max = cc3_call(ctx, "get_max_tokens", ga);
        if (cc3_exc_pending(ctx)) { if (profile_max) json_free(profile_max); goto cleanup; }
        json_t *aa = json_object();
        cc3_arg_node(aa, "api_kwargs", api_kwargs);
        cc3_arg_node(aa, "model", model);
        cc3_arg_node(aa, "reasoning_config", reasoning_config);
        cc3_arg_node(aa, "params", params);
        cc3_arg_node(aa, "profile_max", profile_max);
        json_t *amr = cc3_call(ctx, "_apply_max_tokens", aa);
        if (profile_max) json_free(profile_max);
        if (cc3_exc_pending(ctx)) { if (amr) json_free(amr); goto cleanup; }
        if (cc3_is_obj(amr)) {
            json_t *upd = json_obj_get(amr, "__update__");
            if (cc3_is_obj(upd)) cc3_obj_update(api_kwargs, upd);
        }
        if (amr) json_free(amr);
    }
    {   /* extra_body_from_profile, top_level_from_profile = profile.build_api_kwargs_extras(...) */
        json_t *lm = cc3_get(params, "lmstudio_reasoning_options");
        json_t *ba = json_object();
        cc3_arg_node(ba, "reasoning_config", reasoning_config);
        json_set(ba, "supports_reasoning", json_bool(cc3_truthy(cc3_get(params, "supports_reasoning"))));
        cc3_arg_node(ba, "qwen_session_metadata", cc3_get(params, "qwen_session_metadata"));
        cc3_arg_node(ba, "model", model);
        cc3_arg_node(ba, "base_url", cc3_get(params, "base_url"));
        cc3_arg_node(ba, "ollama_num_ctx", cc3_get(params, "ollama_num_ctx"));
        cc3_arg_node(ba, "session_id", cc3_get(params, "session_id"));
        cc3_arg_node(ba, "cache_scope_id", cc3_get(params, "cache_scope_id"));
        if (!cc3_is_none(lm)) cc3_arg_node(ba, "lmstudio_reasoning_options", lm);
        json_t *pair = cc3_call(ctx, "build_api_kwargs_extras", ba);
        if (cc3_exc_pending(ctx)) { if (pair) json_free(pair); goto cleanup; }
        if (!cc3_is_arr(pair) || pair->c.count != 2) {
            int got = cc3_is_arr(pair) ? (int)pair->c.count : 0;
            char msg[128];
            if (got < 2) snprintf(msg, sizeof msg, "not enough values to unpack (expected 2, got %d)", got);
            else snprintf(msg, sizeof msg, "too many values to unpack (expected 2)");
            cc3_raise_msg(ctx, "ValueError", msg);
            if (pair) json_free(pair);
            goto cleanup;
        }
        extras_body = json_copy(pair->c.items[0]);
        top_level = json_copy(pair->c.items[1]);
        json_free(pair);
    }
    cc3_obj_update(api_kwargs, top_level);
    if (top_level) { json_free(top_level); top_level = NULL; }
    extra_body = json_object();
    {   /* profile_body = profile.build_extra_body(...) */
        json_t *ba = json_object();
        cc3_arg_node(ba, "session_id", cc3_get(params, "session_id"));
        cc3_arg_node(ba, "provider_preferences", cc3_get(params, "provider_preferences"));
        cc3_arg_node(ba, "model", model);
        cc3_arg_node(ba, "base_url", cc3_get(params, "base_url"));
        cc3_arg_node(ba, "reasoning_config", reasoning_config);
        cc3_arg_node(ba, "openrouter_min_coding_score", cc3_get(params, "openrouter_min_coding_score"));
        cc3_arg_node(ba, "data_use", cc3_get(params, "data_use"));
        profile_body = cc3_call(ctx, "build_extra_body", ba);
        if (cc3_exc_pending(ctx)) goto cleanup;
    }
    if (cc3_truthy(profile_body)) cc3_obj_update(extra_body, profile_body);
    if (cc3_truthy(extras_body)) cc3_obj_update(extra_body, extras_body);
    {
        json_t *add = cc3_get(params, "extra_body_additions");
        if (cc3_truthy(add)) cc3_obj_update(extra_body, add);
    }
    if (profile_body) { json_free(profile_body); profile_body = NULL; }
    if (extras_body) { json_free(extras_body); extras_body = NULL; }
    {   /* for k, v in (params.get("request_overrides") or {}).items(): ... */
        json_t *ro = cc3_get(params, "request_overrides");
        if (cc3_is_obj(ro)) {
            for (size_t i = 0; i < ro->c.count; i++) {
                const char *k = ro->c.keys[i];
                json_t *v = ro->c.items[i];
                if (cc3_str_eq(k, "extra_body") && cc3_is_obj(v)) cc3_obj_update(extra_body, v);
                else json_set(api_kwargs, k, json_copy(v));
            }
        }
    }
    if (cc3_truthy(extra_body)) {
        json_t *na = json_object();
        cc3_arg_node(na, "base_url", cc3_get(params, "base_url"));
        json_t *ng = cc3_call(ctx, "is_native_gemini_base_url", na);
        bool native_gemini = false;
        if (cc3_exc_pending(ctx)) {
            if (cc3_exc_is_exception(ctx)) { cc3_exc_clear(ctx); native_gemini = false; }
            else { if (ng) json_free(ng); goto cleanup; }
        } else {
            native_gemini = cc3_truthy(ng);
        }
        if (ng) json_free(ng);
        if (native_gemini) {
            json_t *filtered = json_object();
            for (size_t i = 0; i < extra_body->c.count; i++) {
                const char *k = extra_body->c.keys[i];
                if (cc3_str_eq(k, "thinking_config") || cc3_str_eq(k, "thinkingConfig")
                    || cc3_str_eq(k, "max_output_tokens"))
                    json_set(filtered, k, json_copy(extra_body->c.items[i]));
            }
            json_free(extra_body);
            extra_body = filtered;
        }
        if (cc3_truthy(extra_body)) json_set(api_kwargs, "extra_body", json_copy(extra_body));
    }
    if (extra_body) { json_free(extra_body); extra_body = NULL; }
    {   /* return _finish_kwargs(..., supports_prompt_cache_key=bool(getattr(profile, "supports_prompt_cache_key", False))) */
        json_t *sp = cc3_getattr_def(profile, "supports_prompt_cache_key");
        bool supports = cc3_truthy(sp);
        json_t *fa = json_object();
        cc3_arg_node(fa, "api_kwargs", api_kwargs);
        cc3_arg_node(fa, "sanitized", sanitized);
        cc3_arg_node(fa, "params", params);
        json_set(fa, "supports_prompt_cache_key", json_bool(supports));
        final = cc3_call(ctx, "_finish_kwargs", fa);
        if (cc3_exc_pending(ctx)) goto cleanup;
    }
cleanup:
    if (sanitized) json_free(sanitized);
    if (api_kwargs) json_free(api_kwargs);
    if (reasoning_config) json_free(reasoning_config);
    if (extra_body) json_free(extra_body);
    if (top_level) json_free(top_level);
    if (profile_body) json_free(profile_body);
    if (extras_body) json_free(extras_body);
    return final;
}

/* ── normalize_response ──────────────────────────────────────────────────── */

/* PoP: cc3_normalize_response @ agent/transports/chat_completions.py:normalize_response */
static json_t *cc3_normalize_response(cc3_ctx *ctx, const json_t *response) {
    json_t *finish_reason = NULL, *tool_calls = NULL, *usage = NULL, *provider_data = NULL;
    json_t *final_content = NULL, *final_reasoning = NULL, *text_keys = NULL, *opaque_keys = NULL;
    json_t *out = NULL;
    json_t *choices = cc3_getattr_def(response, "choices");
    if (!choices) {
        const char *tn = cc3_is_obj(response) ? "types.SimpleNamespace"
            : cc3_is_none(response) ? "NoneType"
            : cc3_is_arr(response) ? "list"
            : cc3_is_str(response) ? "str"
            : cc3_is_bool(response) ? "bool"
            : cc3_is_num(response) ? (response->num_val == floor(response->num_val) ? "int" : "float")
            : "object";
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'choices'", tn);
        cc3_raise_msg(ctx, "AttributeError", msg);
        goto cleanup;
    }
    if (cc3_is_none(choices)) {
        cc3_raise_msg(ctx, "TypeError", "'NoneType' object is not subscriptable");
        goto cleanup;
    }
    if (!cc3_is_arr(choices)) {
        cc3_raise_msg(ctx, "TypeError", "'str' object is not subscriptable");
        goto cleanup;
    }
    if (choices->c.count == 0) {
        cc3_raise_msg(ctx, "IndexError", "list index out of range");
        goto cleanup;
    }
    json_t *choice = choices->c.items[0];
    json_t *msg = cc3_getattr_def(choice, "message");
    json_t *fr = cc3_getattr_def(choice, "finish_reason");
    {   /* finish_reason = _normalize_finish_reason(str(_fr) if isinstance(_fr, int) else _fr) or "stop" */
        json_t *fr_arg;
        bool fr_is_int = cc3_is_bool(fr) || (cc3_is_obj(fr) && json_has(fr, "__int__"));
        if (fr_is_int) {
            char *s = cc3_is_bool(fr)
                ? cc3_dup_span(fr->bool_val ? "True" : "False", fr->bool_val ? 4 : 5)
                : cc3_int_marker_str(fr);
            fr_arg = json_string(s);
            free(s);
        } else {
            fr_arg = fr ? json_copy(fr) : json_null();
        }
        json_t *na = json_object();
        cc3_arg_node(na, "finish_reason", fr_arg);
        json_free(fr_arg);
        json_t *nfr = cc3_call(ctx, "_normalize_finish_reason", na);
        if (cc3_exc_pending(ctx)) { if (nfr) json_free(nfr); goto cleanup; }
        if (cc3_truthy(nfr)) finish_reason = nfr;
        else { if (nfr) json_free(nfr); finish_reason = json_string("stop"); }
    }
    {   /* tool_calls = [tc for tc in (self._normalize_tool_call(tc) for tc in msg.tool_calls) if tc is not None] */
        json_t *msg_tc = cc3_getattr_def(msg, "tool_calls");
        if (cc3_truthy(msg_tc)) {
            if (!cc3_is_arr(msg_tc)) {
                cc3_raise_msg(ctx, "TypeError", "'int' object is not iterable");
                goto cleanup;
            }
            tool_calls = json_array();
            for (size_t i = 0; i < msg_tc->c.count; i++) {
                json_t *ta = json_object();
                cc3_arg_node(ta, "tc", msg_tc->c.items[i]);
                json_t *ntc = cc3_call(ctx, "_normalize_tool_call", ta);
                if (cc3_exc_pending(ctx)) { if (ntc) json_free(ntc); goto cleanup; }
                if (!cc3_is_none(ntc)) json_append(tool_calls, ntc);
                else if (ntc) json_free(ntc);
            }
        }
    }
    {   /* usage = Usage.from_openai(response.usage) if hasattr(response, "usage") and response.usage else None */
        if (cc3_is_obj(response) && json_has(response, "usage")) {
            json_t *u = cc3_get(response, "usage");
            if (cc3_truthy(u)) {
                json_t *ua = json_object();
                cc3_arg_node(ua, "u", u);
                usage = cc3_call(ctx, "usage_from_openai", ua);
                if (cc3_exc_pending(ctx)) goto cleanup;
            }
        }
    }
    provider_data = json_object();
    {   /* reasoning_content = _attr_or_model_extra(msg, "reasoning_content") */
        json_t *rc = cc3_attr_or_model_extra_seam(ctx, msg, "reasoning_content");
        if (cc3_exc_pending(ctx)) goto cleanup;
        if (!cc3_is_none(rc)) json_set(provider_data, "reasoning_content", rc);
        else if (rc) json_free(rc);
    }
    {   /* if getattr(msg, "reasoning_details", None): provider_data["reasoning_details"] = msg.reasoning_details */
        json_t *rd = cc3_getattr_def(msg, "reasoning_details");
        if (cc3_truthy(rd)) json_set(provider_data, "reasoning_details", json_copy(rd));
    }
    {   /* opaque_keys, text_keys = declared_carrier_keys() */
        json_t *dk = cc3_call(ctx, "declared_carrier_keys", json_object());
        if (cc3_exc_pending(ctx)) { if (dk) json_free(dk); goto cleanup; }
        if (cc3_is_arr(dk) && dk->c.count == 2) {
            opaque_keys = json_copy(dk->c.items[0]);
            text_keys = json_copy(dk->c.items[1]);
        } else {
            cc3_raise_msg(ctx, "ValueError", "not enough values to unpack (expected 2, got 0)");
        }
        if (dk) json_free(dk);
        if (cc3_exc_pending(ctx)) goto cleanup;
    }
    {   /* for key in dict.fromkeys(("extra_content", *opaque_keys, *text_keys)): ... */
        json_t *keys = json_array();
        json_append(keys, json_string("extra_content"));
        for (size_t i = 0; i < opaque_keys->c.count; i++) json_append(keys, json_copy(opaque_keys->c.items[i]));
        for (size_t i = 0; i < text_keys->c.count; i++) json_append(keys, json_copy(text_keys->c.items[i]));
        json_t *uniq = json_array();
        for (size_t i = 0; i < keys->c.count; i++) {
            const char *k = cc3_jstr(keys->c.items[i]);
            if (!cc3_strarr_has(uniq, k)) json_append(uniq, json_copy(keys->c.items[i]));
        }
        json_free(keys);
        for (size_t i = 0; i < uniq->c.count; i++) {
            const char *key = cc3_jstr(uniq->c.items[i]);
            json_t *value = cc3_attr_or_model_extra_seam(ctx, msg, key);
            if (cc3_exc_pending(ctx)) { if (value) json_free(value); json_free(uniq); goto cleanup; }
            if (!cc3_is_none(value)) {
                json_t *dumped = cc3_dump_extra_content_seam(ctx, value);
                if (cc3_exc_pending(ctx)) {
                    if (dumped) json_free(dumped);
                    if (value) json_free(value);
                    json_free(uniq);
                    goto cleanup;
                }
                json_set(provider_data, key, dumped ? dumped : json_null());
            }
            if (value) json_free(value);
        }
        json_free(uniq);
    }
    if (opaque_keys) { json_free(opaque_keys); opaque_keys = NULL; }
    {   /* content = getattr(msg, "content", None); refusal = _attr_or_model_extra(msg, "refusal") */
        json_t *content = cc3_getattr_def(msg, "content");
        final_content = content ? json_copy(content) : json_null();
        json_t *refusal = cc3_attr_or_model_extra_seam(ctx, msg, "refusal");
        if (cc3_exc_pending(ctx)) goto cleanup;
        if (cc3_is_str(refusal) && refusal->str_val) {
            char *rst = cc3_py_strip(refusal->str_val);
            bool refusal_truthy = rst && rst[0];
            free(rst);
            if (refusal_truthy) {
                json_set(provider_data, "refusal", json_copy(refusal));
                bool content_str_nonempty = false;
                if (cc3_is_str(content) && content->str_val) {
                    char *cst = cc3_py_strip(content->str_val);
                    content_str_nonempty = cst && cst[0];
                    free(cst);
                }
                if (!content_str_nonempty && !cc3_truthy(tool_calls)) {
                    json_free(final_content);
                    final_content = json_copy(refusal);
                    if (cc3_is_none(finish_reason)
                        || (cc3_is_str(finish_reason) && cc3_str_eq(cc3_jstr(finish_reason), "stop"))) {
                        json_free(finish_reason);
                        finish_reason = json_string("content_filter");
                    }
                }
            }
        }
        if (refusal) json_free(refusal);
    }
    {   /* reasoning = getattr(msg, "reasoning", None) or next((provider_data[k] for k in text_keys if provider_data.get(k)), None) */
        json_t *reasoning = cc3_getattr_def(msg, "reasoning");
        if (cc3_truthy(reasoning)) {
            final_reasoning = json_copy(reasoning);
        } else {
            final_reasoning = json_null();
            for (size_t i = 0; i < text_keys->c.count; i++) {
                const char *k = cc3_jstr(text_keys->c.items[i]);
                json_t *pv = cc3_get(provider_data, k);
                if (cc3_truthy(pv)) { final_reasoning = json_copy(pv); break; }
            }
        }
    }
    out = json_object();
    json_set(out, "content", final_content ? final_content : json_null());
    final_content = NULL;
    json_set(out, "tool_calls", tool_calls ? tool_calls : json_null());
    tool_calls = NULL;
    json_set(out, "finish_reason", finish_reason ? finish_reason : json_null());
    finish_reason = NULL;
    json_set(out, "reasoning", final_reasoning ? final_reasoning : json_null());
    final_reasoning = NULL;
    json_set(out, "usage", usage ? usage : json_null());
    usage = NULL;
    if (cc3_truthy(provider_data)) { json_set(out, "provider_data", provider_data); provider_data = NULL; }
    else json_set(out, "provider_data", json_null());
cleanup:
    if (finish_reason) json_free(finish_reason);
    if (tool_calls) json_free(tool_calls);
    if (usage) json_free(usage);
    if (provider_data) json_free(provider_data);
    if (final_content) json_free(final_content);
    if (final_reasoning) json_free(final_reasoning);
    if (text_keys) json_free(text_keys);
    if (opaque_keys) json_free(opaque_keys);
    return out;
}

/* ── validate_response / extract_cache_stats ─────────────────────────────── */

/* PoP: cc3_validate_response @ agent/transports/chat_completions.py:validate_response */
static json_t *cc3_validate_response(cc3_ctx *ctx, const json_t *response) {
    if (cc3_is_none(response)) return json_bool(false);
    json_t *choices = cc3_getattr_def(response, "choices");
    if (!cc3_truthy(choices)) return json_bool(false);
    json_t *a = json_object();
    cc3_arg_node(a, "response", response);
    json_t *shim = cc3_call(ctx, "is_router_timeout_shim", a);
    if (cc3_exc_pending(ctx)) { if (shim) json_free(shim); return NULL; }
    bool is_shim = cc3_truthy(shim);
    if (shim) json_free(shim);
    return json_bool(!is_shim);
}

/* PoP: cc3_extract_cache_stats @ agent/transports/chat_completions.py:extract_cache_stats */
static json_t *cc3_extract_cache_stats(const json_t *response) {
    json_t *usage = cc3_getattr_def(response, "usage");
    if (cc3_is_none(usage)) return json_null();
    json_t *details = cc3_getattr_def(usage, "prompt_tokens_details");
    /* cached = (getattr(details, "cached_tokens", 0) or 0) if details else 0.
     * details is an attribute double (always truthy) when an object. */
    json_t *cached = json_number(0);
    json_t *written = json_number(0);
    if (cc3_double_truthy(details)) {
        json_t *ct = cc3_getattr_def(details, "cached_tokens");
        if (cc3_truthy(ct)) { json_free(cached); cached = json_copy(ct); }
        json_t *cw = cc3_getattr_def(details, "cache_write_tokens");
        if (cc3_truthy(cw)) { json_free(written); written = json_copy(cw); }
    }
    /* cached = cached or getattr(usage, "prompt_cache_hit_tokens", 0) or 0 */
    if (!cc3_truthy(cached)) {
        json_t *hit = cc3_getattr_def(usage, "prompt_cache_hit_tokens");
        if (cc3_truthy(hit)) { json_free(cached); cached = json_copy(hit); }
    }
    if (!cc3_truthy(cached) && !cc3_truthy(written)) {
        json_free(cached); json_free(written);
        return json_null();
    }
    json_t *out = json_object();
    json_set(out, "cached_tokens", cached);
    json_set(out, "creation_tokens", written);
    return out;
}