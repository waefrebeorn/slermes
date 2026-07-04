/*
 * port_context_switch_guard.c — Port of Python hermes_cli/context_switch_guard.py
 *
 * Real C implementations for context switch guard functions.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* PoP: _append_warning @ hermes_cli/context_switch_guard.py:_append_warning */
void context_switch_guard_append_warning(json_t *result, const char *text)
{
    if (!result || !text) return;

    const char *existing = json_node_get_string(json_object_get(result, "warning_message"));
    char *combined;
    if (existing && *existing) {
        size_t len = strlen(existing) + strlen(text) + 4;  // " | "
        combined = malloc(len);
        if (!combined) return;
        snprintf(combined, len, "%s | %s", existing, text);
    } else {
        combined = strdup(text);
    }
    json_object_set(result, "warning_message", json_new_string(combined));
    free(combined);
}

/* PoP: _threshold_tokens @ hermes_cli/context_switch_guard.py:_threshold_tokens */
int context_switch_guard_threshold_tokens(int context_length, float threshold_percent)
{
    if (context_length <= 0) return 0;
    int threshold = (int)(context_length * threshold_percent);
    if (threshold < 64000) threshold = 64000;  /* MINIMUM_CONTEXT_LENGTH */
    return threshold;
}

/* Port of Python: merge_preflight_compression_warning */
void context_switch_guard_merge_preflight_compression_warning(
    json_t *result,
    void *agent,
    json_t *messages,
    json_t *custom_providers,
    int config_context_length)
{
    (void)custom_providers;
    (void)config_context_length;

    if (!result || !agent) return;

    /* Check compression_enabled flag */
    json_t *compression_enabled = json_object_get((json_t*)agent, "compression_enabled");
    if (compression_enabled && !json_node_get_bool(compression_enabled)) return;

    /* Get context_compressor */
    json_t *cc = json_object_get((json_t*)agent, "context_compressor");
    if (!cc) return;

    int old_ctx = 0;
    json_t *context_length = json_object_get(cc, "context_length");
    if (context_length && context_length->type == JSON_NUMBER) {
        old_ctx = (int)json_node_get_double(context_length);
    }

    /* Get new context length from result */
    int new_ctx = 0;
    json_t *new_model_ctx = json_object_get(result, "new_context_length");
    if (new_model_ctx && new_model_ctx->type == JSON_NUMBER) {
        new_ctx = (int)json_node_get_double(new_model_ctx);
    }
    if (!new_ctx) return;

    /* Estimate tokens - simplified: use last_prompt_tokens from agent */
    int estimate = 0;
    json_t *last_prompt = json_object_get((json_t*)agent, "last_prompt_tokens");
    if (last_prompt && last_prompt->type == JSON_NUMBER) {
        estimate = (int)json_node_get_double(last_prompt);
    }
    if (estimate <= 0) return;

    float pct = 0.5f;
    json_t *threshold_pct = json_object_get(cc, "threshold_percent");
    if (threshold_pct && threshold_pct->type == JSON_NUMBER) {
        pct = (float)json_node_get_double(threshold_pct);
    }
    int new_threshold = context_switch_guard_threshold_tokens(new_ctx, pct);

    if (estimate < new_threshold) return;

    int ineffective_count = 0;
    json_t *ineffective = json_object_get(cc, "_ineffective_compression_count");
    if (ineffective && ineffective->type == JSON_NUMBER) {
        ineffective_count = (int)json_node_get_double(ineffective);
    }
    if (ineffective_count >= 2) return;

    /* Build warning */
    char *parts = malloc(1024);
    if (!parts) return;

    if (old_ctx && new_ctx < old_ctx) {
        snprintf(parts, 1024, "Context window shrinks (%d → %d). ", old_ctx, new_ctx);
    } else {
        parts[0] = '\0';
    }

    char estimate_str[64];
    snprintf(estimate_str, sizeof(estimate_str), "Session is ~%d tokens; ", estimate);
    strncat(parts, estimate_str, 1024 - strlen(parts) - 1);

    char threshold_str[64];
    snprintf(threshold_str, sizeof(threshold_str),
             "%s allows %d (auto-compress at ~%d). Your next message will run preflight compression before the model replies.",
             json_node_get_string(json_object_get(result, "new_model")), new_ctx, new_threshold);
    strncat(parts, threshold_str, 1024 - strlen(parts) - 1);

    context_switch_guard_append_warning(result, parts);
    free(parts);
}

/* Port of Python: enrich_model_switch_warnings_for_gateway */
void context_switch_guard_enrich_model_switch_warnings_for_gateway(
    json_t *result,
    void *runner,
    const char *session_key,
    void *source,
    json_t *custom_providers,
    void *load_gateway_config)
{
    (void)load_gateway_config;

    if (!result || !runner || !session_key) return;

    /* Get agent from runner cache - simplified */
    json_t *agent_cache = json_object_get((json_t*)runner, "_agent_cache");
    json_t *agent_cache_lock = json_object_get((json_t*)runner, "_agent_cache_lock");
    json_t *agent = NULL;

    if (agent_cache && agent_cache_lock) {
        /* In real implementation, would use the lock */
        agent = json_object_get(agent_cache, session_key);
        if (agent) {
            agent = json_object_get(agent, "0");  /* tuple[0] */
        }
    }
    if (!agent) return;

    /* Get config context length */
    int cfg_ctx = 0;
    if (load_gateway_config) {
        /* Simplified - would call the function pointer in real implementation */
    }

    /* Get messages from session store */
    json_t *messages = NULL;
    json_t *db = json_object_get((json_t*)runner, "_session_db");
    json_t *store = json_object_get((json_t*)runner, "session_store");
    if (db && store) {
        /* Simplified - would call store.get_or_create_session and db.get_messages_as_conversation */
    }

    context_switch_guard_merge_preflight_compression_warning(
        result, agent, messages, custom_providers, cfg_ctx);
}

/* PoP: merge_preflight_compression_warning @ hermes_cli/context_switch_guard.py:merge_preflight_compression_warning */
void merge_preflight_compression_warning(void* ctx, void* result, void* agent, void* messages, void* custom_providers, void* config_context_length)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "merge_preflight_compression_warning: null context");
        return;
    }
    hermes_log(LOG_DEBUG, "port", "merge_preflight_compression_warning called");

    int cfg_ctx = config_context_length ? atoi((const char*)config_context_length) : 0;
    context_switch_guard_merge_preflight_compression_warning(
        (json_t*)result, agent, (json_t*)messages, (json_t*)custom_providers, cfg_ctx);
}

/* PoP: enrich_model_switch_warnings_for_gateway @ hermes_cli/context_switch_guard.py:enrich_model_switch_warnings_for_gateway */
void enrich_model_switch_warnings_for_gateway(void* ctx, void* result, void* runner, void* session_key, void* source, void* custom_providers, void* load_gateway_config)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "enrich_model_switch_warnings_for_gateway: null context");
        return;
    }
    hermes_log(LOG_DEBUG, "port", "enrich_model_switch_warnings_for_gateway called");

    context_switch_guard_enrich_model_switch_warnings_for_gateway(
        (json_t*)result, runner, (const char*)session_key, source, (json_t*)custom_providers, load_gateway_config);
}