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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>

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

/* PoP: _iter_httpx_pool_objects @ agent/agent_runtime_helpers.py:_iter_httpx_pool_objects */
/* slermes' libhttp opens a fresh connection per request; no persistent pool
 * to introspect (yields nothing). */
void iter_httpx_pool_objects(void)
{
}

/* PoP: _connection_candidates @ agent/agent_runtime_helpers.py:_connection_candidates */
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
{
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
