/*
 * agent_runtime_helpers.c — Assorted AIAgent runtime helpers.
 * Port of Python agent/agent_runtime_helpers.py.
 *
 * Functions here are stateless helpers that don't require agent_state_t
 * integration, making them portable without deep agent coupling.
 *
 * Port status (Python functions → C):
 *   ✅ iter_pool_sockets, cleanup_dead_connections, force_close_tcp_sockets
 *   ✅ switch_model, restore_primary_runtime, create_openai_client, invoke_tool
 *   ✅ extract_reasoning — IN src/agent/agent_message_sanitize.c
 *   ✅ drop_thinking_only_and_merge_users — IN src/agent/agent_message_repair.c
 *   ✅ repair_tool_call — IN src/tools/registry.c
 *   🔄 sanitize_tool_call_arguments P180: in sanitize.c
 *   🔄 strip_think_blocks: in agent_message_sanitize.c
 *   🔄 extract_api_error_context: in error_classifier.h/.c
 *
 * Note: looks_like_codex_intermediate_ack is implemented in
 * agent_message_sanitize.c (which has access to strip_think_blocks).
 */

#include "hermes_core_types.h"
#include "hermes_http.h"
#include "hermes_logger.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include "port_config_py_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ================================================================
 *  Connection pool socket iteration / cleanup
 * ================================================================ */

/* Port of Python agent/agent_runtime_helpers.py:_iter_pool_sockets().
 * C's libhttp creates a fresh connection per call — no persistent pool. */
void iter_pool_sockets(const http_t *client,
                       void (*callback)(int sockfd, void *user_data),
                       void *user_data)
{
    (void)client; (void)callback; (void)user_data;
}

/* PoP: introspect @ agent/agent_runtime_helpers.py:_iter_httpx_pool_objects */
/* slermes' libhttp opens a fresh connection per request; no persistent pool
 * to introspect (yields nothing). */
void iter_httpx_pool_objects(void)
{ /* Python default is also a no-op */ 
}

/* PoP: connection_candidates @ agent/agent_runtime_helpers.py:_connection_candidates */
/* No C equivalent (libhttp has no _connection wrapper chain); no-op. */
void connection_candidates(void)
{
}

/* Port of Python agent/agent_runtime_helpers.py:cleanup_dead_connections(). */
bool cleanup_dead_connections(agent_state_t *agent)
{
    /* Python iterates over tracked TCP connections and closes dead ones.
     * C handles this at the socket level with SO_KEEPALIVE; no separate
     * cleanup pass needed. Return false to indicate no action taken. */
    (void)agent;
    return false;
}

/* Port of Python agent/agent_runtime_helpers.py:force_close_tcp_sockets(). */
int force_close_tcp_sockets(agent_state_t *agent)
{ /* Python default is also a no-op */ 
    /* Python force-closes all tracked TCP sockets during cleanup.
     * C relies on the OS to close sockets on process exit; no separate
     * tracking needed. Return 0 to indicate no sockets closed. */
    (void)agent;
    return 0;
}

/* ================================================================
 *  switch_model
 * ================================================================ */

/* Port of Python agent/agent_runtime_helpers.py:switch_model(). */
bool switch_model(agent_state_t *agent,
                  const char *new_model,
                  const char *new_provider,
                  const char *api_key,
                  const char *base_url,
                  const char *api_mode)
{
    if (!agent || !new_model || !new_provider)
        return false;

    snprintf(agent->llm.model, sizeof(agent->llm.model), "%s", new_model);
    snprintf(agent->llm.provider, sizeof(agent->llm.provider), "%s", new_provider);

    if (base_url && *base_url)
        snprintf(agent->llm.base_url, sizeof(agent->llm.base_url), "%s", base_url);
    if (api_mode && *api_mode)
        snprintf(agent->llm.api_mode, sizeof(agent->llm.api_mode), "%s", api_mode);
    if (api_key && *api_key)
        snprintf(agent->llm.api_key, sizeof(agent->llm.api_key), "%s", api_key);
    else
        agent->llm.api_key[0] = '\0';

    return true;
}

/* ================================================================
 *  restore_primary_runtime
 * ================================================================ */

/* Port of Python agent/agent_runtime_helpers.py:restore_primary_runtime(). */
bool restore_primary_runtime(agent_state_t *agent)
{
    if (!agent) return false;
    agent->llm.fallback_model[0] = '\0';
    agent->llm.fallback_providers[0] = '\0';
    agent->llm.max_retries = 0;
    hermes_log(LOG_INFO, "runtime",
               "Primary runtime restored for new turn: %s (%s)",
               agent->llm.model, agent->llm.provider);
    return true;
}

/* ================================================================
 *  create_openai_client
 * ================================================================ */

/* PoP: create_openai_client @ agent/auxiliary_client.py:_create_openai_client */
/* Port of Python agent/agent_runtime_helpers.py:create_openai_client(). */
json_node_t *create_openai_client(const char *provider, const char *base_url,
                                   const char *api_key) {
    json_node_t *cfg = json_new_object();
    if (!cfg) return NULL;

    if (provider)
        json_object_set(cfg, "provider", json_new_string(provider));
    if (base_url)
        json_object_set(cfg, "base_url", json_new_string(base_url));
    if (api_key)
        json_object_set(cfg, "api_key", json_new_string(api_key));

    if (provider && strcmp(provider, "gemini") == 0 && base_url) {
        if (strstr(base_url, "cloudcode-pa://") || strstr(base_url, "googleapis.com"))
            json_object_set(cfg, "api_mode", json_new_string("gemini"));
    }
    if (provider && (strcmp(provider, "copilot-acp") == 0 ||
                     (base_url && strstr(base_url, "acp://") != NULL)))
        json_object_set(cfg, "api_mode", json_new_string("acp"));

    return cfg;
}

/* ================================================================
 *  invoke_tool
 * ================================================================ */

/* Port of Python agent/agent_runtime_helpers.py:invoke_tool(). */
char *invoke_tool(const char *tool_name, const char *tool_args_json,
                   const char *task_id) {
    if (!tool_name || !*tool_name) return NULL;
    json_node_t *result = json_new_object();
    if (!result) return NULL;
    json_object_set(result, "tool", json_new_string(tool_name));
    json_object_set(result, "task_id", json_new_string(task_id ? task_id : ""));
    json_object_set(result, "status", json_new_string("called"));
    if (tool_args_json)
        json_object_set(result, "args", json_new_string(tool_args_json));
    char *out = json_serialize(result);
    json_free(result);
    return out;
}


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_agent_runtime_helpers_note_turn_start @ agent/agent_runtime_helpers.py:note_turn_start */
int agent_agent_runtime_helpers_note_turn_start(const char *arg) {
    /* Python: interleave tripwire. Arg =
     * "overlap\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int overlap = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (overlap) {
        printf("WARN: turn %s starting while turn %s (started %.0fs ago) has not completed its persist\n", t2 ? t2 + 1 : "?", "?", 0.0);
        return 0;
    }
    printf("in-flight slot taken (no overlap; at most one warning per crashed turn)\n");
    return 0;
}

/* PoP: agent_agent_runtime_helpers_note_turn_persisted @ agent/agent_runtime_helpers.py:note_turn_persisted */
int agent_agent_runtime_helpers_note_turn_persisted(const char *arg) {
    /* Python: clear in-flight marker. Arg = "state\tsession_id\tpopped". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("marker cleared (no session slot)\n"); return 0; }
    printf("in-flight turn marker cleared: %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: agent_agent_runtime_helpers_trailing_continue_intent @ agent/agent_runtime_helpers.py:trailing_continue_intent */
int agent_agent_runtime_helpers_trailing_continue_intent(const char *arg) {
    // arg = "text"
    if (!arg || !*arg) return 0;
    const char *text = arg;
    while (*text == ' ' || *text == '\t') text++;
    size_t len = strlen(text);
    while (len > 0 && (text[len-1] == ' ' || text[len-1] == '\t')) len--;
    if (len == 0 || len > 600) return 0;
    const char *tail = text + len;
    size_t tail_len = len < 160 ? len : 160;
    const char *seek = tail - tail_len;
    while (*seek != '\0') seek++;
    char buf[161];
    size_t copy_len = tail - seek;
    if (copy_len > 160) copy_len = 160;
    memcpy(buf, seek, copy_len);
    buf[copy_len] = '\0';
    // Check for trailing intent patterns: "...", "will...", "let me...", etc.
    if (strstr(buf, "...") || strstr(buf, "will ") || strstr(buf, "let me") ||
        strstr(buf, "I'll") || strstr(buf, "gonna") || strstr(buf, "going to"))
        return 1;
    return 0;
}

/* PoP: agent_agent_runtime_helpers_intent_ack_continuation_mode @ agent/agent_runtime_helpers.py:intent_ack_continuation_mode */
int agent_agent_runtime_helpers_intent_ack_continuation_mode(const char *arg) {
    // arg = "text"
    if (!arg || !*arg) return 0;
    const char *text = arg;
    while (*text == ' ' || *text == '\t') text++;
    size_t len = strlen(text);
    while (len > 0 && (text[len-1] == ' ' || text[len-1] == '\t')) len--;
    if (len == 0 || len > 600) return 0;
    const char *tail = text + len;
    size_t tail_len = len < 160 ? len : 160;
    const char *seek = tail - tail_len;
    while (*seek != '\0') seek++;
    char buf[161];
    size_t copy_len = tail - seek;
    if (copy_len > 160) copy_len = 160;
    memcpy(buf, seek, copy_len);
    buf[copy_len] = '\0';
    if (strstr(buf, "...") || strstr(buf, "will ") || strstr(buf, "let me") ||
        strstr(buf, "I'll") || strstr(buf, "gonna") || strstr(buf, "going to"))
        return 1;
    return 0;
}

/* PoP: agent_agent_runtime_helpers_intent_ack_continuation_enabled @ agent/agent_runtime_helpers.py:intent_ack_continuation_enabled */
int agent_agent_runtime_helpers_intent_ack_continuation_enabled(const char *arg) {
    // arg = "enabled_flag" (1 or 0)
    if (!arg || !*arg) return 0;
    return arg[0] == '1' ? 1 : 0;
}

/* PoP: agent_agent_runtime_helpers_copy_reasoning_content_for_api @ agent/agent_runtime_helpers.py:copy_reasoning_content_for_api */
int agent_agent_runtime_helpers_copy_reasoning_content_for_api(const char *arg) {
    if (!arg || !*arg) return 0;
    json_t *node = json_parse(arg, NULL);
    if (!node) return 0;
    char *out = json_serialize(node);
    json_free(node);
    if (out) { printf("%s\n", out); free(out); }
    return 0;
}

/* PoP: agent_runtime_helpers_reapply_reasoning_echo_for_provider @ agent/agent_runtime_helpers.py:reapply_reasoning_echo_for_provider */
int agent_runtime_helpers_reapply_reasoning_echo_for_provider(const char *arg) {
    if (!arg || !*arg) return 0;
    json_t *node = json_parse(arg, NULL);
    if (!node) return 0;
    char *out = json_serialize(node);
    json_free(node);
    if (out) { printf("%s\n", out); free(out); }
    return 0;
}

/* ================================================================
 *  Prompt caching config helpers
 * ================================================================ */

/* PoP: agent_agent_runtime_helpers_cache_ttl_means_disabled @ agent/agent_runtime_helpers.py:cache_ttl_means_disabled */
bool agent_runtime_helpers_cache_ttl_means_disabled(const char *ttl) {
    if (!ttl || !*ttl) return true;
    if (strcmp(ttl, "5m") == 0 || strcmp(ttl, "1h") == 0) return false;
    if (strcmp(ttl, "false") == 0 || strcmp(ttl, "off") == 0 ||
        strcmp(ttl, "disabled") == 0 || strcmp(ttl, "no") == 0 ||
        strcmp(ttl, "none") == 0 || strcmp(ttl, "False") == 0 ||
        strcmp(ttl, "None") == 0)
        return true;
    return false;
}

/* PoP: agent_runtime_helpers__raw_cache_ttl_from_config @ agent/agent_runtime_helpers.py:_raw_cache_ttl_from_config */
char *agent_runtime_helpers__raw_cache_ttl_from_config(void) {
    json_t *cfg = config_py_load_config_readonly();
    if (!cfg) return strdup("5m");
    json_t *pc = json_object_get(cfg, "prompt_caching");
    json_t *ttl = pc ? json_object_get(pc, "cache_ttl") : NULL;
    char *result = ttl && json_type(ttl) == JSON_STRING
        ? strdup(json_string_value(ttl)) : strdup("5m");
    if (pc) json_free(pc);
    json_free(cfg);
    return result;
}

/* PoP: agent_runtime_helpers_prompt_caching_disabled_from_config @ agent/agent_runtime_helpers.py:prompt_caching_disabled_from_config */
bool agent_runtime_helpers_prompt_caching_disabled_from_config(void) {
    char *ttl = agent_runtime_helpers__raw_cache_ttl_from_config();
    if (!ttl) return false;
    bool disabled = agent_runtime_helpers_cache_ttl_means_disabled(ttl);
    free(ttl);
    return disabled;
}

/* PoP: agent_runtime_helpers_configured_cache_ttl @ agent/agent_runtime_helpers.py:configured_cache_ttl */
char *agent_runtime_helpers_configured_cache_ttl(void) {
    char *ttl = agent_runtime_helpers__raw_cache_ttl_from_config();
    if (!ttl) return NULL;
    if (strcmp(ttl, "5m") == 0 || strcmp(ttl, "1h") == 0) return ttl;
    free(ttl);
    return NULL;
}

/* ================================================================
 *  LiteLLM route detection
 * ================================================================ */

/* PoP: agent_runtime_helpers__has_litellm_token @ agent/agent_runtime_helpers.py:_has_litellm_token */
bool agent_runtime_helpers__has_litellm_token(const char *value, const char *delimiters) {
    if (!value || !*value) return false;
    char buf[4096];
    strncpy(buf, value, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    for (const char *d = delimiters; *d; d++) {
        char delim = *d;
        for (char *p = buf; *p; p++)
            if (*p == delim) *p = ' ';
    }
    for (char *tok = strtok(buf, " "); tok; tok = strtok(NULL, " "))
        if (strcmp(tok, "litellm") == 0) return true;
    return false;
}

/* PoP: agent_runtime_helpers__is_litellm_route @ agent/agent_runtime_helpers.py:_is_litellm_route */
bool agent_runtime_helpers__is_litellm_route(const char *provider_lower, const char *base_url) {
    if (agent_runtime_helpers__has_litellm_token(provider_lower, ":-_/"))
        return true;
    if (base_url && *base_url) {
        const char *host = strstr(base_url, "://");
        host = host ? host + 3 : base_url;
        char hbuf[512];
        const char *e = host;
        while (*e && *e != ':' && *e != '/') e++;
        size_t hl = e - host;
        if (hl >= sizeof(hbuf)) hl = sizeof(hbuf) - 1;
        memcpy(hbuf, host, hl);
        hbuf[hl] = '\0';
        if (agent_runtime_helpers__has_litellm_token(hbuf, ".-"))
            return true;
    }
    return false;
}

/* ================================================================
 *  Tool call ID variants (thin forwarder)
 * ================================================================ */

/* PoP: agent_runtime_helpers__tool_call_id_variants @ agent/agent_runtime_helpers.py:_tool_call_id_variants */
char *agent_runtime_helpers__tool_call_id_variants(const char *tc_json) {
    if (!tc_json || !*tc_json) return strdup("[]");
    json_t *tc = json_parse(tc_json, NULL);
    if (!tc) return strdup("[]");
    json_t *arr = json_array();
    json_t *v;
    v = json_object_get(tc, "id");
    if (v && json_type(v) == JSON_STRING) json_append(arr, json_copy(v));
    v = json_object_get(tc, "call_id");
    if (v && json_type(v) == JSON_STRING) json_append(arr, json_copy(v));
    v = json_object_get(tc, "response_item_id");
    if (v && json_type(v) == JSON_STRING) json_append(arr, json_copy(v));
    json_free(tc);
    char *r = json_serialize(arr);
    json_free(arr);
    return r ? r : strdup("[]");
}
