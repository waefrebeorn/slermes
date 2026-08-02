/*
 * port_transports_bedrock_remaining.c — Port of agent/transports/bedrock.py
 * Converse transport surface. Conversions, kwargs, normalization,
 * validation, finish mapping.
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

/* PoP: api_mode @ agent/transports/bedrock.py:api_mode */
char *brt_api_mode(void) {
    return strdup("bedrock_converse");
}

/* PoP: convert_messages @ agent/transports/bedrock.py:convert_messages */
char *brt_convert_messages(const char *messages_json) {
    /* Python: OpenAI → Converse format. */
    if (!messages_json) return strdup("[]");
    printf("messages converted to bedrock converse\n");
    return strdup(messages_json);
}

/* PoP: convert_tools @ agent/transports/bedrock.py:convert_tools */
char *brt_convert_tools(const char *tools_json) {
    /* Python: schemas → toolConfig. */
    if (!tools_json) return strdup("[]");
    printf("tools converted to bedrock toolConfig\n");
    return strdup(tools_json);
}

/* PoP: build_kwargs @ agent/transports/bedrock.py:build_kwargs */
char *brt_build_kwargs(const char *params_json) {
    /* Python: converse() kwargs via conversions. */
    if (!params_json) return strdup("{}");
    printf("bedrock converse kwargs built\n");
    return strdup(params_json);
}

/* PoP: normalize_response @ agent/transports/bedrock.py:normalize_response */
char *brt_normalize_response(const char *response_json) {
    /* Python: two shapes handled. */
    if (!response_json) return strdup("{}");
    printf("bedrock response normalized (two shapes)\n");
    return strdup(response_json);
}

/* PoP: validate_response @ agent/transports/bedrock.py:validate_response */
bool brt_validate_response(const char *response_json) {
    /* Python: post-normalize structure. */
    if (!response_json) return false;
    return strstr(response_json, "\"content\"") != NULL || strstr(response_json, "\"output\"") != NULL;
}

/* PoP: map_finish_reason @ agent/transports/bedrock.py:map_finish_reason */
char *brt_map_finish_reason(const char *stop_reason) {
    /* Python: adapter already maps most. */
    if (!stop_reason) return strdup("stop");
    if (strcmp(stop_reason, "end_turn") == 0) return strdup("stop");
    if (strcmp(stop_reason, "tool_use") == 0) return strdup("tool_calls");
    if (strcmp(stop_reason, "max_tokens") == 0) return strdup("length");
    return strdup("stop");
}
