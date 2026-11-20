/* bedrock_transport_flow2.c — FLOW port of the REMAINING UNCLAIMED defs of
 * agent/bedrock_adapter.py (the Bedrock Converse transport data path). The
 * task names agent/transports/bedrock.py; that module is a thin delegation
 * shell whose every method is already CLAIMED by transports_bedrock_ports.c,
 * and bedrock_transport_flow.c (bt2_*) already claimed 27 adapter defs. This
 * file ports the 12 STILL-UNCLAIMED defs (grep PoP, CRLF-aware, dotted class
 * methods included):
 *   _new_client, _cached_client, BedrockOpenAISigV4Auth.__init__,
 *   BedrockOpenAISigV4Auth.auth_flow, build_bedrock_openai_http_client,
 *   _stale_error_types, _boto3_chain_has_credentials, bind_bedrock_runtime,
 *   _ResponseParts.__init__, _ResponseParts.absorb_reasoning,
 *   _ResponseParts.build, bedrock_static_context_length.
 * PoP lines credit the REAL defining module agent/bedrock_adapter.py.
 *
 * Claimed siblings are SEAMED (resolve_bedrock_runtime_region,
 * bedrock_region_from_runtime_url, bedrock_guardrail_config, _require_boto3,
 * scoped_aws_session_kwargs, _encode_redacted, _persisted_reasoning_details).
 * Ported siblings compose REAL: _cached_client -> _new_client;
 * build_bedrock_openai_http_client -> BedrockOpenAISigV4Auth.__init__.
 *
 * ALL collaborator I/O (claimed siblings, foreign-module imports, boto3 /
 * botocore / httpx constructors and methods, the on_text callback) rides the
 * injected universal seam bt3_call; the harness scripts results and records
 * the ordered trace the oracle diffs against LIVE agent/bedrock_adapter.py
 * under the same scripts.
 *
 * Seam name table (name -> live call):
 *   get_hermes_home_override        hermes_constants (args {})
 *   hermes_home_key                 hermes_constants (args {})
 *   _require_boto3                  CLAIMED sibling (args {}); null -> None,
 *                                   else a boto3 double (.Session / .client)
 *   scoped_aws_session_kwargs       CLAIMED sibling (args {}); result MUST be
 *                                   a dict (live contract; splatted as
 *                                   Session(**kwargs))
 *   boto3_Session                   boto3.Session(**kwargs) (args = kwargs);
 *                                   null -> None, else a session double
 *                                   (.client / .get_credentials)
 *   botocore_Config                 botocore.config.Config(**kwargs) (args =
 *                                   kwargs); null -> None config carrier,
 *                                   else a config double (.merge)
 *   config_merge                    config.merge(other) (args {other}); the
 *                                   receiver carrier is checked BEFORE the
 *                                   argument Config ctor fires
 *   factory_client                  factory.client(service, region_name=,
 *                                   config=) (args {service, region_name,
 *                                   config}); result is the client value
 *   resolve_bedrock_runtime_region  CLAIMED sibling (args {})
 *   httpx_Client                    httpx.Client(**kwargs) (args = kwargs;
 *                                   auth projects to {region, service})
 *   session_get_credentials         session.get_credentials() (args {});
 *                                   null -> None, else a credentials double
 *   credentials_get_frozen_credentials  creds.get_frozen_credentials() ({})
 *   SigV4Auth_ctor                  botocore.auth.SigV4Auth(frozen, service,
 *                                   region) (args {credentials, service,
 *                                   region}); null -> None signer
 *   signer_add_auth                 signer.add_auth(aws_request) (args
 *                                   {aws_request}); null -> headers unchanged,
 *                                   dict -> aws_request.headers.update(dict)
 *   AWSRequest_ctor                 botocore.awsrequest.AWSRequest(method=,
 *                                   url=, data=, headers=) (args {method, url,
 *                                   data, headers}); null -> None carrier,
 *                                   dict -> headers seeded from result
 *                                   ["headers"] when a dict else {}
 *   import_module                   importlib.import_module(module) (args
 *                                   {module}); dict -> attribute double,
 *                                   null/other -> getattr AttributeError
 *   botocore_get_session            botocore.session.get_session() (args {});
 *                                   null -> None, else a session double
 *   session_set_config_variable     session.set_config_variable(name, value)
 *                                   (args {name, value})
 *   bedrock_region_from_runtime_url CLAIMED sibling (args {base_url})
 *   bedrock_guardrail_config        CLAIMED sibling (args {})
 *   build_anthropic_bedrock_client  agent.anthropic_adapter (args {region,
 *                                   timeout})
 *   get_provider_request_timeout    hermes_cli.timeouts (args {provider,
 *                                   model})
 *   on_text                         absorb_reasoning callback (args {text})
 *   _encode_redacted                CLAIMED sibling (args {redacted})
 *   _persisted_reasoning_details    CLAIMED sibling (args {ordered_blocks})
 *   _longest_key_match              agent.model_metadata (args {key}; the
 *                                   DEFAULT_CONTEXT_LENGTHS table arg is a
 *                                   constant and is NOT recorded)
 *
 * Representation contract (shared with tests/oracle_bedrock_transport_flow2.py):
 *   - scripts: MISSING key -> fail-closed RuntimeError("no script for <name>");
 *     null -> None; {"$seq":[...]} per-call queue (exhaustion raises
 *     RuntimeError("script exhausted: <name>")); {"__raise__":cls,"msg":m}
 *     raises (trace ret is the WHOLE marker); any other value reused (a bare
 *     ARRAY is a LIST RESULT, not a queue). None-valued seam arg keys dropped
 *     on BOTH sides.
 *   - tuple -> array; None -> JSON null; integral floats collapse to ints on
 *     both sides; bytes/bytearray -> {"__bytes__":[int,...]}; SimpleNamespace
 *     and ported-class instances project to dicts; uncaught exception ->
 *     {"__raise__":cls,"msg":str(e)}.
 *   - except Exception / suppress(Exception) sites (_boto3_chain_has_credentials,
 *     _stale_error_types' suppress(ImportError, AttributeError)) swallow the
 *     matching family; the BaseException trio (SystemExit/KeyboardInterrupt/
 *     GeneratorExit) passes through. Full builtin exception-parent table below.
 *   - cache keys: _cached_client's (region, read_timeout) tuple key and
 *     _bedrock_clients_by_home's (home, service, region, read_timeout) tuple key
 *     are encoded as "|"-joined str(component) strings (str(None)=="None");
 *     the oracle decodes them back to tuples for the live call and re-encodes
 *     for the echo. Domain: region/service/home strings without "|",
 *     read_timeout int or null.
 *   - mutation windows echoed POST-CALL (on RAISE too): _cached_client
 *     {"cache","_bedrock_clients_by_home"}; bind_bedrock_runtime {"agent"};
 *     absorb_reasoning {"block","parts"}; auth_flow {"request"}. Module state
 *     (_inference_profile_model_cache, _bedrock_clients_by_home) rides
 *     ctx->state, seeded from in.state and echoed for the keys PRESENT in the
 *     fixture state when no explicit window applies.
 *
 * Fixture domain (ASCII scope):
 *   - strings ASCII (str.strip/lower transcribed byte-wise; strip set is
 *     White_Space incl \x1c-\x1f). Non-str scalar raise positions use
 *     int/bool/str/list/null only (no floats — JSON numbers lose int/float
 *     identity for the type name).
 *   - read_timeout / usage_data values / timeout: int or null (numeric);
 *     integral floats collapse to ints.
 *   - scoped_aws_session_kwargs result: a dict (splatted; non-mapping ** is
 *     outside the live contract and not driven).
 *   - request: {method, url, content, headers}; headers dict or null (null ->
 *     AttributeError 'items'); content bytes marker / null / str; url str.
 *   - AWSRequest_ctor result: null or a dict (headers seeded from ["headers"]
 *     when a dict else {}); signer_add_auth result: null or a headers dict.
 *   - import_module result: dict (attribute double) or null/other (getattr
 *     AttributeError, suppressed).
 *   - agent / parts / block / cache: dicts (attribute doubles live); agent null
 *     -> setattr AttributeError after the region seam fires.
 *   - bedrock_static_context_length model_id: str (non-str -> regex TypeError);
 *     _inference_profile_model_cache values str; _longest_key_match result null
 *     or a 2-array (hit[1]; <2 -> IndexError "tuple index out of range").
 *
 * libjson-only (default oracle deps suffice). Self-contained: the harness
 * #includes "agent/transports/bedrock_transport_flow2.c".
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

/* ── context + seam plumbing ─────────────────────────────────────────────── */

typedef struct bt3_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
    json_t *state;     /* owned: module-state carrier (seeded from in.state) */
} bt3_ctx;

static bool bt3_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool bt3_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool bt3_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool bt3_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool bt3_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool bt3_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool bt3_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static bool bt3_truthy(const json_t *v) {
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
static const char *bt3_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static json_t *bt3_get(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}

/* ── string helpers (caller frees) ───────────────────────────────────────── */

static char *bt3_dup_span(const char *s, size_t len) {
    char *r = (char *)malloc(len + 1);
    if (!r) return NULL;
    memcpy(r, s, len);
    r[len] = '\0';
    return r;
}
static bool bt3_is_ws(char c) {
    unsigned char u = (unsigned char)c;
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r'
        || (u >= 0x1c && u <= 0x1f);
}
static char *bt3_py_strip(const char *s) {
    if (!s) return bt3_dup_span("", 0);
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && bt3_is_ws(s[a])) a++;
    while (b > a && bt3_is_ws(s[b - 1])) b--;
    return bt3_dup_span(s + a, b - a);
}
static char *bt3_py_lower(const char *s) {
    char *r = bt3_dup_span(s ? s : "", strlen(s ? s : ""));
    if (!r) return NULL;
    for (char *p = r; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    return r;
}
/* str(v) for a scalar (null -> "None"; no-float domain: numbers read as int). */
static char *bt3_pystr(const json_t *v) {
    if (bt3_is_none(v)) return bt3_dup_span("None", 4);
    if (bt3_is_bool(v)) return bt3_dup_span(v->bool_val ? "True" : "False", v->bool_val ? 4 : 5);
    if (bt3_is_num(v)) {
        char buf[64]; double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
        else snprintf(buf, sizeof buf, "%g", d);
        return bt3_dup_span(buf, strlen(buf));
    }
    if (bt3_is_str(v)) return bt3_dup_span(bt3_jstr(v), strlen(bt3_jstr(v)));
    return bt3_dup_span("", 0);
}
static const char *bt3_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return "int";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}
static bool bt3_str_has(const char *hay, const char *needle) {
    return strstr(hay ? hay : "", needle) != NULL;
}
/* "\n".join / "\n\n".join over an array of str nodes (caller frees). */
static char *bt3_join(const json_t *arr, const char *sep) {
    size_t seplen = strlen(sep), total = 0;
    for (size_t i = 0; i < arr->c.count; i++) {
        total += strlen(bt3_jstr(arr->c.items[i]));
        if (i) total += seplen;
    }
    char *r = (char *)malloc(total + 1);
    if (!r) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < arr->c.count; i++) {
        if (i) { memcpy(r + o, sep, seplen); o += seplen; }
        const char *s = bt3_jstr(arr->c.items[i]);
        size_t n = strlen(s);
        memcpy(r + o, s, n); o += n;
    }
    r[o] = '\0';
    return r;
}
/* concat two C strings (caller frees). */
static char *bt3_concat(const char *a, const char *b) {
    size_t na = strlen(a), nb = strlen(b);
    char *r = (char *)malloc(na + nb + 1);
    if (!r) return NULL;
    memcpy(r, a, na); memcpy(r + na, b, nb); r[na + nb] = '\0';
    return r;
}

/* ── exception plumbing (full builtin hierarchy) ─────────────────────────── */

static bool bt3_exc_pending(const bt3_ctx *ctx) { return ctx && ctx->exc != NULL; }
static void bt3_exc_clear(bt3_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}
static void bt3_raise_msg(bt3_ctx *ctx, const char *cls, const char *msg) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
    json_set(ctx->exc, "msg", json_string(msg ? msg : ""));
}
static void bt3_raise_attr(bt3_ctx *ctx, const json_t *recv, const char *attr) {
    char msg[256];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", bt3_typename(recv), attr);
    bt3_raise_msg(ctx, "AttributeError", msg);
}
/* py3.14 setattr-on-None form (trailing clause). */
static void bt3_raise_setattr(bt3_ctx *ctx, const json_t *recv, const char *attr) {
    char msg[256];
    snprintf(msg, sizeof msg,
             "'%s' object has no attribute '%s' and no __dict__ for setting new attributes",
             bt3_typename(recv), attr);
    bt3_raise_msg(ctx, "AttributeError", msg);
}
static void bt3_raise_not_iterable(bt3_ctx *ctx, const json_t *v) {
    char msg[256];
    snprintf(msg, sizeof msg, "'%s' object is not iterable", bt3_typename(v));
    bt3_raise_msg(ctx, "TypeError", msg);
}
static void bt3_raise_re_type(bt3_ctx *ctx, const json_t *v) {
    char msg[256];
    snprintf(msg, sizeof msg, "expected string or bytes-like object, got '%s'", bt3_typename(v));
    bt3_raise_msg(ctx, "TypeError", msg);
}
static void bt3_raise_plus_type(bt3_ctx *ctx, const json_t *left) {
    char msg[256];
    snprintf(msg, sizeof msg, "unsupported operand type(s) for +: '%s' and 'str'", bt3_typename(left));
    bt3_raise_msg(ctx, "TypeError", msg);
}
static const char *bt3_exc_class(const bt3_ctx *ctx) {
    return json_get_str(ctx->exc, "__raise__", "");
}
/* FULL builtin exception subclass chain (parent lookup). */
static const char *bt3_exc_parent(const char *c) {
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
        if (bt3_str_eq(T[i].c, c)) return T[i].p;
    return NULL;
}
static bool bt3_exc_matches(const bt3_ctx *ctx, const char *const *classes, size_t n) {
    if (!bt3_exc_pending(ctx)) return false;
    const char *cls = bt3_exc_class(ctx);
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (bt3_str_eq(cls, classes[i])) return true;
        const char *p = bt3_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}
static bool bt3_exc_is_exception(const bt3_ctx *ctx) {
    static const char *E[] = {"Exception"};
    return bt3_exc_matches(ctx, E, 1);
}

/* ── universal seam (fail-closed) ────────────────────────────────────────── */

/* args ownership is TRANSFERRED (NULL -> empty object). Result is NEW. */
static json_t *bt3_call(bt3_ctx *ctx, const char *name, json_t *args) {
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
    json_set(ev, "args", args);
    if (raises) json_set(ev, "ret", json_copy(ctx->exc));
    else json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(ctx->trace, ev);
    if (fc_marker) json_free(fc_marker);
    return ret;
}

/* Arg builders: None-valued keys dropped on BOTH sides ("" kept). Borrowed val
 * is copied; the caller keeps ownership of the source node. */
static void bt3_arg_node(json_t *args, const char *key, const json_t *val) {
    if (!bt3_is_none(val)) json_set(args, key, json_copy(val));
}
/* Drop None-valued keys from a borrowed dict into a NEW object (for ** splat). */
static json_t *bt3_clean_kwargs(const json_t *kw) {
    json_t *r = json_object();
    if (bt3_is_obj(kw)) {
        for (size_t i = 0; i < kw->c.count; i++) {
            json_t *v = kw->c.items[i];
            if (!bt3_is_none(v)) json_set(r, kw->c.keys[i], json_copy(v));
        }
    }
    return r;
}

/* ── cache-key encoding ("|"-joined str(component); str(None)=="None") ────── */

static char *bt3_key2(const json_t *region, const json_t *rt) {
    char *a = bt3_pystr(region), *b = bt3_pystr(rt);
    char *r = bt3_concat(a ? a : "", "|");
    char *r2 = bt3_concat(r ? r : "", b ? b : "");
    free(a); free(b); free(r);
    return r2;
}
static char *bt3_key4(const json_t *home, const json_t *service, const json_t *region, const json_t *rt) {
    char *h = bt3_pystr(home), *s = bt3_pystr(service), *g = bt3_pystr(region), *t = bt3_pystr(rt);
    char *p1 = bt3_concat(h ? h : "", "|");
    char *p2 = bt3_concat(p1 ? p1 : "", s ? s : "");
    char *p3 = bt3_concat(p2 ? p2 : "", "|");
    char *p4 = bt3_concat(p3 ? p3 : "", g ? g : "");
    char *p5 = bt3_concat(p4 ? p4 : "", "|");
    char *p6 = bt3_concat(p5 ? p5 : "", t ? t : "");
    free(h); free(s); free(g); free(t);
    free(p1); free(p2); free(p3); free(p4); free(p5);
    return p6;
}

/* ── regex transcriptions (verbatim live patterns) ───────────────────────── */

/* _APPLICATION_PROFILE_ARN_RE = re.compile(r":application-inference-profile/") */
static bool bt3_app_profile_arn(const char *s) {
    return bt3_str_has(s, ":application-inference-profile/");
}
/* _CATALOG_FAMILY_ID_RE =
 *   re.compile(r"(?:^|[./])(?:anthropic|openai|xai)\.([a-z0-9][a-z0-9.:-]*)$")
 * Applied to the LOWERCASED id. Returns group(1) (caller frees) or NULL.
 * Leftmost match: scan start positions p; at p the prefix is ^ (p==0, zero
 * width) or a consumed [./]; then an issuer literal + "."; then a capture that
 * starts [a-z0-9] and runs of [a-z0-9.:-] to end-of-string ($). */
static bool bt3_fam_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
        || c == '.' || c == ':' || c == '-';
}
static bool bt3_fam_first(char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}
static char *bt3_catalog_family(const char *low) {
    if (!low) return NULL;
    size_t n = strlen(low);
    static const char *issuers[] = {"anthropic", "openai", "xai"};
    for (size_t p = 0; p < n; p++) {
        /* two prefix branches: ^ (only at p==0, offset 0) then [./] (offset 1) */
        for (int branch = 0; branch < 2; branch++) {
            size_t iss;
            if (branch == 0) {
                if (p != 0) continue;
                iss = 0;
            } else {
                if (low[p] != '.' && low[p] != '/') continue;
                iss = p + 1;
            }
            for (int k = 0; k < 3; k++) {
                size_t il = strlen(issuers[k]);
                if (iss + il >= n) continue;
                if (strncmp(low + iss, issuers[k], il) != 0) continue;
                if (low[iss + il] != '.') continue;
                size_t cap = iss + il + 1;
                if (cap >= n || !bt3_fam_first(low[cap])) continue;
                bool ok = true;
                for (size_t i = cap; i < n; i++)
                    if (!bt3_fam_char(low[i])) { ok = false; break; }
                if (ok) return bt3_dup_span(low + cap, n - cap);
            }
        }
    }
    return NULL;
}

/* ── module constants (verbatim from the live module) ────────────────────── */

#define BT3_BOTO_CONNECT_TIMEOUT_SECONDS 60

static const char *BT3_NO_CREDS_MSG =
    "No AWS credentials available for Bedrock OpenAI Responses. "
    "Configure AWS_ACCESS_KEY_ID/AWS_SECRET_ACCESS_KEY, AWS_PROFILE, SSO, or an instance/task role.";

/* _STOP_REASON_TO_FINISH_REASON (unknown -> "stop"). */
static const char *bt3_stop_map(const char *stop_reason) {
    static const struct { const char *k, *v; } T[] = {
        {"end_turn", "stop"}, {"stop_sequence", "stop"}, {"tool_use", "tool_calls"},
        {"max_tokens", "length"}, {"content_filtered", "content_filter"},
        {"guardrail_intervened", "content_filter"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (bt3_str_eq(T[i].k, stop_reason)) return T[i].v;
    return "stop";
}

/* BEDROCK_CONTEXT_LENGTHS (insertion order preserved; max(matches, key=len)
 * keeps the FIRST longest on ties). */
static const struct { const char *k; int v; } BT3_CONTEXT_LENGTHS[] = {
    {"xai.grok-4.6", 500000},
    {"anthropic.claude-fable-5", 1000000}, {"anthropic.claude-fable", 1000000},
    {"anthropic.claude-sonnet-5", 1000000}, {"anthropic.claude-opus-5", 1000000},
    {"anthropic.claude-opus-4-8", 1000000}, {"anthropic.claude-opus-4-7", 1000000},
    {"anthropic.claude-mythos", 1000000}, {"anthropic.claude-opus-4-6", 1000000},
    {"anthropic.claude-sonnet-4-6", 1000000},
    {"anthropic.claude-sonnet-4-5", 200000}, {"anthropic.claude-haiku-4-5", 200000},
    {"anthropic.claude-opus-4", 200000}, {"anthropic.claude-sonnet-4", 200000},
    {"anthropic.claude-3-5-sonnet", 200000}, {"anthropic.claude-3-5-haiku", 200000},
    {"anthropic.claude-3-opus", 200000}, {"anthropic.claude-3-sonnet", 200000},
    {"anthropic.claude-3-haiku", 200000},
    {"amazon.nova-pro", 300000}, {"amazon.nova-lite", 300000}, {"amazon.nova-micro", 128000},
    {"meta.llama4-maverick", 128000}, {"meta.llama4-scout", 128000},
    {"meta.llama3-3-70b-instruct", 128000}, {"mistral.mistral-large", 128000},
    {"deepseek.v3", 128000},
    {"amazon.nova-2-lite", 1000000}, {"ai21.jamba-1-5", 256000},
    {"google.gemma-4-26b", 256000}, {"google.gemma-4-31b", 256000},
    {"minimax.minimax-m2.1", 196000}, {"minimax.minimax-m2.5", 196000},
    {"mistral.devstral-2", 256000}, {"mistral.mistral-large-3", 256000},
    {"moonshotai.kimi-k2.5", 256000}, {"moonshot.kimi-k2-thinking", 256000},
    {"moonshotai.kimi-k2-thinking", 256000},
    {"moonshotai.kimi-k3", 1000000}, {"qwen.qwen3-coder", 256000},
    {"zai.glm-4.7", 203000}, {"zai.glm-5", 200000}, {"zai.glm-5.3", 1000000},
    {"openai.gpt-6.1-sol", 1000000},
    {"openai.gpt-6-astra", 1050000}, {"openai.gpt-6.1-terra", 1050000},
    {"openai.gpt-6-sol", 1050000}, {"openai.gpt-6-luna", 1050000},
    {"openai.gpt-5.5", 1050000}, {"openai.gpt-5.6-sol", 1050000},
    {"openai.gpt-5.6-terra", 1050000}, {"openai.gpt-5.6-luna", 1050000},
    {"openai.gpt-5.4", 1050000},
};

/* ── client construction flows ───────────────────────────────────────────── */

/* PoP: bt3_new_client @ agent/bedrock_adapter.py:_new_client */
static json_t *bt3_new_client(bt3_ctx *ctx, const json_t *factory, const json_t *service,
                              const json_t *region, const json_t *read_timeout) {
    /* config = Config(retries={"total_max_attempts":1}) if service=="bedrock-runtime" else Config() */
    json_t *a = json_object();
    if (bt3_is_str(service) && bt3_str_eq(bt3_jstr(service), "bedrock-runtime")) {
        json_t *retries = json_object();
        json_set(retries, "total_max_attempts", json_number(1));
        json_set(a, "retries", retries);
    }
    json_t *cfg = bt3_call(ctx, "botocore_Config", a);
    if (bt3_exc_pending(ctx)) { if (cfg) json_free(cfg); return NULL; }
    /* if read_timeout is not None: config = config.merge(Config(read_timeout=..., connect_timeout=60)) */
    if (!bt3_is_none(read_timeout)) {
        /* receiver carrier check BEFORE the argument Config ctor fires */
        if (bt3_is_none(cfg)) { bt3_raise_attr(ctx, cfg, "merge"); if (cfg) json_free(cfg); return NULL; }
        json_t *a2 = json_object();
        bt3_arg_node(a2, "read_timeout", read_timeout);
        json_set(a2, "connect_timeout", json_number(BT3_BOTO_CONNECT_TIMEOUT_SECONDS));
        json_t *other = bt3_call(ctx, "botocore_Config", a2);
        if (bt3_exc_pending(ctx)) { if (cfg) json_free(cfg); if (other) json_free(other); return NULL; }
        json_t *a3 = json_object();
        bt3_arg_node(a3, "other", other);
        json_t *merged = bt3_call(ctx, "config_merge", a3);
        if (other) json_free(other);
        if (bt3_exc_pending(ctx)) { if (cfg) json_free(cfg); if (merged) json_free(merged); return NULL; }
        if (cfg) json_free(cfg);
        cfg = merged;
    }
    /* return factory.client(service, region_name=region, config=config) */
    if (bt3_is_none(factory)) { bt3_raise_attr(ctx, factory, "client"); if (cfg) json_free(cfg); return NULL; }
    json_t *a4 = json_object();
    bt3_arg_node(a4, "service", service);
    bt3_arg_node(a4, "region_name", region);
    bt3_arg_node(a4, "config", cfg);
    json_t *client = bt3_call(ctx, "factory_client", a4);
    if (cfg) json_free(cfg);
    if (bt3_exc_pending(ctx)) { if (client) json_free(client); return NULL; }
    return client;
}

/* PoP: bt3_cached_client @ agent/bedrock_adapter.py:_cached_client */
static json_t *bt3_cached_client(bt3_ctx *ctx, json_t *cache, const json_t *service,
                                 const json_t *region, const json_t *read_timeout) {
    json_t *ov = bt3_call(ctx, "get_hermes_home_override", json_object());
    if (bt3_exc_pending(ctx)) { if (ov) json_free(ov); return NULL; }
    if (bt3_is_none(ov)) {
        if (ov) json_free(ov);
        char *key = bt3_key2(region, read_timeout);
        json_t *existing = bt3_get(cache, key ? key : "");
        if (!existing) {
            /* cache[key] = _new_client(_require_boto3(), service, region, read_timeout) */
            json_t *boto = bt3_call(ctx, "_require_boto3", json_object());
            if (bt3_exc_pending(ctx)) { free(key); if (boto) json_free(boto); return NULL; }
            json_t *client = bt3_new_client(ctx, boto, service, region, read_timeout);
            if (boto) json_free(boto);
            if (bt3_exc_pending(ctx)) { free(key); if (client) json_free(client); return NULL; }
            json_set(cache, key ? key : "", client ? json_copy(client) : json_null());
            free(key);
            return client;
        }
        free(key);
        return json_copy(existing);
    }
    if (ov) json_free(ov);
    /* scoped: home_key = (hermes_home_key(), service, region, read_timeout) */
    json_t *hk = bt3_call(ctx, "hermes_home_key", json_object());
    if (bt3_exc_pending(ctx)) { if (hk) json_free(hk); return NULL; }
    char *key4 = bt3_key4(hk, service, region, read_timeout);
    if (hk) json_free(hk);
    json_t *home = bt3_get(ctx->state, "_bedrock_clients_by_home");
    json_t *existing = bt3_get(home, key4 ? key4 : "");
    if (!bt3_is_none(existing)) { free(key4); return json_copy(existing); }
    /* kwargs = scoped_aws_session_kwargs() */
    json_t *kwargs = bt3_call(ctx, "scoped_aws_session_kwargs", json_object());
    if (bt3_exc_pending(ctx)) { free(key4); if (kwargs) json_free(kwargs); return NULL; }
    /* client = _new_client(_require_boto3().Session(**kwargs), service, region, read_timeout) */
    json_t *boto = bt3_call(ctx, "_require_boto3", json_object());
    if (bt3_exc_pending(ctx)) { free(key4); if (kwargs) json_free(kwargs); if (boto) json_free(boto); return NULL; }
    if (bt3_is_none(boto)) {
        bt3_raise_attr(ctx, boto, "Session");
        free(key4); if (kwargs) json_free(kwargs); if (boto) json_free(boto);
        return NULL;
    }
    json_t *sess_args = bt3_clean_kwargs(kwargs);
    if (kwargs) json_free(kwargs);
    json_t *sess = bt3_call(ctx, "boto3_Session", sess_args);
    if (boto) json_free(boto);
    if (bt3_exc_pending(ctx)) { free(key4); if (sess) json_free(sess); return NULL; }
    json_t *client = bt3_new_client(ctx, sess, service, region, read_timeout);
    if (sess) json_free(sess);
    if (bt3_exc_pending(ctx)) { free(key4); if (client) json_free(client); return NULL; }
    if (!bt3_is_obj(home)) { home = json_object(); json_set(ctx->state, "_bedrock_clients_by_home", home); }
    json_set(home, key4 ? key4 : "", client ? json_copy(client) : json_null());
    free(key4);
    return client;
}

/* ── Bedrock Mantle OpenAI SigV4 auth ────────────────────────────────────── */

/* PoP: bt3_sigv4_init @ agent/bedrock_adapter.py:BedrockOpenAISigV4Auth.__init__ */
static json_t *bt3_sigv4_init(bt3_ctx *ctx, const json_t *region, const json_t *service) {
    /* self.region = (region or "").strip() or resolve_bedrock_runtime_region() */
    if (bt3_truthy(region) && !bt3_is_str(region)) { bt3_raise_attr(ctx, region, "strip"); return NULL; }
    const char *rs = bt3_is_str(region) ? bt3_jstr(region) : "";
    char *stripped = bt3_py_strip(rs);
    json_t *region_val = NULL;
    if (stripped && stripped[0]) {
        region_val = json_string(stripped);
    } else {
        region_val = bt3_call(ctx, "resolve_bedrock_runtime_region", json_object());
        if (bt3_exc_pending(ctx)) { free(stripped); if (region_val) json_free(region_val); return NULL; }
    }
    free(stripped);
    json_t *out = json_object();
    json_set(out, "region", region_val ? region_val : json_null());
    json_set(out, "service", service ? json_copy(service) : json_null());
    return out;
}

/* PoP: bt3_auth_flow @ agent/bedrock_adapter.py:BedrockOpenAISigV4Auth.auth_flow */
static json_t *bt3_auth_flow(bt3_ctx *ctx, const json_t *auth_region, const json_t *auth_service,
                             const json_t *request) {
    /* kwargs = scoped_aws_session_kwargs() */
    json_t *kwargs = bt3_call(ctx, "scoped_aws_session_kwargs", json_object());
    if (bt3_exc_pending(ctx)) { if (kwargs) json_free(kwargs); return NULL; }
    /* credentials = _require_boto3().Session(**kwargs).get_credentials() */
    json_t *boto = bt3_call(ctx, "_require_boto3", json_object());
    if (bt3_exc_pending(ctx)) { if (kwargs) json_free(kwargs); if (boto) json_free(boto); return NULL; }
    if (bt3_is_none(boto)) {
        bt3_raise_attr(ctx, boto, "Session");
        if (kwargs) json_free(kwargs); if (boto) json_free(boto);
        return NULL;
    }
    json_t *sess_args = bt3_clean_kwargs(kwargs);
    if (kwargs) json_free(kwargs);
    json_t *sess = bt3_call(ctx, "boto3_Session", sess_args);
    if (boto) json_free(boto);
    if (bt3_exc_pending(ctx)) { if (sess) json_free(sess); return NULL; }
    if (bt3_is_none(sess)) { bt3_raise_attr(ctx, sess, "get_credentials"); if (sess) json_free(sess); return NULL; }
    json_t *creds = bt3_call(ctx, "session_get_credentials", json_object());
    if (sess) json_free(sess);
    if (bt3_exc_pending(ctx)) { if (creds) json_free(creds); return NULL; }
    if (bt3_is_none(creds)) {
        bt3_raise_msg(ctx, "RuntimeError", BT3_NO_CREDS_MSG);
        if (creds) json_free(creds);
        return NULL;
    }
    /* headers = {str(k): str(v) for k,v in request.headers.items()
     *            if str(k).lower() not in {"authorization","x-amz-date","x-amz-security-token"}} */
    json_t *headers_in = bt3_get(request, "headers");
    if (!bt3_is_obj(headers_in)) {
        bt3_raise_attr(ctx, headers_in, "items");
        if (creds) json_free(creds);
        return NULL;
    }
    json_t *filt = json_object();
    for (size_t i = 0; i < headers_in->c.count; i++) {
        /* JSON object keys are str, so str(k) == k; filter on str(k).lower() */
        const char *kraw = headers_in->c.keys[i];
        char *kl = bt3_py_lower(kraw);
        bool drop = bt3_str_eq(kl, "authorization") || bt3_str_eq(kl, "x-amz-date")
                 || bt3_str_eq(kl, "x-amz-security-token");
        free(kl);
        if (!drop) {
            char *vs = bt3_pystr(headers_in->c.items[i]);
            json_set(filt, kraw, json_string(vs ? vs : ""));
            free(vs);
        }
    }
    /* aws_request = AWSRequest(method=request.method, url=str(request.url),
     *                          data=request.content or b"", headers=headers) */
    json_t *content = bt3_get(request, "content");
    json_t *a = json_object();
    bt3_arg_node(a, "method", bt3_get(request, "method"));
    char *urlstr = bt3_pystr(bt3_get(request, "url"));
    json_set(a, "url", json_string(urlstr ? urlstr : ""));
    free(urlstr);
    json_t *data = bt3_truthy(content) ? json_copy(content) : json_object();
    if (!bt3_truthy(content)) { json_set(data, "__bytes__", json_array()); }
    json_set(a, "data", data);
    json_set(a, "headers", filt);
    json_t *aws_req = bt3_call(ctx, "AWSRequest_ctor", a);
    if (bt3_exc_pending(ctx)) { if (creds) json_free(creds); if (aws_req) json_free(aws_req); return NULL; }
    /* SigV4Auth(credentials.get_frozen_credentials(), self.service, self.region).add_auth(aws_request) */
    json_t *frozen = bt3_call(ctx, "credentials_get_frozen_credentials", json_object());
    if (bt3_exc_pending(ctx)) { if (creds) json_free(creds); if (aws_req) json_free(aws_req); if (frozen) json_free(frozen); return NULL; }
    if (creds) json_free(creds);
    json_t *sa = json_object();
    bt3_arg_node(sa, "credentials", frozen);
    bt3_arg_node(sa, "service", auth_service);
    bt3_arg_node(sa, "region", auth_region);
    json_t *signer = bt3_call(ctx, "SigV4Auth_ctor", sa);
    if (frozen) json_free(frozen);
    if (bt3_exc_pending(ctx)) { if (aws_req) json_free(aws_req); if (signer) json_free(signer); return NULL; }
    if (bt3_is_none(signer)) {
        bt3_raise_attr(ctx, signer, "add_auth");
        if (aws_req) json_free(aws_req); if (signer) json_free(signer);
        return NULL;
    }
    json_t *aa = json_object();
    bt3_arg_node(aa, "aws_request", aws_req);
    json_t *addres = bt3_call(ctx, "signer_add_auth", aa);
    if (signer) json_free(signer);
    if (bt3_exc_pending(ctx)) { if (aws_req) json_free(aws_req); if (addres) json_free(addres); return NULL; }
    /* request.headers.update(dict(aws_request.headers.items())) — carrier check */
    if (!bt3_is_obj(aws_req)) {
        bt3_raise_attr(ctx, aws_req, "headers");
        if (aws_req) json_free(aws_req); if (addres) json_free(addres);
        return NULL;
    }
    json_t *ah = bt3_get(aws_req, "headers");
    if (!bt3_is_obj(ah)) { json_set(aws_req, "headers", json_object()); ah = bt3_get(aws_req, "headers"); }
    if (bt3_is_obj(addres)) {
        for (size_t i = 0; i < addres->c.count; i++)
            json_set(ah, addres->c.keys[i], json_copy(addres->c.items[i]));
    }
    if (addres) json_free(addres);
    json_t *req_headers = json_copy(headers_in);
    for (size_t i = 0; i < ah->c.count; i++)
        json_set(req_headers, ah->c.keys[i], json_copy(ah->c.items[i]));
    if (aws_req) json_free(aws_req);
    /* yield request */
    json_t *req_out = json_object();
    json_t *m = bt3_get(request, "method");
    json_set(req_out, "method", m ? json_copy(m) : json_null());
    json_t *u = bt3_get(request, "url");
    json_set(req_out, "url", u ? json_copy(u) : json_null());
    json_set(req_out, "content", content ? json_copy(content) : json_null());
    json_set(req_out, "headers", req_headers);
    json_t *ret = json_array();
    json_append(ret, req_out);
    return ret;
}

/* PoP: bt3_build_http_client @ agent/bedrock_adapter.py:build_bedrock_openai_http_client */
static json_t *bt3_build_http_client(bt3_ctx *ctx, const json_t *region, const json_t *timeout) {
    /* kwargs = {"auth": BedrockOpenAISigV4Auth(region)} */
    json_t *svc = json_string("bedrock");
    json_t *auth = bt3_sigv4_init(ctx, region, svc);
    json_free(svc);
    if (bt3_exc_pending(ctx)) { if (auth) json_free(auth); return NULL; }
    json_t *kwargs = json_object();
    json_set(kwargs, "auth", auth ? auth : json_null());
    /* if isinstance(timeout,(int,float)) and not isinstance(timeout,bool) and timeout > 0 */
    if (bt3_is_num(timeout) && timeout->num_val > 0)
        json_set(kwargs, "timeout", json_copy(timeout));
    json_t *client = bt3_call(ctx, "httpx_Client", kwargs);
    if (bt3_exc_pending(ctx)) { if (client) json_free(client); return NULL; }
    return client;
}

/* ── stale-connection error types ────────────────────────────────────────── */

/* PoP: bt3_stale_error_types @ agent/bedrock_adapter.py:_stale_error_types */
static json_t *bt3_stale_error_types(bt3_ctx *ctx) {
    static const char *mods[2] = {"botocore.exceptions", "urllib3.exceptions"};
    static const char *names0[2] = {"ConnectionError", "HTTPClientError"};
    static const char *names1[3] = {"ProtocolError", "NewConnectionError", "ConnectionError"};
    json_t *types = json_array();
    for (int m = 0; m < 2; m++) {
        const char **names = (m == 0) ? names0 : names1;
        int nn = (m == 0) ? 2 : 3;
        /* types += [getattr(importlib.import_module(module), name) for name in names]
         * — import_module fires ONCE PER NAME (the call is inside the comprehension);
         * the first ImportError/AttributeError (import or getattr) suppresses the
         * WHOLE module contribution (no partial add). */
        json_t *tmp = json_array();
        bool suppressed = false;
        for (int i = 0; i < nn; i++) {
            json_t *a = json_object();
            json_set(a, "module", json_string(mods[m]));
            json_t *mod = bt3_call(ctx, "import_module", a);
            if (bt3_exc_pending(ctx)) {
                static const char *SUP[] = {"ImportError", "AttributeError"};
                if (bt3_exc_matches(ctx, SUP, 2)) { bt3_exc_clear(ctx); if (mod) json_free(mod); suppressed = true; break; }
                if (mod) json_free(mod);
                json_free(tmp); json_free(types);
                return NULL;
            }
            /* getattr(mod, name): non-obj carrier or missing name -> AttributeError */
            json_t *v = bt3_is_obj(mod) ? bt3_get(mod, names[i]) : NULL;
            if (!v) { if (mod) json_free(mod); suppressed = true; break; }
            json_append(tmp, json_copy(v));
            if (mod) json_free(mod);
        }
        if (suppressed) { json_free(tmp); continue; }
        for (size_t i = 0; i < tmp->c.count; i++)
            json_append(types, json_copy(tmp->c.items[i]));
        json_free(tmp);
    }
    return types;
}

/* ── AWS credential chain detection ──────────────────────────────────────── */

/* PoP: bt3_boto3_chain_has_credentials @ agent/bedrock_adapter.py:_boto3_chain_has_credentials */
static json_t *bt3_boto3_chain_has_credentials(bt3_ctx *ctx, const json_t *instance_metadata) {
    /* with suppress(Exception): ... return False */
    json_t *sess = bt3_call(ctx, "botocore_get_session", json_object());
    if (bt3_exc_pending(ctx)) {
        if (bt3_exc_is_exception(ctx)) { bt3_exc_clear(ctx); if (sess) json_free(sess); return json_bool(false); }
        if (sess) json_free(sess);
        return NULL;
    }
    if (bt3_is_none(sess)) { if (sess) json_free(sess); return json_bool(false); }
    if (!bt3_truthy(instance_metadata)) {
        json_t *a = json_object();
        json_set(a, "name", json_string("ec2_metadata_disabled"));
        json_set(a, "value", json_string("true"));
        json_t *r = bt3_call(ctx, "session_set_config_variable", a);
        if (bt3_exc_pending(ctx)) {
            if (bt3_exc_is_exception(ctx)) { bt3_exc_clear(ctx); if (sess) json_free(sess); if (r) json_free(r); return json_bool(false); }
            if (sess) json_free(sess); if (r) json_free(r);
            return NULL;
        }
        if (r) json_free(r);
    }
    json_t *creds = bt3_call(ctx, "session_get_credentials", json_object());
    if (bt3_exc_pending(ctx)) {
        if (bt3_exc_is_exception(ctx)) { bt3_exc_clear(ctx); if (sess) json_free(sess); if (creds) json_free(creds); return json_bool(false); }
        if (sess) json_free(sess); if (creds) json_free(creds);
        return NULL;
    }
    if (sess) json_free(sess);
    if (bt3_is_none(creds)) { if (creds) json_free(creds); return json_bool(false); }
    json_t *frozen = bt3_call(ctx, "credentials_get_frozen_credentials", json_object());
    if (creds) json_free(creds);
    if (bt3_exc_pending(ctx)) {
        if (bt3_exc_is_exception(ctx)) { bt3_exc_clear(ctx); if (frozen) json_free(frozen); return json_bool(false); }
        if (frozen) json_free(frozen);
        return NULL;
    }
    /* resolved = frozen if credentials is not None else None; bool(resolved and resolved.access_key)
     * resolved double is an object (always truthy when non-None). */
    bool resolved_truthy = !bt3_is_none(frozen);
    json_t *ak = bt3_get(frozen, "access_key");
    bool r = resolved_truthy && bt3_truthy(ak);
    if (frozen) json_free(frozen);
    return json_bool(r);
}

/* ── runtime binding ─────────────────────────────────────────────────────── */

/* PoP: bt3_bind_bedrock_runtime @ agent/bedrock_adapter.py:bind_bedrock_runtime */
static json_t *bt3_bind_bedrock_runtime(bt3_ctx *ctx, json_t *agent, const json_t *base_url, const json_t *api_mode) {
    /* agent._bedrock_region = bedrock_region_from_runtime_url(base_url) — RHS first */
    json_t *a = json_object();
    bt3_arg_node(a, "base_url", base_url);
    json_t *region = bt3_call(ctx, "bedrock_region_from_runtime_url", a);
    if (bt3_exc_pending(ctx)) { if (region) json_free(region); return NULL; }
    if (!bt3_is_obj(agent)) {
        bt3_raise_setattr(ctx, agent, "_bedrock_region");
        if (region) json_free(region);
        return NULL;
    }
    json_set(agent, "_bedrock_region", region ? region : json_null());
    /* agent._bedrock_guardrail_config = bedrock_guardrail_config() */
    json_t *gr = bt3_call(ctx, "bedrock_guardrail_config", json_object());
    if (bt3_exc_pending(ctx)) { if (gr) json_free(gr); return NULL; }
    json_set(agent, "_bedrock_guardrail_config", gr ? gr : json_null());
    json_set(agent, "client", json_null());
    json_set(agent, "_client_kwargs", json_object());
    json_set(agent, "api_key", json_string("aws-sdk"));
    json_set(agent, "_anthropic_api_key", json_string("aws-sdk"));
    json_set(agent, "_anthropic_base_url", base_url ? json_copy(base_url) : json_null());
    json_set(agent, "_is_anthropic_oauth", json_bool(false));
    if (bt3_is_str(api_mode) && bt3_str_eq(bt3_jstr(api_mode), "anthropic_messages")) {
        /* timeout=get_provider_request_timeout("bedrock", getattr(agent,"model",None)) */
        json_t *ta = json_object();
        json_set(ta, "provider", json_string("bedrock"));
        bt3_arg_node(ta, "model", bt3_get(agent, "model"));
        json_t *timeout = bt3_call(ctx, "get_provider_request_timeout", ta);
        if (bt3_exc_pending(ctx)) { if (timeout) json_free(timeout); return NULL; }
        /* build_anthropic_bedrock_client(agent._bedrock_region, timeout=timeout) */
        json_t *ba = json_object();
        bt3_arg_node(ba, "region", bt3_get(agent, "_bedrock_region"));
        bt3_arg_node(ba, "timeout", timeout);
        if (timeout) json_free(timeout);
        json_t *client = bt3_call(ctx, "build_anthropic_bedrock_client", ba);
        if (bt3_exc_pending(ctx)) { if (client) json_free(client); return NULL; }
        json_set(agent, "_anthropic_client", client ? client : json_null());
    } else {
        json_set(agent, "_anthropic_client", json_null());
    }
    return json_null();
}

/* ── _ResponseParts accumulator ──────────────────────────────────────────── */

/* PoP: bt3_parts_init @ agent/bedrock_adapter.py:_ResponseParts.__init__ */
static json_t *bt3_parts_init(bt3_ctx *ctx) {
    (void)ctx;
    json_t *out = json_object();
    json_set(out, "text_parts", json_array());
    json_set(out, "reasoning_details", json_array());
    json_set(out, "tool_calls", json_array());
    return out;
}

/* PoP: bt3_absorb_reasoning @ agent/bedrock_adapter.py:_ResponseParts.absorb_reasoning */
static json_t *bt3_absorb_reasoning(bt3_ctx *ctx, json_t *parts, const json_t *reasoning,
                                    json_t *block, const json_t *on_text) {
    if (!bt3_is_obj(reasoning)) return json_null();
    /* if isinstance(reasoning.get("reasoningText"), dict): reasoning = {**reasoning, **reasoningText} */
    json_t *r = json_copy(reasoning);
    json_t *rt = bt3_get(r, "reasoningText");
    if (bt3_is_obj(rt)) {
        for (size_t i = 0; i < rt->c.count; i++)
            json_set(r, rt->c.keys[i], json_copy(rt->c.items[i]));
    }
    /* thinking_text = reasoning.get("text", "") */
    json_t *tt = bt3_get(r, "text");
    if (bt3_truthy(tt)) {
        if (bt3_truthy(on_text)) {
            json_t *a = json_object();
            bt3_arg_node(a, "text", tt);
            json_t *r2 = bt3_call(ctx, "on_text", a);
            if (bt3_exc_pending(ctx)) { json_free(r); if (r2) json_free(r2); return NULL; }
            if (r2) json_free(r2);
        }
        /* block["text"] = block.get("text","") + str(thinking_text) */
        json_t *bt = bt3_get(block, "text");
        if (bt && !bt3_is_str(bt)) { bt3_raise_plus_type(ctx, bt); json_free(r); return NULL; }
        const char *cur = bt ? bt3_jstr(bt) : "";
        char *tts = bt3_pystr(tt);
        char *nt = bt3_concat(cur, tts ? tts : "");
        json_set(block, "text", json_string(nt ? nt : ""));
        free(tts); free(nt);
    }
    /* signature = reasoning.get("signature"); if isinstance(signature,str) and signature */
    json_t *sig = bt3_get(r, "signature");
    if (bt3_is_str(sig) && bt3_jstr(sig)[0]) {
        json_t *bs = bt3_get(block, "signature");
        if (bs && !bt3_is_str(bs)) { bt3_raise_plus_type(ctx, bs); json_free(r); return NULL; }
        const char *cur = bs ? bt3_jstr(bs) : "";
        char *ns = bt3_concat(cur, bt3_jstr(sig));
        json_set(block, "signature", json_string(ns ? ns : ""));
        free(ns);
    }
    /* encoded = _encode_redacted(reasoning.get("redactedContent")) */
    json_t *ea = json_object();
    bt3_arg_node(ea, "redacted", bt3_get(r, "redactedContent"));
    json_t *encoded = bt3_call(ctx, "_encode_redacted", ea);
    if (bt3_exc_pending(ctx)) { json_free(r); if (encoded) json_free(encoded); return NULL; }
    if (bt3_truthy(encoded)) {
        json_t *detail = json_object();
        json_set(detail, "type", json_string("redacted_thinking"));
        json_set(detail, "data", json_copy(encoded));
        json_t *rd = bt3_get(parts, "reasoning_details");
        if (!bt3_is_arr(rd)) { json_set(parts, "reasoning_details", json_array()); rd = bt3_get(parts, "reasoning_details"); }
        json_append(rd, detail);
        json_set(block, "redactedContentBase64", json_copy(encoded));
    }
    if (encoded) json_free(encoded);
    json_free(r);
    return json_null();
}

/* PoP: bt3_parts_build @ agent/bedrock_adapter.py:_ResponseParts.build */
static json_t *bt3_parts_build(bt3_ctx *ctx, const json_t *parts, const json_t *ordered_blocks,
                               const json_t *usage_data, const json_t *stop_reason, const json_t *model) {
    json_t *msg = json_object();
    json_set(msg, "role", json_string("assistant"));
    /* content = "\n".join(self.text_parts) if self.text_parts else None */
    json_t *tp = bt3_get(parts, "text_parts");
    if (bt3_is_arr(tp) && tp->c.count > 0) {
        char *j = bt3_join(tp, "\n");
        json_set(msg, "content", json_string(j ? j : ""));
        free(j);
    } else {
        json_set(msg, "content", json_null());
    }
    /* tool_calls = self.tool_calls or None */
    json_t *tc = bt3_get(parts, "tool_calls");
    json_set(msg, "tool_calls", bt3_truthy(tc) ? json_copy(tc) : json_null());
    /* reasoning_details = _persisted_reasoning_details(ordered_blocks) or self.reasoning_details or None */
    json_t *pa = json_object();
    bt3_arg_node(pa, "ordered_blocks", ordered_blocks);
    json_t *persisted = bt3_call(ctx, "_persisted_reasoning_details", pa);
    if (bt3_exc_pending(ctx)) { json_free(msg); if (persisted) json_free(persisted); return NULL; }
    json_t *rd;
    if (bt3_truthy(persisted)) {
        rd = json_copy(persisted);
    } else {
        json_t *srd = bt3_get(parts, "reasoning_details");
        rd = bt3_truthy(srd) ? json_copy(srd) : json_null();
    }
    if (persisted) json_free(persisted);
    json_set(msg, "reasoning_details", rd);
    /* reasoning_content = "\n\n".join(texts) if (texts := [b["reasoningContent"]["text"]
     *   for b in ordered_blocks if isinstance(b.get("reasoningContent"),dict)
     *   and b["reasoningContent"].get("text")]) else None */
    if (!bt3_is_arr(ordered_blocks)) {
        bt3_raise_not_iterable(ctx, ordered_blocks);
        json_free(msg);
        return NULL;
    }
    json_t *texts = json_array();
    for (size_t i = 0; i < ordered_blocks->c.count; i++) {
        json_t *b = ordered_blocks->c.items[i];
        if (!bt3_is_obj(b)) { bt3_raise_attr(ctx, b, "get"); json_free(msg); json_free(texts); return NULL; }
        json_t *rcb = bt3_get(b, "reasoningContent");
        if (bt3_is_obj(rcb)) {
            json_t *t = bt3_get(rcb, "text");
            if (bt3_truthy(t)) json_append(texts, json_copy(t));
        }
    }
    if (texts->c.count > 0) {
        char *j = bt3_join(texts, "\n\n");
        json_set(msg, "reasoning_content", json_string(j ? j : ""));
        free(j);
    } else {
        json_set(msg, "reasoning_content", json_null());
    }
    json_free(texts);
    /* bedrock_content_blocks = ordered_blocks or None */
    json_set(msg, "bedrock_content_blocks", bt3_truthy(ordered_blocks) ? json_copy(ordered_blocks) : json_null());
    /* usage_data.get(k, 0) */
    if (!bt3_is_obj(usage_data)) { bt3_raise_attr(ctx, usage_data, "get"); json_free(msg); return NULL; }
    double cr = json_get_num(usage_data, "cacheReadInputTokens", 0);
    double cw = json_get_num(usage_data, "cacheWriteInputTokens", 0);
    double ot = json_get_num(usage_data, "outputTokens", 0);
    double it = json_get_num(usage_data, "inputTokens", 0);
    double prompt = it + cr + cw;
    json_t *usage = json_object();
    json_set(usage, "prompt_tokens", json_number(prompt));
    json_set(usage, "completion_tokens", json_number(ot));
    json_set(usage, "total_tokens", json_number(prompt + ot));
    json_set(usage, "cache_read_input_tokens", json_number(cr));
    json_set(usage, "cache_creation_input_tokens", json_number(cw));
    /* finish_reason = _STOP_REASON_TO_FINISH_REASON.get(stop_reason, "stop") */
    const char *fr = bt3_stop_map(bt3_is_str(stop_reason) ? bt3_jstr(stop_reason) : "");
    if (bt3_truthy(tc) && bt3_str_eq(fr, "stop")) fr = "tool_calls";
    json_t *choice = json_object();
    json_set(choice, "index", json_number(0));
    json_set(choice, "message", msg);
    json_set(choice, "finish_reason", json_string(fr));
    json_t *choices = json_array();
    json_append(choices, choice);
    json_t *out = json_object();
    json_set(out, "choices", choices);
    json_set(out, "usage", usage);
    json_set(out, "model", model ? json_copy(model) : json_null());
    return out;
}

/* ── offline context window ──────────────────────────────────────────────── */

/* PoP: bt3_static_context_length @ agent/bedrock_adapter.py:bedrock_static_context_length */
static json_t *bt3_static_context_length(bt3_ctx *ctx, const json_t *model_id) {
    if (!bt3_is_str(model_id)) { bt3_raise_re_type(ctx, model_id); return NULL; }
    const char *mid = bt3_jstr(model_id);
    char *owned_mid = NULL;
    /* if _APPLICATION_PROFILE_ARN_RE.search(model_id): model_id = cache.get(model_id, model_id) */
    if (bt3_app_profile_arn(mid)) {
        json_t *cache = bt3_get(ctx->state, "_inference_profile_model_cache");
        json_t *v = bt3_get(cache, mid);
        if (v) {
            if (!bt3_is_str(v)) { bt3_raise_attr(ctx, v, "lower"); return NULL; }
            owned_mid = bt3_dup_span(bt3_jstr(v), strlen(bt3_jstr(v)));
            mid = owned_mid ? owned_mid : mid;
        }
    }
    char *low = bt3_py_lower(mid);
    /* matches = [key for key in BEDROCK_CONTEXT_LENGTHS if key in lowered]; max(key=len) */
    size_t bestlen = 0;
    int bestval = 0;
    for (size_t i = 0; i < sizeof(BT3_CONTEXT_LENGTHS) / sizeof(BT3_CONTEXT_LENGTHS[0]); i++) {
        const char *key = BT3_CONTEXT_LENGTHS[i].k;
        if (bt3_str_has(low, key)) {
            size_t kl = strlen(key);
            if (kl > bestlen) { bestlen = kl; bestval = BT3_CONTEXT_LENGTHS[i].v; }
        }
    }
    if (bestlen > 0) { free(low); free(owned_mid); return json_number(bestval); }
    /* family = _CATALOG_FAMILY_ID_RE.search(lowered) */
    char *fam = bt3_catalog_family(low);
    free(low);
    if (!fam) { free(owned_mid); return json_null(); }
    /* hit = _longest_key_match(DEFAULT_CONTEXT_LENGTHS, family.group(1)) */
    json_t *a = json_object();
    json_set(a, "key", json_string(fam));
    free(fam);
    json_t *hit = bt3_call(ctx, "_longest_key_match", a);
    free(owned_mid);
    if (bt3_exc_pending(ctx)) { if (hit) json_free(hit); return NULL; }
    if (bt3_is_none(hit)) { if (hit) json_free(hit); return json_null(); }
    if (!bt3_is_arr(hit) || hit->c.count < 2) {
        bt3_raise_msg(ctx, "IndexError", "tuple index out of range");
        if (hit) json_free(hit);
        return NULL;
    }
    json_t *val = json_copy(hit->c.items[1]);
    json_free(hit);
    return val;
}