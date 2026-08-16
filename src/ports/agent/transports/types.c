/*
 * ports/agent/transports/types.c — C11 port of agent/transports/types.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/agent/port_transports_types_ports.c ── */
/*
 * port_transports_types_remaining.c — Port of agent/transports/types.py
 * provider-data accessor surface. Function/arguments, call ids,
 * extra content, reasoning, anthropic blocks, codex items.
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
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

/* PoP: tpt_type @ agent/transports/types.py:type */
char *tpt_type(void) {
    return strdup("function");
}

/* PoP: tpt_function @ agent/transports/types.py:function */
char *tpt_function(const char *provider_data_json) {
    /* Python: self accessor for tc.function.name/arguments. */
    if (!provider_data_json) return strdup("{}");
    printf("tool call function accessor\n");
    return strdup(provider_data_json);
}

/* PoP: tpt_call_id @ agent/transports/types.py:call_id */
char *tpt_call_id(const char *provider_data_json) {
    /* Python: codex call_id from provider_data. */
    char *v = pd_get(provider_data_json, "call_id");
    return v ? v : strdup("");
}

/* PoP: tpt_response_item_id @ agent/transports/types.py:response_item_id */
char *tpt_response_item_id(const char *provider_data_json) {
    char *v = pd_get(provider_data_json, "response_item_id");
    return v ? v : strdup("");
}

/* PoP: tpt_extra_content @ agent/transports/types.py:extra_content */
char *tpt_extra_content(const char *provider_data_json) {
    /* Python: gemini thought_signature. */
    char *v = pd_get(provider_data_json, "extra_content");
    return v ? v : strdup("");
}

/* PoP: tpt_reasoning_content @ agent/transports/types.py:reasoning_content */
char *tpt_reasoning_content(const char *provider_data_json) {
    char *v = pd_get(provider_data_json, "reasoning_content");
    return v ? v : strdup("");
}

/* PoP: tpt_reasoning_details @ agent/transports/types.py:reasoning_details */
char *tpt_reasoning_details(const char *provider_data_json) {
    char *v = pd_get(provider_data_json, "reasoning_details");
    return v ? v : strdup("[]");
}

/* PoP: tpt_anthropic_content_blocks @ agent/transports/types.py:anthropic_content_blocks */
char *tpt_anthropic_content_blocks(const char *provider_data_json) {
    /* Python: verbatim order-preserving blocks. */
    char *v = pd_get(provider_data_json, "anthropic_content_blocks");
    return v ? v : strdup("[]");
}

/* PoP: tpt_codex_reasoning_items @ agent/transports/types.py:codex_reasoning_items */
char *tpt_codex_reasoning_items(const char *provider_data_json) {
    char *v = pd_get(provider_data_json, "codex_reasoning_items");
    return v ? v : strdup("[]");
}

/* PoP: tpt_codex_message_items @ agent/transports/types.py:codex_message_items */
char *tpt_codex_message_items(const char *provider_data_json) {
    char *v = pd_get(provider_data_json, "codex_message_items");
    return v ? v : strdup("[]");
}


/* ── from src/agent/transport_common.c ── */
/*
 * transport_common.c — port of agent/transports/types.py + base.py.
 *
 *
 * Dependency ROOT for agent/transports/* ports. Adapters register a vtable
 * here; the agent loop resolves one by api_mode at runtime (no static
 * dispatch). Network I/O is NOT owned here (matches Python: the provider
 * client performs the call; the transport only shapes data).
 */
#include "transport_common.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

/* Local strdup (glibc hides it under -std=c11; codebase convention per
 * llm_client.c:xstrdup). Avoids feature-macro fragility. */
static char *xstrdup(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ── ToolCall ──────────────────────────────────────────────────────────── */
/* PoP: tool_call_create @ agent/transports/types.py:build_tool_call */
tool_call_t *tool_call_create(const char *id, const char *name,
                              const char *arguments_json, const char *provider_data_json)
{
    tool_call_t *tc = calloc(1, sizeof(*tc));
    if (!tc) return NULL;
    tc->id = id ? xstrdup(id) : NULL;
    tc->name = name ? xstrdup(name) : xstrdup("");
    tc->arguments = arguments_json ? xstrdup(arguments_json) : xstrdup("{}");
    tc->provider_data = provider_data_json ? xstrdup(provider_data_json) : NULL;
    return tc;
}

void tool_call_free(tool_call_t *tc)
{
    if (!tc) return;
    free(tc->id); free(tc->name); free(tc->arguments); free(tc->provider_data);
    free(tc);
}

/* ── NormalizedResponse ─────────────────────────────────────────────────── */
normalized_response_t *normalized_response_create(void)
{
    return calloc(1, sizeof(normalized_response_t));
}

void normalized_response_free(normalized_response_t *nr)
{
    if (!nr) return;
    free(nr->content);
    free(nr->finish_reason);
    free(nr->reasoning);
    free(nr->provider_data);
    if (nr->usage) free(nr->usage);
    if (nr->tool_calls) {
        for (size_t i = 0; i < nr->tool_call_count; i++)
            tool_call_free(nr->tool_calls[i]);
        free(nr->tool_calls);
    }
    free(nr);
}

/* map_finish_reason: mapping is a NULL-terminated array of {raw, normalized}
 * string pairs. Falls back to "stop" for unknown/empty (port of types.map_finish_reason). */
/* PoP: map_finish_reason @ agent/transports/base.py:map_finish_reason */
char *map_finish_reason(const char *raw_reason, const char *const *mapping)
{
    if (!raw_reason || !*raw_reason) return xstrdup("stop");
    if (mapping) {
        for (size_t i = 0; mapping[i] && mapping[i + 1]; i += 2) {
            if (strcmp(mapping[i], raw_reason) == 0)
                return xstrdup(mapping[i + 1]);
        }
    }
    return xstrdup(raw_reason);
}

/* ── transport_map_finish_reason / transport_normalize_role ──────────────── */
/* Provider-aware finish-reason map (fulfils hermes_transport_common.h, used
 * by provider_google.c etc). Returns static strings. */
/* PoP: transport_map_finish_reason @ agent/transports/types.py:map_finish_reason */
const char *transport_map_finish_reason(const char *provider, const char *raw_reason)
{
    if (!raw_reason || !*raw_reason) return "stop";
    if (!provider || !*provider) return raw_reason;  /* identity */
    /* Google/Gemini uses "STOP","MAX_TOKENS","END_TURN","tool_code",... */
    if (strcmp(provider, "google") == 0) {
        if (strcmp(raw_reason, "STOP") == 0) return "stop";
        if (strcmp(raw_reason, "MAX_TOKENS") == 0) return "length";
        if (strcmp(raw_reason, "END_TURN") == 0) return "stop";
        if (strcmp(raw_reason, "TOOL_CODE") == 0) return "tool_calls";
        if (strcmp(raw_reason, "SAFETY") == 0) return "content_filter";
        return "stop";
    }
    if (strcmp(provider, "anthropic") == 0) {
        if (strcmp(raw_reason, "end_turn") == 0) return "stop";
        if (strcmp(raw_reason, "max_tokens") == 0) return "length";
        if (strcmp(raw_reason, "tool_use") == 0) return "tool_calls";
        if (strcmp(raw_reason, "content_filter") == 0) return "content_filter";
        return "stop";
    }
    if (strcmp(provider, "bedrock") == 0) {
        if (strcmp(raw_reason, "end_turn") == 0) return "stop";
        if (strcmp(raw_reason, "max_tokens") == 0) return "length";
        if (strcmp(raw_reason, "tool_use") == 0) return "tool_calls";
        return "stop";
    }
    if (strcmp(provider, "codex") == 0) {
        if (strcmp(raw_reason, "length") == 0) return "length";
        if (strcmp(raw_reason, "tool_calls") == 0) return "tool_calls";
        if (strcmp(raw_reason, "stop") == 0) return "stop";
        return "stop";
    }
    /* openai / default: pass through */
    return raw_reason;
}

const char *transport_normalize_role(const char *role)
{
    if (!role || !*role) return "user";
    if (strcmp(role, "system") == 0) return "system";
    if (strcmp(role, "user") == 0) return "user";
    if (strcmp(role, "assistant") == 0) return "assistant";
    if (strcmp(role, "tool") == 0) return "tool";
    if (strcmp(role, "function") == 0) return "tool";  /* lenient */
    return "user";
}

/* ── Transport registry (port of base.py ProviderTransport dispatch) ────── */
struct transport_registry {
    provider_transport_t **entries;
    size_t count;
    size_t cap;
};

transport_registry_t *transport_registry_create(void)
{
    transport_registry_t *reg = calloc(1, sizeof(*reg));
    if (!reg) return NULL;
    reg->cap = 8;
    reg->entries = calloc(reg->cap, sizeof(provider_transport_t *));
    return reg;
}

void transport_registry_free(transport_registry_t *reg)
{
    if (!reg) return;
    /* Registry does NOT own the vtable structs (registered by adapters that
     * live for the process lifetime); just drop the array. */
    free(reg->entries);
    free(reg);
}

int transport_registry_register(transport_registry_t *reg, provider_transport_t *t)
{
    if (!reg || !t || !t->api_mode) return -1;
    /* Replace if api_mode already registered (idempotent re-init). */
    for (size_t i = 0; i < reg->count; i++) {
        if (strcmp(reg->entries[i]->api_mode, t->api_mode) == 0) {
            reg->entries[i] = t;
            return 0;
        }
    }
    if (reg->count == reg->cap) {
        size_t ncap = reg->cap * 2;
        provider_transport_t **ne = realloc(reg->entries, ncap * sizeof(*ne));
        if (!ne) return -1;
        reg->entries = ne; reg->cap = ncap;
    }
    reg->entries[reg->count++] = t;
    return 0;
}

provider_transport_t *transport_registry_get(transport_registry_t *reg, const char *api_mode)
{
    if (!reg || !api_mode) return NULL;
    for (size_t i = 0; i < reg->count; i++)
        if (strcmp(reg->entries[i]->api_mode, api_mode) == 0)
            return reg->entries[i];
    return NULL;
}

char **transport_registry_list(transport_registry_t *reg)
{
    if (!reg) return NULL;
    char **out = calloc(reg->count + 1, sizeof(char *));
    for (size_t i = 0; i < reg->count; i++)
        out[i] = (char *)reg->entries[i]->api_mode; /* borrowed */
    out[reg->count] = NULL;
    return out;
}

/* ── Global registry (lazy singleton) ───────────────────────────────────── */
static transport_registry_t *g_transport_registry = NULL;

transport_registry_t *transport_registry_init(void)
{
    if (g_transport_registry) return g_transport_registry;
    g_transport_registry = transport_registry_create();
    return g_transport_registry;
}

void transport_registry_shutdown(void)
{
    if (!g_transport_registry) return;
    transport_registry_free(g_transport_registry);
    g_transport_registry = NULL;
}

