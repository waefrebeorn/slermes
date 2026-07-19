/*
 * provider.h — Abstract provider interface for Hermes C.
 * Phase 101-110: Multi-provider support.
 *
 * Each provider implements:
 *  - build_url: construct the API endpoint URL
 *  - build_headers: construct auth + content-type headers
 *  - parse_response: extract content, reasoning, tool_calls from response JSON
 *  - build_message_body: construct the JSON message body (provider-specific format)
 */

/**
 * @defgroup providers Provider System
 * @brief Provider operations tables and registration.
 *
 * Defines provider_ops_t interface (build_url, build_headers,
 * build_request_body, parse_response, parse_stream_chunk, free_response)
 * and the provider registration system (max 32 providers).
 *
 * Implementations: OpenAI, OpenRouter, DeepSeek, xAI, Anthropic,
 * Google, Azure, Bedrock, Custom.
 *
 * @{
 */
#ifndef PROVIDER_H
#define PROVIDER_H

#include "hermes.h"
#include "hermes_json.h"

#include "credential_pool.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Provider Interface
 * ================================================================ */

/* Provider identification */
typedef enum {
    PROVIDER_OPENAI,       /* OpenAI-compatible (OpenAI, Groq, Together, etc.) */
    PROVIDER_ANTHROPIC,    /* Anthropic API format */
    PROVIDER_GOOGLE,       /* Google AI / Gemini */
    PROVIDER_OPENROUTER,   /* OpenRouter — model routing with preferences */
    PROVIDER_DEEPSEEK,     /* DeepSeek — context caching, FIM */
    PROVIDER_XAI,          /* xAI — Grok API */
    PROVIDER_AZURE,        /* Azure OpenAI — api-key auth, deployment URL */
    PROVIDER_BEDROCK,      /* AWS Bedrock — SigV4 auth, Converse API */
    PROVIDER_CUSTOM,       /* Custom provider */
    PROVIDER_CODEX,        /* OpenAI Responses API — Codex, xAI, GitHub Models */
} provider_type_t;

/* Opaque provider handle */
typedef struct provider_t provider_t;

/* Response parsed from provider-specific format */
typedef struct {
    char *content;
    char *reasoning;
    char *encrypted_content;  /* L07: xAI encrypted reasoning content */
    int   input_tokens;
    int   output_tokens;
    int   tool_calls_count;
    tool_call_t tool_calls[64];
    char  finish_reason[32];   /* B22: "stop", "length", "tool_calls", "content_filter" */
} provider_response_t;

/* Provider operations (function pointers) */
typedef struct {
    /* Build the API URL. Returns malloc'd string or NULL. */
    char *(*build_url)(const provider_t *p, const char *base_url);

    /* Build HTTP headers. Returns malloc'd string or NULL. */
    char *(*build_headers)(const provider_t *p, const char *api_key);

    /* Build the JSON request body. Returns malloc'd string or NULL. */
    char *(*build_request_body)(const provider_t *p,
                                const message_t **messages, size_t msg_count,
                                json_node_t *tools_json,
                                bool streaming);

    /* Parse API response into common format. Returns provider_response. */
    provider_response_t *(*parse_response)(const provider_t *p,
                                            const char *response_body);

    /* Parse streaming chunk. Returns provider_response with partial content. */
    provider_response_t *(*parse_stream_chunk)(const provider_t *p,
                                                const char *chunk);

    /* Free a provider response */
    void (*free_response)(provider_response_t *resp);

    /* B32: FIM (Fill-in-the-Middle) — optional, NULL = not supported.
     * Build FIM request body with prompt + suffix for code completion.
     * Returns malloc'd JSON string or NULL. */
    char *(*build_fim_body)(const provider_t *p,
                            const char *prompt,
                            const char *suffix,
                            int max_tokens);

    /* B32: Parse FIM response (returns text in content field, not message.content).
     * Returns provider_response with content set to FIM completion text. */
    provider_response_t *(*parse_fim_response)(const provider_t *p,
                                                const char *response_body);

    /* B32: Build FIM endpoint URL (e.g., /beta/completions instead of /chat/completions).
     * Returns malloc'd URL or NULL. Defaults to build_url if NULL. */
    char *(*build_fim_url)(const provider_t *p, const char *base_url);

    /* Provider name */
    const char *name;
} provider_ops_t;

/* Provider instance */
struct provider_t {
    provider_type_t type;
    const provider_ops_t *ops;
    char name[64];
    char model[128];
    char api_key[2048];
    char base_url[512];
    void *data; /* Provider-specific data */
    credential_pool_t *pool; /* P82: optional credential pool for multi-key rotation */
    bool  system_cached; /* P91: system prompt cache primed flag */
    /* LLM request params wired from config */
    provider_config_t config;
    /* PR04: env var fallback — alternative env var names for API key lookup */
    const char *env_vars[8]; /* NULL-terminated array of env var names */
    /* PR05: auth type — how this provider authenticates */
    int auth_type; /* 0=api_key, 1=oauth, 2=aws_sdk, 3=copilot, 4=bearer */
    /* PR06: display metadata for provider selection UI */
    char display_name[128];
    char signup_url[512];
    /* PR07: default aux model — cheap model for subtasks (compression, etc.) */
    char default_aux_model[128];
};

/* ================================================================
 *  Provider Registry
 * ================================================================ */

/* Register a provider implementation */
void register_provider(provider_type_t type, const provider_ops_t *ops);
int provider_get_count(void);

/* Create a provider instance from config */
provider_t *provider_create(const char *provider_name,
                             const char *model,
                             const char *api_key,
                             const char *base_url);

/* Free a provider instance */
void provider_free(provider_t *p);

/* Attach a credential pool to a provider instance.
 * The pool is NOT owned by the provider — caller must free it separately
 * after provider_free(). Use NULL to detach. */
void provider_set_credential_pool(provider_t *p, credential_pool_t *pool);

/* Get the credential pool attached to this provider (may be NULL). */
credential_pool_t *provider_get_credential_pool(const provider_t *p);

/* B32: FIM (Fill-in-the-Middle) code completion call.
 * Returns provider_response with content set to FIM completion text.
 * Returns NULL if provider does not support FIM.
 * Caller must free response with provider_free_response(). */
provider_response_t *provider_fim(provider_t *p,
                                   const char *prompt,
                                   const char *suffix,
                                   int max_tokens);

/* Check if provider supports FIM */
bool provider_has_fim(const provider_t *p);

/* Get provider operations (convenience) */
static inline const provider_ops_t *provider_ops(const provider_t *p) {
    return p ? p->ops : NULL;
}

/* P91: System prompt caching — set whether cache has been primed */
static inline void provider_set_system_cached(provider_t *p, bool cached) {
    if (p) p->system_cached = cached;
}

/* P91: Check if system prompt cache is primed */
static inline bool provider_get_system_cached(const provider_t *p) {
    return p ? p->system_cached : false;
}

/* ================================================================
 *  Built-in Provider Implementations
 * ================================================================ */

/* OpenAI-compatible (covers OpenAI, DeepSeek, OpenRouter, Groq, etc.) */
extern const provider_ops_t PROVIDER_OPS_OPENAI;

/* OpenRouter with model routing and provider preferences */
extern const provider_ops_t PROVIDER_OPS_OPENROUTER;

/* DeepSeek with context caching and FIM support */
extern const provider_ops_t PROVIDER_OPS_DEEPSEEK;

/* xAI (Grok) with native API support */
extern const provider_ops_t PROVIDER_OPS_XAI;

/* Anthropic API format */
extern const provider_ops_t PROVIDER_OPS_ANTHROPIC;

/* Google Gemini API format */
extern const provider_ops_t PROVIDER_OPS_GOOGLE;

/* Azure OpenAI API format */
extern const provider_ops_t PROVIDER_OPS_AZURE;

/* AWS Bedrock Converse API */
extern const provider_ops_t PROVIDER_OPS_BEDROCK;

/* Bedrock utility functions — ported from Python bedrock_adapter.py */
bool bedrock_is_context_overflow(const char *error_message);
const char *classify_bedrock_error(const char *error_message);
char *bedrock_extract_provider_from_arn(const char *arn);
int  get_bedrock_context_length(const char *model_id);
bool is_anthropic_bedrock_model(const char *model_id);
/* Port of Python: model_supports_tool_use */
bool model_supports_tool_use(const char *model_id);
const char *resolve_aws_auth_env_var(void);
bool has_aws_credentials(void);
const char *resolve_bedrock_region(void);
json_t *bedrock_convert_tools_to_converse(const json_t *tools);
json_t *bedrock_convert_content_to_converse(const json_t *content);
json_t *bedrock_convert_messages_to_converse(const json_t *messages);
json_t *bedrock_normalize_converse_response(const json_t *response);

/* Google provider utility functions */
bool google_is_native_base_url(const char *base_url);
char *google_coerce_content_to_text(const json_t *content);
char *google_tool_call_extra_signature(const json_t *tool_call);
json_t *translate_tool_call_to_gemini(const json_t *tool_call);
json_t *translate_tool_result_to_gemini(const json_t *message, const json_t *tool_name_by_call_id);
json_t *google_translate_tools_to_gemini(const json_t *tools);
json_t *google_translate_tool_choice_to_gemini(const json_t *tool_choice);
json_t *google_normalize_thinking_config(const json_t *config);
json_t *google_extract_multimodal_parts(const json_t *content);
json_t *google_tool_call_extra_from_part(const json_t *part);
json_t *google_build_gemini_contents(const json_t *messages);

/* Port of Python gemini_native_adapter.py:gemini_http_error().
 * Generate structured error info from a Gemini API HTTP response.
 * Returns JSON object with: code, message, retry_after, details.
 * Caller must free with json_free(). */
json_t *gemini_http_error(int status_code, const char *body_text);

/* Port of Python gemini_native_adapter.py:probe_gemini_tier().
 * Probe a Google AI Studio API key and return its tier.
 * Returns a string literal: "free", "paid", or "unknown".
 * "unknown" means the probe failed; callers should proceed without blocking. */
const char *google_probe_gemini_tier(const char *api_key,
                                      const char *base_url,
                                      const char *model,
                                      int timeout_sec);
/* Port of Python gemini_native_adapter.py:is_free_tier_quota_error().
 * Return True when a Gemini 429 message indicates free-tier exhaustion. */
bool google_is_free_tier_quota_error(const char *error_message);

/* Port of Python gemini_native_adapter.py:build_gemini_request().
 * Build a Gemini native API request JSON from OpenAI-style arguments.
 * Returns a JSON string (caller must free). */
char *google_build_gemini_request(const json_t *messages,
                                   const json_t *tools,
                                   const json_t *tool_choice,
                                   double temperature,
                                   int max_tokens,
                                   double top_p,
                                   const json_t *stop,
                                   const json_t *thinking_config);

/* Port of Python gemini_native_adapter.py:translate_gemini_response().
 * Translates a Gemini native API response into an OpenAI-compatible
 * chat completion response JSON string (caller must free). */
char *google_translate_gemini_response(const json_t *resp, const char *model);

/* Port of Python gemini_native_adapter.py:translate_stream_event().
 * Translate a Gemini SSE stream event into a JSON array of stream chunk
 * strings (caller must free). Each chunk is an object. */
char *google_translate_stream_event(const json_t *event, const char *model);

/* Helper: make an empty response JSON for a given model (caller must free). */
char *google_empty_response(const char *model);


/* Custom (user-defined) provider */
extern const provider_ops_t PROVIDER_OPS_CUSTOM;

/* Custom-provider request-shaping helpers (port of agent/agent_init.py). */
char *custom_normalized_base_url(const char *value);
bool custom_provider_model_matches(const char *agent_model, const json_t *entry);
json_t *custom_provider_extra_body_for_agent(const char *provider,
                                           const char *model,
                                           const char *base_url,
                                           const json_t *custom_providers);
json_t *custom_merge_extra_body(const char *provider, const char *model,
                               const char *base_url,
                               const json_t *custom_providers,
                               const json_t *existing_extra_body);

/* Codex Responses API provider */
extern const provider_ops_t PROVIDER_OPS_CODEX;

/* Anthropic provider utility functions — ported from Python anthropic_adapter.py */
/* Port of Python: is_oauth_token */
bool is_oauth_token(const char *key);
char *anthropic_normalize_base_url_text(const char *base_url);
bool anthropic_is_third_party_endpoint(const char *base_url);
/* Port of Python: is_kimi_coding_endpoint */
bool is_kimi_coding_endpoint(const char *base_url);
/* Port of Python: model_name_is_kimi_family */
bool model_name_is_kimi_family(const char *model);
/* Port of Python: is_kimi_family_endpoint */
bool is_kimi_family_endpoint(const char *base_url, const char *model);
bool anthropic_is_deepseek_endpoint(const char *base_url);
/* Port of Python: requires_bearer_auth */
bool requires_bearer_auth(const char *base_url);
bool anthropic_base_url_needs_1m_beta(const char *base_url);
bool anthropic_is_minimax_endpoint(const char *base_url);
/* Port of Python: is_azure_anthropic_endpoint */
bool is_azure_anthropic_endpoint(const char *base_url);
json_t *anthropic_common_betas_for_base_url(const char *base_url, bool drop_context_1m_beta);
/* Port of Python: is_bedrock_model_id */
bool is_bedrock_model_id(const char *model_id);
int  anthropic_resolve_positive_max_tokens(int value);

/* Port of Python anthropic_adapter.py:_resolve_anthropic_messages_max_tokens() */
int  resolve_anthropic_messages_max_tokens(int requested, const char *model);

/* Port of Python anthropic_adapter.py:get_anthropic_max_output() */
int  anthropic_get_model_max_output(const char *model);

/* Port of Python anthropic_adapter.py:normalize_model_name() */
char *normalize_model_name(const char *model, bool preserve_dots);

/* Port of Python anthropic_adapter.py:_sanitize_tool_id() */
char *anthropic_sanitize_tool_id(const char *tool_id);

/* Port of Python anthropic_adapter.py:resolve_anthropic_token() */
char *resolve_anthropic_token(void);

/* Port of Python anthropic_adapter.py:is_claude_code_token_valid() */
bool is_claude_code_token_valid(json_t *creds);

/* Port of Python anthropic_adapter.py:read_hermes_oauth_credentials() */
json_t *read_hermes_oauth_credentials(void);

/* Port of Python anthropic_adapter.py:_image_source_from_openai_url() */
json_t *image_source_from_openai_url(const char *url);

/* Port of Python anthropic_adapter.py:_evict_old_screenshots() */
void evict_old_screenshots(json_t *result);

/* Port of Python anthropic_adapter.py:_strip_orphaned_tool_blocks() */
void strip_orphaned_tool_blocks(json_t *result);

/* Port of Python anthropic_adapter.py:_merge_consecutive_roles() */
json_t *anthropic_merge_consecutive_roles(json_t *result);

/* Port of Python anthropic_adapter.py:_convert_content_part_to_anthropic().
 * Convert a single OpenAI-style content part to Anthropic format.
 * Returns json_t* (caller must json_free), or NULL on input null. */
json_t *anthropic_convert_content_part_to_anthropic(const json_t *part);

/* Port of Python anthropic_adapter.py:_convert_content_to_anthropic().
 * Convert an OpenAI-style multimodal content array to Anthropic blocks.
 * Returns new json array (caller must json_free). */
json_t *anthropic_convert_content_to_anthropic(const json_t *content);

/* Port of Python anthropic_adapter.py:_content_parts_to_anthropic_blocks().
 * Convert OpenAI-style tool-message content parts to Anthropic tool_result
 * inner blocks. Filters to text and image types only — excludes tool_use. */
json_t *anthropic_content_parts_to_anthropic_blocks(const json_t *parts);

/* Port of Python anthropic_adapter.py:_convert_user_message().
 * Validate and convert a user message to Anthropic format.
 * Returns new json object (caller must json_free). */
json_t *anthropic_convert_user_message(const json_t *content);

/* Port of Python anthropic_adapter.py:_extract_preserved_thinking_blocks().
 * Return Anthropic thinking blocks previously preserved on the message. */
json_t *anthropic_extract_preserved_thinking_blocks(const json_t *message);

/* Port of Python anthropic_adapter.py:_convert_assistant_message().
 * Convert an assistant message to Anthropic content blocks.
 * Handles thinking blocks, regular content, tool calls, and
 * reasoning_content injection for Kimi/DeepSeek endpoints.
 * Returns new json object (caller must json_free). */
json_t *anthropic_convert_assistant_message(const json_t *m);

/* Port of Python anthropic_adapter.py:convert_tools_to_anthropic().
 * Convert OpenAI tool definitions to Anthropic format.
 * Tools param: JSON array of tool dicts. Returns new json array (caller must json_free). */
json_t *anthropic_convert_tools_to_anthropic(const json_t *tools);

/* Port of Python anthropic_adapter.py:convert_messages_to_anthropic().
 * Convert OpenAI-format messages to Anthropic format.
 * Messages: JSON array of OpenAI-format message dicts.
 * system_out: set to json_string (string content), json_array (content blocks),
 *    or NULL (no system message). Caller must json_free.
 * messages_out: set to json_array of Anthropic-format dicts. Caller must json_free.
 * base_url/model: used for endpoint-specific thinking signature handling. */
void convert_messages_to_anthropic(const json_t *messages,
                                              const char *base_url,
                                              const char *model,
                                              json_t **system_out,
                                              json_t **messages_out);

/* Port of Python anthropic_adapter.py:_manage_thinking_signatures().
 * Strip or preserve thinking blocks based on endpoint type.
 * Mutates result in place. base_url and model determine endpoint-specific logic. */
void manage_thinking_signatures(json_t *result,
                                           const char *base_url,
                                           const char *model);

/* Port of Python anthropic_adapter.py:build_anthropic_kwargs().
 * Build kwargs dict for the Anthropic Messages API call.
 * Returns new json_t* dict (caller must json_free).
 * NOTE: OAuth-specific Claude Code transforms not implemented (C has no OAuth flow). */
json_t *anthropic_build_kwargs(
    const char *model,
    const json_t *messages,
    const json_t *tools,
    int max_tokens,
    const json_t *reasoning_config,
    const char *tool_choice,
    bool preserve_dots,
    int context_length,
    const char *base_url,
    bool fast_mode,
    bool drop_context_1m_beta);

/* Port of Python anthropic_adapter.py:_generate_pkce().
 * Generate PKCE code_verifier and S256 code_challenge.
 * Returns newly allocated strings via out-params (caller must free). */
/* Port of Python: generate_pkce */
void generate_pkce(char **verifier_out, char **challenge_out);

/* Port of Python anthropic_adapter.py:refresh_anthropic_oauth_pure().
 * Refresh an Anthropic OAuth token. Returns new json_t* dict (caller must json_free)
 * with access_token, refresh_token, expires_at_ms, or NULL on failure. */
json_t *anthropic_refresh_oauth(const char *refresh_token, bool use_json);

/* Port of Python anthropic_adapter.py:_refresh_oauth_token().
 * Attempt to refresh an expired OAuth token from credential dict.
 * Returns new access_token string or NULL. Caller must free. */
char *anthropic_refresh_oauth_token(json_t *creds);

/* Port of Python anthropic_adapter.py:_to_plain_data(). */
json_t *anthropic_to_plain_data(const json_t *value);

/* Port of Python anthropic_adapter.py:_get_anthropic_sdk(). */
const char *anthropic_get_sdk(void);

/* Port of Python anthropic_adapter.py:_detect_claude_code_version(). */
char *anthropic_detect_claude_code_version(void);

/* Port of Python anthropic_adapter.py:_get_claude_code_version(). */
const char *anthropic_get_claude_code_version(void);

/* Port of Python anthropic_adapter.py:_build_anthropic_client_with_bearer_hook(). */
char *anthropic_build_client_with_bearer_hook(const char *token_provider, const char *base_url, double timeout, bool drop_context_1m_beta);

/* Port of Python anthropic_adapter.py:build_anthropic_client(). */
char *anthropic_build_client(const char *api_key, const char *base_url, double timeout, bool drop_context_1m_beta);

/* Port of Python anthropic_adapter.py:build_anthropic_bedrock_client(). */
char *anthropic_build_bedrock_client(const char *region);

/* Port of Python anthropic_adapter.py:_read_claude_code_credentials_from_keychain(). */
json_t *anthropic_read_creds_from_keychain(void);

/* Port of Python anthropic_adapter.py:read_claude_code_credentials(). */
json_t *anthropic_read_claude_code_creds(void);

/* Port of Python anthropic_adapter.py:_write_claude_code_credentials(). */
void anthropic_write_claude_code_creds(const char *access_token, const char *refresh_token, long long expires_at_ms, json_t *scopes);

/* Port of Python anthropic_adapter.py:_resolve_claude_code_token_from_credentials(). */
char *anthropic_resolve_creds_token(json_t *creds);

/* Port of Python anthropic_adapter.py:_prefer_refreshable_claude_code_token(). */
char *anthropic_prefer_refreshable_token(const char *env_token, json_t *creds);

/* Port of Python anthropic_adapter.py:run_oauth_setup_token(). */
char *anthropic_run_oauth_setup(void);

/* Port of Python anthropic_adapter.py:run_hermes_oauth_login_pure(). */
json_t *anthropic_run_oauth_login(void);

/* Register all built-in providers */
void register_provider_builtins(void);

#ifdef __cplusplus
}
#endif

/** @} */ /* end of providers group */

#endif /* PROVIDER_H */
