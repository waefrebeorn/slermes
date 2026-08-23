/*
 * error_surface.c — Faithful C11 port of agent/error_surface.py.
 *
 * Maps the internal failure taxonomy (agent.error_classifier.FailoverReason
 * values carried in turn results as ``failure_reason``) onto a small, stable
 * wire descriptor: {"layer","code","retryable"[,"provider","model"]}.
 * Intentionally dependency-light and NEVER raises.
 */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"
#include "error_classifier.h"
#include "port_hermes_state.h"
#include "error_surface.h"

/* failure_reason (FailoverReason.value) → UI layer. Unlisted reasons fall
 * back to LAYER_PROVIDER (every FailoverReason classifies a provider call). */
static const struct { const char *reason; const char *layer; } REASON_TO_LAYER[] = {
    { "auth",              ES_LAYER_AUTH },
    { "auth_permanent",    ES_LAYER_AUTH },
    { "billing",           ES_LAYER_BILLING },
    { "billing_unverified",ES_LAYER_BILLING },
};

/* Transport-ish reasons: failure is between us and the base_url. */
static const char *const TRANSPORT_REASONS[] = { "timeout", "ssl_cert_verification" };

/* Deterministic-for-the-request reasons — a bare "Retry" repeats the failure. */
static const char *const NON_RETRYABLE_REASONS[] = {
    "auth", "auth_permanent", "billing", "billing_unverified",
    "content_policy_blocked", "provider_policy_blocked", "model_not_found",
    "format_error", "ssl_cert_verification",
};

/* Providers whose base_url is user-supplied rather than a known vendor. */
static const char *const CUSTOM_ENDPOINT_PROVIDERS[] = {
    "custom", "local", "llama.cpp", "llamacpp", "ollama", "lmstudio", "vllm",
};

/* Message fragments that mark a mid-stream connection drop (deliberately
 * narrow: our own retry-exhaustion summaries + OpenAI SDK stream aborts). */
static const char *const STREAM_DROP_FRAGMENTS[] = {
    "stream connection",
    "peer closed connection",
    "incomplete chunked read",
    "connection broken",
    "stream ended prematurely",
    "sse",
    "mid-stream",
};

/* API/transport exception module prefixes vs. our own dispatcher bugs. */
static const char *const API_EXC_MODULE_PREFIXES[] = {
    "openai", "httpx", "httpcore", "anthropic", "botocore", "boto3",
    "google", "grpc", "requests", "aiohttp", "ssl", "socket", "urllib",
};

/* failover_reason_t → FailoverReason.value wire string. */
const char *es_failover_reason_value(failover_reason_t reason)
{
    switch (reason) {
    case FAILOVER_AUTH:                        return "auth";
    case FAILOVER_AUTH_PERMANENT:              return "auth_permanent";
    case FAILOVER_BILLING:                     return "billing";
    case FAILOVER_RATE_LIMIT:                  return "rate_limit";
    case FAILOVER_OVERLOADED:                  return "overloaded";
    case FAILOVER_SERVER_ERROR:                return "server_error";
    case FAILOVER_TIMEOUT:                     return "timeout";
    case FAILOVER_CONTEXT_OVERFLOW:            return "context_overflow";
    case FAILOVER_PAYLOAD_TOO_LARGE:           return "payload_too_large";
    case FAILOVER_IMAGE_TOO_LARGE:             return "image_too_large";
    case FAILOVER_MODEL_NOT_FOUND:             return "model_not_found";
    case FAILOVER_FORMAT_ERROR:                return "format_error";
    case FAILOVER_INVALID_ENCRYPTED_CONTENT:   return "invalid_encrypted_content";
    case FAILOVER_MULTIMODAL_TOOL_UNSUPPORTED: return "multimodal_tool_content_unsupported";
    case FAILOVER_PROVIDER_POLICY_BLOCKED:     return "provider_policy_blocked";
    case FAILOVER_CONTENT_POLICY_BLOCKED:      return "content_policy_blocked";
    case FAILOVER_THINKING_SIGNATURE:          return "thinking_signature";
    case FAILOVER_LONG_CONTEXT_TIER:           return "long_context_tier";
    case FAILOVER_OAUTH_LONG_CONTEXT_BETA_FORBIDDEN: return "oauth_long_context_beta_forbidden";
    case FAILOVER_LLAMA_CPP_GRAMMAR:           return "llama_cpp_grammar_pattern";
    case FAILOVER_UNKNOWN: default:            return "unknown";
    }
}

/* ── private helpers (mirror the Python underscore functions) ──────────── */

/* PoP: _is_custom_endpoint @ agent/error_surface.py:_is_custom_endpoint */
int es_is_custom_endpoint(const char *provider)
{
    if (!provider) return 0;
    while (*provider == ' ' || *provider == '\t') provider++;
    char lo[128];
    size_t o = 0;
    for (; provider[o] && o + 1 < sizeof lo; o++)
        lo[o] = (char)tolower((unsigned char)provider[o]);
    /* right-strip */
    while (o > 0 && (lo[o-1] == ' ' || lo[o-1] == '\t')) o--;
    lo[o] = '\0';
    if (!o) return 0;
    for (size_t i = 0; i < sizeof(CUSTOM_ENDPOINT_PROVIDERS)/sizeof(char*); i++)
        if (strcmp(lo, CUSTOM_ENDPOINT_PROVIDERS[i]) == 0)
            return 1;
    return strncmp(lo, "custom:", 7) == 0;
}

/* PoP: _looks_like_stream_drop @ agent/error_surface.py:_looks_like_stream_drop */
int es_looks_like_stream_drop(const char *message)
{
    if (!message) return 0;
    char *lo = strdup(message);
    if (!lo) return 0;
    for (char *p = lo; *p; p++) *p = (char)tolower((unsigned char)*p);
    int hit = 0;
    for (size_t i = 0; i < sizeof(STREAM_DROP_FRAGMENTS)/sizeof(char*); i++)
        if (strstr(lo, STREAM_DROP_FRAGMENTS[i])) { hit = 1; break; }
    free(lo);
    return hit;
}

/* PoP: _surface @ agent/error_surface.py:_surface */
static json_t *es_surface(const char *layer, const char *code, int retryable,
                          const char *provider, const char *model)
{
    json_t *out = json_object();
    if (!out) return NULL;
    json_set(out, "layer", json_string(layer));
    json_set(out, "code", json_string(code));
    json_set(out, "retryable", json_bool(retryable ? 1 : 0));
    if (provider && *provider)
        json_set(out, "provider", json_string(provider));
    if (model && *model)
        json_set(out, "model", json_string(model));
    return out;
}

static int str_in_set(const char *s, const char *const *set, size_t n)
{
    if (!s) return 0;
    for (size_t i = 0; i < n; i++)
        if (strcmp(s, set[i]) == 0) return 1;
    return 0;
}

/* ── public API ─────────────────────────────────────────────────────────── */

/* PoP: build_error_surface_from_result @ agent/error_surface.py:build_error_surface_from_result */
json_t *es_build_error_surface_from_result(const json_t *result,
                                           const char *provider,
                                           const char *model)
{
    if (!result || result->type != JSON_OBJECT)
        return NULL;

    json_t *jerr = json_obj_get((json_t *)result, "error");
    const char *err_raw = (jerr && jerr->type == JSON_STRING) ? jerr->str_val : "";
    char *error_text = strdup(err_raw ? err_raw : "");

    json_t *jreason = json_obj_get((json_t *)result, "failure_reason");
    const char *rraw = (jreason && jreason->type == JSON_STRING) ? jreason->str_val : "";
    char reason[128];
    {
        size_t a = 0, o = 0;
        while (rraw[a] == ' ' || rraw[a] == '\t' || rraw[a] == '\n' || rraw[a] == '\r') a++;
        for (; rraw[a] && o + 1 < sizeof reason; a++)
            reason[o++] = rraw[a];
        while (o > 0 && (reason[o-1]==' '||reason[o-1]=='\t'||reason[o-1]=='\n'||reason[o-1]=='\r')) o--;
        reason[o] = '\0';
    }

    if ((!error_text || !*error_text) && !reason[0]) {
        free(error_text);
        return NULL;
    }

    json_t *out = NULL;

    /* Disk-full wins outright: the fix (free space) is unrelated to the
     * provider stack, and hermes_state owns the pattern list. */
    if (error_text && *error_text && hs_is_disk_full_error(error_text)) {
        out = es_surface(ES_LAYER_DISK, "disk_full", 0, provider, model);
        free(error_text);
        return out;
    }

    json_t *jbilling = json_obj_get((json_t *)result, "billing_block");
    int billing_block = jbilling && !(jbilling->type == JSON_NULL) &&
                        !(jbilling->type == JSON_BOOL && jbilling->bool_val == 0);
    if (billing_block || strcmp(reason, "billing") == 0 ||
        strcmp(reason, "billing_unverified") == 0) {
        out = es_surface(ES_LAYER_BILLING, reason[0] ? reason : "billing", 0,
                         provider, model);
        free(error_text);
        return out;
    }

    if (!reason[0]) {
        /* Failed result without a classified reason (legacy paths). */
        if (es_looks_like_stream_drop(error_text))
            out = es_surface(ES_LAYER_STREAMING, "stream_drop", 1, provider, model);
        else
            out = es_surface(ES_LAYER_PROVIDER, "unknown", 1, provider, model);
        free(error_text);
        return out;
    }

    /* reason → layer mapping with transport/streaming fallbacks. */
    const char *layer = NULL;
    for (size_t i = 0; i < sizeof(REASON_TO_LAYER)/sizeof(REASON_TO_LAYER[0]); i++)
        if (strcmp(reason, REASON_TO_LAYER[i].reason) == 0) { layer = REASON_TO_LAYER[i].layer; break; }
    if (!layer) {
        if (str_in_set(reason, TRANSPORT_REASONS, 2) && es_is_custom_endpoint(provider))
            layer = ES_LAYER_ENDPOINT;
        else if (es_looks_like_stream_drop(error_text))
            layer = ES_LAYER_STREAMING;
        else
            layer = ES_LAYER_PROVIDER;
    }

    /* Prefer the classifier's own retry verdict when the result carries it;
     * the reason-set fallback covers older results. */
    json_t *jretry = json_obj_get((json_t *)result, "failure_retryable");
    int retryable;
    if (jretry && jretry->type == JSON_BOOL)
        retryable = jretry->bool_val ? 1 : 0;
    else
        retryable = !str_in_set(reason, NON_RETRYABLE_REASONS,
                                sizeof(NON_RETRYABLE_REASONS)/sizeof(char*));

    out = es_surface(layer, reason, retryable, provider, model);
    free(error_text);
    return out;
}

/* PoP: build_error_surface_from_exception @ agent/error_surface.py:build_error_surface_from_exception */
json_t *es_build_error_surface_from_exception(const char *exc_module,
                                              const char *exc_type,
                                              const char *message,
                                              int has_status_code,
                                              int status_code,
                                              const char *provider,
                                              const char *model)
{
    const char *msg = (message && *message) ? message : (exc_type ? exc_type : "");
    char msgbuf[1024];
    if (!msg || !*msg)
        msg = "";
    snprintf(msgbuf, sizeof msgbuf, "%s", msg);

    if (hs_is_disk_full_error(msgbuf))
        return es_surface(ES_LAYER_DISK, "disk_full", 0, provider, model);

    /* api_like: exception module prefix in the SDK/transport set, or the
     * exception carries a status_code attribute. */
    int api_like = has_status_code;
    if (!api_like && exc_module && *exc_module) {
        char first_seg[64];
        size_t o = 0;
        for (; exc_module[o] && exc_module[o] != '.' && o + 1 < sizeof first_seg; o++)
            first_seg[o] = exc_module[o];
        first_seg[o] = '\0';
        api_like = str_in_set(first_seg, API_EXC_MODULE_PREFIXES,
                              sizeof(API_EXC_MODULE_PREFIXES)/sizeof(char*));
    }

    if (!api_like)
        return es_surface(ES_LAYER_GATEWAY, exc_type ? exc_type : "", 1,
                          provider, model);

    /* classify through the real pipeline (same taxonomy as the retry loop). */
    classified_error_t cls;
    memset(&cls, 0, sizeof cls);
    classify_api_error(status_code > 0 ? status_code : -1, msgbuf,
                       provider ? provider : "", model ? model : "",
                       0, 0, &cls);
    const char *reason = es_failover_reason_value(cls.reason);

    json_t *synthetic = json_object();
    if (!synthetic) return NULL;
    json_set(synthetic, "error", json_string((cls.message[0]) ? cls.message : msgbuf));
    json_set(synthetic, "failure_reason", json_string(reason));

    json_t *surface = es_build_error_surface_from_result(synthetic, provider, model);
    json_free(synthetic);
    if (surface)
        json_set(surface, "retryable", json_bool(cls.retryable ? 1 : 0));
    return surface;
}
