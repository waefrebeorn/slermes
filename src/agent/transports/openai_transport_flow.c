/* openai_transport_flow.c — FLOW + leaves port of the UNCLAIMED remainder of
 * the OpenAI Chat Completions transport. In this quarry snapshot the OpenAI
 * transport lives at agent/transports/chat_completions.py (the task's
 * agent/transports/openai.py path does not exist here; the module docstring is
 * "OpenAI Chat Completions transport (default api_mode for OpenAI-compatible
 * providers)"). PoP lines therefore name the LIVE module path. The claimed
 * siblings live in chat_completions_ports.c / chat_completion_helpers_flow.c /
 * provider_profile.c and are SEAMED here; three unclaimed transport-type leaves
 * (ToolCall._pd, NormalizedResponse._pd, Usage.from_openai) come from
 * agent/transports/types.py.
 *
 * Decision-sequence port of the transport's sanitization / kwargs / normalization
 * layer: the xAI tool-search bridge aliaser, the provider-profile signature-replay
 * gate, the reasoning-carrier wire table, the thought-signature replay predicate,
 * the pydantic attribute/model_extra reader, the extra_content model_dump retry
 * ladder, the Pareto coding-score floor, the GPT-5/Codex developer-role swap, the
 * max_tokens budget applier, the base-kwargs scaffold, the prompt-cache-key tail,
 * the Mistral tool-id route gate + stable base62 id digest, the message sanitizer,
 * the ChatCompletionsTransport._normalize_tool_call method, and the two
 * provider_data accessors + Usage.from_openai.
 *
 * ALL collaborator I/O (claimed siblings, foreign-module imports, the model_dump
 * object method) rides the injected universal seam oai_call; the harness scripts
 * results and records the ordered trace the oracle diffs against LIVE
 * agent/transports/chat_completions.py under the same scripts. Ported functions
 * call each other REAL (internal oai_* composition: _sanitize_message ->
 * _mistral_tool_id / _has_replayable_thought_signature; _normalize_tool_call ->
 * _attr_or_model_extra / _dump_extra_content).
 *
 * Seam name table (name -> live call):
 *   _alias_reserved_tools            agent.transports.codex._alias_reserved_tools
 *                                    (in-function import; name_of/rename lambdas
 *                                    are never evaluated -> args project
 *                                    {tools, reserved_names:["tool_search"]})
 *   get_provider_profile             providers.get_provider_profile (in-function)
 *   route_reasoning_carriers         agent.message_sanitization.route_reasoning_carriers
 *   base_url_host_matches            utils.base_url_host_matches (in-function)
 *   coalesce_tool_call_id            session_rules.turns.coalesce_tool_call_id (module-top)
 *   is_moonshot_model                agent.moonshot_schema.is_moonshot_model (module-top)
 *   sanitize_moonshot_tools          agent.moonshot_schema.sanitize_moonshot_tools (module-top)
 *   _raise_gemini_thinking_max_tokens  CLAIMED sibling (chat_completions_ports.c)
 *   _add_prompt_cache_key            CLAIMED sibling (chat_completions_ports.c);
 *                                    void mutator: a {"__update__":{...}} script
 *                                    result merges into api_kwargs (both sides)
 *   max_tokens_param_fn              params["max_tokens_param_fn"] callback (truthy
 *                                    non-empty dict marker models presence)
 *   model_dump                       extra.model_dump(**kwargs) object method
 *                                    (has_model_dump input flag attaches it)
 *
 * Representation contract (shared with tests/oracle_openai_transport_flow.py):
 *   - scripts: MISSING key -> fail-closed RuntimeError("no script for <name>");
 *     null -> None; {"$seq":[...]} per-call queue (exhaustion raises
 *     RuntimeError("script exhausted: <name>")); {"__raise__":cls,"msg":m,...}
 *     raises (trace ret is the WHOLE marker); any other value reused (a bare ARRAY
 *     is a LIST RESULT, not a queue). None-valued seam arg keys dropped on BOTH sides.
 *   - get_provider_profile results are attribute dicts (oracle materializes a
 *     SimpleNamespace; C reads the JSON fields). _alias_reserved_tools results are
 *     2-element arrays [tools, alias_map] (returned verbatim, no unpack).
 *     route_reasoning_carriers results are frozensets -> SORTED string arrays.
 *   - tuple -> array; set/frozenset -> sorted array; None -> JSON null; integral
 *     floats collapse to ints on both sides; uncaught exception -> {"__raise__":cls}.
 *   - except TypeError / except Exception sites (_dump_extra_content) swallow the
 *     matching family; the BaseException trio (SystemExit/KeyboardInterrupt/
 *     GeneratorExit) passes through. Full builtin exception-parent table below.
 *   - OMIT_TEMPERATURE (providers.base object() sentinel) is modeled by the string
 *     marker "__OMIT_TEMPERATURE__" in profile.fixed_temperature; the oracle
 *     materializes it to the real sentinel for the `is` identity test.
 *
 * Fixture domain (ASCII scope):
 *   - strings ASCII (str.strip/lower transcribed byte-wise; str.strip set is
 *     White_Space incl \x1c-\x1f; float() strips the NARROWER set without
 *     \x1c-\x1f). base_url / provider / model are str-or-null.
 *   - str()-computed positions carry strings or integers only (no non-integral
 *     float repr); _pareto_score returns a number-or-null (integral floats
 *     collapse via norm).
 *   - coalesce_tool_call_id scripts are str-or-null ("" is falsy); profile scripts
 *     carry name + replays_tool_call_signatures; max_tokens_param_fn scripts are
 *     dicts (dict.update domain); route_reasoning_carriers scripts are string arrays.
 *   - _mistral_tool_id non-str keys use null/bool/list/dict/non-integral float only
 *     (JSON numbers lose int/float identity for the TypeError type name).
 *   - _normalize_tool_call tc.extra_content is a PLAIN value (no model_dump method;
 *     has_model_dump rides only the direct _dump_extra_content cases).
 *   - provider_data (_pd) truthy non-dict cases use list/str/bool (no numbers).
 *   - profile.fixed_temperature is numeric / null / the OMIT marker; profile and
 *     self doubles always carry the attributes the live body reads directly
 *     (one absent-fixed_temperature case exercises the AttributeError branch).
 *
 * libjson-only (default oracle deps suffice). Self-contained: the harness
 * #includes "agent/transports/openai_transport_flow.c". SHA-256 is implemented
 * inline (no libcrypto dependency).
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

/* ── context + seam plumbing ─────────────────────────────────────────────── */

typedef struct oai_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} oai_ctx;

static bool oai_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool oai_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool oai_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool oai_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool oai_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool oai_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool oai_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
/* `x is True` identity gate (JSON_BOOL true only). */
static bool oai_is_true(const json_t *v) { return v && v->type == JSON_BOOL && v->bool_val; }

static bool oai_truthy(const json_t *v) {
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
static const char *oai_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
/* borrowed object field or NULL */
static json_t *oai_get(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}
/* getattr(obj, key, None) — non-object / absent reads NULL. Borrowed. */
static json_t *oai_getattr_def(const json_t *obj, const char *key) {
    if (!oai_is_obj(obj)) return NULL;
    return json_obj_get(obj, key);
}

/* ── string helpers (caller frees) ───────────────────────────────────────── */

static char *oai_dup_span(const char *s, size_t len) {
    char *r = (char *)malloc(len + 1);
    if (!r) return NULL;
    memcpy(r, s, len);
    r[len] = '\0';
    return r;
}
static bool oai_is_ws(char c) {
    unsigned char u = (unsigned char)c;
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r'
        || (u >= 0x1c && u <= 0x1f);
}
static char *oai_py_strip(const char *s) {
    if (!s) return oai_dup_span("", 0);
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && oai_is_ws(s[a])) a++;
    while (b > a && oai_is_ws(s[b - 1])) b--;
    return oai_dup_span(s + a, b - a);
}
static char *oai_py_lower(const char *s) {
    char *r = oai_dup_span(s ? s : "", strlen(s ? s : ""));
    if (!r) return NULL;
    for (char *p = r; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    return r;
}
/* str(v) for a scalar (null -> "None"). Caller frees. */
static char *oai_pystr(const json_t *v) {
    if (oai_is_none(v)) return oai_dup_span("None", 4);
    if (oai_is_bool(v)) return oai_dup_span(v->bool_val ? "True" : "False", v->bool_val ? 4 : 5);
    if (oai_is_num(v)) {
        char buf[64]; double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
        else snprintf(buf, sizeof buf, "%g", d);
        return oai_dup_span(buf, strlen(buf));
    }
    if (oai_is_str(v)) return oai_dup_span(oai_jstr(v), strlen(oai_jstr(v)));
    return oai_dup_span("", 0);
}
/* str(v or "") for a scalar (falsy -> ""). Caller frees. */
static char *oai_str_or_empty(const json_t *v) {
    if (!oai_truthy(v)) return oai_dup_span("", 0);
    return oai_pystr(v);
}

/* ── exception plumbing (full builtin hierarchy) ─────────────────────────── */

static bool oai_exc_pending(const oai_ctx *ctx) { return ctx && ctx->exc != NULL; }
static void oai_exc_clear(oai_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}
static void oai_raise_msg(oai_ctx *ctx, const char *cls, const char *msg) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
    json_set(ctx->exc, "msg", json_string(msg ? msg : ""));
}
static const char *oai_exc_class(const oai_ctx *ctx) {
    return json_get_str(ctx->exc, "__raise__", "");
}
/* FULL builtin exception subclass chain (parent lookup). */
static const char *oai_exc_parent(const char *c) {
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
        if (oai_str_eq(T[i].c, c)) return T[i].p;
    return NULL;
}
static bool oai_exc_matches(const oai_ctx *ctx, const char *const *classes, size_t n) {
    if (!oai_exc_pending(ctx)) return false;
    const char *cls = oai_exc_class(ctx);
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (oai_str_eq(cls, classes[i])) return true;
        const char *p = oai_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}
/* except Exception (NOT the BaseException trio). */
static bool oai_exc_is_exception(const oai_ctx *ctx) {
    static const char *E[] = {"Exception"};
    return oai_exc_matches(ctx, E, 1);
}
/* except TypeError (TypeError + subclasses). */
static bool oai_exc_is_typeerror(const oai_ctx *ctx) {
    static const char *E[] = {"TypeError"};
    return oai_exc_matches(ctx, E, 1);
}

/* ── universal seam (fail-closed) ────────────────────────────────────────── */

/* args ownership is TRANSFERRED (NULL -> empty object). Result is NEW. */
static json_t *oai_call(oai_ctx *ctx, const char *name, json_t *args) {
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
static void oai_arg_str(json_t *args, const char *key, const char *val) {
    if (!val) return;
    json_set(args, key, json_string(val));
}
static void oai_arg_node(json_t *args, const char *key, const json_t *val) {
    if (!oai_is_none(val)) json_set(args, key, json_copy(val));
}
/* dict.update: copy every src key into dest. */
static void oai_obj_update(json_t *dest, const json_t *src) {
    if (!oai_is_obj(dest) || !oai_is_obj(src)) return;
    for (size_t i = 0; i < src->c.count; i++)
        json_set(dest, src->c.keys[i], json_copy(src->c.items[i]));
}
/* in-place sort of a string array (frozenset projection) */
static int oai_cmp_jstr(const void *a, const void *b) {
    const json_t *ja = *(const json_t *const *)a;
    const json_t *jb = *(const json_t *const *)b;
    return strcmp(oai_jstr(ja), oai_jstr(jb));
}
static void oai_sort_str_array(json_t *arr) {
    if (oai_is_arr(arr) && arr->c.count > 1)
        qsort(arr->c.items, arr->c.count, sizeof(json_t *), oai_cmp_jstr);
}
/* string-array membership / append (strip_keys / keys lists) */
static bool oai_strarr_has(const json_t *arr, const char *s) {
    if (!oai_is_arr(arr)) return false;
    for (size_t i = 0; i < arr->c.count; i++)
        if (oai_is_str(arr->c.items[i]) && oai_str_eq(arr->c.items[i]->str_val, s)) return true;
    return false;
}
static void oai_strarr_add(json_t *arr, const char *s) {
    json_append(arr, json_string(s));
}

/* ── SHA-256 (self-contained, for _mistral_tool_id) ──────────────────────── */

static uint32_t oai_rotr(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }
static void oai_sha256(const unsigned char *data, size_t len, unsigned char out[32]) {
    static const uint32_t K[64] = {
        0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
        0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
        0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
        0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
        0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
        0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
        0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
        0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u,
    };
    uint32_t h[8] = {
        0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u,
    };
    size_t padded = ((len + 8) / 64 + 1) * 64;
    unsigned char *buf = (unsigned char *)calloc(padded, 1);
    if (!buf) { memset(out, 0, 32); return; }
    memcpy(buf, data, len);
    buf[len] = 0x80;
    uint64_t bitlen = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++) buf[padded - 1 - i] = (unsigned char)(bitlen >> (8 * i));
    for (size_t off = 0; off < padded; off += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++)
            w[i] = ((uint32_t)buf[off + 4*i] << 24) | ((uint32_t)buf[off + 4*i+1] << 16)
                 | ((uint32_t)buf[off + 4*i+2] << 8) | (uint32_t)buf[off + 4*i+3];
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = oai_rotr(w[i-15],7) ^ oai_rotr(w[i-15],18) ^ (w[i-15] >> 3);
            uint32_t s1 = oai_rotr(w[i-2],17) ^ oai_rotr(w[i-2],19) ^ (w[i-2] >> 10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = oai_rotr(e,6) ^ oai_rotr(e,11) ^ oai_rotr(e,25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            uint32_t S0 = oai_rotr(a,2) ^ oai_rotr(a,13) ^ oai_rotr(a,22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + maj;
            hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }
    free(buf);
    for (int i = 0; i < 8; i++) {
        out[4*i]   = (unsigned char)(h[i] >> 24);
        out[4*i+1] = (unsigned char)(h[i] >> 16);
        out[4*i+2] = (unsigned char)(h[i] >> 8);
        out[4*i+3] = (unsigned char)(h[i]);
    }
}

/* ── Python float() string parser (ASCII scope, for _pareto_score) ───────── */

/* float() strips the NARROWER whitespace set (no \x1c-\x1f). */
static bool oai_float_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}
static bool oai_ascii_digit(char c) { return c >= '0' && c <= '9'; }
/* Validate the Python float grammar over [s,len) and build the underscore-free
 * clean buffer. Underscores are legal ONLY between two digits. Returns false on
 * a grammar violation (ValueError). */
static bool oai_float_grammar(const char *s, size_t len, char *clean, size_t *clen) {
    size_t i = 0, cl = 0;
    bool anydigit = false;
    /* integer part */
    while (i < len && (oai_ascii_digit(s[i]) || s[i] == '_')) {
        if (s[i] == '_') {
            if (i == 0 || !oai_ascii_digit(s[i-1]) || i + 1 >= len || !oai_ascii_digit(s[i+1]))
                return false;
        } else { anydigit = true; clean[cl++] = s[i]; }
        i++;
    }
    /* fractional part */
    if (i < len && s[i] == '.') {
        clean[cl++] = s[i]; i++;
        while (i < len && (oai_ascii_digit(s[i]) || s[i] == '_')) {
            if (s[i] == '_') {
                if (i == 0 || !oai_ascii_digit(s[i-1]) || i + 1 >= len || !oai_ascii_digit(s[i+1]))
                    return false;
            } else { anydigit = true; clean[cl++] = s[i]; }
            i++;
        }
    }
    if (!anydigit) return false;
    /* exponent */
    if (i < len && (s[i] == 'e' || s[i] == 'E')) {
        clean[cl++] = s[i]; i++;
        if (i < len && (s[i] == '+' || s[i] == '-')) { clean[cl++] = s[i]; i++; }
        bool expdigit = false;
        while (i < len && (oai_ascii_digit(s[i]) || s[i] == '_')) {
            if (s[i] == '_') {
                if (i == 0 || !oai_ascii_digit(s[i-1]) || i + 1 >= len || !oai_ascii_digit(s[i+1]))
                    return false;
            } else { expdigit = true; clean[cl++] = s[i]; }
            i++;
        }
        if (!expdigit) return false;
    }
    if (i != len) return false;
    clean[cl] = '\0';
    *clen = cl;
    return true;
}
/* float(s) on an ASCII string: 1 on success (*out set), 0 on ValueError. */
static bool oai_parse_float(const char *s, double *out) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && oai_float_ws(s[a])) a++;
    while (b > a && oai_float_ws(s[b - 1])) b--;
    if (a == b) return false;
    const char *p = s + a;
    size_t len = b - a;
    bool neg = false;
    if (p[0] == '+' || p[0] == '-') { neg = (p[0] == '-'); p++; len--; }
    if (len == 0) return false;
    /* inf / infinity / nan (case-insensitive) */
    if (len <= 8) {
        char low[9];
        for (size_t i = 0; i < len; i++) {
            char c = p[i];
            low[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
        }
        low[len] = '\0';
        if (oai_str_eq(low, "inf") || oai_str_eq(low, "infinity")) {
            *out = neg ? -HUGE_VAL : HUGE_VAL;
            return true;
        }
        if (oai_str_eq(low, "nan")) { *out = NAN; return true; }
    }
    char *clean = (char *)malloc(len + 2);
    if (!clean) return false;
    size_t cl = 0;
    if (!oai_float_grammar(p, len, clean, &cl)) { free(clean); return false; }
    if (neg) { memmove(clean + 1, clean, cl + 1); clean[0] = '-'; }
    char *end = NULL;
    double v = strtod(clean, &end);
    bool ok = end && *end == '\0';
    free(clean);
    if (!ok) return false;
    *out = v;
    return true;
}
/* ── pure leaves ─────────────────────────────────────────────────────────── */

/* PoP: oai_has_replayable_thought_signature @ agent/transports/chat_completions.py:_has_replayable_thought_signature */
static bool oai_has_replayable_thought_signature(const json_t *extra_content) {
    if (!oai_is_obj(extra_content)) return false;
    json_t *candidate = oai_get(extra_content, "thought_signature");
    json_t *google = oai_get(extra_content, "google");
    if (oai_is_none(candidate) && oai_is_obj(google))
        candidate = json_obj_get(google, "thought_signature");
    if (!oai_is_str(candidate)) return false;
    char *st = oai_py_strip(candidate->str_val);
    bool ok = st && st[0] != '\0';
    free(st);
    return ok;
}

/* PoP: oai_attr_or_model_extra @ agent/transports/chat_completions.py:_attr_or_model_extra */
static json_t *oai_attr_or_model_extra(const json_t *obj, const char *name) {
    json_t *value = oai_getattr_def(obj, name);
    if (oai_is_none(value) && oai_is_obj(obj) && json_has(obj, "model_extra")) {
        json_t *me = json_obj_get(obj, "model_extra");
        value = oai_is_obj(me) ? json_obj_get(me, name) : NULL;
    }
    return value ? json_copy(value) : json_null();
}

/* PoP: oai_pareto_score @ agent/transports/chat_completions.py:_pareto_score */
static json_t *oai_pareto_score(const json_t *raw) {
    /* raw not in (None, "") -> else float(raw); TypeError/ValueError -> None */
    if (oai_is_none(raw)) return json_null();
    if (oai_is_str(raw) && raw->str_val && raw->str_val[0] == '\0') return json_null();
    double score = 0.0;
    bool have = false;
    if (oai_is_bool(raw)) { score = raw->bool_val ? 1.0 : 0.0; have = true; }
    else if (oai_is_num(raw)) { score = raw->num_val; have = true; }
    else if (oai_is_str(raw)) { have = oai_parse_float(raw->str_val, &score); }
    /* array/object: float() raises TypeError -> caught -> None */
    if (!have) return json_null();
    if (score >= 0.0 && score <= 1.0) return json_number(score);
    return json_null();
}

/* PoP: oai_swap_developer_role @ agent/transports/chat_completions.py:_swap_developer_role */
static json_t *oai_swap_developer_role(const json_t *sanitized, const char *model_lower) {
    /* DEVELOPER_ROLE_MODELS = ("gpt-5", "codex") — agent/prompt_builder.py */
    if (oai_truthy(sanitized) && oai_is_arr(sanitized) && sanitized->c.count > 0) {
        json_t *first = sanitized->c.items[0];
        if (oai_is_obj(first) && oai_str_eq(oai_jstr(oai_get(first, "role")), "system")
            && (strstr(model_lower ? model_lower : "", "gpt-5")
                || strstr(model_lower ? model_lower : "", "codex"))) {
            json_t *out = json_array();
            json_t *nf = json_copy(first);
            json_set(nf, "role", json_string("developer"));
            json_append(out, nf);
            for (size_t i = 1; i < sanitized->c.count; i++)
                json_append(out, json_copy(sanitized->c.items[i]));
            return out;
        }
    }
    return sanitized ? json_copy(sanitized) : json_null();
}

/* PoP: oai_mistral_tool_id @ agent/transports/chat_completions.py:_mistral_tool_id */
static json_t *oai_mistral_tool_id(oai_ctx *ctx, const json_t *key) {
    if (!oai_is_str(key)) {
        /* _MISTRAL_TOOL_ID_RE.fullmatch(non-str) -> TypeError */
        const char *tn = "object";
        if (oai_is_none(key)) tn = "NoneType";
        else if (oai_is_bool(key)) tn = "bool";
        else if (oai_is_arr(key)) tn = "list";
        else if (oai_is_obj(key)) tn = "dict";
        else if (oai_is_num(key)) tn = (key->num_val == floor(key->num_val)) ? "int" : "float";
        char msg[128];
        snprintf(msg, sizeof(msg), "expected string or bytes-like object, got '%s'", tn);
        oai_raise_msg(ctx, "TypeError", msg);
        return NULL;
    }
    const char *k = key->str_val ? key->str_val : "";
    size_t n = strlen(k);
    /* ^[A-Za-z0-9]{9}$ fullmatch -> passthrough */
    if (n == 9) {
        bool alnum = true;
        for (size_t i = 0; i < 9; i++) {
            char c = k[i];
            if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) {
                alnum = false;
                break;
            }
        }
        if (alnum) return json_string(k);
    }
    unsigned char dig[32];
    oai_sha256((const unsigned char *)k, n, dig);
    uint64_t number = 0;
    for (int i = 0; i < 8; i++) number = (number << 8) | dig[i];
    /* _MISTRAL_ID_ALPHABET = string.digits + ascii_uppercase + ascii_lowercase */
    static const char ALPH[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    char chars[10];
    for (int i = 0; i < 9; i++) {
        chars[i] = ALPH[number % 62];
        number /= 62;
    }
    chars[9] = '\0';
    return json_string(chars);
}

/* shared (provider_data or {}).get(key) body for the two _pd accessors */
static json_t *oai_pd_get(oai_ctx *ctx, const json_t *pd, const char *key) {
    if (oai_truthy(pd)) {
        if (oai_is_obj(pd)) {
            json_t *v = json_obj_get(pd, key);
            return v ? json_copy(v) : json_null();
        }
        const char *tn = oai_is_arr(pd) ? "list" : oai_is_str(pd) ? "str"
                       : oai_is_bool(pd) ? "bool"
                       : (pd->num_val == floor(pd->num_val) ? "int" : "float");
        char msg[128];
        snprintf(msg, sizeof(msg), "'%s' object has no attribute 'get'", tn);
        oai_raise_msg(ctx, "AttributeError", msg);
        return NULL;
    }
    return json_null();
}

/* PoP: oai_tool_call_pd @ agent/transports/types.py:_pd */
static json_t *oai_tool_call_pd(oai_ctx *ctx, const json_t *provider_data, const char *key) {
    return oai_pd_get(ctx, provider_data, key);
}

/* PoP: oai_normalized_response_pd @ agent/transports/types.py:_pd */
static json_t *oai_normalized_response_pd(oai_ctx *ctx, const json_t *provider_data, const char *key) {
    return oai_pd_get(ctx, provider_data, key);
}

/* PoP: oai_usage_from_openai @ agent/transports/types.py:from_openai */
static json_t *oai_usage_from_openai(const json_t *u) {
    static const char *KS[] = {"prompt_tokens", "completion_tokens", "total_tokens"};
    json_t *out = json_object();
    for (int i = 0; i < 3; i++) {
        json_t *v = oai_getattr_def(u, KS[i]);   /* getattr(u, k, 0): absent -> 0 */
        json_t *val;
        if (!v || !oai_truthy(v)) val = json_number(0);   /* ... or 0 */
        else val = json_copy(v);
        json_set(out, KS[i], val);
    }
    json_set(out, "cached_tokens", json_number(0));
    return out;
}

/* ── _dump_extra_content (model_dump retry ladder) ───────────────────────── */

/* PoP: oai_dump_extra_content @ agent/transports/chat_completions.py:_dump_extra_content */
static json_t *oai_dump_extra_content(oai_ctx *ctx, const json_t *extra, bool has_model_dump) {
    if (has_model_dump && oai_is_obj(extra)) {
        for (int round = 0; round < 2; round++) {
            json_t *kw = json_object();
            if (round == 0) json_set(kw, "warnings", json_bool(false));
            json_t *a = json_object();
            json_set(a, "kwargs", kw);
            json_t *r = oai_call(ctx, "model_dump", a);
            if (oai_exc_pending(ctx)) {
                if (r) json_free(r);
                if (oai_exc_is_typeerror(ctx)) { oai_exc_clear(ctx); continue; }
                if (oai_exc_is_exception(ctx)) { oai_exc_clear(ctx); break; }
                return NULL;   /* BaseException passthrough */
            }
            return r ? r : json_null();
        }
    }
    return oai_is_none(extra) ? json_null() : json_copy(extra);
}

/* ── route / provider gates ──────────────────────────────────────────────── */

/* PoP: oai_profile_replays_tool_call_signatures @ agent/transports/chat_completions.py:_profile_replays_tool_call_signatures */
static json_t *oai_profile_replays_tool_call_signatures(oai_ctx *ctx, const json_t *provider) {
    char *s = oai_str_or_empty(provider);
    char *st = oai_py_strip(s);
    char *name = oai_py_lower(st);
    free(s); free(st);
    if (!name || name[0] == '\0') { free(name); return json_bool(false); }
    json_t *a = json_object();
    oai_arg_str(a, "name", name);
    json_t *profile = oai_call(ctx, "get_provider_profile", a);
    if (oai_exc_pending(ctx)) { free(name); if (profile) json_free(profile); return NULL; }
    if (oai_is_none(profile)) { free(name); if (profile) json_free(profile); return json_bool(false); }
    json_t *pn = oai_get(profile, "name");
    bool eq = oai_is_str(pn) && oai_str_eq(pn->str_val, name);
    free(name);
    if (!eq) { json_free(profile); return json_bool(false); }
    json_t *rep = oai_get(profile, "replays_tool_call_signatures");
    json_t *ret = rep ? json_copy(rep) : json_null();
    json_free(profile);
    return ret;
}

/* PoP: oai_wire_reasoning_carriers @ agent/transports/chat_completions.py:_wire_reasoning_carriers */
static json_t *oai_wire_reasoning_carriers(oai_ctx *ctx, const json_t *provider, const json_t *base_url) {
    json_t *a = json_object();
    oai_arg_node(a, "provider", provider);
    oai_arg_node(a, "base_url", base_url);
    json_t *r = oai_call(ctx, "route_reasoning_carriers", a);
    if (oai_exc_pending(ctx)) { if (r) json_free(r); return NULL; }
    if (oai_is_arr(r)) oai_sort_str_array(r);   /* frozenset -> sorted array */
    return r ? r : json_null();
}

/* PoP: oai_route_requires_mistral_tool_ids @ agent/transports/chat_completions.py:_route_requires_mistral_tool_ids */
static json_t *oai_route_requires_mistral_tool_ids(oai_ctx *ctx, const json_t *provider,
                                                   const json_t *model, const json_t *base_url) {
    char *s = oai_str_or_empty(provider);
    char *st = oai_py_strip(s);
    char *pl = oai_py_lower(st);
    bool mistral = oai_str_eq(pl, "mistral");
    free(s); free(st); free(pl);
    if (mistral) return json_bool(true);
    char *bu = oai_str_or_empty(base_url);
    json_t *a = json_object();
    oai_arg_str(a, "base_url", bu);
    oai_arg_str(a, "host", "mistral.ai");
    free(bu);
    json_t *m = oai_call(ctx, "base_url_host_matches", a);
    if (oai_exc_pending(ctx)) { if (m) json_free(m); return NULL; }
    bool mb = oai_truthy(m);
    if (m) json_free(m);
    if (mb) return json_bool(true);
    char *ms = oai_str_or_empty(model);
    char *ml = oai_py_lower(ms);   /* (model or "").lower() — NO strip */
    static const char *MARKERS[] = {
        "mistral", "magistral", "mixtral", "devstral", "codestral", "ministral", "pixtral", "voxtral",
    };
    bool found = false;
    for (size_t i = 0; i < sizeof(MARKERS) / sizeof(MARKERS[0]); i++)
        if (ml && strstr(ml, MARKERS[i])) { found = true; break; }
    free(ms); free(ml);
    return json_bool(found);
}

/* PoP: oai_rename_tool_search_bridge_for_xai @ agent/transports/chat_completions.py:_rename_tool_search_bridge_for_xai */
static json_t *oai_rename_tool_search_bridge_for_xai(oai_ctx *ctx, const json_t *tools) {
    json_t *a = json_object();
    oai_arg_node(a, "tools", tools);
    json_t *reserved = json_array();
    json_append(reserved, json_string("tool_search"));
    json_set(a, "reserved_names", reserved);
    /* name_of / rename lambdas are constructed but never evaluated by the
     * seamed collaborator -> not projected into args (both sides). */
    json_t *r = oai_call(ctx, "_alias_reserved_tools", a);
    if (oai_exc_pending(ctx)) { if (r) json_free(r); return NULL; }
    return r ? r : json_null();   /* live returns the (tools, alias_map) tuple verbatim */
}

/* ── kwargs builders ─────────────────────────────────────────────────────── */

/* one max_tokens candidate: raise-gemini seam -> param-fn seam -> update.
 * Returns false when a raise is pending (exc parked). */
static bool oai_max_tokens_step(oai_ctx *ctx, json_t *api_kwargs, const json_t *model,
                                const json_t *reasoning_config, const json_t *requested) {
    json_t *a1 = json_object();
    oai_arg_node(a1, "model", model);
    oai_arg_node(a1, "reasoning_config", reasoning_config);
    oai_arg_node(a1, "requested", requested);
    json_t *raised = oai_call(ctx, "_raise_gemini_thinking_max_tokens", a1);
    if (oai_exc_pending(ctx)) { if (raised) json_free(raised); return false; }
    json_t *a2 = json_object();
    oai_arg_node(a2, "value", raised);
    json_t *upd = oai_call(ctx, "max_tokens_param_fn", a2);
    if (raised) json_free(raised);
    if (oai_exc_pending(ctx)) { if (upd) json_free(upd); return false; }
    oai_obj_update(api_kwargs, upd);
    if (upd) json_free(upd);
    return true;
}

/* PoP: oai_apply_max_tokens @ agent/transports/chat_completions.py:_apply_max_tokens */
static void oai_apply_max_tokens(oai_ctx *ctx, json_t *api_kwargs, const json_t *model,
                                 const json_t *reasoning_config, json_t *params,
                                 const json_t *profile_max) {
    bool fn = oai_truthy(oai_get(params, "max_tokens_param_fn"));
    const json_t *cands[2] = {
        oai_get(params, "ephemeral_max_output_tokens"),
        oai_get(params, "max_tokens"),
    };
    for (int i = 0; i < 2; i++) {
        if (!oai_is_none(cands[i]) && fn) {
            oai_max_tokens_step(ctx, api_kwargs, model, reasoning_config, cands[i]);
            return;
        }
    }
    if (oai_truthy(profile_max) && fn)
        oai_max_tokens_step(ctx, api_kwargs, model, reasoning_config, profile_max);
}

/* PoP: oai_base_kwargs @ agent/transports/chat_completions.py:_base_kwargs */
static json_t *oai_base_kwargs(oai_ctx *ctx, const json_t *model, const json_t *sanitized,
                               const json_t *tools, json_t *params, const json_t *profile) {
    json_t *kw = json_object();
    json_set(kw, "model", model ? json_copy(model) : json_null());
    json_set(kw, "messages", sanitized ? json_copy(sanitized) : json_null());
    if (!oai_is_none(profile)) {
        /* profile.fixed_temperature — direct attribute read on the double */
        json_t *ft = oai_getattr_def(profile, "fixed_temperature");
        if (!ft) {
            oai_raise_msg(ctx, "AttributeError",
                          "'types.SimpleNamespace' object has no attribute 'fixed_temperature'");
            json_free(kw);
            return NULL;
        }
        if (oai_is_str(ft) && oai_str_eq(ft->str_val, "__OMIT_TEMPERATURE__")) {
            /* profile.fixed_temperature is OMIT_TEMPERATURE -> pass */
        } else if (!oai_is_none(ft)) {
            json_set(kw, "temperature", json_copy(ft));
        } else {
            json_t *pt = oai_get(params, "temperature");
            if (!oai_is_none(pt)) json_set(kw, "temperature", json_copy(pt));
        }
    }
    json_t *to = oai_get(params, "timeout");
    if (!oai_is_none(to)) json_set(kw, "timeout", json_copy(to));
    if (oai_truthy(tools)) {
        json_t *a = json_object();
        oai_arg_node(a, "model", model);
        json_t *moon = oai_call(ctx, "is_moonshot_model", a);
        if (oai_exc_pending(ctx)) { if (moon) json_free(moon); json_free(kw); return NULL; }
        bool m = oai_truthy(moon);
        if (moon) json_free(moon);
        if (m) {
            json_t *a2 = json_object();
            oai_arg_node(a2, "tools", tools);
            json_t *san = oai_call(ctx, "sanitize_moonshot_tools", a2);
            if (oai_exc_pending(ctx)) { if (san) json_free(san); json_free(kw); return NULL; }
            json_set(kw, "tools", san ? san : json_null());
        } else {
            json_set(kw, "tools", json_copy(tools));
        }
    }
    return kw;
}

/* PoP: oai_finish_kwargs @ agent/transports/chat_completions.py:_finish_kwargs */
static json_t *oai_finish_kwargs(oai_ctx *ctx, json_t *api_kwargs, const json_t *sanitized,
                                 json_t *params, const json_t *supports) {
    json_t *a = json_object();
    oai_arg_node(a, "api_kwargs", api_kwargs);
    oai_arg_node(a, "messages", sanitized);
    oai_arg_node(a, "tools", oai_get(api_kwargs, "tools"));
    json_set(a, "supports_prompt_cache_key", supports ? json_copy(supports) : json_bool(false));
    oai_arg_node(a, "session_id", oai_get(params, "session_id"));
    oai_arg_node(a, "cache_scope_id", oai_get(params, "cache_scope_id"));
    json_t *r = oai_call(ctx, "_add_prompt_cache_key", a);
    if (oai_exc_pending(ctx)) { if (r) json_free(r); return NULL; }
    /* void sibling: a {"__update__":{...}} script result models its api_kwargs
     * mutation; any other return is ignored (live returns None). */
    if (oai_is_obj(r)) {
        json_t *upd = json_obj_get(r, "__update__");
        if (upd) oai_obj_update(api_kwargs, upd);
    }
    if (r) json_free(r);
    return json_copy(api_kwargs);
}

/* ── _sanitize_message (the message sanitizer flow) ──────────────────────── */

/* PoP: oai_sanitize_message @ agent/transports/chat_completions.py:_sanitize_message */
static json_t *oai_sanitize_message(oai_ctx *ctx, const json_t *msg, bool strip_extra_content,
                                    const json_t *strip_reasoning, const json_t *native_type,
                                    bool mistral_tool_ids) {
    if (!oai_is_obj(msg)) return json_null();
    /* _STRIP_MSG_KEYS (MESSAGE_UID="message_uid", DATA_USE="data_use") */
    static const char *STRIP_MSG[] = {
        "codex_reasoning_items", "codex_message_items", "tool_name", "effect_disposition",
        "timestamp", "platform_message_id", "message_id", "api_content",
        "anthropic_content_blocks", "bedrock_content_blocks", "message_uid", "data_use",
    };
    static const char NATIVE_SUF[] = ".native_assistant";
    const size_t NATIVE_SUF_LEN = sizeof(NATIVE_SUF) - 1;
    json_t *strip_keys = json_array();
    json_t *out = json_object();
    json_t *kept_details = NULL;
    json_t *copied = NULL;
    json_t *keys = NULL;
    json_t *mapped = NULL;
    json_t *rk = NULL;
    bool remapped = false;
    bool had_copied = false;

    for (size_t i = 0; i < msg->c.count; i++) {
        const char *k = msg->c.keys[i];
        bool hit = (k[0] == '_');
        for (size_t j = 0; j < sizeof(STRIP_MSG) / sizeof(STRIP_MSG[0]); j++)
            if (oai_str_eq(k, STRIP_MSG[j])) { hit = true; break; }
        if (hit) oai_strarr_add(strip_keys, k);
    }
    const char *role = oai_jstr(oai_get(msg, "role"));
    bool assistant = oai_str_eq(role, "assistant");
    bool tool = oai_str_eq(role, "tool");
    if (assistant && oai_is_arr(strip_reasoning)) {
        for (size_t i = 0; i < strip_reasoning->c.count; i++) {
            const char *k = oai_jstr(strip_reasoning->c.items[i]);
            if (json_has(msg, k)) oai_strarr_add(strip_keys, k);
        }
    }
    json_t *rd = oai_get(msg, "reasoning_details");
    if (!oai_strarr_has(strip_keys, "reasoning_details") && oai_is_arr(rd)) {
        json_t *kept = json_array();
        for (size_t i = 0; i < rd->c.count; i++) {
            json_t *d = rd->c.items[i];
            bool drop = false;
            if (oai_is_obj(d)) {
                json_t *ty = oai_get(d, "type");
                if (oai_is_str(ty)) {
                    const char *t = ty->str_val;
                    size_t tl = strlen(t);
                    bool native_suf = tl >= NATIVE_SUF_LEN
                        && oai_str_eq(t + tl - NATIVE_SUF_LEN, NATIVE_SUF);
                    bool neq = oai_is_none(native_type) || !oai_str_eq(t, oai_jstr(native_type));
                    drop = native_suf && neq;
                }
            }
            if (!drop) json_append(kept, json_copy(d));
        }
        if (kept->c.count != rd->c.count) {
            oai_strarr_add(strip_keys, "reasoning_details");
            kept_details = kept;
        } else {
            json_free(kept);
        }
    }
    if (tool && json_has(msg, "name")) oai_strarr_add(strip_keys, "name");
    for (size_t i = 0; i < msg->c.count; i++) {
        if (!oai_strarr_has(strip_keys, msg->c.keys[i]))
            json_set(out, msg->c.keys[i], json_copy(msg->c.items[i]));
    }
    if (mistral_tool_ids && tool) {
        json_t *tcarg = json_object();
        json_t *tcid = oai_get(msg, "tool_call_id");
        json_set(tcarg, "id", tcid ? json_copy(tcid) : json_null());
        json_t *a = json_object();
        json_set(a, "tc", tcarg);
        rk = oai_call(ctx, "coalesce_tool_call_id", a);
        if (oai_exc_pending(ctx)) goto fail;
        if (oai_truthy(rk)) {
            mapped = oai_mistral_tool_id(ctx, rk);
            if (oai_exc_pending(ctx)) goto fail;
            json_t *orig = json_obj_get(msg, "tool_call_id");
            if (!orig) {
                /* msg["tool_call_id"] direct subscript */
                oai_raise_msg(ctx, "KeyError", "'tool_call_id'");
                goto fail;
            }
            bool neq = !(oai_is_str(orig) && oai_str_eq(oai_jstr(mapped), orig->str_val));
            if (neq) {
                json_set(out, "tool_call_id", mapped);   /* ownership transferred */
                mapped = NULL;
                remapped = true;
            }
        }
        if (rk) { json_free(rk); rk = NULL; }
    }
    if (kept_details) {
        if (kept_details->c.count > 0) {
            json_set(out, "reasoning_details", kept_details);   /* transferred */
        } else {
            json_free(kept_details);
        }
        kept_details = NULL;
    }
    json_t *tool_calls = oai_get(msg, "tool_calls");
    if (assistant && json_has(msg, "tool_calls")
        && (oai_is_none(tool_calls) || (oai_is_arr(tool_calls) && tool_calls->c.count == 0))) {
        json_obj_del(out, "tool_calls");
        oai_strarr_add(strip_keys, "tool_calls");
    } else if (oai_is_arr(tool_calls)) {
        for (size_t idx = 0; idx < tool_calls->c.count; idx++) {
            json_t *tc = tool_calls->c.items[idx];
            if (!oai_is_obj(tc)) continue;
            keys = json_array();
            if (json_has(tc, "call_id")) oai_strarr_add(keys, "call_id");
            if (json_has(tc, "response_item_id")) oai_strarr_add(keys, "response_item_id");
            if (json_has(tc, "extra_content")
                && (strip_extra_content
                    || !oai_has_replayable_thought_signature(oai_get(tc, "extra_content"))))
                oai_strarr_add(keys, "extra_content");
            if (mistral_tool_ids) {
                json_t *a = json_object();
                oai_arg_node(a, "tc", tc);
                json_t *key = oai_call(ctx, "coalesce_tool_call_id", a);
                if (oai_exc_pending(ctx)) { if (key) json_free(key); goto fail; }
                if (oai_truthy(key)) {
                    mapped = oai_mistral_tool_id(ctx, key);
                    if (oai_exc_pending(ctx)) { json_free(key); goto fail; }
                }
                if (key) json_free(key);
            }
            json_t *tcid = oai_get(tc, "id");
            bool map_diff = mapped
                && !(oai_is_str(tcid) && oai_str_eq(oai_jstr(mapped), oai_jstr(tcid)));
            if (keys->c.count > 0 || map_diff) {
                if (!copied) {
                    copied = json_array();
                    for (size_t j = 0; j < tool_calls->c.count; j++)
                        json_append(copied, json_copy(tool_calls->c.items[j]));
                }
                json_t *ntc = json_object();
                for (size_t j = 0; j < tc->c.count; j++) {
                    if (!oai_strarr_has(keys, tc->c.keys[j]))
                        json_set(ntc, tc->c.keys[j], json_copy(tc->c.items[j]));
                }
                if (mapped) {
                    json_set(ntc, "id", mapped);   /* transferred */
                    mapped = NULL;
                }
                json_free(copied->c.items[idx]);
                copied->c.items[idx] = ntc;
            } else if (mapped) {
                json_free(mapped);
                mapped = NULL;
            }
            json_free(keys);
            keys = NULL;
        }
        if (copied) {
            json_set(out, "tool_calls", copied);   /* transferred */
            had_copied = true;
            copied = NULL;
        }
    }
    bool any_strip = strip_keys->c.count > 0;
    json_free(strip_keys);
    if (any_strip || remapped || had_copied) return out;
    json_free(out);
    return json_null();
fail:
    if (rk) json_free(rk);
    if (mapped) json_free(mapped);
    if (keys) json_free(keys);
    if (copied) json_free(copied);
    if (kept_details) json_free(kept_details);
    json_free(strip_keys);
    json_free(out);
    return NULL;
}

/* ── ChatCompletionsTransport._normalize_tool_call ───────────────────────── */

/* PoP: oai_normalize_tool_call @ agent/transports/chat_completions.py:_normalize_tool_call */
static json_t *oai_normalize_tool_call(oai_ctx *ctx, const json_t *slf, const json_t *tc) {
    json_t *tc_function = oai_getattr_def(tc, "function");
    json_t *name = oai_getattr_def(tc_function, "name");
    if (oai_is_none(tc_function) || oai_is_none(name)) return json_null();
    json_t *alias_map = oai_get(slf, "_last_wire_aliases");
    const char *nm = oai_jstr(name);
    char *final_name = NULL;
    if (oai_is_none(alias_map)) {
        /* static reverse: only the xAI alias folds back to tool_search */
        final_name = oai_dup_span(oai_str_eq(nm, "hermes_tool_search") ? "tool_search" : nm,
                                  strlen(oai_str_eq(nm, "hermes_tool_search") ? "tool_search" : nm));
    } else {
        json_t *mapped_name = oai_is_obj(alias_map) ? json_obj_get(alias_map, nm) : NULL;
        const char *fn = (mapped_name && oai_is_str(mapped_name)) ? mapped_name->str_val : nm;
        final_name = oai_dup_span(fn, strlen(fn));
    }
    json_t *arguments = oai_getattr_def(tc_function, "arguments");
    json_t *extra = oai_attr_or_model_extra(tc, "extra_content");
    json_t *fn_sig = oai_attr_or_model_extra(tc_function, "thought_signature");
    json_t *pd = json_object();
    if (!oai_is_none(extra)) {
        /* plain-value extra_content: no model_dump method on the wire double */
        json_t *dumped = oai_dump_extra_content(ctx, extra, false);
        if (oai_exc_pending(ctx)) {
            if (dumped) json_free(dumped);
            free(final_name); json_free(extra); json_free(fn_sig); json_free(pd);
            return NULL;
        }
        json_set(pd, "extra_content", dumped);
    }
    if (!oai_is_none(fn_sig)) json_set(pd, "thought_signature", json_copy(fn_sig));
    json_t *call = json_object();
    json_t *id = oai_getattr_def(tc, "id");
    json_set(call, "id", id ? json_copy(id) : json_null());
    json_set(call, "name", json_string(final_name ? final_name : ""));
    json_set(call, "arguments", oai_is_none(arguments) ? json_string("{}") : json_copy(arguments));
    if (pd->c.count > 0) {
        json_set(call, "provider_data", pd);
    } else {
        json_free(pd);
        json_set(call, "provider_data", json_null());
    }
    if (oai_is_true(oai_getattr_def(tc_function, "args_repaired")))
        json_set(call, "args_repaired", json_bool(true));
    free(final_name);
    json_free(extra);
    json_free(fn_sig);
    return call;
}
