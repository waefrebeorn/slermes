/**
 * @file plugin_llm.h
 * @brief Plugin LLM facade — host-owned LLM access for trusted plugins.
 *
 * Port of Python agent/plugin_llm.py (1046 lines).
 * Provides trust-gated LLM completion for plugins. Each plugin's ability
 * to override provider, model, agent_id, and profile is governed by
 * config.yaml: plugins.entries.<id>.llm.*
 *
 * MIT License — WuBu Slermes Project
 */
#ifndef PLUGIN_LLM_H
#define PLUGIN_LLM_H

#include "hermes_core_types.h"
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Token usage (Port of Python PluginLlmUsage)
 * ================================================================ */

typedef struct {
    int  input_tokens;
    int  output_tokens;
    int  total_tokens;
    int  cache_read_tokens;
    int  cache_write_tokens;
    double cost_usd;          /* NULL sentinel: -1.0 */
} plugin_llm_usage_t;

/* ================================================================
 *  Completion result (Port of Python PluginLlmCompleteResult)
 * ================================================================ */

typedef struct {
    char            *text;          /* malloc'd response text */
    char            *provider;      /* malloc'd provider name */
    char            *model;         /* malloc'd model name */
    char            *agent_id;      /* malloc'd agent id */
    plugin_llm_usage_t usage;
    json_node_t     *audit;         /* JSON object, may be NULL */
} plugin_llm_result_t;

/* ================================================================
 *  Structured result (Port of Python PluginLlmStructuredResult)
 * ================================================================ */

typedef struct {
    char            *text;          /* malloc'd response text */
    char            *provider;      /* malloc'd provider name */
    char            *model;         /* malloc'd model name */
    char            *agent_id;      /* malloc'd agent id */
    plugin_llm_usage_t usage;
    json_node_t     *parsed;        /* Parsed JSON, may be NULL */
    char            *content_type;  /* "json" or "text", malloc'd */
    json_node_t     *audit;         /* JSON object, may be NULL */
} plugin_llm_structured_result_t;

/* ================================================================
 *  Trust policy (Port of Python _TrustPolicy)
 * ================================================================ */

typedef struct {
    char    plugin_id[128];
    bool    allow_provider_override;
    bool    allow_model_override;
    bool    allow_agent_id_override;
    bool    allow_profile_override;
    char    allowed_providers[1024];  /* comma-separated, "*" = any */
    char    allowed_models[1024];     /* comma-separated, "*" = any */
    bool    allow_any_provider;
    bool    allow_any_model;
} plugin_llm_trust_policy_t;

/* ================================================================
 *  Input block types (Port of Python PluginLlmTextInput/ImageInput)
 * ================================================================ */

#define PLUGIN_LLM_INPUT_TYPE_TEXT  0
#define PLUGIN_LLM_INPUT_TYPE_IMAGE 1

typedef struct {
    int    type;              /* PLUGIN_LLM_INPUT_TYPE_TEXT or _IMAGE */
    char  *text;              /* text content (for text type) */
    char  *url;               /* image URL (for image type) */
    char  *data;              /* base64-encoded image data */
    size_t data_len;          /* length of base64 data */
    char  *mime_type;         /* MIME type for image */
    char  *file_name;         /* optional file name */
} plugin_llm_input_t;

/* ================================================================
 *  Trust policy resolution
 * ================================================================ */

/**
 * Resolve trust policy for a plugin from a JSON config object.
 * The JSON should match the shape of plugins.entries.<id>.llm from config.yaml.
 * When config_json is NULL, returns a fully restrictive (deny-all) policy.
 *
 * Port of Python: _resolve_trust_policy() + _coerce_allowlist()
 */
plugin_llm_trust_policy_t resolve_trust_policy(
    const char *plugin_id,
    const json_node_t *config_json);

/**
 * Check overrides against the trust policy.
 * Modifies the override pointers:
 *   - Sets to NULL if not allowed (reverting to defaults)
 *   - Leaves unchanged if allowed
 * Returns 0 on success, -1 on trust error with err_msg set.
 *
 * Port of Python: _check_overrides()
 */
int check_overrides(
    const plugin_llm_trust_policy_t *policy,
    const char **provider,
    const char **model,
    const char **agent_id,
    const char **profile,
    char *err_msg, size_t err_sz);

/* ================================================================
 *  Message building helpers
 * ================================================================ */

/**
 * Build a structured message list suitable for llm_chat_completion.
 * Returns a JSON array of message objects. Caller must free with json_free().
 *
 * Port of Python: _build_structured_messages()
 */
json_node_t *plugin_llm_build_structured_messages(
    const char *instructions,
    const plugin_llm_input_t *inputs,
    int input_count,
    bool json_mode,
    const json_node_t *json_schema,
    const char *schema_name,
    const char *system_prompt);

/* ================================================================
 *  Response extraction helpers
 * ================================================================ */

/**
 * Extract token usage from an llm_response_t.
 *
 * Port of Python: _extract_usage()
 */
plugin_llm_usage_t extract_usage(const llm_response_t *response);

/**
 * Extract text content from an llm_response_t.
 * Returns malloc'd string. Caller must free.
 *
 * Port of Python: _extract_text()
 */
char *extract_text(const llm_response_t *response);

/* ================================================================
 *  Main API — completion
 * ================================================================ */

/**
 * Run a host-owned chat completion for a plugin.
 *
 * @param plugin_id     Plugin identifier (for trust policy lookup)
 * @param llm_cfg       Pre-resolved llm_config_t (from resolve_task)
 * @param messages      JSON array of message objects
 * @param provider      Provider override (NULL = use default)
 * @param model         Model override (NULL = use default)
 * @param agent_id      Agent ID override (NULL = "default")
 * @param profile       Profile override (NULL = use default)
 * @param temperature   Temperature override (-1.0 = use default)
 * @param max_tokens    Max tokens override (0 = use default)
 * @param extra_body    Extra JSON body fields (may be NULL)
 * @param policy_json   Plugin LLM config JSON (for trust policy, may be NULL)
 * @param err_msg       Error message buffer (may be NULL)
 * @param err_sz        Size of err_msg buffer
 *
 * @return malloc'd result, or NULL on error. Caller must free with
 *         plugin_llm_result_free().
 *
 * Port of Python: PluginLlm.complete() + _invoke_sync()
 */
plugin_llm_result_t *plugin_llm_complete(
    const char *plugin_id,
    llm_config_t *llm_cfg,
    json_node_t *messages,
    const char *provider,
    const char *model,
    const char *agent_id,
    const char *profile,
    double temperature,
    int max_tokens,
    json_node_t *extra_body,
    json_node_t *policy_json,
    char *err_msg, size_t err_sz);

/**
 * Run a host-owned structured completion for a plugin.
 *
 * Same parameters as plugin_llm_complete() plus structured-specific fields.
 *
 * Port of Python: PluginLlm.complete_structured()
 */
plugin_llm_structured_result_t *plugin_llm_complete_structured(
    const char *plugin_id,
    llm_config_t *llm_cfg,
    const char *instructions,
    const plugin_llm_input_t *inputs,
    int input_count,
    bool json_mode,
    const json_node_t *json_schema,
    const char *schema_name,
    const char *system_prompt,
    const char *provider,
    const char *model,
    const char *agent_id,
    const char *profile,
    double temperature,
    int max_tokens,
    json_node_t *policy_json,
    char *err_msg, size_t err_sz);

/* ================================================================
 *  Free functions
 * ================================================================ */

void plugin_llm_result_free(plugin_llm_result_t *result);
void plugin_llm_structured_result_free(plugin_llm_structured_result_t *result);
void plugin_llm_input_free(plugin_llm_input_t *input);

#ifdef __cplusplus
}
#endif

#endif /* PLUGIN_LLM_H */
