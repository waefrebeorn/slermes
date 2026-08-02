/*
 * port_transports_types_remaining.c — Port of agent/transports/types.py
 * provider-data accessor surface. Function/arguments, call ids,
 * extra content, reasoning, anthropic blocks, codex items.
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

static char *pd_get(const char *provider_data_json, const char *key) {
    if (!provider_data_json) return NULL;
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = strstr(provider_data_json, needle);
    if (!p) return NULL;
    const char *colon = strchr(p, ':');
    if (!colon) return NULL;
    const char *v = colon + 1;
    while (*v == ' ' || *v == '"') v++;
    const char *e = v;
    while (*e && *e != '"' && *e != ',' && *e != '}') e++;
    if (e == v) return strdup("");
    return strndup(v, (size_t)(e - v));
}

/* PoP: type @ agent/transports/types.py:type */
char *tpt_type(void) {
    return strdup("function");
}

/* PoP: function @ agent/transports/types.py:function */
char *tpt_function(const char *provider_data_json) {
    /* Python: self accessor for tc.function.name/arguments. */
    if (!provider_data_json) return strdup("{}");
    printf("tool call function accessor\n");
    return strdup(provider_data_json);
}

/* PoP: call_id @ agent/transports/types.py:call_id */
char *tpt_call_id(const char *provider_data_json) {
    /* Python: codex call_id from provider_data. */
    char *v = pd_get(provider_data_json, "call_id");
    return v ? v : strdup("");
}

/* PoP: response_item_id @ agent/transports/types.py:response_item_id */
char *tpt_response_item_id(const char *provider_data_json) {
    char *v = pd_get(provider_data_json, "response_item_id");
    return v ? v : strdup("");
}

/* PoP: extra_content @ agent/transports/types.py:extra_content */
char *tpt_extra_content(const char *provider_data_json) {
    /* Python: gemini thought_signature. */
    char *v = pd_get(provider_data_json, "extra_content");
    return v ? v : strdup("");
}

/* PoP: reasoning_content @ agent/transports/types.py:reasoning_content */
char *tpt_reasoning_content(const char *provider_data_json) {
    char *v = pd_get(provider_data_json, "reasoning_content");
    return v ? v : strdup("");
}

/* PoP: reasoning_details @ agent/transports/types.py:reasoning_details */
char *tpt_reasoning_details(const char *provider_data_json) {
    char *v = pd_get(provider_data_json, "reasoning_details");
    return v ? v : strdup("[]");
}

/* PoP: anthropic_content_blocks @ agent/transports/types.py:anthropic_content_blocks */
char *tpt_anthropic_content_blocks(const char *provider_data_json) {
    /* Python: verbatim order-preserving blocks. */
    char *v = pd_get(provider_data_json, "anthropic_content_blocks");
    return v ? v : strdup("[]");
}

/* PoP: codex_reasoning_items @ agent/transports/types.py:codex_reasoning_items */
char *tpt_codex_reasoning_items(const char *provider_data_json) {
    char *v = pd_get(provider_data_json, "codex_reasoning_items");
    return v ? v : strdup("[]");
}

/* PoP: codex_message_items @ agent/transports/types.py:codex_message_items */
char *tpt_codex_message_items(const char *provider_data_json) {
    char *v = pd_get(provider_data_json, "codex_message_items");
    return v ? v : strdup("[]");
}
