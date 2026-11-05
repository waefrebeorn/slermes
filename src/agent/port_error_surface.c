/*
 * port_error_surface.c — Port of Python: agent/error_surface.py
 *
 * The whole classification table is data; the builders are branch logic over
 * a turn-result dict. Two advisory lookups (provider label, auth kind / key
 * env var) resolve through Python registries that have no C equivalent, so
 * they arrive as parameters instead of being re-derived here.
 */

#include "port_error_surface.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Python string helpers ────────────────────────────────────────── */

static char *lower_dup(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = '\0';
    return out;
}

static char *strip_dup(const char *s)
{
    if (!s) return NULL;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static int contains_ci(const char *haystack, const char *needle_lower)
{
    if (!haystack || !needle_lower) return 0;
    char *h = lower_dup(haystack);
    if (!h) return 0;
    int found = strstr(h, needle_lower) != NULL;
    free(h);
    return found;
}

static int one_of(const char *value, const char *const *set, size_t n)
{
    if (!value) return 0;
    for (size_t i = 0; i < n; i++)
        if (strcmp(value, set[i]) == 0) return 1;
    return 0;
}

/* Python str(x or "") for a JSON node. */
static char *node_str(const json_t *n)
{
    if (!n) return strdup("");
    char buf[64];
    switch (n->type) {
    case JSON_STRING: return strdup(n->str_val ? n->str_val : "");
    case JSON_NUMBER: snprintf(buf, sizeof(buf), "%.17g", n->num_val); return strdup(buf);
    case JSON_BOOL:   return strdup(n->bool_val ? "True" : "False");
    case JSON_NULL:   return strdup("");
    default:          return json_serialize(n);
    }
}

static int py_truthy(const json_t *n)
{
    if (!n) return 0;
    switch (n->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return n->bool_val;
    case JSON_NUMBER: return n->num_val != 0.0;
    case JSON_STRING: return n->str_val && n->str_val[0];
    case JSON_ARRAY:
    case JSON_OBJECT: return n->c.count > 0;
    }
    return 0;
}

/* ── tables ───────────────────────────────────────────────────────── */

static const struct { const char *reason, *layer; } REASON_TO_LAYER[] = {
    { "auth", ESC_LAYER_AUTH }, { "auth_permanent", ESC_LAYER_AUTH },
    { "billing", ESC_LAYER_BILLING }, { "billing_unverified", ESC_LAYER_BILLING },
    { "loop_error", ESC_LAYER_GATEWAY }, { "interpreter_shutdown", ESC_LAYER_GATEWAY },
    { "session_busy", ESC_LAYER_GATEWAY }, { "gateway_connection", ESC_LAYER_GATEWAY },
    { "host_error", ESC_LAYER_GATEWAY }, { "workspace_unavailable", ESC_LAYER_GATEWAY },
    { "launch_credentials_unavailable", ESC_LAYER_GATEWAY },
    { "local_runtime_unavailable", ESC_LAYER_ENDPOINT },
    { "truncated", ESC_LAYER_PROVIDER }, { "empty_response", ESC_LAYER_PROVIDER },
    { "invalid_response", ESC_LAYER_PROVIDER }, { "context_overflow", ESC_LAYER_PROVIDER },
};

static const char *const TRANSPORT_REASONS[] = { "timeout", "ssl_cert_verification" };
static const char *const FREE_TIER_RETRYABLE[] = { "rate_limited", "at_capacity", "outage" };

static const char *const NON_RETRYABLE_REASONS[] = {
    "auth", "auth_permanent", "billing", "billing_unverified",
    "content_policy_blocked", "provider_policy_blocked", "data_policy_denied",
    "model_not_found", "format_error", "ssl_cert_verification",
    "context_overflow", "interpreter_shutdown", "upstream_blocked",
};

static const char *const CUSTOM_ENDPOINT_PROVIDERS[] = {
    "custom", "local", "llama.cpp", "llamacpp", "ollama", "lmstudio", "vllm",
};

static const char *const STREAM_DROP_FRAGMENTS[] = {
    "stream connection", "peer closed connection", "incomplete chunked read",
    "connection broken", "stream ended prematurely", "sse", "mid-stream",
};

static const char *const API_EXC_MODULE_PREFIXES[] = {
    "openai", "httpx", "httpcore", "anthropic", "botocore", "boto3", "google",
    "grpc", "requests", "aiohttp", "ssl", "socket", "urllib",
};

static const char *const DISK_FULL_MARKERS[] = {
    "no space left on device", "not enough space", "database or disk is full",
    "disk full", "full disk", "enospc",
};

#define NELEM(a) (sizeof(a) / sizeof((a)[0]))

/* ── pure predicates ─────────────────────────────────────────────── */

/* PoP: esc_is_custom_endpoint @ agent/error_surface.py:_is_custom_endpoint */
int esc_is_custom_endpoint(const char *provider)
{
    char *p = strip_dup(provider ? provider : "");
    if (!p) return 0;
    for (char *q = p; *q; q++) *q = (char)tolower((unsigned char)*q);
    int custom = one_of(p, CUSTOM_ENDPOINT_PROVIDERS, NELEM(CUSTOM_ENDPOINT_PROVIDERS)) ||
                 strncmp(p, "custom:", 7) == 0;
    free(p);
    return custom;
}

/* PoP: esc_looks_like_stream_drop @ agent/error_surface.py:_looks_like_stream_drop */
int esc_looks_like_stream_drop(const char *message)
{
    for (size_t i = 0; i < NELEM(STREAM_DROP_FRAGMENTS); i++)
        if (contains_ci(message, STREAM_DROP_FRAGMENTS[i])) return 1;
    return 0;
}

/* PoP: esc_is_non_retryable_reason @ agent/error_surface.py:_NON_RETRYABLE_REASONS */
int esc_is_non_retryable_reason(const char *reason)
{
    return one_of(reason, NON_RETRYABLE_REASONS, NELEM(NON_RETRYABLE_REASONS));
}

/* PoP: esc_disk_full @ agent/error_surface.py:_disk_full */
int esc_disk_full(const char *text)
{
    for (size_t i = 0; i < NELEM(DISK_FULL_MARKERS); i++)
        if (contains_ci(text, DISK_FULL_MARKERS[i])) return 1;
    return 0;
}

/* PoP: esc_result_layer @ agent/error_surface.py:_result_layer */
const char *esc_result_layer(const char *reason, const char *error_text,
                            const char *provider)
{
    if (reason) {
        for (size_t i = 0; i < NELEM(REASON_TO_LAYER); i++)
            if (strcmp(reason, REASON_TO_LAYER[i].reason) == 0)
                return REASON_TO_LAYER[i].layer;
        if (one_of(reason, TRANSPORT_REASONS, NELEM(TRANSPORT_REASONS)) &&
            esc_is_custom_endpoint(provider))
            return ESC_LAYER_ENDPOINT;
    }
    return esc_looks_like_stream_drop(error_text) ? ESC_LAYER_STREAMING
                                                  : ESC_LAYER_PROVIDER;
}

/* ── descriptor builder ──────────────────────────────────────────── */

/* PoP: esc_surface @ agent/error_surface.py:_surface */
json_t *esc_surface(const char *layer, const char *code, int retryable,
                    const char *provider, const char *model,
                    const char *provider_label, const char *auth_kind,
                    const char *api_key_env)
{
    json_t *surface = json_object();
    if (!surface) return NULL;
    json_set(surface, "layer", json_string(layer ? layer : ""));
    json_set(surface, "code", json_string(code ? code : ""));
    json_set(surface, "retryable", json_bool(retryable ? true : false));
    /* identity only when truthy, as the Python dict comprehension does */
    if (provider && *provider) json_set(surface, "provider", json_string(provider));
    if (model && *model) json_set(surface, "model", json_string(model));
    if (provider && *provider) {
        if (provider_label && *provider_label)
            json_set(surface, "provider_label", json_string(provider_label));
        if (layer && strcmp(layer, ESC_LAYER_AUTH) == 0) {
            const char *kind = (auth_kind && *auth_kind) ? auth_kind : "api_key";
            json_set(surface, "auth_kind", json_string(kind));
            if (strcmp(kind, "api_key") == 0 && api_key_env && *api_key_env)
                json_set(surface, "api_key_env", json_string(api_key_env));
        }
    }
    return surface;
}

/* PoP: esc_build_from_result @ agent/error_surface.py:build_error_surface_from_result */
json_t *esc_build_from_result(const char *result_json, const char *provider,
                              const char *model, const char *provider_label,
                              const char *auth_kind, const char *api_key_env)
{
    json_t *doc = result_json ? json_parse(result_json, NULL) : NULL;
    if (!doc || doc->type != JSON_OBJECT) {
        if (doc) json_free(doc);
        return NULL;
    }

    char *error_text = node_str(json_obj_get(doc, "error"));
    char *raw_reason = node_str(json_obj_get(doc, "failure_reason"));
    char *reason = strip_dup(raw_reason ? raw_reason : "");
    free(raw_reason);

    json_t *out = NULL;
    if (!error_text[0] && !reason[0]) goto done;

    if (error_text[0] && esc_disk_full(error_text)) {
        out = esc_surface(ESC_LAYER_DISK, "disk_full", 0, provider, model,
                          provider_label, auth_kind, api_key_env);
        goto done;
    }

    if (py_truthy(json_obj_get(doc, "billing_block")) ||
        strcmp(reason, "billing") == 0 || strcmp(reason, "billing_unverified") == 0) {
        char *window = node_str(json_obj_get(doc, "usage_window_message"));
        if (window[0]) {
            out = esc_surface(ESC_LAYER_BILLING, "usage_window", 1, provider,
                              model, provider_label, auth_kind, api_key_env);
            if (out) {
                json_set(out, "message", json_string(window));
                json_t *resets = json_obj_get(doc, "failure_resets_at");
                if (resets && resets->type == JSON_NUMBER)
                    json_set(out, "resets_at", json_number(resets->num_val));
                json_t *limit = json_obj_get(doc, "usage_limit");
                if (limit && limit->type == JSON_OBJECT)
                    json_set(out, "usage_limit", json_copy(limit));
            }
            free(window);
            goto done;
        }
        free(window);
        out = esc_surface(ESC_LAYER_BILLING, reason[0] ? reason : "billing", 0,
                          provider, model, provider_label, auth_kind, api_key_env);
        goto done;
    }

    {
        json_t *free_tier = json_obj_get(doc, "free_tier");
        if (free_tier && free_tier->type == JSON_OBJECT) {
            char *kind = node_str(json_obj_get(free_tier, "kind"));
            if (kind[0]) {
                char code[128];
                snprintf(code, sizeof(code), "free_tier_%s", kind);
                int retryable = one_of(kind, FREE_TIER_RETRYABLE,
                                       NELEM(FREE_TIER_RETRYABLE));
                out = esc_surface(ESC_LAYER_PROVIDER, code, retryable, provider,
                                  model, provider_label, auth_kind, api_key_env);
                if (out) {
                    char *msg = node_str(json_obj_get(free_tier, "message"));
                    if (msg[0]) json_set(out, "message", json_string(msg));
                    free(msg);
                }
                free(kind);
                goto done;
            }
            free(kind);
        }
    }

    if (!reason[0]) {
        int drop = esc_looks_like_stream_drop(error_text);
        out = esc_surface(drop ? ESC_LAYER_STREAMING : ESC_LAYER_PROVIDER,
                          drop ? "stream_drop" : "unknown", 1, provider, model,
                          provider_label, auth_kind, api_key_env);
        goto done;
    }

    {
        int retryable;
        json_t *flag = json_obj_get(doc, "failure_retryable");
        if (flag && flag->type == JSON_BOOL) retryable = flag->bool_val;
        else retryable = !esc_is_non_retryable_reason(reason);

        out = esc_surface(esc_result_layer(reason, error_text, provider), reason,
                          retryable, provider, model, provider_label, auth_kind,
                          api_key_env);
        if (out) {
            json_t *resets = json_obj_get(doc, "failure_resets_at");
            if (resets && resets->type == JSON_NUMBER)
                json_set(out, "resets_at", json_number(resets->num_val));
        }
    }

done:
    free(error_text);
    free(reason);
    json_free(doc);
    return out;
}
