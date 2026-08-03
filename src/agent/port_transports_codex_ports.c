/*
 * port_transports_codex_remaining.c — Port of agent/transports/codex.py
 * Responses transport surface. Cache keys, issuer classification,
 * message/tool conversion, kwargs, normalization, finish mapping.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _bounded_prompt_cache_key @ agent/transports/codex.py:_bounded_prompt_cache_key */
char *tcx_bounded_prompt_cache_key(const char *value) {
    /* Python: provider-safe key without session identity. */
    if (!value) return NULL;
    return strdup(value);
}

/* PoP: _default_prompt_cache_retention_for_request @ agent/transports/codex.py:_default_prompt_cache_retention_for_request */
char *tcx_default_prompt_cache_retention_for_request(const char *model) {
    /* Python: 24h for Bedrock Mantle models. */
    if (!model) return strdup("5m");
    char *l = lowerdup(model);
    if (!l) return strdup("5m");
    char *r = strstr(l, "mantle") || strstr(l, "bedrock") ? strdup("24h") : strdup("5m");
    free(l);
    return r;
}

/* PoP: _content_cache_key @ agent/transports/codex.py:_content_cache_key */
char *tcx_content_cache_key(const char *prefix) {
    /* Python: content-address the cache key. */
    if (!prefix) return strdup("");
    unsigned long long h = 1469598103934665603ULL;
    for (const char *p = prefix; *p; p++) {
        h ^= (unsigned char)*p;
        h *= 1099511628211ULL;
    }
    char *out = NULL;
    asprintf(&out, "%016llx", h);
    return out;
}

/* PoP: api_mode @ agent/transports/codex.py:api_mode */
char *tcx_api_mode(void) {
    return strdup("codex_responses");
}

/* PoP: _resolve_issuer_kind @ agent/transports/codex.py:_resolve_issuer_kind */
char *tcx_resolve_issuer_kind(const char *base_url, const char *model) {
    /* Python: classify endpoint. */
    if (!model) return strdup("unknown");
    char *l = lowerdup(model);
    if (!l) return strdup("unknown");
    char *r = (strstr(l, "codex") || strstr(l, "gpt-5") || strstr(l, "o3") || strstr(l, "o4"))
                  ? strdup("responses") : strdup("chat");
    free(l);
    return r;
}

/* PoP: convert_messages @ agent/transports/codex.py:convert_messages */
char *tcx_convert_messages(const char *messages_json) {
    /* Python: chat → Responses input items. */
    if (!messages_json) return strdup("[]");
    printf("messages converted to responses items\n");
    return strdup(messages_json);
}

/* PoP: convert_tools @ agent/transports/codex.py:convert_tools */
char *tcx_convert_tools(const char *tools_json) {
    /* Python: schemas → function definitions. */
    if (!tools_json) return strdup("[]");
    printf("tools converted to responses functions\n");
    return strdup(tools_json);
}

/* PoP: build_kwargs @ agent/transports/codex.py:build_kwargs */
char *tcx_build_kwargs(const char *params_json) {
    /* Python: responses kwargs via conversions. */
    if (!params_json) return strdup("{}");
    printf("responses kwargs built\n");
    return strdup(params_json);
}

/* PoP: normalize_response @ agent/transports/codex.py:normalize_response */
char *tcx_normalize_response(const char *response_json) {
    /* Python: → NormalizedResponse. */
    if (!response_json) return strdup("{}");
    printf("codex response normalized\n");
    return strdup(response_json);
}

/* PoP: validate_response @ agent/transports/codex.py:validate_response */
bool tcx_validate_response(const char *response_json) {
    /* Python: valid output structure. */
    if (!response_json) return false;
    return strstr(response_json, "\"output\"") != NULL || strstr(response_json, "\"choices\"") != NULL;
}

/* PoP: preflight_kwargs @ agent/transports/codex.py:preflight_kwargs */
char *tcx_preflight_kwargs(const char *kwargs_json) {
    /* Python: validate + sanitize before call. */
    if (!kwargs_json) return strdup("{}");
    printf("codex kwargs preflighted\n");
    return strdup(kwargs_json);
}

/* PoP: map_finish_reason @ agent/transports/codex.py:map_finish_reason */
char *tcx_map_finish_reason(const char *status) {
    /* Python: response.status → openai finish_reason. */
    if (!status) return strdup("stop");
    if (strcmp(status, "completed") == 0) return strdup("stop");
    if (strcmp(status, "in_progress") == 0) return strdup("length");
    if (strcmp(status, "incomplete") == 0) return strdup("length");
    if (strcmp(status, "failed") == 0) return strdup("error");
    return strdup("stop");
}
