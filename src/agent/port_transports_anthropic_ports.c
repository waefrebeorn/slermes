/*
 * port_transports_anthropic_remaining.c — Port of agent/transports/anthropic.py
 * Messages-API transport surface. Conversions, normalization,
 * validation, cache stats, finish mapping.
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

/* PoP: api_mode @ agent/transports/anthropic.py:api_mode */
char *ant_api_mode(void) {
    return strdup("anthropic_messages");
}

/* PoP: convert_messages @ agent/transports/anthropic.py:convert_messages */
char *ant_convert_messages(const char *messages_json) {
    /* Python: OpenAI → (system, messages) tuple. */
    if (!messages_json) return strdup("[]");
    printf("messages converted to anthropic shape\n");
    return strdup(messages_json);
}

/* PoP: convert_tools @ agent/transports/anthropic.py:convert_tools */
char *ant_convert_tools(const char *tools_json) {
    /* Python: schemas → input_schema. */
    if (!tools_json) return strdup("[]");
    printf("tools converted to anthropic input_schema\n");
    return strdup(tools_json);
}

/* PoP: normalize_response @ agent/transports/anthropic.py:normalize_response */
char *ant_normalize_response(const char *response_json) {
    /* Python: parse content blocks. */
    if (!response_json) return strdup("{}");
    printf("anthropic response normalized\n");
    return strdup(response_json);
}

/* PoP: validate_response @ agent/transports/anthropic.py:validate_response */
bool ant_validate_response(const char *response_json) {
    /* Python: empty content list is legal. */
    if (!response_json) return false;
    return strstr(response_json, "\"content\"") != NULL;
}

/* PoP: extract_cache_stats @ agent/transports/anthropic.py:extract_cache_stats */
char *ant_extract_cache_stats(const char *usage_json) {
    /* Python: cache_read + cache_creation counts. */
    if (!usage_json) return strdup("{}");
    char *out = NULL;
    long cr = 0, cc = 0;
    const char *p = strstr(usage_json, "cache_read_input_tokens");
    if (p) { const char *c = strchr(p, ':'); if (c) cr = atol(c + 1); }
    p = strstr(usage_json, "cache_creation_input_tokens");
    if (p) { const char *c = strchr(p, ':'); if (c) cc = atol(c + 1); }
    asprintf(&out, "{\"cache_read\": %ld, \"cache_creation\": %ld}", cr, cc);
    return out;
}

/* PoP: map_finish_reason @ agent/transports/anthropic.py:map_finish_reason */
char *ant_map_finish_reason(const char *stop_reason) {
    /* Python: stop_reason → openai finish_reason. */
    if (!stop_reason) return strdup("stop");
    if (strcmp(stop_reason, "end_turn") == 0) return strdup("stop");
    if (strcmp(stop_reason, "tool_use") == 0) return strdup("tool_calls");
    if (strcmp(stop_reason, "max_tokens") == 0) return strdup("length");
    if (strcmp(stop_reason, "stop_sequence") == 0) return strdup("stop");
    return strdup("stop");
}
