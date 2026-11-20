/* ===========================================================================
 * error_classifier_flow.c -- FLOW + leaf port of the UNCLAIMED remainder of
 * agent/error_classifier.py (the priority-ordered API-error classification
 * pipeline minus its already-claimed leaves). 34 live defs:
 *   _v, _first_match, _billing_hints, _is_anthropic_thinking_rejection,
 *   _gemini_prompt_block_marker, _is_gemini_prompt_block, _from_cause_chain,
 *   _status_of, _body_of, _headers_of, _status_code_from_body,
 *   _code_from_payload, _body_message_candidates, _openrouter_wrapped_message,
 *   _build_error_msg, _is_server_injected_param_rejection,
 *   _is_codex_masked_replay_rejection, _moa_special_cases, _nous_welcome_tier,
 *   _provider_special_cases, _status_403, _status_404, _status_429,
 *   _status_5xx, _is_nous_route, _by_status, _by_error_code, _by_transport,
 *   _shape_context, _off_route_host, _plugin_verdict, _profile_verdict,
 *   _Ctx.__post_init__, _Ctx.large_session.
 *
 * Self-contained: libjson only. Every live dependency that is NOT ported here
 * is injected through ONE universal seam:
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * args is a BORROWED object (the port owns it and frees it after the call);
 * the result is a NEW json_t owned by the caller (NULL == None; a
 * {"__raise__":kind,"message":msg} object == a raised exception). A scripted
 * JSON null result is delivered as a NULL pointer so truthiness reads falsy.
 * Fail-closed: no channel installed, or a missing/exhausted script, yields
 * {"__raise__":"RuntimeError","message":"no script for <name>"} /
 * "no seam installed for <name>", recorded by the dispatcher BEFORE the port
 * propagates it.
 *
 * ENTRIES. json_t *ec_<fn>(ec_io_t *io, const json_t *in, json_t *st)
 *   io  -- universal seam channel (NULL => fail-closed production default)
 *   in  -- borrowed case input object (never mutated)
 *   st  -- fresh object the entry fills with the final state projection.
 *          These classifiers mutate no caller state, so st stays {} on every
 *          path (success AND raise). Caller owns the returned json_t.
 *          None projects to NULL; bool to json_bool; tuples to arrays.
 *
 * Ported siblings compose REAL (e.g. _provider_special_cases ->
 * _nous_welcome_tier / _is_gemini_prompt_block / _is_codex_masked_replay_rejection
 * / _is_anthropic_thinking_rejection; _is_gemini_prompt_block ->
 * _from_cause_chain(_gemini_prompt_block_marker); _Ctx.__post_init__ ->
 * _from_cause_chain(_headers_of); _shape_context -> _from_cause_chain(_status_of);
 * _build_error_msg -> _openrouter_wrapped_message; _is_codex_masked_replay_rejection
 * -> _body_message_candidates; _status_5xx -> _is_server_injected_param_rejection;
 * _by_status -> the ported status handlers + _is_nous_route + _first_match).
 *
 * Claimed siblings are SEAMED (PoP elsewhere): _extract_error_code,
 * _error_obj, _json_dict, _has_usage_limit_transient_signal,
 * _rate_limit_reset_seconds, _model_id_missing_known_prefix,
 * _is_openrouter_upstream_error, _extract_upstream_provider_name,
 * _classify_400, _classify_402. The unported unclaimed leaf
 * _classify_image_tool_422 is seamed too (no PoP re-claim). The stale
 * printf-stub claims _classify_by_status/_classify_by_message/
 * _classify_by_error_code target names that no longer exist in the quarry;
 * the live _by_status/_by_error_code ported here are unclaimed.
 *
 * BaseException kinds (SystemExit, KeyboardInterrupt, GeneratorExit,
 * CancelledError, BaseException, BaseExceptionGroup) PASS THROUGH every
 * `except Exception` site (_plugin_verdict, _profile_verdict, _body_of) via
 * the FULL builtin hierarchy table below (ec_exc_is_exception walks parents).
 *
 * Seam name -> live call table:
 *   extract_error_code              _extract_error_code(body)
 *   error_obj                       _error_obj(body)
 *   json_dict                       _json_dict(text)
 *   has_usage_limit_transient_signal _has_usage_limit_transient_signal(error_msg, body, response_headers)
 *   rate_limit_reset_seconds        _rate_limit_reset_seconds(error_msg, body, response_headers)
 *   model_id_missing_known_prefix   _model_id_missing_known_prefix(model, provider)
 *   is_openrouter_upstream_error    _is_openrouter_upstream_error(body, provider)
 *   extract_upstream_provider_name  _extract_upstream_provider_name(body)
 *   classify_400                    _classify_400(c)            [ctx projection]
 *   classify_402                    _classify_402(error_msg, dict, code, body)
 *   classify_image_tool_422         _classify_image_tool_422(c) [ctx projection]
 *   first_match.verdict             a callable rule verdict(error_msg)
 *   pick                            _from_cause_chain's pick(exc) callable
 *   response.json                   exc.response.json()
 *   get_plugin_error_classification hermes_cli.plugins.get_plugin_error_classification(**kw)
 *   get_provider_profile            providers.get_provider_profile(provider_slug)
 *   profile.classify_api_error      profile.classify_api_error(error, **kw)
 *   welcome_route_refusal           hermes_cli.anon_auth.welcome_route_refusal(status, msg, base_url)
 *   route_is_welcome_host           hermes_cli.anon_auth.route_is_welcome_host(base_url)
 *   parse_welcome_refusal           hermes_cli.anon_auth.parse_welcome_refusal(body)
 *   welcome_refusal_from_headers    agent.nous_rate_guard.welcome_refusal_from_headers(headers, body_wait=...)
 *   get_reasoning_stale_timeout_floor agent.reasoning_timeouts.get_reasoning_stale_timeout_floor(model)
 *   provider_owns_route             hermes_cli.route_identity.provider_owns_route(provider_slug, base_url)
 *   base_url_hostname               utils.base_url_hostname(url)
 *   base_url_host_matches           utils.base_url_host_matches(base_url, host)
 *   time.time                       time.time()
 *   logger.debug / logger.info      logger.debug/info(fmt, *args)
 *
 * Exception double model (in.error / in.ctx.error / chain cause/context):
 *   {"kind":ClassName, "message":str(exc), "attrs":{...},
 *    "cause":<spec|null>, "context":<spec|null>}
 *   kind names a real builtin (full hierarchy) or a seeded stub class
 *   (MoAPresetNotFoundError(ValueError), InvalidProviderResponseError(RuntimeError),
 *   GeminiAPIError / APIError / RateLimitError / AuthenticationError(Exception)).
 *   message == str(exc) for the fixture domain (KeyError doubles excluded:
 *   str(KeyError(k))==repr(k)). attrs.response is either a plain dict (botocore
 *   ResponseMetadata branch) or a tagged object double {"__response__":true,
 *   "status_code":N, "headers":{...}} whose .json() is the response.json seam.
 *   Seam args project an exc to {"kind","message"} (proj_exc).
 *
 * _Ctx model (in.ctx): raw fields {error, status_code, body, msg, provider,
 *   model, approx_tokens, context_length, num_messages, base_url, anonymous}.
 *   ec_ctx_build runs the ported __post_init__ (fires the extract_error_code
 *   seam; derives error_type/code/headers/provider_slug/model_slug real).
 *   large_session reads raw fields only (no post_init).
 *
 * Domain restrictions (documented, enforced by the generator):
 *   - ASCII inputs; strip()/lower() ASCII. str()-positions carry strings or
 *     integral numbers (integral floats normalize to int on both sides).
 *   - reason carries a FailoverReason VALUE string. Verdict dicts compare
 *     order-insensitively (reason + explicit hint keys only).
 *   - status_code attrs are int-or-null (bool excluded: _status_of's first
 *     branch admits bool, an out-of-domain quirk).
 *   - response_headers is null / dict / non-dict-without-get.
 *   - rules verdicts: static dict, {"__fn__":"billing_hints"} (real ported),
 *     or {"__callable__":true} (first_match.verdict seam).
 *   - no regex is used by any ported def (the regex-heavy leaves
 *     _is_byte_size_limit_message / _account_gates_model / the reasoning trio
 *     stay unported; _by_message which needs them is not ported).
 * =========================================================================== */

#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

typedef struct ec_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} ec_io_t;

/* ---- truthiness / accessors ---- */

static int ec_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}

/* `is not None` identity: present-non-null. */
static int ec_present(const json_t *v)
{
    return (v && v->type != JSON_NULL) ? 1 : 0;
}

static const char *ec_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *ec_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static double ec_num(const json_t *v)
{
    if (v && v->type == JSON_NUMBER) return v->num_val;
    if (v && v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return 0.0;
}

static int ec_is_int(const json_t *v)
{
    /* isinstance(x, int) admits bool; the fixture domain excludes bool
     * status codes, so a plain JSON_NUMBER models int here. */
    return v && v->type == JSON_NUMBER;
}

static int ec_is_dict(const json_t *v)
{
    return v && v->type == JSON_OBJECT;
}

/* ---- raise specs ---- */

static int ec_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *ec_kind(const json_t *r)
{
    const char *k = ec_str(ec_get(r, "__raise__"));
    return k ? k : "";
}

static json_t *ec_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}

/* malloc'd printf-style string builder */
static char *ec_xfmt(const char *fmt, ...)
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

/* ---- FULL builtin exception hierarchy (CPython 3.14) ----
 * parent table; unknown kinds default to Exception (synthetic subclass). */
typedef struct { const char *name; const char *parent; } ec_exc_row_t;
static const ec_exc_row_t EC_EXC_TABLE[] = {
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
    /* seeded stub classes (agent.errors + SDK lookalikes) */
    {"MoAPresetNotFoundError", "ValueError"},
    {"InvalidProviderResponseError", "RuntimeError"},
    {"GeminiAPIError", "Exception"},
    {"APIError", "Exception"},
    {"RateLimitError", "Exception"},
    {"AuthenticationError", "Exception"},
    {NULL, NULL}
};

static const char *ec_exc_parent(const char *kind)
{
    for (int i = 0; EC_EXC_TABLE[i].name; i++)
        if (strcmp(EC_EXC_TABLE[i].name, kind) == 0)
            return EC_EXC_TABLE[i].parent;
    return "Exception";  /* unknown kind == synthetic Exception subclass */
}

/* isinstance(exc, target): walk the parent chain from kind. */
static int ec_exc_is_instance(const char *kind, const char *target)
{
    const char *cur = kind;
    for (int depth = 0; cur && depth < 64; depth++) {
        if (strcmp(cur, target) == 0) return 1;
        cur = ec_exc_parent(cur);
    }
    return 0;
}

/* caught by `except Exception` iff Exception is an ancestor (or kind itself).
 * BaseException-only kinds (trio + CancelledError + BaseException(Group))
 * return 0 and PASS THROUGH. */
static int ec_exc_is_exception(const char *kind)
{
    return ec_exc_is_instance(kind, "Exception");
}

static int ec_is_base(const char *kind)
{
    return !ec_exc_is_exception(kind);
}

/* ---- universal seam call ----
 * args OWNED by the caller (built fresh), borrowed by the dispatcher, freed
 * here. Result caller-owned (NULL == None). Fail-closed with no channel. */
static json_t *ec_call(ec_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = ec_xfmt("no seam installed for %s", name);
        r = ec_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

static json_t *ec_args(void)
{
    return json_object();
}

/* logger.debug/info: record fmt + raw positional args. */
static json_t *ec_log(ec_io_t *io, const char *name, const char *fmt, json_t *args)
{
    json_t *a = ec_args();
    json_set(a, "fmt", json_string(fmt));
    json_set(a, "args", args ? args : json_array());
    return ec_call(io, name, a);
}

/* ---- string helpers (ASCII domain) ---- */

static int ec_is_ws(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
        || (c >= 0x1c && c <= 0x1f);
}

static char *ec_strip(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && ec_is_ws((unsigned char)s[a])) a++;
    while (b > a && ec_is_ws((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

static char *ec_lower(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        out[i] = (char)((c >= 'A' && c <= 'Z') ? c + 32 : c);
    }
    out[n] = '\0';
    return out;
}

static int ec_has(const char *hay, const char *needle)
{
    if (!hay || !needle) return 0;
    return strstr(hay, needle) != NULL;
}

static int ec_any_in(const char *hay, const char *const *pats)
{
    if (!hay) return 0;
    for (int i = 0; pats[i]; i++)
        if (strstr(hay, pats[i])) return 1;
    return 0;
}

/* Python str() projection (domain: strings / integral numbers / bool / None).
 * Caller frees. */
static char *ec_pystr(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return ec_xfmt("None");
    if (v->type == JSON_BOOL) return ec_xfmt(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == 0.0) return ec_xfmt("0");
        if (d == (double)(long long)d) return ec_xfmt("%.0f", d);
        return ec_xfmt("%.17g", d);
    }
    if (v->type == JSON_STRING) return ec_xfmt("%s", v->str_val ? v->str_val : "");
    char *s = json_serialize(v);
    return s ? s : ec_xfmt("");
}

/* str(x or "") projection: falsy -> "", else str(x). Caller frees. */
static char *ec_pystr_or_empty(const json_t *v)
{
    if (!ec_truth(v)) return ec_xfmt("");
    return ec_pystr(v);
}

/* (x or "").strip().lower() — caller frees. */
static char *ec_norm_slug(const json_t *v)
{
    char *s = ec_pystr_or_empty(v);
    char *t = ec_strip(s ? s : "");
    free(s);
    char *l = ec_lower(t ? t : "");
    free(t);
    return l;
}

/* exc double -> str(exc) (domain: message == str(exc)). Caller frees. */
static char *ec_exc_str(const json_t *exc)
{
    const char *m = ec_str(ec_get(exc, "message"));
    return ec_xfmt("%s", m ? m : "");
}

/* proj_exc: seam-arg projection of an exc double. Caller owns. */
static json_t *ec_proj_exc(const json_t *exc)
{
    json_t *o = json_object();
    const char *k = ec_str(ec_get(exc, "kind"));
    const char *m = ec_str(ec_get(exc, "message"));
    json_set(o, "kind", json_string(k ? k : ""));
    json_set(o, "message", json_string(m ? m : ""));
    return o;
}

/* proj_ctx: seam-arg projection of a _Ctx (classify_400 / classify_image_tool_422).
 * error projects to proj_exc; the rest copy verbatim. Caller owns. */
static json_t *ec_proj_ctx(const json_t *ctx)
{
    json_t *o = json_object();
    static const char *keys[] = {"status_code", "body", "msg", "provider", "model",
        "approx_tokens", "context_length", "num_messages", "base_url", "anonymous",
        "error_type", "error_code", "code", "headers", "provider_slug", "model_slug"};
    for (int i = 0; i < 16; i++) {
        json_t *v = ec_get(ctx, keys[i]);
        json_set(o, keys[i], v ? json_copy(v) : json_null());
    }
    json_set(o, "error", ec_proj_exc(ec_get(ctx, "error")));
    return o;
}
/* ---- pattern tables (lowercased substrings; verbatim from the quarry) ---- */

static const char *const EC_FREE_TIER[] = {
    "model_not_supported_on_free_tier", "not available on the free tier",
    "isn't available on the free tier", NULL};
static const char *const EC_BILLING_BASE[] = {
    "insufficient credits", "insufficient_quota", "insufficient balance", "credit balance",
    "credits exhausted", "credits have been exhausted", "requires available credits",
    "account balance is too low", "no usable credits", "top up your credits", "payment required",
    "billing hard limit", "exceeded your current quota", "account is deactivated",
    "plan does not include", "out of extra usage", "out of funds", "run out of funds",
    "balance_depleted", "insufficient funds", "can only afford", "budget limit exceeded", NULL};
static const char *const EC_BILLING_TAIL[] = {"hard billing limit", NULL};
static const char *const EC_LONG_QUOTA[] = {
    "tokens per day", "daily limit", "daily quota", "weekly usage limit", "weekly limit",
    "reached your session usage limit", NULL};
static const char *const EC_QUOTA_EXTRA[] = {
    "quota exceeded", "quota_exceeded", "upgrade for higher limits", NULL};
static const char *const EC_WALL_EXTRA[] = {
    "key limit exceeded", "spending limit", "budget limit", "requires a subscription",
    "upgrade for access", NULL};
static const char *const EC_WALL_WORDS_EXTRA[] = {"quota", "billing", "free tier", "free-tier", NULL};
static const char *const EC_RESOURCE_EXHAUSTED[] = {
    "resource exhausted", "resource_exhausted", "resource-exhausted", "resourceexhausted", NULL};
static const char *const EC_RATE_LIMIT[] = {
    "rate limit", "rate_limit", "too many requests", "throttled", "requests per minute",
    "tokens per minute", "requests per day", "try again in", "please retry after",
    "resource exhausted", "resource_exhausted", "resource-exhausted", "resourceexhausted",
    "rate increased too quickly", "throttlingexception", "too many concurrent requests",
    "servicequotaexceededexception", "throttling", NULL};
static const char *const EC_OVERLOADED[] = {
    "overloaded", "temporarily overloaded", "service is temporarily overloaded",
    "service may be temporarily overloaded", "server is overloaded", "server overloaded",
    "server overload", "server_overload", "service overloaded", "service is overloaded",
    "upstream overloaded", "currently overloaded",
    "upstream model provider is temporarily unavailable. please try again in a moment.",
    "at capacity", "over capacity", NULL};
static const char *const EC_USAGE_LIMIT[] = {
    "usage limit", "quota", "limit exceeded", "key limit exceeded", NULL};
static const char *const EC_EMPTY_RESPONSE[] = {
    "returned an empty response", "empty response despite retries",
    "provider returned an empty response", "model returning empty responses",
    "empty response stream", NULL};
static const char *const EC_MEMORY_CEILING[] = {
    "memory guard", "memory limit exceeded", "memory_guard_tier", "dynamic ceiling",
    "memory ceiling", "available memory", "out of memory", "insufficient memory",
    "prefill would require", "predicted peak would", "prefill safety cap", "metal_cap", NULL};
static const char *const EC_CONTEXT_OVERFLOW[] = {
    "context length", "context size", "maximum context", "token limit", "too many tokens",
    "reduce the length", "exceeds the limit", "context window", "prompt is too long",
    "prompt exceeds max length", "max_tokens", "maximum number of tokens",
    "exceeds the max_model_len", "max_model_len", "prompt length", "input is too long",
    "maximum model length", "context length exceeded", "truncating input",
    "slot context", "n_ctx_slot",
    "\xe8\xb6\x85\xe8\xbf\x87\xe6\x9c\x80\xe5\xa4\xa7\xe9\x95\xbf\xe5\xba\xa6",  /* 超过最大长度 */
    "\xe4\xb8\x8a\xe4\xb8\x8b\xe6\x96\x87\xe9\x95\xbf\xe5\xba\xa6",              /* 上下文长度 */
    "tokens in request more than max tokens allowed",
    "input is too long", "max input token", "input token",
    "exceeds the maximum number of input tokens", "maximum allowed input length", NULL};
static const char *const EC_MODEL_NOT_FOUND[] = {
    "is not a valid model", "invalid model", "model not found", "model_not_found",
    "does not exist", "no such model", "unknown model", "unsupported model",
    "no endpoints found that support tool use", "openrouter catalog", "is no longer free", NULL};
static const char *const EC_PROVIDER_POLICY_BLOCKED[] = {
    "no endpoints available matching your guardrail",
    "no endpoints available matching your data policy",
    "no endpoints found matching your data policy", NULL};
static const char *const EC_CONTENT_POLICY_BLOCKED[] = {
    "flagged for possible cybersecurity risk", "trusted access for cyber",
    "violates our usage policies", "violates openai's usage policies",
    "your request was flagged by", "prompt was flagged by our safety",
    "responses cannot be generated due to safety", "content_filter",
    "responsibleaipolicyviolation", "new_sensitive", "content exists risk",
    "data may contain inappropriate content", "data_inspection_failed", NULL};
static const char *const EC_PROMPT_INJECTION_GUARDRAIL[] = {
    "prompt injection patterns detected", NULL};
static const char *const EC_ACCOUNT_POLICY_BLOCK[] = {
    "blocked for a previous policy violation", NULL};
static const char *const EC_THINKING_MUTATION[] = {
    "signature", "cannot be modified", "must remain as they were", NULL};
static const char *const EC_MOA_SHAPE_BUGS[] = {
    "'types.SimpleNamespace' object is not iterable",
    "'types.SimpleNamespace' object has no attribute 'index'", NULL};
static const char *const EC_UPSTREAM_BLOCKED[] = {
    "your request was blocked", "request blocked", "sorry, you have been blocked",
    "enable javascript and cookies to continue", "cdn-cgi/challenge-platform",
    "cf-browser-verification", "challenge-error-text", "__cf_chl", "cf-error-details",
    "attention required! | cloudflare", NULL};
static const char *const EC_SSL_CERT_VERIFY[] = {
    "certificate verify failed", "certificate_verify_failed",
    "unable to get local issuer certificate", "self-signed certificate",
    "self signed certificate", "certificate has expired",
    "hostname mismatch, certificate is not valid", "unable to verify the first certificate", NULL};
static const char *const EC_SSL_TRANSIENT[] = {
    "bad record mac", "ssl alert", "tls alert", "ssl handshake failure", "tlsv1 alert",
    "sslv3 alert", "bad_record_mac", "ssl_alert", "tls_alert", "tls_alert_internal_error",
    "[ssl:", NULL};
static const char *const EC_SERVER_DISCONNECT[] = {
    "server disconnected", "peer closed connection", "connection reset by peer",
    "connection was closed", "network connection lost", "unexpected eof",
    "incomplete chunked read", NULL};
static const char *const EC_PREMATURE_CLOSE[] = {
    "connection reset", "response ended prematurely", "remoteprotocolerror",
    "localprotocolerror", NULL};
static const char *const EC_CONNECTION_MESSAGE[] = {
    "connection refused", "econnrefused", "no route to host", "network is unreachable",
    "network unreachable", "name or service not known",
    "temporary failure in name resolution", "nodename nor servname provided",
    "getaddrinfo failed", "getaddrinfo enotfound", "eai_again", "fetch failed",
    "failed to fetch", "upstream connect error", NULL};
static const char *const EC_TRANSPORT_TYPES[] = {
    "ReadTimeout", "ConnectTimeout", "PoolTimeout", "ConnectError", "RemoteProtocolError",
    "ConnectionError", "ConnectionResetError", "ConnectionAbortedError", "BrokenPipeError",
    "TimeoutError", "ReadError", "ServerDisconnectedError", "SSLError", "SSLZeroReturnError",
    "SSLWantReadError", "SSLWantWriteError", "SSLEOFError", "SSLSyscallError",
    "APIConnectionError", "APITimeoutError", NULL};
static const char *const EC_TRANSPORT_MARKERS[] = {"Connection", "Timeout", "DNS", "SSL", NULL};
static const char *const EC_REQUEST_VALIDATION[] = {
    "unknown parameter", "unsupported parameter", "unrecognized request argument",
    "invalid_request_error", "unknown_parameter", "unsupported_parameter", NULL};
static const char *const EC_ROUTE_MODEL_REJECTION[] = {
    "model is not supported", "model_not_supported", "does not support this model",
    "is not supported when using", "not supported with this", "not supported for this account", NULL};
static const char *const EC_UNVERIFIED_BILLING[] = {"out of extra usage", NULL};
static const char *const EC_PARAM_REJECTION_WORDS[] = {
    "not supported", "unsupported", "unknown", "unrecognized", NULL};

/* composite tables built once by concatenation (set-equal to the quarry tuples) */
static const char **EC_BILLING = NULL;        /* BASE + FREE_TIER + TAIL */
static const char **EC_QUOTA_WINDOW = NULL;   /* LONG_QUOTA + QUOTA_EXTRA */
static const char **EC_ACCOUNT_WALL = NULL;   /* BILLING + QUOTA_WINDOW + WALL_EXTRA */
static const char **EC_WALL_WORDS = NULL;     /* ACCOUNT_WALL + WALL_WORDS_EXTRA */
static const char **EC_403_404_WALL = NULL;   /* ACCOUNT_WALL + RESOURCE_EXHAUSTED */
static const char **EC_WELCOME_403_NAMED = NULL; /* CONTENT_POLICY + (BILLING minus FREE_TIER) */

static const char **ec_concat(int n, const char *const *tabs[], const char *const *skip)
{
    size_t total = 0;
    for (int t = 0; t < n; t++)
        for (int i = 0; tabs[t][i]; i++) {
            int excluded = 0;
            if (skip) for (int j = 0; skip[j]; j++)
                if (strcmp(tabs[t][i], skip[j]) == 0) { excluded = 1; break; }
            if (!excluded) total++;
        }
    const char **out = (const char **)malloc((total + 1) * sizeof(char *));
    if (!out) return NULL;
    size_t k = 0;
    for (int t = 0; t < n; t++)
        for (int i = 0; tabs[t][i]; i++) {
            int excluded = 0;
            if (skip) for (int j = 0; skip[j]; j++)
                if (strcmp(tabs[t][i], skip[j]) == 0) { excluded = 1; break; }
            if (!excluded) out[k++] = tabs[t][i];
        }
    out[k] = NULL;
    return out;
}

static void ec_tables_init(void)
{
    if (EC_BILLING) return;
    const char *const *b_tabs[] = {EC_BILLING_BASE, EC_FREE_TIER, EC_BILLING_TAIL};
    EC_BILLING = ec_concat(3, b_tabs, NULL);
    const char *const *q_tabs[] = {EC_LONG_QUOTA, EC_QUOTA_EXTRA};
    EC_QUOTA_WINDOW = ec_concat(2, q_tabs, NULL);
    const char *const *a_tabs[] = {EC_BILLING, EC_QUOTA_WINDOW, EC_WALL_EXTRA};
    EC_ACCOUNT_WALL = ec_concat(3, a_tabs, NULL);
    const char *const *w_tabs[] = {EC_ACCOUNT_WALL, EC_WALL_WORDS_EXTRA};
    EC_WALL_WORDS = ec_concat(2, w_tabs, NULL);
    const char *const *x_tabs[] = {EC_ACCOUNT_WALL, EC_RESOURCE_EXHAUSTED};
    EC_403_404_WALL = ec_concat(2, x_tabs, NULL);
    const char *const *n_tabs[] = {EC_CONTENT_POLICY_BLOCKED, EC_BILLING};
    EC_WELCOME_403_NAMED = ec_concat(2, n_tabs, EC_FREE_TIER);
}

/* ---- structured code sets ---- */
static const char *const EC_BILLING_CODES[] = {
    "insufficient_quota", "billing_not_active", "payment_required", "insufficient_credits",
    "no_usable_credits", "balance_depleted", "model_not_supported_on_free_tier",
    "member_spend_cap_exceeded", "terminal_quota_exhausted", "personal-team-blocked:spending-limit",
    "credit_balance_exhausted", "organization_spend_limit_exceeded",
    "organization_usage_limit_exceeded", "project_spend_limit_exceeded",
    "insufficient_credits_for_paid_model", NULL};
static const char *const EC_MEMORY_CODES[] = {
    "prefill_memory_exceeded", "prefill_memory_aborted", "omlx_prefill_memory_exceeded", NULL};
static const char *const EC_403_TRANSIENT_CODES[] = {"upstream_unavailable", NULL};
static const char *const EC_5XX_VALIDATION_CODES[] = {
    "unknown_parameter", "unsupported_parameter", "invalid_request_error", NULL};

static int ec_in_set(const char *s, const char *const *set)
{
    if (!s) return 0;
    for (int i = 0; set[i]; i++) if (strcmp(s, set[i]) == 0) return 1;
    return 0;
}

/* ---- verdicts ---- */
typedef enum {
    VID_BILLING, VID_RATE_LIMIT, VID_AUTH_ROTATE, VID_AUTH_FALLBACK,
    VID_MODEL_NOT_FOUND, VID_UPSTREAM_BLOCKED, VID_CONTENT_BLOCKED,
    VID_GUARDRAIL_BLOCKED, VID_MODEL_ENTITLEMENT, VID_FORMAT_ERROR,
    VID_POLICY_BLOCKED, VID_SSL_CERT, VID_CONTEXT_OVERFLOW, VID_PAYLOAD_TOO_LARGE,
    VID_OVERLOADED, VID_SERVER_ERROR, VID_TIMEOUT, VID_UNKNOWN,
    VID_INVALID_ENCRYPTED, VID_REASONING_MANDATORY, VID_ROLE_ALTERNATION,
    VID_MALFORMED_TOOL_ARGS, VID_BILLING_HINTS, VID_SEAM
} ec_vid_t;

static json_t *ec_vobj(const char *reason)
{
    json_t *o = json_object();
    json_set(o, "reason", json_string(reason));
    return o;
}
static void ec_hint(json_t *o, const char *k, int b)
{
    json_set(o, k, json_bool(b ? true : false));
}

/* _v(reason, **hints): {"reason": reason, **hints}. hints borrowed. */
static json_t *ec_v(const char *reason, const json_t *hints)
{
    json_t *o = ec_vobj(reason);
    if (ec_is_dict(hints))
        for (size_t i = 0; i < hints->c.count; i++)
            json_set(o, hints->c.keys[i], json_copy(hints->c.items[i]));
    return o;
}

static json_t *ec_billing_verdict(void)
{
    json_t *o = ec_vobj("billing");
    ec_hint(o, "retryable", 0);
    ec_hint(o, "should_rotate_credential", 1);
    ec_hint(o, "should_fallback", 1);
    return o;
}

/* build a constant verdict by id (VID_BILLING_HINTS/VID_SEAM handled by caller) */
static json_t *ec_const_verdict(int vid)
{
    json_t *o;
    switch (vid) {
    case VID_BILLING: return ec_billing_verdict();
    case VID_RATE_LIMIT:
        o = ec_vobj("rate_limit"); ec_hint(o, "should_rotate_credential", 1);
        ec_hint(o, "should_fallback", 1); return o;
    case VID_AUTH_ROTATE:
        o = ec_vobj("auth"); ec_hint(o, "retryable", 0);
        ec_hint(o, "should_rotate_credential", 1); ec_hint(o, "should_fallback", 1); return o;
    case VID_AUTH_FALLBACK:
        o = ec_vobj("auth"); ec_hint(o, "retryable", 0); ec_hint(o, "should_fallback", 1); return o;
    case VID_MODEL_NOT_FOUND:
        o = ec_vobj("model_not_found"); ec_hint(o, "retryable", 0); ec_hint(o, "should_fallback", 1); return o;
    case VID_UPSTREAM_BLOCKED:
        o = ec_vobj("upstream_blocked"); ec_hint(o, "retryable", 0); ec_hint(o, "should_fallback", 1); return o;
    case VID_CONTENT_BLOCKED:
        o = ec_vobj("content_policy_blocked"); ec_hint(o, "retryable", 0); ec_hint(o, "should_fallback", 1); return o;
    case VID_GUARDRAIL_BLOCKED:
        o = ec_vobj("guardrail_blocked"); ec_hint(o, "retryable", 0); ec_hint(o, "should_fallback", 1); return o;
    case VID_MODEL_ENTITLEMENT:
        o = ec_vobj("model_entitlement"); ec_hint(o, "retryable", 0);
        ec_hint(o, "should_rotate_credential", 1); ec_hint(o, "should_fallback", 1); return o;
    case VID_FORMAT_ERROR:
        o = ec_vobj("format_error"); ec_hint(o, "retryable", 0); ec_hint(o, "should_fallback", 1); return o;
    case VID_POLICY_BLOCKED:
        o = ec_vobj("provider_policy_blocked"); ec_hint(o, "retryable", 0); ec_hint(o, "should_fallback", 1); return o;
    case VID_SSL_CERT:
        o = ec_vobj("ssl_cert_verification"); ec_hint(o, "retryable", 0); ec_hint(o, "should_fallback", 1); return o;
    case VID_CONTEXT_OVERFLOW:
        o = ec_vobj("context_overflow"); ec_hint(o, "should_compress", 1); return o;
    case VID_PAYLOAD_TOO_LARGE:
        o = ec_vobj("payload_too_large"); ec_hint(o, "should_compress", 1); return o;
    case VID_OVERLOADED: return ec_vobj("overloaded");
    case VID_SERVER_ERROR: return ec_vobj("server_error");
    case VID_TIMEOUT: return ec_vobj("timeout");
    case VID_UNKNOWN: return ec_vobj("unknown");
    case VID_INVALID_ENCRYPTED: return ec_vobj("invalid_encrypted_content");
    case VID_REASONING_MANDATORY:
        o = ec_vobj("reasoning_mandatory"); ec_hint(o, "should_compress", 0); ec_hint(o, "should_fallback", 0); return o;
    case VID_ROLE_ALTERNATION:
        o = ec_vobj("role_alternation"); ec_hint(o, "retryable", 0); ec_hint(o, "should_fallback", 1); return o;
    case VID_MALFORMED_TOOL_ARGS:
        o = ec_vobj("format_error"); ec_hint(o, "retryable", 0); ec_hint(o, "should_fallback", 0); return o;
    default: return ec_vobj("unknown");
    }
}

/* ---- _billing_hints ---- */
static json_t *ec_billing_hints(ec_io_t *io, const char *error_msg)
{
    (void)io;
    json_t *o = ec_billing_verdict();
    json_t *ctx = json_object();
    if (ec_any_in(error_msg, EC_UNVERIFIED_BILLING)) {
        json_set(ctx, "billing_unverified", json_bool(true));
        json_set(ctx, "possible_content_filter", json_bool(true));
    }
    json_set(o, "error_context", ctx);
    return o;
}

/* ---- _first_match ----
 * rules: json array of [patterns_array, verdict_spec]. verdict_spec is a
 * static verdict dict (reason string), {"__fn__":"billing_hints"} (real), or
 * {"__callable__":true} (first_match.verdict seam). Returns verdict or NULL. */
static json_t *ec_first_match_json(ec_io_t *io, const char *error_msg, const json_t *rules)
{
    if (!ec_is_dict(rules) && !(rules && rules->type == JSON_ARRAY)) return NULL;
    if (!rules || rules->type != JSON_ARRAY) return NULL;
    for (size_t i = 0; i < rules->c.count; i++) {
        json_t *rule = rules->c.items[i];
        if (!rule || rule->type != JSON_ARRAY || rule->c.count < 2) continue;
        json_t *pats = rule->c.items[0];
        json_t *verdict = rule->c.items[1];
        int hit = 0;
        if (pats && pats->type == JSON_ARRAY)
            for (size_t j = 0; j < pats->c.count; j++) {
                const char *p = ec_str(pats->c.items[j]);
                if (p && ec_has(error_msg, p)) { hit = 1; break; }
            }
        if (!hit) continue;
        const char *fn = ec_str(ec_get(verdict, "__fn__"));
        if (fn && strcmp(fn, "billing_hints") == 0)
            return ec_billing_hints(io, error_msg);
        json_t *callable = ec_get(verdict, "__callable__");
        if (callable && callable->type == JSON_BOOL && callable->bool_val) {
            json_t *a = ec_args();
            json_set(a, "error_msg", json_string(error_msg ? error_msg : ""));
            return ec_call(io, "first_match.verdict", a);
        }
        return json_copy(verdict);
    }
    return NULL;
}

/* static rule table form (composed flows) */
typedef struct { const char *const *pats; int vid; } ec_rule_t;

static json_t *ec_first_match_tbl(ec_io_t *io, const char *error_msg,
                                  const ec_rule_t *rules, int n)
{
    for (int i = 0; i < n; i++) {
        if (ec_any_in(error_msg, rules[i].pats)) {
            if (rules[i].vid == VID_BILLING_HINTS)
                return ec_billing_hints(io, error_msg);
            return ec_const_verdict(rules[i].vid);
        }
    }
    return NULL;
}

static const ec_rule_t EC_404_RULES[] = {
    /* the wall->billing head rule is checked directly in ec_status_404
     * (its pattern set is the composite EC_403_404_WALL) */
    {EC_PROVIDER_POLICY_BLOCKED, VID_POLICY_BLOCKED},
    {EC_MODEL_NOT_FOUND, VID_MODEL_NOT_FOUND},
};
static const ec_rule_t EC_OVERFLOW_5XX_RULES[] = {
    {EC_EMPTY_RESPONSE, VID_SERVER_ERROR},
    {EC_MEMORY_CEILING, VID_OVERLOADED},
    {EC_CONTEXT_OVERFLOW, VID_CONTEXT_OVERFLOW},
};
static const ec_rule_t EC_SSL_RULES[] = {
    {EC_SSL_CERT_VERIFY, VID_SSL_CERT},
    {EC_SSL_TRANSIENT, VID_TIMEOUT},
};

/* ---- pure predicates ---- */

/* PoP: ec_is_anthropic_thinking_rejection @ agent/error_classifier.py:_is_anthropic_thinking_rejection */
static json_t *ec_is_anthropic_thinking_rejection(ec_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const char *msg = ec_str(ec_get(in, "msg"));
    if (!msg) msg = "";
    int hit = ec_has(msg, "thinking") &&
        (ec_any_in(msg, EC_THINKING_MUTATION) ||
         (ec_has(msg, "redacted_thinking") && ec_has(msg, "invalid")));
    return json_bool(hit ? true : false);
}

/* PoP: ec_is_server_injected_param_rejection @ agent/error_classifier.py:_is_server_injected_param_rejection */
static json_t *ec_is_server_injected_param_rejection(ec_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const char *error_msg = ec_str(ec_get(in, "error_msg"));
    char *slug = ec_norm_slug(ec_get(in, "provider"));
    int result = 0;
    /* _SERVER_INJECTED_PARAM_SENDERS: prompt_cache_retention only */
    const char *param = "prompt_cache_retention";
    const char *const senders[] = {"meta", "muse", "msl", "model-api", "bedrock", "mantle", NULL};
    if (error_msg && error_msg[0] && ec_has(error_msg, param) &&
        ec_any_in(error_msg, EC_PARAM_REJECTION_WORDS)) {
        int sender_hit = 0;
        for (int i = 0; senders[i]; i++)
            if (ec_has(slug, senders[i])) { sender_hit = 1; break; }
        result = !sender_hit;
    }
    free(slug);
    return json_bool(result ? true : false);
}

/* PoP: ec_gemini_prompt_block_marker @ agent/error_classifier.py:_gemini_prompt_block_marker */
static json_t *ec_gemini_prompt_block_marker(ec_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *exc = ec_get(in, "exc");
    const char *kind = ec_str(ec_get(exc, "kind"));
    const char *code = ec_str(ec_get(ec_get(exc, "attrs"), "code"));
    int is_block = kind && strcmp(kind, "GeminiAPIError") == 0 &&
        code && strcmp(code, "gemini_prompt_blocked") == 0;
    return is_block ? json_bool(true) : NULL;
}
/* ---- cause-chain extraction cluster ---- */

#define EC_PICK_SEAM    0
#define EC_PICK_HEADERS 1
#define EC_PICK_GEMINI  2
#define EC_PICK_STATUS  3

/* response double helpers: a tagged {"__response__":true,...} object double
 * models an httpx-like response (attributes); a plain dict models botocore's
 * response mapping. */
static int ec_resp_is_obj(const json_t *response)
{
    return ec_is_dict(response) && json_has(response, "__response__");
}

/* _headers_of(exc): getattr(getattr(exc,"response",None),"headers",None);
 * return headers if headers and hasattr(headers,"get") else None. */
static json_t *ec_headers_of_node(ec_io_t *io, const json_t *exc)
{
    (void)io;
    const json_t *response = ec_get(ec_get(exc, "attrs"), "response");
    const json_t *headers = NULL;
    if (ec_resp_is_obj(response))
        headers = ec_get(response, "headers");
    /* a truthy dict has .get; anything else -> None */
    if (ec_is_dict(headers) && headers->c.count > 0)
        return json_copy(headers);
    return NULL;
}

/* _status_of(exc) */
static json_t *ec_status_of_node(ec_io_t *io, const json_t *exc)
{
    (void)io;
    const json_t *attrs = ec_get(exc, "attrs");
    const json_t *code = ec_get(attrs, "status_code");
    if (ec_is_int(code)) return json_copy(code);
    code = ec_get(attrs, "status");
    if (ec_is_int(code) && ec_num(code) >= 100 && ec_num(code) < 600)
        return json_copy(code);
    const json_t *response = ec_get(attrs, "response");
    const json_t *meta;
    if (ec_is_dict(response) && !ec_resp_is_obj(response)) {
        meta = ec_get(response, "ResponseMetadata");
        code = ec_is_dict(meta) ? ec_get(meta, "HTTPStatusCode") : NULL;
    } else {
        code = ec_present(response) ? ec_get(response, "status_code") : NULL;
    }
    if (ec_is_int(code) && ec_num(code) >= 100 && ec_num(code) < 600)
        return json_copy(code);
    return NULL;
}

/* _gemini_prompt_block_marker(exc) -> True or None */
static json_t *ec_gemini_marker_node(const json_t *exc)
{
    const char *kind = ec_str(ec_get(exc, "kind"));
    const char *code = ec_str(ec_get(ec_get(exc, "attrs"), "code"));
    int is_block = kind && strcmp(kind, "GeminiAPIError") == 0 &&
        code && strcmp(code, "gemini_prompt_blocked") == 0;
    return is_block ? json_bool(true) : NULL;
}

static json_t *ec_apply_pick(ec_io_t *io, const json_t *exc, int pick_id)
{
    switch (pick_id) {
    case EC_PICK_HEADERS: return ec_headers_of_node(io, exc);
    case EC_PICK_GEMINI:  return ec_gemini_marker_node(exc);
    case EC_PICK_STATUS:  return ec_status_of_node(io, exc);
    default: {
        json_t *a = ec_args();
        json_set(a, "exc", ec_proj_exc(exc));
        return ec_call(io, "pick", a);
    }
    }
}

/* _from_cause_chain(error, pick, default): first non-None pick over the
 * error + __cause__/__context__ chain (max 5). Returns value/raise/default. */
static json_t *ec_from_cause_chain(ec_io_t *io, const json_t *error,
                                   int pick_id, const json_t *def)
{
    const json_t *current = error;
    for (int depth = 0; depth < 5; depth++) {
        if (!current) break;
        json_t *found = ec_apply_pick(io, current, pick_id);
        if (ec_is_raise(found)) return found;
        if (ec_present(found)) return found;
        json_free(found);
        const json_t *attrs_c = current;  /* cause/context live on the spec */
        const json_t *cause = ec_get(attrs_c, "cause");
        if (!ec_present(cause)) cause = ec_get(attrs_c, "context");
        if (!ec_present(cause)) break;
        current = cause;
    }
    return def ? json_copy(def) : NULL;
}

/* _body_of(exc): body attr if dict; else response.json() (except Exception). */
static json_t *ec_body_of_node(ec_io_t *io, const json_t *exc)
{
    const json_t *attrs = ec_get(exc, "attrs");
    const json_t *body = ec_get(attrs, "body");
    if (ec_is_dict(body)) return json_copy(body);
    const json_t *response = ec_get(attrs, "response");
    if (!ec_present(response)) return NULL;
    if (!ec_resp_is_obj(response)) {
        /* a plain dict has no .json -> AttributeError caught -> None */
        return NULL;
    }
    json_t *r = ec_call(io, "response.json", ec_args());
    if (ec_is_raise(r)) {
        if (ec_is_base(ec_kind(r))) return r;   /* BaseException passes through */
        json_free(r);                            /* except Exception -> None */
        return NULL;
    }
    if (ec_is_dict(r)) return r;
    json_free(r);
    return NULL;
}

/* _status_code_from_body(body): numeric 400-599 from error.code/code. */
static json_t *ec_status_code_from_body_node(ec_io_t *io, const json_t *body)
{
    if (!ec_is_dict(body)) return NULL;
    json_t *a = ec_args();
    json_set(a, "body", json_copy(body));
    json_t *err = ec_call(io, "error_obj", a);
    if (ec_is_raise(err)) return err;
    static const char *keys[] = {"status_code", "status", "http_status", "code"};
    const json_t *cand = NULL;
    for (int i = 0; i < 4; i++) {
        cand = ec_get(err, keys[i]);
        if (ec_is_int(cand) && ec_num(cand) >= 400 && ec_num(cand) < 600) {
            json_t *out = json_copy(cand); json_free(err); return out;
        }
    }
    cand = ec_get(body, "code");
    if (ec_is_int(cand) && ec_num(cand) >= 400 && ec_num(cand) < 600) {
        json_t *out = json_copy(cand); json_free(err); return out;
    }
    json_free(err);
    return NULL;
}

/* _code_from_payload(payload, top_keys, peek_message) -> str.
 * error_obj is read DIRECTLY (payload.get("error", {})); only _json_dict is
 * seamed. Recursive for a nested JSON error.message. */
static json_t *ec_code_from_payload_node(ec_io_t *io, const json_t *payload,
                                         const char *const *top_keys, int nkeys,
                                         int peek_message)
{
    if (!ec_is_dict(payload)) return json_string("");
    const json_t *error_obj = ec_get(payload, "error");
    if (!error_obj) error_obj = NULL;  /* default {} behaves as empty dict */
    if (ec_is_dict(error_obj)) {
        const json_t *code = ec_get(error_obj, "code");
        if (!ec_truth(code)) code = ec_get(error_obj, "type");
        if (!ec_truth(code)) code = NULL;
        if (code && code->type != JSON_STRING) {
            const json_t *status = ec_get(error_obj, "status");
            code = ec_truth(status) ? status : code;
        }
        if (code && code->type == JSON_STRING) {
            char *s = ec_strip(code->str_val ? code->str_val : "");
            if (s && s[0] && strcmp(s, "400") != 0) {
                json_t *out = json_string(s); free(s); return out;
            }
            free(s);
        }
        const json_t *message = ec_get(error_obj, "message");
        const char *ms = ec_str(message);
        if (peek_message && ms) {
            char *mt = ec_strip(ms);
            int starts = mt && mt[0] == '{';
            free(mt);
            if (starts) {
                json_t *a = ec_args();
                json_set(a, "text", json_copy(message));
                json_t *inner = ec_call(io, "json_dict", a);
                if (ec_is_raise(inner)) return inner;
                static const char *nk[] = {"code", "error_code"};
                json_t *nested = ec_code_from_payload_node(io, inner, nk, 2, 0);
                json_free(inner);
                if (ec_is_raise(nested)) return nested;
                const char *ns = ec_str(nested);
                if (ns && ns[0]) return nested;
                json_free(nested);
            }
        }
    }
    /* top_keys fallback: first truthy payload.get(k) */
    const json_t *code = NULL;
    for (int i = 0; i < nkeys; i++) {
        const json_t *v = ec_get(payload, top_keys[i]);
        if (ec_truth(v)) { code = v; break; }
    }
    char *text = NULL;
    if (code && (code->type == JSON_STRING || code->type == JSON_NUMBER ||
                 code->type == JSON_BOOL)) {
        char *ps = ec_pystr(code);
        text = ec_strip(ps ? ps : "");
        free(ps);
    }
    const char *t = text ? text : "";
    json_t *out = json_string((t[0] && strcmp(t, "400") != 0) ? t : "");
    free(text);
    return out;
}

/* _body_message_candidates(body): the 5 yielded values as an array. */
static json_t *ec_body_message_candidates_node(ec_io_t *io, const json_t *body)
{
    json_t *a = ec_args();
    json_set(a, "body", json_copy(body));
    json_t *err = ec_call(io, "error_obj", a);
    if (ec_is_raise(err)) return err;
    json_t *arr = json_array();
    json_t *v;
    v = ec_get(err, "message");
    json_append(arr, v ? json_copy(v) : json_null());
    v = ec_get(body, "message");
    json_append(arr, v ? json_copy(v) : json_null());
    v = ec_get(body, "errorMessage");
    json_append(arr, v ? json_copy(v) : json_null());
    const json_t *args = ec_get(body, "errorArgs");
    v = ec_is_dict(args) ? ec_get(args, "reason") : NULL;
    json_append(arr, v ? json_copy(v) : json_null());
    const json_t *detail = ec_get(body, "detail");
    if (ec_is_dict(detail)) v = ec_get(detail, "message");
    else if (detail && detail->type == JSON_STRING) v = (json_t *)detail;
    else v = NULL;
    json_append(arr, v ? json_copy(v) : json_null());
    json_free(err);
    return arr;
}

/* _openrouter_wrapped_message(err_obj): lowercased inner message. */
static json_t *ec_openrouter_wrapped_message_node(ec_io_t *io, const json_t *err_obj)
{
    const json_t *metadata = ec_get(err_obj, "metadata");
    /* live: metadata = err_obj.get("metadata", {}) -- an ABSENT key defaults to
     * {} (a dict), so json_dict fires even with no metadata; a present non-dict
     * (incl. null) skips it. */
    int metadata_is_dict = ec_is_dict(metadata) || metadata == NULL;
    json_t *inner = NULL;
    if (metadata_is_dict) {
        const json_t *raw = ec_get(metadata, "raw");
        json_t *a = ec_args();
        json_set(a, "text", raw ? json_copy(raw) : json_null());
        inner = ec_call(io, "json_dict", a);
        if (ec_is_raise(inner)) return inner;
    }
    if (!ec_truth(inner)) {
        json_free(inner);
        return json_string("");
    }
    json_t *a = ec_args();
    json_set(a, "body", json_copy(inner));
    json_t *eo = ec_call(io, "error_obj", a);
    json_free(inner);
    if (ec_is_raise(eo)) return eo;
    const json_t *msg = ec_get(eo, "message");
    char *ps = ec_pystr_or_empty(msg);
    char *low = ec_lower(ps ? ps : "");
    free(ps);
    json_free(eo);
    json_t *out = json_string(low ? low : "");
    free(low);
    return out;
}

/* _build_error_msg(error, body): lowercased str(error) + body + wrapped. */
static json_t *ec_build_error_msg_node(ec_io_t *io, const json_t *error, const json_t *body)
{
    char *raw_e = ec_exc_str(error);
    char *raw_msg = ec_lower(raw_e ? raw_e : "");
    free(raw_e);
    char *body_msg = ec_xfmt("");
    char *metadata_msg = ec_xfmt("");
    if (ec_is_dict(body)) {
        json_t *a = ec_args();
        json_set(a, "body", json_copy(body));
        json_t *err_obj = ec_call(io, "error_obj", a);
        if (ec_is_raise(err_obj)) {
            free(raw_msg); free(body_msg); free(metadata_msg);
            return err_obj;
        }
        char *s1ps = ec_pystr_or_empty(ec_get(err_obj, "message"));
        char *s1 = ec_lower(s1ps ? s1ps : ""); free(s1ps);
        char *s2ps = ec_pystr_or_empty(ec_get(body, "message"));
        char *s2 = ec_lower(s2ps ? s2ps : ""); free(s2ps);
        free(body_msg);
        body_msg = (s1 && s1[0]) ? s1 : s2;
        if (s1 && s1[0]) free(s2); else free(s1);
        if (ec_truth(err_obj)) {
            json_t *mm = ec_openrouter_wrapped_message_node(io, err_obj);
            if (ec_is_raise(mm)) {
                json_free(err_obj); free(raw_msg); free(body_msg); free(metadata_msg);
                return mm;
            }
            free(metadata_msg);
            const char *ms = ec_str(mm);
            metadata_msg = ec_xfmt("%s", ms ? ms : "");
            json_free(mm);
        }
        json_free(err_obj);
    }
    /* parts join */
    size_t cap = strlen(raw_msg) + strlen(body_msg) + strlen(metadata_msg) + 3;
    char *out = (char *)malloc(cap);
    if (!out) { free(raw_msg); free(body_msg); free(metadata_msg); return NULL; }
    size_t k = 0;
    k += (size_t)snprintf(out + k, cap - k, "%s", raw_msg);
    if (body_msg[0] && !ec_has(raw_msg, body_msg))
        k += (size_t)snprintf(out + k, cap - k, " %s", body_msg);
    if (metadata_msg[0] && !ec_has(raw_msg, metadata_msg) && !ec_has(body_msg, metadata_msg))
        k += (size_t)snprintf(out + k, cap - k, " %s", metadata_msg);
    free(raw_msg); free(body_msg); free(metadata_msg);
    json_t *res = json_string(out);
    free(out);
    return res;
}

/* ---- _Ctx model ---- */

/* __post_init__: derive error_type/error_code/code/headers/slugs in place.
 * Returns NULL on success or a raise spec. */
static json_t *ec_ctx_post_init(ec_io_t *io, json_t *ctx)
{
    const json_t *error = ec_get(ctx, "error");
    const char *kind = ec_str(ec_get(error, "kind"));
    json_set(ctx, "error_type", json_string(kind ? kind : ""));
    json_t *a = ec_args();
    json_set(a, "body", json_copy(ec_get(ctx, "body")));
    json_t *ecode = ec_call(io, "extract_error_code", a);
    if (ec_is_raise(ecode)) return ecode;
    const char *ecs = ec_str(ecode);
    char *code = ec_lower(ecs ? ecs : "");
    json_set(ctx, "error_code", json_string(ecs ? ecs : ""));
    json_set(ctx, "code", json_string(code ? code : ""));
    free(code);
    json_free(ecode);
    json_t *def = json_object();
    json_t *headers = ec_from_cause_chain(io, error, EC_PICK_HEADERS, def);
    json_free(def);
    if (ec_is_raise(headers)) return headers;
    json_set(ctx, "headers", headers ? headers : json_null());
    char *pslug = ec_norm_slug(ec_get(ctx, "provider"));
    char *mslug = ec_norm_slug(ec_get(ctx, "model"));
    json_set(ctx, "provider_slug", json_string(pslug ? pslug : ""));
    json_set(ctx, "model_slug", json_string(mslug ? mslug : ""));
    free(pslug); free(mslug);
    return NULL;
}

/* build a ctx json from raw fixture fields; optionally run __post_init__. */
static json_t *ec_ctx_build(ec_io_t *io, const json_t *ctx_in, int run_post_init)
{
    json_t *ctx = json_object();
    static const char *fields[] = {"error", "status_code", "body", "msg", "provider",
        "model", "approx_tokens", "context_length", "num_messages", "base_url", "anonymous"};
    for (int i = 0; i < 11; i++) {
        json_t *v = ec_get(ctx_in, fields[i]);
        json_set(ctx, fields[i], v ? json_copy(v) : json_null());
    }
    if (run_post_init) {
        json_t *r = ec_ctx_post_init(io, ctx);
        if (ec_is_raise(r)) { json_free(ctx); return r; }
    }
    return ctx;
}

/* ---- extraction-cluster entries ---- */

/* PoP: ec_from_cause_chain_entry @ agent/error_classifier.py:_from_cause_chain */
static json_t *ec_from_cause_chain_entry(ec_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ec_from_cause_chain(io, ec_get(in, "error"), EC_PICK_SEAM, ec_get(in, "default"));
}

/* PoP: ec_status_of_entry @ agent/error_classifier.py:_status_of */
static json_t *ec_status_of_entry(ec_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ec_status_of_node(io, ec_get(in, "exc"));
}

/* PoP: ec_body_of_entry @ agent/error_classifier.py:_body_of */
static json_t *ec_body_of_entry(ec_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ec_body_of_node(io, ec_get(in, "exc"));
}

/* PoP: ec_headers_of_entry @ agent/error_classifier.py:_headers_of */
static json_t *ec_headers_of_entry(ec_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ec_headers_of_node(io, ec_get(in, "exc"));
}

/* PoP: ec_status_code_from_body_entry @ agent/error_classifier.py:_status_code_from_body */
static json_t *ec_status_code_from_body_entry(ec_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ec_status_code_from_body_node(io, ec_get(in, "body"));
}

/* PoP: ec_code_from_payload_entry @ agent/error_classifier.py:_code_from_payload */
static json_t *ec_code_from_payload_entry(ec_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *tk = ec_get(in, "top_keys");
    const char *keys[16];
    int nkeys = 0;
    if (tk && tk->type == JSON_ARRAY)
        for (size_t i = 0; i < tk->c.count && nkeys < 16; i++) {
            const char *s = ec_str(tk->c.items[i]);
            if (s) keys[nkeys++] = s;
        }
    int peek = ec_truth(ec_get(in, "peek_message"));
    return ec_code_from_payload_node(io, ec_get(in, "payload"), keys, nkeys, peek);
}

/* PoP: ec_body_message_candidates_entry @ agent/error_classifier.py:_body_message_candidates */
static json_t *ec_body_message_candidates_entry(ec_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ec_body_message_candidates_node(io, ec_get(in, "body"));
}

/* PoP: ec_openrouter_wrapped_message_entry @ agent/error_classifier.py:_openrouter_wrapped_message */
static json_t *ec_openrouter_wrapped_message_entry(ec_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ec_openrouter_wrapped_message_node(io, ec_get(in, "err_obj"));
}

/* PoP: ec_build_error_msg_entry @ agent/error_classifier.py:_build_error_msg */
static json_t *ec_build_error_msg_entry(ec_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ec_build_error_msg_node(io, ec_get(in, "error"), ec_get(in, "body"));
}

/* PoP: ec_ctx_post_init_entry @ agent/error_classifier.py:__post_init__ */
static json_t *ec_ctx_post_init_entry(ec_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *ctx = ec_ctx_build(io, ec_get(in, "ctx"), 0);
    if (ec_is_raise(ctx)) return ctx;
    json_t *r = ec_ctx_post_init(io, ctx);
    if (ec_is_raise(r)) { json_free(ctx); return r; }
    json_t *out = json_object();
    static const char *df[] = {"error_type", "error_code", "code", "headers",
                               "provider_slug", "model_slug"};
    for (int i = 0; i < 6; i++)
        json_set(out, df[i], json_copy(ec_get(ctx, df[i])));
    json_free(ctx);
    return out;
}

/* PoP: ec_large_session_entry @ agent/error_classifier.py:large_session */
static json_t *ec_large_session_entry(ec_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *ctx = ec_get(in, "ctx");
    double approx = ec_num(ec_get(ctx, "approx_tokens"));
    double clen = ec_num(ec_get(ctx, "context_length"));
    double nmsg = ec_num(ec_get(ctx, "num_messages"));
    double frac = ec_num(ec_get(in, "frac"));
    double tokens = ec_num(ec_get(in, "tokens"));
    double messages = ec_num(ec_get(in, "messages"));
    int hit = (approx > clen * frac) ||
        (clen <= 256000 && (approx > tokens || nmsg > messages));
    return json_bool(hit ? true : false);
}
/* ---- provider special cases + pipeline nodes ---- */

static const char *const EC_FAILover_VALUES[] = {
    "auth", "auth_permanent", "billing", "rate_limit", "upstream_rate_limit",
    "upstream_blocked", "overloaded", "server_error", "timeout", "ssl_cert_verification",
    "context_overflow", "payload_too_large", "image_too_large", "image_corrupt",
    "model_not_found", "provider_policy_blocked", "data_policy_denied",
    "content_policy_blocked", "guardrail_blocked", "model_entitlement",
    "incomplete_response", "format_error", "role_alternation", "invalid_encrypted_content",
    "multimodal_tool_content_unsupported", "reasoning_mandatory", "thinking_signature",
    "long_context_tier", "oauth_long_context_beta_forbidden", "llama_cpp_grammar_pattern",
    "unknown", NULL};

static int ec_valid_reason(const char *s)
{
    return ec_in_set(s, EC_FAILover_VALUES);
}

static int ec_status_is(const json_t *ctx, int code)
{
    const json_t *s = ec_get(ctx, "status_code");
    return ec_is_int(s) && ec_num(s) == code;
}

static int ec_large_session_calc(const json_t *ctx, double frac, double tokens, double messages)
{
    double approx = ec_num(ec_get(ctx, "approx_tokens"));
    double clen = ec_num(ec_get(ctx, "context_length"));
    double nmsg = ec_num(ec_get(ctx, "num_messages"));
    return (approx > clen * frac) ||
        (clen <= 256000 && (approx > tokens || nmsg > messages));
}

/* _is_gemini_prompt_block(error) */
static json_t *ec_is_gemini_prompt_block_node(ec_io_t *io, const json_t *error)
{
    json_t *def = json_bool(false);
    json_t *r = ec_from_cause_chain(io, error, EC_PICK_GEMINI, def);
    json_free(def);
    return r;
}

/* _is_codex_masked_replay_rejection(c) */
static json_t *ec_is_codex_masked_replay_node(ec_io_t *io, const json_t *ctx)
{
    const char *pslug = ec_str(ec_get(ctx, "provider_slug"));
    const json_t *status = ec_get(ctx, "status_code");
    int status_ok = !ec_present(status) || (ec_is_int(status) && ec_num(status) == 400);
    if (!pslug || strcmp(pslug, "openai-codex") != 0 || !status_ok)
        return json_bool(false);
    const json_t *body = ec_get(ctx, "body");
    if (!ec_is_dict(body)) body = NULL;
    const json_t *detail = ec_get(body, "detail");
    char *dps = ec_pystr_or_empty(detail);
    char *dt = ec_strip(dps ? dps : ""); free(dps);
    char *dl = ec_lower(dt ? dt : ""); free(dt);
    int detail_hit = dl && strcmp(dl, "unsupported content type") == 0;
    free(dl);
    const char *msg = ec_str(ec_get(ctx, "msg"));
    if (detail_hit || (!ec_truth(body) && ec_has(msg, "unsupported content type") && ec_has(msg, "detail")))
        return json_bool(true);
    json_t *cands = ec_body_message_candidates_node(io, body);
    if (ec_is_raise(cands)) return cands;
    char *body_msg = ec_xfmt("");
    if (cands && cands->type == JSON_ARRAY)
        for (size_t i = 0; i < cands->c.count; i++) {
            json_t *m = cands->c.items[i];
            if (ec_truth(m)) {
                char *ps = ec_pystr(m);
                char *st = ec_strip(ps ? ps : ""); free(ps);
                char *lw = ec_lower(st ? st : ""); free(st);
                free(body_msg); body_msg = lw;
                break;
            }
        }
    json_free(cands);
    const char *code = ec_str(ec_get(ctx, "code"));
    char *msg_strip = ec_strip(msg ? msg : "");
    int hit = (code && strcmp(code, "invalid_prompt") == 0 &&
               body_msg && strcmp(body_msg, "request blocked.") == 0) ||
              (msg_strip && strcmp(msg_strip, "invalid_prompt: request blocked.") == 0);
    free(body_msg); free(msg_strip);
    return json_bool(hit ? true : false);
}

/* _is_nous_route(c) */
static json_t *ec_is_nous_route_node(ec_io_t *io, const json_t *ctx)
{
    char *pslug = ec_norm_slug(ec_get(ctx, "provider"));
    int nous = pslug && strcmp(pslug, "nous") == 0;
    free(pslug);
    if (nous) return json_bool(true);
    const json_t *base_url = ec_get(ctx, "base_url");
    if (!ec_truth(base_url)) return json_bool(false);
    json_t *a = ec_args();
    json_set(a, "base_url", json_copy(base_url));
    json_set(a, "host", json_string("nousresearch.com"));
    return ec_call(io, "base_url_host_matches", a);
}

/* _nous_welcome_tier(c) */
static json_t *ec_nous_welcome_tier_node(ec_io_t *io, const json_t *ctx)
{
    const json_t *status = ec_get(ctx, "status_code");
    const char *msg = ec_str(ec_get(ctx, "msg"));
    const char *provider = ec_str(ec_get(ctx, "provider"));
    const json_t *base_url = ec_get(ctx, "base_url");
    int anonymous = ec_truth(ec_get(ctx, "anonymous"));
    json_t *a, *r;

    if (!anonymous) {
        if (provider && strcmp(provider, "nous") == 0 && ec_status_is(ctx, 400)) {
            a = ec_args();
            json_set(a, "status", json_copy(status));
            json_set(a, "msg", json_string(msg ? msg : ""));
            r = ec_call(io, "welcome_route_refusal", a);
            if (ec_is_raise(r)) return r;
            const char *rs = ec_str(r);
            int named = rs && strcmp(rs, "named_on_welcome_host") == 0;
            json_free(r);
            if (named) {
                json_t *o = ec_vobj("format_error");
                ec_hint(o, "retryable", 0); ec_hint(o, "should_fallback", 1);
                json_t *ec = json_object();
                json_set(ec, "welcome_route", json_string("named_on_welcome_host"));
                json_set(o, "error_context", ec);
                return o;
            }
        }
        return NULL;
    }
    if (ec_status_is(ctx, 402)) {
        a = ec_args();
        json_set(a, "base_url", json_copy(base_url));
        r = ec_call(io, "route_is_welcome_host", a);
        if (ec_is_raise(r)) return r;
        int welcome_host = ec_truth(r);
        json_free(r);
        if (welcome_host) {
            json_t *refusal = json_object();
            json_set(refusal, "reason", json_string("refused"));
            json_set(refusal, "retry_after", json_number(0));
            json_set(refusal, "alternates", json_array());
            json_set(refusal, "upgrade_url", json_string(""));
            json_t *o = ec_vobj("auth_permanent");
            ec_hint(o, "retryable", 0); ec_hint(o, "should_fallback", 1);
            json_t *ec = json_object();
            json_set(ec, "welcome_refusal", refusal);
            json_set(o, "error_context", ec);
            return o;
        }
    }
    if (ec_status_is(ctx, 429)) {
        a = ec_args();
        json_set(a, "body", json_copy(ec_get(ctx, "body")));
        json_t *refusal = ec_call(io, "parse_welcome_refusal", a);
        if (ec_is_raise(refusal)) return refusal;
        if (!ec_truth(refusal)) {
            json_free(refusal);
            a = ec_args();
            json_set(a, "error_msg", json_string(msg ? msg : ""));
            json_set(a, "body", json_copy(ec_get(ctx, "body")));
            json_set(a, "response_headers", json_copy(ec_get(ctx, "headers")));
            json_t *bw = ec_call(io, "rate_limit_reset_seconds", a);
            if (ec_is_raise(bw)) return bw;
            a = ec_args();
            json_set(a, "headers", json_copy(ec_get(ctx, "headers")));
            json_set(a, "body_wait", bw ? bw : json_null());
            refusal = ec_call(io, "welcome_refusal_from_headers", a);
            if (ec_is_raise(refusal)) return refusal;
        }
        json_t *ec = json_object();
        json_set(ec, "welcome_refusal", refusal ? json_copy(refusal) : json_null());
        const char *reason = ec_str(ec_get(refusal, "reason"));
        if (reason && (strcmp(reason, "model_not_free") == 0 || strcmp(reason, "feature_not_free") == 0)) {
            json_t *o = ec_vobj("model_not_found");
            ec_hint(o, "retryable", 0); ec_hint(o, "should_fallback", 1);
            json_set(o, "error_context", ec);
            json_free(refusal);
            return o;
        }
        double retry_after = ec_num(ec_get(refusal, "retry_after"));
        if (retry_after > 0) {
            json_t *t = ec_call(io, "time.time", ec_args());
            if (ec_is_raise(t)) { json_free(ec); json_free(refusal); return t; }
            json_set(ec, "reset_at", json_number(ec_num(t) + retry_after));
            json_free(t);
        }
        json_free(refusal);
        json_t *o = ec_vobj("rate_limit");
        ec_hint(o, "should_fallback", 1);
        json_set(o, "error_context", ec);
        return o;
    }
    int plain_403 = !ec_any_in(msg, EC_WELCOME_403_NAMED);
    a = ec_args();
    json_set(a, "status", json_copy(status));
    json_set(a, "msg", json_string(msg ? msg : ""));
    json_set(a, "base_url", plain_403 ? json_copy(base_url) : json_null());
    json_t *kind = ec_call(io, "welcome_route_refusal", a);
    if (ec_is_raise(kind)) return kind;
    if (!ec_present(kind)) { json_free(kind); return NULL; }
    json_t *ec = json_object();
    json_set(ec, "welcome_route", json_copy(kind));
    json_free(kind);
    json_t *o = ec_vobj(ec_status_is(ctx, 403) ? "auth_permanent" : "format_error");
    ec_hint(o, "retryable", 0); ec_hint(o, "should_fallback", 1);
    json_set(o, "error_context", ec);
    return o;
}

/* _provider_special_cases(c) */
static json_t *ec_provider_special_cases_node(ec_io_t *io, const json_t *ctx)
{
    const char *msg = ec_str(ec_get(ctx, "msg"));
    const char *code = ec_str(ec_get(ctx, "code"));
    const json_t *status = ec_get(ctx, "status_code");
    json_t *o;

    if (code && strcmp(code, "data_policy_denied") == 0) {
        o = ec_vobj("data_policy_denied");
        ec_hint(o, "retryable", 0); ec_hint(o, "should_fallback", 0);
        return o;
    }
    json_t *welcome = ec_nous_welcome_tier_node(io, ctx);
    if (ec_is_raise(welcome)) return welcome;
    if (welcome) return welcome;

    int content_hit = ec_any_in(msg, EC_CONTENT_POLICY_BLOCKED);
    if (!content_hit) {
        json_t *g = ec_is_gemini_prompt_block_node(io, ec_get(ctx, "error"));
        if (ec_is_raise(g)) return g;
        content_hit = ec_truth(g);
        json_free(g);
    }
    if (content_hit) return ec_const_verdict(VID_CONTENT_BLOCKED);
    if (ec_any_in(msg, EC_PROMPT_INJECTION_GUARDRAIL)) return ec_const_verdict(VID_GUARDRAIL_BLOCKED);
    int status_403_or_none = (ec_is_int(status) && ec_num(status) == 403) || !ec_present(status);
    if (ec_has(msg, "bad-credentials") && (status_403_or_none || ec_has(msg, "unauthenticated")))
        return ec_const_verdict(VID_AUTH_ROTATE);
    if (ec_any_in(msg, EC_ACCOUNT_POLICY_BLOCK)) return ec_const_verdict(VID_POLICY_BLOCKED);
    json_t *codex = ec_is_codex_masked_replay_node(io, ctx);
    if (ec_is_raise(codex)) return codex;
    int codex_hit = ec_truth(codex);
    json_free(codex);
    if (codex_hit) {
        o = ec_vobj("invalid_encrypted_content");
        ec_hint(o, "retryable", 0); ec_hint(o, "should_fallback", 1);
        return o;
    }
    if (ec_status_is(ctx, 400) &&
        ((code && strcmp(code, "thinking_signature_invalid") == 0) || ec_has(msg, "thinking_signature_invalid")))
        return ec_const_verdict(VID_INVALID_ENCRYPTED);
    if (ec_status_is(ctx, 400)) {
        json_t *tin = json_object();
        json_set(tin, "msg", json_string(msg ? msg : ""));
        json_t *tr = ec_is_anthropic_thinking_rejection(io, tin, NULL);
        json_free(tin);
        if (ec_truth(tr)) { json_free(tr); return ec_vobj("thinking_signature"); }
        json_free(tr);
    }
    if (ec_status_is(ctx, 429) && ec_has(msg, "extra usage") && ec_has(msg, "long context")) {
        o = ec_vobj("long_context_tier"); ec_hint(o, "should_compress", 1); return o;
    }
    if (ec_status_is(ctx, 400) && ec_has(msg, "long context beta") && ec_has(msg, "not yet available"))
        return ec_vobj("oauth_long_context_beta_forbidden");
    int grammar_hit = ec_has(msg, "error parsing grammar") || ec_has(msg, "json-schema-to-grammar") ||
        (ec_has(msg, "unable to generate parser") && ec_has(msg, "template")) ||
        (ec_has(msg, "invalid json schema") && ec_has(msg, "regex lookaround") && ec_has(msg, "not supported"));
    if (ec_status_is(ctx, 400) && grammar_hit && !ec_has(msg, "no user query found"))
        return ec_vobj("llama_cpp_grammar_pattern");
    if (ec_has(msg, "do not have an active grok subscription") ||
        (ec_has(msg, "out of available resources") && ec_has(msg, "grok")))
        return ec_const_verdict(VID_AUTH_FALLBACK);
    return NULL;
}

/* _moa_special_cases(c) */
static json_t *ec_moa_special_cases_node(ec_io_t *io, const json_t *ctx)
{
    (void)io;
    const char *pslug = ec_str(ec_get(ctx, "provider_slug"));
    const json_t *error = ec_get(ctx, "error");
    char *es = ec_exc_str(error);
    int moa_shape = pslug && strcmp(pslug, "moa") == 0 && ec_any_in(es, EC_MOA_SHAPE_BUGS);
    free(es);
    if (moa_shape) {
        json_t *o = ec_vobj("format_error"); ec_hint(o, "retryable", 0); return o;
    }
    const char *kind = ec_str(ec_get(error, "kind"));
    if (kind && strcmp(kind, "MoAPresetNotFoundError") == 0) {
        json_t *o = ec_vobj("model_not_found"); ec_hint(o, "retryable", 0); return o;
    }
    if (kind && strcmp(kind, "InvalidProviderResponseError") == 0) {
        json_t *o = ec_vobj("incomplete_response");
        ec_hint(o, "retryable", 0); ec_hint(o, "should_fallback", 1);
        return o;
    }
    return NULL;
}

/* _status_403(c) */
static json_t *ec_status_403_node(ec_io_t *io, const json_t *ctx)
{
    (void)io;
    const char *code = ec_str(ec_get(ctx, "code"));
    const char *msg = ec_str(ec_get(ctx, "msg"));
    const char *pslug = ec_str(ec_get(ctx, "provider_slug"));
    if (ec_in_set(code, EC_403_TRANSIENT_CODES)) return ec_const_verdict(VID_OVERLOADED);
    int xai_spend = pslug && strcmp(pslug, "xai-oauth") == 0 &&
        code && strcmp(code, "personal-team-blocked:spending-limit") == 0;
    if (xai_spend || ec_any_in(msg, EC_403_404_WALL)) return ec_billing_verdict();
    if (ec_any_in(msg, EC_UPSTREAM_BLOCKED)) return ec_const_verdict(VID_UPSTREAM_BLOCKED);
    return ec_const_verdict(VID_AUTH_FALLBACK);
}

/* _status_404(c) */
static json_t *ec_status_404_node(ec_io_t *io, const json_t *ctx)
{
    const char *code = ec_str(ec_get(ctx, "code"));
    const char *msg = ec_str(ec_get(ctx, "msg"));
    if (ec_in_set(code, EC_BILLING_CODES)) return ec_billing_verdict();
    if (ec_any_in(msg, EC_403_404_WALL)) return ec_billing_verdict();
    json_t *v = ec_first_match_tbl(io, msg, EC_404_RULES, 2);
    if (v) return v;
    json_t *a = ec_args();
    json_set(a, "model", json_string(ec_str(ec_get(ctx, "model_slug")) ? ec_str(ec_get(ctx, "model_slug")) : ""));
    json_set(a, "provider", json_string(ec_str(ec_get(ctx, "provider_slug")) ? ec_str(ec_get(ctx, "provider_slug")) : ""));
    json_t *r = ec_call(io, "model_id_missing_known_prefix", a);
    if (ec_is_raise(r)) return r;
    int hit = ec_truth(r);
    json_free(r);
    return ec_const_verdict(hit ? VID_MODEL_NOT_FOUND : VID_UNKNOWN);
}

/* _status_429(c) */
static json_t *ec_status_429_node(ec_io_t *io, const json_t *ctx)
{
    const char *code = ec_str(ec_get(ctx, "code"));
    const char *msg = ec_str(ec_get(ctx, "msg"));
    const json_t *body = ec_get(ctx, "body");
    const json_t *headers = ec_get(ctx, "headers");
    json_t *a, *r;

    if (ec_in_set(code, EC_BILLING_CODES)) return ec_billing_verdict();
    if (ec_any_in(msg, EC_OVERLOADED)) return ec_const_verdict(VID_OVERLOADED);

    a = ec_args();
    json_set(a, "body", json_copy(body));
    json_set(a, "provider", json_string(ec_str(ec_get(ctx, "provider_slug")) ? ec_str(ec_get(ctx, "provider_slug")) : ""));
    r = ec_call(io, "is_openrouter_upstream_error", a);
    if (ec_is_raise(r)) return r;
    int upstream = ec_truth(r);
    json_free(r);
    if (upstream) {
        a = ec_args();
        json_set(a, "body", json_copy(body));
        json_t *up = ec_call(io, "extract_upstream_provider_name", a);
        if (ec_is_raise(up)) return up;
        json_t *ec = json_object();
        if (ec_truth(up)) json_set(ec, "upstream_provider", json_copy(up));
        json_free(up);
        json_t *o = ec_vobj("upstream_rate_limit");
        ec_hint(o, "should_fallback", 1);
        json_set(o, "error_context", ec);
        return o;
    }
    a = ec_args();
    json_set(a, "error_msg", json_string(msg ? msg : ""));
    json_set(a, "body", json_copy(body));
    json_set(a, "response_headers", json_copy(headers));
    json_t *transient = ec_call(io, "has_usage_limit_transient_signal", a);
    if (ec_is_raise(transient)) return transient;
    int trans = ec_truth(transient);
    json_free(transient);
    if (!trans && ec_any_in(msg, EC_LONG_QUOTA)) return ec_billing_verdict();
    int quota_wall = (code && strcmp(code, "usage_limit_reached") == 0) ||
        ec_has(msg, "usage_limit_reached") || ec_any_in(msg, EC_USAGE_LIMIT) || ec_any_in(msg, EC_ACCOUNT_WALL);
    int explicit_rl = ec_any_in(msg, EC_RATE_LIMIT);
    if (quota_wall && !explicit_rl && !trans) return ec_billing_verdict();
    a = ec_args();
    json_set(a, "error_msg", json_string(msg ? msg : ""));
    json_set(a, "body", json_copy(body));
    json_set(a, "response_headers", json_copy(headers));
    json_t *reset = ec_call(io, "rate_limit_reset_seconds", a);
    if (ec_is_raise(reset)) return reset;
    if (ec_truth(reset)) {
        json_t *t = ec_call(io, "time.time", ec_args());
        if (ec_is_raise(t)) { json_free(reset); return t; }
        json_t *ec = json_object();
        json_set(ec, "reset_at", json_number(ec_num(t) + ec_num(reset)));
        json_free(t); json_free(reset);
        json_t *o = ec_vobj("rate_limit");
        ec_hint(o, "should_rotate_credential", 1); ec_hint(o, "should_fallback", 1);
        json_set(o, "error_context", ec);
        return o;
    }
    json_free(reset);
    return ec_const_verdict(VID_RATE_LIMIT);
}

/* _status_5xx(c) */
static json_t *ec_status_5xx_node(ec_io_t *io, const json_t *ctx)
{
    const char *code = ec_str(ec_get(ctx, "code"));
    const char *msg = ec_str(ec_get(ctx, "msg"));
    int validation = ec_any_in(msg, EC_REQUEST_VALIDATION) || ec_in_set(code, EC_5XX_VALIDATION_CODES);
    if (validation) {
        json_t *tin = json_object();
        json_set(tin, "error_msg", json_string(msg ? msg : ""));
        json_set(tin, "provider", json_string(ec_str(ec_get(ctx, "provider_slug")) ? ec_str(ec_get(ctx, "provider_slug")) : ""));
        json_t *inj = ec_is_server_injected_param_rejection(io, tin, NULL);
        json_free(tin);
        int injected = ec_truth(inj);
        json_free(inj);
        if (!injected) return ec_const_verdict(VID_FORMAT_ERROR);
    }
    json_t *v = ec_first_match_tbl(io, msg, EC_OVERFLOW_5XX_RULES, 3);
    return v ? v : ec_const_verdict(VID_SERVER_ERROR);
}

/* _by_status(c) */
static json_t *ec_by_status_node(ec_io_t *io, const json_t *ctx)
{
    const json_t *status = ec_get(ctx, "status_code");
    if (!ec_present(status)) return NULL;
    if (!ec_is_int(status)) return NULL;
    int s = (int)ec_num(status);
    json_t *a, *r;
    switch (s) {
    case 400:
        a = ec_args(); json_set(a, "ctx", ec_proj_ctx(ctx));
        return ec_call(io, "classify_400", a);
    case 401: return ec_const_verdict(VID_AUTH_ROTATE);
    case 402: {
        json_t *nous = ec_is_nous_route_node(io, ctx);
        if (ec_is_raise(nous)) return nous;
        int is_nous = ec_truth(nous);
        json_free(nous);
        const char *code = ec_str(ec_get(ctx, "code"));
        a = ec_args();
        json_set(a, "error_msg", json_string(ec_str(ec_get(ctx, "msg")) ? ec_str(ec_get(ctx, "msg")) : ""));
        json_set(a, "code", json_string(is_nous && code ? code : ""));
        json_set(a, "body", json_copy(ec_get(ctx, "body")));
        return ec_call(io, "classify_402", a);
    }
    case 403: return ec_status_403_node(io, ctx);
    case 404: return ec_status_404_node(io, ctx);
    case 408: return ec_const_verdict(VID_TIMEOUT);
    case 413: return ec_const_verdict(VID_PAYLOAD_TOO_LARGE);
    case 422:
        a = ec_args(); json_set(a, "ctx", ec_proj_ctx(ctx));
        return ec_call(io, "classify_image_tool_422", a);
    case 429: return ec_status_429_node(io, ctx);
    case 500: case 502: return ec_status_5xx_node(io, ctx);
    case 503: case 529: {
        json_t *v = ec_first_match_tbl(io, ec_str(ec_get(ctx, "msg")), EC_OVERFLOW_5XX_RULES, 3);
        return v ? v : ec_const_verdict(VID_OVERLOADED);
    }
    default:
        if (s >= 400 && s < 500) return ec_const_verdict(VID_FORMAT_ERROR);
        if (s >= 500 && s < 600) return ec_const_verdict(VID_SERVER_ERROR);
        return NULL;
    }
}

/* _by_error_code(c) */
static json_t *ec_by_error_code_node(ec_io_t *io, const json_t *ctx)
{
    (void)io;
    const char *code = ec_str(ec_get(ctx, "code"));
    const char *msg = ec_str(ec_get(ctx, "msg"));
    const char *pslug = ec_str(ec_get(ctx, "provider_slug"));
    if (!code) code = "";
    if (strcmp(code, "provider_stream_non_json_data") == 0 && ec_has(msg, "request validation failed:"))
        return ec_const_verdict(VID_FORMAT_ERROR);
    if (strcmp(code, "resource_exhausted") == 0 || strcmp(code, "throttled") == 0 ||
        strcmp(code, "rate_limit_exceeded") == 0) {
        json_t *o = ec_vobj("rate_limit"); ec_hint(o, "should_rotate_credential", 1); return o;
    }
    if (ec_in_set(code, EC_BILLING_CODES)) return ec_billing_verdict();
    if (strcmp(code, "model_not_found") == 0 || strcmp(code, "model_not_available") == 0 ||
        strcmp(code, "invalid_model") == 0) return ec_const_verdict(VID_MODEL_NOT_FOUND);
    if (strcmp(code, "context_length_exceeded") == 0 || strcmp(code, "max_tokens_exceeded") == 0)
        return ec_const_verdict(VID_CONTEXT_OVERFLOW);
    if (ec_in_set(code, EC_MEMORY_CODES)) return ec_const_verdict(VID_OVERLOADED);
    if (strcmp(code, "invalid_encrypted_content") == 0) return ec_const_verdict(VID_INVALID_ENCRYPTED);
    /* provider family */
    const char *family = pslug ? pslug : "";
    if (pslug) {
        if (strcmp(pslug, "openai-codex") == 0) family = "openai";
        else if (strcmp(pslug, "google") == 0 || strcmp(pslug, "google-gemini") == 0 ||
                 strcmp(pslug, "google-ai-studio") == 0 || strcmp(pslug, "vertex") == 0 ||
                 strcmp(pslug, "google-vertex") == 0) family = "gemini";
    }
    if (strcmp(family, "openai") == 0 && strcmp(code, "server_error") == 0)
        return ec_const_verdict(VID_SERVER_ERROR);
    if (strcmp(family, "gemini") == 0) {
        if (strcmp(code, "unavailable") == 0) return ec_const_verdict(VID_OVERLOADED);
        if (strcmp(code, "deadline_exceeded") == 0) return ec_const_verdict(VID_TIMEOUT);
        if (strcmp(code, "internal") == 0) return ec_const_verdict(VID_SERVER_ERROR);
    }
    if (strcmp(family, "anthropic") == 0) {
        if (strcmp(code, "api_error") == 0) return ec_const_verdict(VID_SERVER_ERROR);
        if (strcmp(code, "rate_limit_error") == 0) return ec_const_verdict(VID_RATE_LIMIT);
    }
    return NULL;
}

/* _by_transport(c) */
static json_t *ec_by_transport_node(ec_io_t *io, const json_t *ctx)
{
    const char *msg = ec_str(ec_get(ctx, "msg"));
    const json_t *status = ec_get(ctx, "status_code");
    const char *error_type = ec_str(ec_get(ctx, "error_type"));
    const char *kind = ec_str(ec_get(ec_get(ctx, "error"), "kind"));
    json_t *ssl = ec_first_match_tbl(io, msg, EC_SSL_RULES, 2);
    if (ssl) return ssl;
    if (ec_any_in(msg, EC_SERVER_DISCONNECT) && !ec_truth(status)) {
        json_t *a = ec_args();
        json_set(a, "model", json_string(ec_str(ec_get(ctx, "model")) ? ec_str(ec_get(ctx, "model")) : ""));
        json_t *floor = ec_call(io, "get_reasoning_stale_timeout_floor", a);
        if (ec_is_raise(floor)) return floor;
        int has_floor = ec_present(floor);
        json_free(floor);
        if (has_floor) return ec_const_verdict(VID_TIMEOUT);
        return ec_const_verdict(ec_large_session_calc(ctx, 0.6, 120000, 200)
                                ? VID_CONTEXT_OVERFLOW : VID_TIMEOUT);
    }
    if (ec_any_in(msg, EC_PREMATURE_CLOSE) && !ec_truth(status))
        return ec_const_verdict(VID_TIMEOUT);
    if (error_type && strcmp(error_type, "RuntimeError") == 0 &&
        ec_has(msg, "consecutive stale attempts") && ec_has(msg, "aborting this call")) {
        json_t *o = ec_vobj("timeout"); ec_hint(o, "retryable", 0); ec_hint(o, "should_fallback", 1); return o;
    }
    int transport = ec_in_set(error_type, EC_TRANSPORT_TYPES) ||
        ec_any_in(error_type, EC_TRANSPORT_MARKERS) ||
        (kind && ec_exc_is_instance(kind, "OSError"));
    return transport ? ec_const_verdict(VID_TIMEOUT) : NULL;
}

/* _shape_context(c, verdict, hooked) */
static json_t *ec_shape_context_node(ec_io_t *io, const json_t *ctx,
                                     const json_t *verdict, int hooked)
{
    json_t *extra = json_object();
    const char *reason = ec_str(ec_get(verdict, "reason"));
    const char *msg = ec_str(ec_get(ctx, "msg"));
    const char *error_type = ec_str(ec_get(ctx, "error_type"));
    const char *kind = ec_str(ec_get(ec_get(ctx, "error"), "kind"));
    const json_t *status = ec_get(ctx, "status_code");
    int is_timeout_exc = kind && ec_exc_is_instance(kind, "TimeoutError");
    if (is_timeout_exc || ec_has(error_type, "Timeout") || ec_has(msg, "timed out")) {
        json_set(extra, "transport", json_string("timeout"));
    } else {
        int disc = ec_any_in(msg, EC_SERVER_DISCONNECT) || ec_any_in(msg, EC_PREMATURE_CLOSE) ||
            ec_any_in(msg, EC_CONNECTION_MESSAGE);
        if ((reason && strcmp(reason, "timeout") == 0 && !ec_truth(status)) || disc)
            json_set(extra, "transport", json_string("connection"));
    }
    json_t *st = ec_from_cause_chain(io, ec_get(ctx, "error"), EC_PICK_STATUS, NULL);
    if (ec_is_raise(st)) { json_free(extra); return st; }
    const json_t *body = NULL;
    if (!ec_present(st)) body = ec_get(ec_get(ec_get(ctx, "error"), "attrs"), "body");
    json_free(st);
    const json_t *err = body;
    if (ec_is_dict(body) && json_has(body, "error")) err = ec_get(body, "error");
    int structured = 0;
    if (err && err->type == JSON_STRING) {
        char *t = ec_strip(err->str_val ? err->str_val : "");
        structured = t && t[0];
        free(t);
    } else if (ec_is_dict(err)) {
        structured = ec_truth(ec_get(err, "type")) || ec_truth(ec_get(err, "code")) ||
            ec_truth(ec_get(err, "message"));
    }
    if (structured) json_set(extra, "statusless_structured", json_bool(true));
    if (hooked) {
        const json_t *sf = ec_get(verdict, "should_fallback");
        if (sf && sf->type == JSON_BOOL && !sf->bool_val)
            json_set(extra, "hook_declines_fallback", json_bool(true));
        return extra;
    }
    int status_in_quota = !ec_present(status) ||
        (ec_is_int(status) && (ec_num(status) == 402 || ec_num(status) == 403 ||
                               ec_num(status) == 404 || ec_num(status) == 429));
    int policy_refusal = reason && (strcmp(reason, "data_policy_denied") == 0 ||
        strcmp(reason, "content_policy_blocked") == 0 || strcmp(reason, "guardrail_blocked") == 0);
    if (status_in_quota && !(reason && strcmp(reason, "upstream_rate_limit") == 0) &&
        !policy_refusal && ec_any_in(msg, EC_QUOTA_WINDOW))
        json_set(extra, "quota_exhausted", json_bool(true));
    int status_in_route = !ec_present(status) || (ec_is_int(status) && ec_num(status) == 400);
    int reason_fr = reason && (strcmp(reason, "format_error") == 0 || strcmp(reason, "unknown") == 0);
    if (status_in_route && reason_fr && ec_any_in(msg, EC_ROUTE_MODEL_REJECTION) &&
        !ec_any_in(msg, EC_WALL_WORDS))
        json_set(extra, "route_rejects_model", json_bool(true));
    return extra;
}

/* _off_route_host(c) */
static json_t *ec_off_route_host_node(ec_io_t *io, const json_t *ctx)
{
    json_t *a = ec_args();
    json_set(a, "url", json_copy(ec_get(ctx, "base_url")));
    json_t *host = ec_call(io, "base_url_hostname", a);
    if (ec_is_raise(host)) return host;
    if (!ec_truth(host)) { json_free(host); return json_string(""); }
    a = ec_args();
    json_set(a, "provider_slug", json_string(ec_str(ec_get(ctx, "provider_slug")) ? ec_str(ec_get(ctx, "provider_slug")) : ""));
    json_set(a, "base_url", json_copy(ec_get(ctx, "base_url")));
    json_t *owns = ec_call(io, "provider_owns_route", a);
    if (ec_is_raise(owns)) { json_free(host); return owns; }
    int owns_true = owns && owns->type == JSON_BOOL && owns->bool_val;
    json_free(owns);
    if (owns_true) { json_free(host); return json_string(""); }
    return host;
}

/* _plugin_verdict(c) */
static json_t *ec_plugin_verdict_node(ec_io_t *io, const json_t *ctx)
{
    json_t *a = ec_args();
    json_set(a, "provider", json_copy(ec_get(ctx, "provider")));
    json_set(a, "model", json_copy(ec_get(ctx, "model")));
    json_set(a, "status_code", json_copy(ec_get(ctx, "status_code")));
    json_set(a, "error_type", json_copy(ec_get(ctx, "error_type")));
    json_set(a, "error_code", json_copy(ec_get(ctx, "error_code")));
    json_set(a, "error_message", json_copy(ec_get(ctx, "msg")));
    json_set(a, "error_body", json_copy(ec_get(ctx, "body")));
    json_set(a, "error", ec_proj_exc(ec_get(ctx, "error")));
    json_set(a, "approx_tokens", json_copy(ec_get(ctx, "approx_tokens")));
    json_set(a, "context_length", json_copy(ec_get(ctx, "context_length")));
    json_set(a, "num_messages", json_copy(ec_get(ctx, "num_messages")));
    json_t *verdict = ec_call(io, "get_plugin_error_classification", a);
    if (ec_is_raise(verdict)) {
        if (ec_is_base(ec_kind(verdict))) return verdict;
        json_t *la = json_array();
        json_append(la, json_string(ec_str(ec_get(verdict, "message")) ? ec_str(ec_get(verdict, "message")) : ""));
        json_t *lr = ec_log(io, "logger.debug", "Plugin error classification unavailable: %s", la);
        json_free(verdict);
        if (ec_is_raise(lr)) return lr;
        json_free(lr);
        return NULL;
    }
    if (ec_present(verdict)) {
        const char *reason = ec_str(ec_get(verdict, "reason"));
        json_t *la = json_array();
        json_append(la, json_string(reason ? reason : ""));
        json_append(la, json_copy(ec_get(ctx, "provider")));
        json_append(la, json_copy(ec_get(ctx, "status_code")));
        json_t *lr = ec_log(io, "logger.info",
            "API error classified by plugin hook: %s (provider=%s, status=%s)", la);
        if (ec_is_raise(lr)) { json_free(verdict); return lr; }
        json_free(lr);
    }
    return verdict;
}

/* _profile_verdict(c) */
static json_t *ec_profile_verdict_node(ec_io_t *io, const json_t *ctx)
{
    const char *pslug = ec_str(ec_get(ctx, "provider_slug"));
    if (!pslug || !pslug[0]) return NULL;
    json_t *a = ec_args();
    json_set(a, "provider_slug", json_string(pslug));
    json_t *profile = ec_call(io, "get_provider_profile", a);
    json_t *result = NULL;
    int failed = 0;
    char *fail_msg = NULL;
    if (ec_is_raise(profile)) {
        if (ec_is_base(ec_kind(profile))) return profile;
        failed = 1; fail_msg = ec_xfmt("%s", ec_str(ec_get(profile, "message")) ? ec_str(ec_get(profile, "message")) : "");
        json_free(profile); profile = NULL;
    }
    if (!failed) {
        int callable = ec_truth(ec_get(profile, "classify_api_error"));
        if (!callable) { json_free(profile); return NULL; }
        a = ec_args();
        json_set(a, "error", ec_proj_exc(ec_get(ctx, "error")));
        json_set(a, "status_code", json_copy(ec_get(ctx, "status_code")));
        json_set(a, "error_code", json_copy(ec_get(ctx, "error_code")));
        json_set(a, "message", json_copy(ec_get(ctx, "msg")));
        json_set(a, "body", json_copy(ec_get(ctx, "body")));
        json_set(a, "model", json_copy(ec_get(ctx, "model")));
        result = ec_call(io, "profile.classify_api_error", a);
        json_free(profile);
        if (ec_is_raise(result)) {
            if (ec_is_base(ec_kind(result))) return result;
            failed = 1; fail_msg = ec_xfmt("%s", ec_str(ec_get(result, "message")) ? ec_str(ec_get(result, "message")) : "");
            json_free(result); result = NULL;
        }
    }
    if (failed) {
        json_t *la = json_array();
        json_append(la, json_string(pslug));
        json_append(la, json_string(fail_msg ? fail_msg : ""));
        free(fail_msg);
        json_t *lr = ec_log(io, "logger.debug",
            "Provider profile error classification failed for %s: %s", la);
        if (ec_is_raise(lr)) return lr;
        json_free(lr);
        return NULL;
    }
    if (!ec_is_dict(result)) { json_free(result); return NULL; }
    const json_t *reason_node = ec_get(result, "reason");
    const char *reason = ec_str(reason_node);
    char *reason_norm = NULL;
    if (reason) {
        char *t = ec_strip(reason);
        reason_norm = ec_lower(t ? t : "");
        free(t);
        if (!ec_valid_reason(reason_norm)) { free(reason_norm); json_free(result); return NULL; }
    } else {
        json_free(result); return NULL;
    }
    /* hints */
    static const char *flags[] = {"retryable", "should_compress", "should_rotate_credential", "should_fallback"};
    json_t *hints = json_object();
    for (int i = 0; i < 4; i++)
        if (json_has(result, flags[i]))
            json_set(hints, flags[i], json_bool(ec_truth(ec_get(result, flags[i])) ? true : false));
    const json_t *sf = ec_get(hints, "should_fallback");
    int sf_true = sf && sf->type == JSON_BOOL && sf->bool_val;
    int retryable_client = strcmp(reason_norm, "rate_limit") == 0 ||
        strcmp(reason_norm, "upstream_rate_limit") == 0 || strcmp(reason_norm, "overloaded") == 0 ||
        strcmp(reason_norm, "context_overflow") == 0 || strcmp(reason_norm, "payload_too_large") == 0 ||
        strcmp(reason_norm, "long_context_tier") == 0 || strcmp(reason_norm, "thinking_signature") == 0;
    if (sf_true && !json_has(hints, "retryable") && !retryable_client)
        json_set(hints, "retryable", json_bool(false));
    json_t *verdict = ec_v(reason_norm, hints);
    json_free(hints);
    const json_t *rctx = ec_get(result, "error_context");
    if (ec_is_dict(rctx)) json_set(verdict, "error_context", json_copy(rctx));
    json_free(result);
    json_t *la = json_array();
    json_append(la, json_string(reason_norm));
    json_append(la, json_copy(ec_get(ctx, "provider")));
    json_append(la, json_copy(ec_get(ctx, "status_code")));
    json_t *lr = ec_log(io, "logger.info",
        "API error classified by provider profile: %s (provider=%s, status=%s)", la);
    free(reason_norm);
    if (ec_is_raise(lr)) { json_free(verdict); return lr; }
    json_free(lr);
    return verdict;
}
/* ---- verdict-helper entries ---- */

/* PoP: ec_v_entry @ agent/error_classifier.py:_v */
static json_t *ec_v_entry(ec_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const char *reason = ec_str(ec_get(in, "reason"));
    return ec_v(reason ? reason : "unknown", ec_get(in, "hints"));
}

/* PoP: ec_first_match_entry @ agent/error_classifier.py:_first_match */
static json_t *ec_first_match_entry(ec_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ec_first_match_json(io, ec_str(ec_get(in, "error_msg")), ec_get(in, "rules"));
}

/* PoP: ec_billing_hints_entry @ agent/error_classifier.py:_billing_hints */
static json_t *ec_billing_hints_entry(ec_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ec_billing_hints(io, ec_str(ec_get(in, "error_msg")));
}

/* PoP: ec_is_gemini_prompt_block_entry @ agent/error_classifier.py:_is_gemini_prompt_block */
static json_t *ec_is_gemini_prompt_block_entry(ec_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return ec_is_gemini_prompt_block_node(io, ec_get(in, "error"));
}

/* ---- ctx-driven entries (build ctx + run __post_init__, then the node) ---- */

static json_t *ec_ctx_wrap(ec_io_t *io, const json_t *in, json_t *st,
                           json_t *(*node)(ec_io_t *, const json_t *))
{
    (void)st;
    json_t *ctx = ec_ctx_build(io, ec_get(in, "ctx"), 1);
    if (ec_is_raise(ctx)) return ctx;
    json_t *ret = node(io, ctx);
    json_free(ctx);
    return ret;
}

/* PoP: ec_moa_special_cases_entry @ agent/error_classifier.py:_moa_special_cases */
static json_t *ec_moa_special_cases_entry(ec_io_t *io, const json_t *in, json_t *st)
{ return ec_ctx_wrap(io, in, st, ec_moa_special_cases_node); }

/* PoP: ec_nous_welcome_tier_entry @ agent/error_classifier.py:_nous_welcome_tier */
static json_t *ec_nous_welcome_tier_entry(ec_io_t *io, const json_t *in, json_t *st)
{ return ec_ctx_wrap(io, in, st, ec_nous_welcome_tier_node); }

/* PoP: ec_provider_special_cases_entry @ agent/error_classifier.py:_provider_special_cases */
static json_t *ec_provider_special_cases_entry(ec_io_t *io, const json_t *in, json_t *st)
{ return ec_ctx_wrap(io, in, st, ec_provider_special_cases_node); }

/* PoP: ec_is_codex_masked_replay_rejection_entry @ agent/error_classifier.py:_is_codex_masked_replay_rejection */
static json_t *ec_is_codex_masked_replay_rejection_entry(ec_io_t *io, const json_t *in, json_t *st)
{ return ec_ctx_wrap(io, in, st, ec_is_codex_masked_replay_node); }

/* PoP: ec_status_403_entry @ agent/error_classifier.py:_status_403 */
static json_t *ec_status_403_entry(ec_io_t *io, const json_t *in, json_t *st)
{ return ec_ctx_wrap(io, in, st, ec_status_403_node); }

/* PoP: ec_status_404_entry @ agent/error_classifier.py:_status_404 */
static json_t *ec_status_404_entry(ec_io_t *io, const json_t *in, json_t *st)
{ return ec_ctx_wrap(io, in, st, ec_status_404_node); }

/* PoP: ec_status_429_entry @ agent/error_classifier.py:_status_429 */
static json_t *ec_status_429_entry(ec_io_t *io, const json_t *in, json_t *st)
{ return ec_ctx_wrap(io, in, st, ec_status_429_node); }

/* PoP: ec_status_5xx_entry @ agent/error_classifier.py:_status_5xx */
static json_t *ec_status_5xx_entry(ec_io_t *io, const json_t *in, json_t *st)
{ return ec_ctx_wrap(io, in, st, ec_status_5xx_node); }

/* PoP: ec_is_nous_route_entry @ agent/error_classifier.py:_is_nous_route */
static json_t *ec_is_nous_route_entry(ec_io_t *io, const json_t *in, json_t *st)
{ return ec_ctx_wrap(io, in, st, ec_is_nous_route_node); }

/* PoP: ec_by_status_entry @ agent/error_classifier.py:_by_status */
static json_t *ec_by_status_entry(ec_io_t *io, const json_t *in, json_t *st)
{ return ec_ctx_wrap(io, in, st, ec_by_status_node); }

/* PoP: ec_by_error_code_entry @ agent/error_classifier.py:_by_error_code */
static json_t *ec_by_error_code_entry(ec_io_t *io, const json_t *in, json_t *st)
{ return ec_ctx_wrap(io, in, st, ec_by_error_code_node); }

/* PoP: ec_by_transport_entry @ agent/error_classifier.py:_by_transport */
static json_t *ec_by_transport_entry(ec_io_t *io, const json_t *in, json_t *st)
{ return ec_ctx_wrap(io, in, st, ec_by_transport_node); }

/* PoP: ec_off_route_host_entry @ agent/error_classifier.py:_off_route_host */
static json_t *ec_off_route_host_entry(ec_io_t *io, const json_t *in, json_t *st)
{ return ec_ctx_wrap(io, in, st, ec_off_route_host_node); }

/* PoP: ec_plugin_verdict_entry @ agent/error_classifier.py:_plugin_verdict */
static json_t *ec_plugin_verdict_entry(ec_io_t *io, const json_t *in, json_t *st)
{ return ec_ctx_wrap(io, in, st, ec_plugin_verdict_node); }

/* PoP: ec_profile_verdict_entry @ agent/error_classifier.py:_profile_verdict */
static json_t *ec_profile_verdict_entry(ec_io_t *io, const json_t *in, json_t *st)
{ return ec_ctx_wrap(io, in, st, ec_profile_verdict_node); }

/* PoP: ec_shape_context_entry @ agent/error_classifier.py:_shape_context */
static json_t *ec_shape_context_entry(ec_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *ctx = ec_ctx_build(io, ec_get(in, "ctx"), 1);
    if (ec_is_raise(ctx)) return ctx;
    int hooked = ec_truth(ec_get(in, "hooked"));
    json_t *ret = ec_shape_context_node(io, ctx, ec_get(in, "verdict"), hooked);
    json_free(ctx);
    return ret;
}