/*
 * provider_anthropic.c — Anthropic Messages API provider.
 * Supports Claude models via api.anthropic.com.
 *
 * Key differences from OpenAI:
 *  - x-api-key header (not Bearer)
 *  - System message is top-level "system" field, not in messages array
 *  - Only "user" and "assistant" roles (no "system", no "tool")
 *  - Content is array of content blocks, not a string
 *  - Tool calls are "tool_use" content blocks
 *  - Tool results are "tool_result" content blocks in user messages
 *  - Tools use "input_schema" not "parameters", no "type":"function" wrapper
 */

#include "libcrypto/crypto.h"
#include "hermes_auth.h"
#include "hermes_core_types.h"
#include "hermes_logger.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_url_safety.h"
#include "provider.h"
#include "provider_profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

/* ================================================================
 *  Model-aware helpers
 * ================================================================ */

/* Substring patterns for model version detection */
static const char *ADAPTIVE_SUBSTRINGS[] = {"4-6", "4.6", "4-7", "4.7", "4-8", "4.8", NULL};
static const char *XHIGH_EFFORT_SUBSTRINGS[] = {"4-7", "4.7", "4-8", "4.8", NULL};
static const char *NO_SAMPLING_SUBSTRINGS[] = {"4-7", "4.7", "4-8", "4.8", NULL};
static const char *FAST_MODE_SUBSTRINGS[] = {"opus-4-6", "opus-4.6", NULL};

/* Normalize model name: replace '.' with '-' for matching */
static void normalize_model_key(const char *model, char *out, size_t out_size) {
    if (!model || !*model) { out[0] = '\0'; return; }
    const char *slash = strrchr(model, '/');
    if (slash) model = slash + 1;  /* strip provider prefix */
    size_t i, j = 0;
    for (i = 0; model[i] && j < out_size - 1; i++) {
        if (model[i] == '.')
            out[j++] = '-';
        else
            out[j++] = model[i];
    }
    out[j] = '\0';
}

static bool model_contains_any(const char *model, const char **patterns) {
    if (!model || !*model) return false;
    char normalized[128];
    normalize_model_key(model, normalized, sizeof(normalized));
    for (int i = 0; patterns[i]; i++) {
        if (strstr(normalized, patterns[i]))
            return true;
    }
    return false;
}

/* PoP: supports_adaptive_thinking @ agent/anthropic_adapter.py:_supports_adaptive_thinking */
/* Port of Python agent/anthropic_adapter.py:_supports_adaptive_thinking().
 * Claude 4.6+ models support adaptive thinking (type="adaptive" + output_config.effort) */
static bool supports_adaptive_thinking(const char *model) {
    if (!model) return false;
    bool result = model_contains_any(model, ADAPTIVE_SUBSTRINGS);
    hermes_log(LOG_DEBUG, "anthropic", "supports_adaptive_thinking(%s) = %d", model, result);
    return result;
}

/* PoP: supports_xhigh_effort @ agent/anthropic_adapter.py:_supports_xhigh_effort */
/* Port of Python agent/anthropic_adapter.py:_supports_xhigh_effort().
 * Opus 4.7+ models accept the "xhigh" effort level */
static bool supports_xhigh_effort(const char *model) {
    if (!model) return false;
    bool result = model_contains_any(model, XHIGH_EFFORT_SUBSTRINGS);
    hermes_log(LOG_DEBUG, "anthropic", "supports_xhigh_effort(%s) = %d", model, result);
    return result;
}

/* PoP: forbids_sampling_params @ agent/anthropic_adapter.py:_forbids_sampling_params */
/* Port of Python agent/anthropic_adapter.py:_forbids_sampling_params().
 * Opus 4.7+ models reject temperature/top_p/top_k (any non-null value → 400) */
static bool forbids_sampling_params(const char *model) {
    if (!model) return false;
    bool result = model_contains_any(model, NO_SAMPLING_SUBSTRINGS);
    hermes_log(LOG_DEBUG, "anthropic", "forbids_sampling_params(%s) = %d", model, result);
    return result;
}

/* PoP: supports_fast_mode @ agent/anthropic_adapter.py:_supports_fast_mode */
/* Port of Python agent/anthropic_adapter.py:_supports_fast_mode().
 * Opus 4.6 supports fast mode (speed="fast" + beta header) */
static bool supports_fast_mode(const char *model) {
    if (!model) return false;
    bool result = model_contains_any(model, FAST_MODE_SUBSTRINGS);
    hermes_log(LOG_DEBUG, "anthropic", "supports_fast_mode(%s) = %d", model, result);
    return result;
}

/* Adventure effort → adaptive effort mapping.
 * Low/medium/high/xhigh/max preserved; minimal→low; others→medium. */
static const char *map_to_adaptive_effort(const char *effort) {
    if (!effort || !*effort) return "medium";
    if (strcmp(effort, "low") == 0)     return "low";
    if (strcmp(effort, "medium") == 0)   return "medium";
    if (strcmp(effort, "high") == 0)     return "high";
    if (strcmp(effort, "xhigh") == 0)    return "xhigh";
    if (strcmp(effort, "max") == 0)      return "max";
    if (strcmp(effort, "minimal") == 0)  return "low";
    return "medium";
}

/* Port of Python anthropic_adapter.py:_get_anthropic_max_output(). */
int anthropic_get_model_max_output(const char *model) {
    if (!model || !*model) return 128000;
    char normalized[128];
    normalize_model_key(model, normalized, sizeof(normalized));

    /* Table sorted by longest key first for longest-prefix match */
    struct { const char *key; int limit; } limits[] = {
        {"claude-opus-4-8",      128000},
        {"claude-opus-4-7",      128000},
        {"claude-opus-4-6",      128000},
        {"claude-sonnet-4-6",     64000},
        {"claude-opus-4-5",       64000},
        {"claude-sonnet-4-5",     64000},
        {"claude-haiku-4-5",      64000},
        {"claude-opus-4",         32000},
        {"claude-sonnet-4",       64000},
        {"claude-3-7-sonnet",    128000},
        {"claude-3-5-sonnet",      8192},
        {"claude-3-5-haiku",       8192},
        {"claude-3-opus",          4096},
        {"claude-3-sonnet",        4096},
        {"claude-3-haiku",         4096},
        {"minimax",              131072},
        {"qwen3",                 65536},
        {NULL, 0}
    };
    int best = 128000;
    size_t best_len = 0;
    for (int i = 0; limits[i].key; i++) {
        if (strstr(normalized, limits[i].key)) {
            size_t klen = strlen(limits[i].key);
            if (klen > best_len) {
                best_len = klen;
                best = limits[i].limit;
            }
        }
    }
    return best;
}

/* ================================================================
 *  URL building
 * ================================================================ */

static char *anthropic_build_url(const provider_t *p, const char *base_url) {
    (void)p;
    if (!base_url || !*base_url)
        base_url = "https://api.anthropic.com/v1";

    /* If URL already includes /messages, use as-is */
    if (strstr(base_url, "/messages"))
        return strdup(base_url);

    size_t len = strlen(base_url);
    char *url = (char *)malloc(len + 12);
    if (!url) return NULL;

    if (base_url[len-1] == '/')
        snprintf(url, len + 12, "%smessages", base_url);
    else
        snprintf(url, len + 12, "%s/messages", base_url);
    return url;
}

/* ================================================================
 *  Headers
 * ================================================================ */

static char *anthropic_build_headers(const provider_t *p, const char *api_key) {
    const char *base_url = p ? p->base_url : NULL;

    /* Determine auth header type: Bearer for MiniMax/Azure, x-api-key for native Anthropic */
    bool use_bearer = requires_bearer_auth(base_url);

    /* Collect beta headers using endpoint-aware beta resolution */
    json_t *beta_list = anthropic_common_betas_for_base_url(base_url, false);
    char betas[512] = "";
    size_t pos = 0;
    size_t n = json_len(beta_list);

    /* Prompt caching beta — always sent first if applicable */
    if (p && provider_get_system_cached(p)) {
        pos += snprintf(betas + pos, sizeof(betas) - pos,
                        "anthropic-beta: ephemeral-cache-2025-05-20");
    }

    for (size_t i = 0; i < n; i++) {
        json_t *beta_item = json_get(beta_list, i);
        const char *b = (beta_item && beta_item->type == JSON_STRING) ? beta_item->str_val : "";
        if (!*b) continue;
        pos += snprintf(betas + pos, sizeof(betas) - pos,
                        "%s%s",
                        pos > 0 ? "\r\nanthropic-beta: " : "anthropic-beta: ",
                        b);
    }
    json_free(beta_list);

    char *headers = (char *)malloc(1536);
    if (!headers) return NULL;

    const char *auth_header = "";
    const char *auth_value = "";
    if (api_key && *api_key) {
        if (use_bearer) {
            auth_header = "Authorization: Bearer ";
        } else {
            auth_header = "x-api-key: ";
        }
        auth_value = api_key;
    }

    snprintf(headers, 1536,
        "%s%s\r\n"
        "anthropic-version: 2023-06-01\r\n"
        "%s"
        "Content-Type: application/json\r\n"
        "Accept: application/json",
        auth_header, auth_value,
        betas);

    return headers;
}

/* ================================================================
 *  Request body building
 * ================================================================ */

/* Build a content block array from a text string */
static json_t *text_block(const char *text) {
    json_t *block = json_object();
    json_set(block, "type", json_string("text"));
    json_set(block, "text", json_string(text ? text : ""));
    return block;
}

/* Build a tool_use content block */
static json_t *tool_use_block(const tool_call_t *tc) {
    json_t *block = json_object();
    json_set(block, "type", json_string("tool_use"));
    json_set(block, "id", json_string(tc->id));
    json_set(block, "name", json_string(tc->name));

    /* Parse arguments JSON as an object */
    char *err = NULL;
    json_t *input = json_parse(tc->arguments, &err);
    if (input) {
        json_set(block, "input", input);
    } else {
        json_set(block, "input", json_object()); /* empty object fallback */
        free(err);
    }
    return block;
}

/* Build a tool_result content block */
static json_t *tool_result_block(const char *tool_use_id, const char *content) {
    json_t *block = json_object();
    json_set(block, "type", json_string("tool_result"));
    json_set(block, "tool_use_id", json_string(tool_use_id));
    json_set(block, "content", json_string(content ? content : ""));
    return block;
}

static char *anthropic_build_request_body(const provider_t *p,
                                           const message_t **messages,
                                           size_t msg_count,
                                           json_t *tools_json,
                                           bool streaming) {
    (void)p;
    json_t *root = json_object();
    if (!root) return NULL;

    /* Model */
    json_set(root, "model", json_string(
        p->model[0] ? p->model : "claude-sonnet-4-20250514"));

    /* LLM params from config — skip temperature/top_p on 4.7+ that reject them */
    bool is_adaptive = supports_adaptive_thinking(p->model);
    bool skip_sampling = forbids_sampling_params(p->model);

    /* max_tokens with model-aware output ceiling */
    int max_tok = p->config.max_tokens > 0
        ? p->config.max_tokens
        : anthropic_get_model_max_output(p->model);
    json_set(root, "max_tokens", json_number(max_tok));

    if (!skip_sampling) {
        if (p->config.temperature >= 0.0f)
            json_set(root, "temperature", json_number(p->config.temperature));
        if (p->config.top_p > 0.0f && p->config.top_p < 1.0f)
            json_set(root, "top_p", json_number(p->config.top_p));
    }
    if (p->config.stop_count > 0) {
        json_t *stop_arr = json_array();
        for (int i = 0; i < p->config.stop_count && i < HERMES_STOP_SEQUENCES_MAX; i++)
            if (p->config.stop_sequences[i][0])
                json_append(stop_arr, json_string(p->config.stop_sequences[i]));
        if (json_len(stop_arr) > 0) json_set(root, "stop_sequences", stop_arr);
        else json_free(stop_arr);
    }

    /* P06: Anthropic service_tier (e.g. "default") */
    if (p->config.service_tier[0])
        json_set(root, "service_tier", json_string(p->config.service_tier));

    /* U07: Anthropic fast mode (speed="fast") for Opus 4.6+ */
    if (supports_fast_mode(p->model))
        json_set(root, "speed", json_string("fast"));

    /* response_format + metadata */
    if (p->config.response_format[0]) {
        json_t *rf = json_parse(p->config.response_format, NULL);
        if (rf) { json_set(root, "response_format", json_copy(rf)); json_free(rf); }
    } else if (p->config.json_mode) {
        json_t *rf = json_new_object();
        json_object_set(rf, "type", json_new_string("json_object"));
        json_set(root, "response_format", rf);
    }
    if (p->config.metadata[0]) {
        json_t *md = json_parse(p->config.metadata, NULL);
        if (md) { json_set(root, "metadata", md); json_free(md); }
    }

    /* tool_choice + parallel_tool_calls */
    if (p->config.tool_choice[0]) {
        json_t *tc = json_parse(p->config.tool_choice, NULL);
        if (tc) { json_set(root, "tool_choice", tc); json_free(tc); }
        else { json_set(root, "tool_choice", json_string(p->config.tool_choice)); }
    }
    if (!p->config.parallel_tool_calls)
        json_set(root, "disable_parallel_tool_use", json_bool(true));

    /* S8 R01: Anthropic extended thinking — adaptive vs classic */
    if (p->config.reasoning_effort[0] && strcmp(p->config.reasoning_effort, "none") != 0) {
        const char *effort = p->config.reasoning_effort;

        if (is_adaptive) {
            /* Claude 4.6+ uses adaptive thinking with output_config.effort */
            json_t *thinking = json_object();
            json_set(thinking, "type", json_string("adaptive"));
            /* display: "summarized" keeps reasoning blocks populated for Hermes CLI */
            json_set(thinking, "display", json_string("summarized"));
            json_set(root, "thinking", thinking);

            /* Map effort */
            const char *adaptive_effort = map_to_adaptive_effort(effort);
            /* Downgrade xhigh→max on models that don't support xhigh (pre-4.7) */
            if (strcmp(adaptive_effort, "xhigh") == 0 && !supports_xhigh_effort(p->model))
                adaptive_effort = "max";

            json_t *output_config = json_object();
            json_set(output_config, "effort", json_string(adaptive_effort));
            json_set(root, "output_config", output_config);
        } else {
            /* Classic thinking with budget_tokens for pre-4.6 models */
            int budget = 0;
            if (strcmp(effort, "low") == 0)        budget = 8192;
            else if (strcmp(effort, "medium") == 0) budget = 16384;
            else if (strcmp(effort, "high") == 0)   budget = 32768;
            else budget = atoi(effort); /* direct number */
            if (budget >= 1024) {
                json_t *thinking = json_object();
                json_set(thinking, "type", json_string("enabled"));
                json_set(thinking, "budget_tokens", json_number(budget));
                json_set(root, "thinking", thinking);

                /* Anthropic requires temperature=1 when thinking is enabled on classic models */
                if (!skip_sampling)
                    json_set(root, "temperature", json_number(1.0f));

                /* Ensure max_tokens >= budget + 4096 for thinking to have room */
                if (max_tok < budget + 4096)
                    json_set(root, "max_tokens", json_number(budget + 4096));
            }
        }
    }

    /* Stream flag */
    if (streaming)
        json_set(root, "stream", json_bool(true));

    /* extra_body — merge arbitrary JSON fields into request body */
    if (p->config.extra_body[0]) {
        json_t *eb = json_parse(p->config.extra_body, NULL);
        if (eb && eb->type == JSON_OBJECT) {
            for (size_t i = 0; i < eb->c.count; i++) {
                json_t *copy = json_copy(eb->c.items[i]);
                if (copy)
                    json_set(root, eb->c.keys[i], copy);
            }
        }
        json_free(eb);
    }

    /* Tools (convert from OpenAI format to Anthropic format) */
    if (tools_json && json_len(tools_json) > 0) {
        json_t *anthropic_tools = json_array();
        size_t n = json_len(tools_json);
        for (size_t i = 0; i < n; i++) {
            json_t *ot = json_get(tools_json, i);
            /* OpenAI: {"type":"function","function":{"name":"...","description":"...","parameters":{...}}} */
            /* Anthropic: {"name":"...","description":"...","input_schema":{...}} */
            json_t *fn = json_obj_get(ot, "function");
            if (!fn) continue;

            json_t *at = json_object();
            json_set(at, "name", json_copy(json_obj_get(fn, "name")));
            json_set(at, "description", json_copy(json_obj_get(fn, "description")));

            /* Map OpenAI "parameters" to Anthropic "input_schema" */
            json_t *params = json_obj_get(fn, "parameters");
            if (params)
                json_set(at, "input_schema", json_copy(params));
            else
                json_set(at, "input_schema", json_object());

            json_append(anthropic_tools, at);
        }
        if (json_len(anthropic_tools) > 0) {
            json_set(root, "tools", anthropic_tools);
            /* B27: cache_control on tools — top-level field in request body */
            json_t *tools_cc = json_object();
            json_set(tools_cc, "type", json_string("ephemeral"));
            json_set(root, "cache_control", tools_cc);
        } else
            json_free(anthropic_tools);
    }

    /* System message (top-level, separate from messages) */
    /* Collect all system messages into one string */
    char system_text[4096] = "";
    bool has_system = false;

    /* Messages array: Anthropic only uses user + assistant roles.
     * System messages are extracted to the top-level "system" field.
     * Tool results become user messages with tool_result content blocks. */
    json_t *msgs = json_array();
    if (!msgs) { json_free(root); return NULL; }

    for (size_t i = 0; i < msg_count; i++) {
        const message_t *msg = messages[i];
        if (!msg) continue;

        /* Extract system messages */
        if (msg->role == MSG_SYSTEM) {
            if (msg->content) {
                if (has_system) {
                    size_t cur = strlen(system_text);
                    size_t add = strlen(msg->content);
                    if (cur + add + 1 < sizeof(system_text)) {
                        memcpy(system_text + cur, msg->content, add);
                        system_text[cur + add] = '\0';
                    }
                } else {
                    snprintf(system_text, sizeof(system_text), "%s", msg->content);
                    has_system = true;
                }
            }
            continue;
        }

        /* Build Anthropic message */
        const char *role_str = NULL;
        json_t *content_arr = json_array();

        if (msg->role == MSG_USER) {
            role_str = "user";
            if (msg->tool_call_id) {
                /* This is a tool result — wrap in tool_result block */
                json_append(content_arr, tool_result_block(
                    msg->tool_call_id,
                    msg->content ? msg->content : ""));
            } else {
                /* Normal user text */
                json_append(content_arr, text_block(msg->content));
            }
        } else if (msg->role == MSG_ASSISTANT) {
            role_str = "assistant";
            if (msg->tool_calls_count > 0) {
                /* Text content first (if any) */
                if (msg->content && msg->content[0])
                    json_append(content_arr, text_block(msg->content));
                /* Tool use blocks */
                for (int j = 0; j < msg->tool_calls_count; j++)
                    json_append(content_arr, tool_use_block(&msg->tool_calls[j]));
            } else {
                json_append(content_arr, text_block(msg->content));
            }
        } else if (msg->role == MSG_TOOL) {
            /* Tool result messages: convert to user with tool_result block.
             * The agent loop creates MSG_TOOL messages. Anthropic
             * represents these as user messages with tool_result content. */
            role_str = "user";
            if (msg->tool_call_id) {
                json_append(content_arr, tool_result_block(
                    msg->tool_call_id,
                    msg->content ? msg->content : ""));
            } else {
                json_append(content_arr, text_block(msg->content));
            }
        } else {
            continue; /* skip unknown roles */
        }

        if (json_len(content_arr) == 0) {
            json_free(content_arr);
            continue;
        }

        json_t *am = json_object();
        json_set(am, "role", json_string(role_str));
        json_set(am, "content", content_arr);
        json_append(msgs, am);
    }

    /* B27: cache_control on the last user message's last content block */
    if (json_len(msgs) > 0) {
        json_t *last_msg = json_get(msgs, json_len(msgs) - 1);
        const char *last_role = json_get_str(last_msg, "role", "");
        if (strcmp(last_role, "user") == 0) {
            json_t *last_content = json_obj_get(last_msg, "content");
            if (last_content && json_len(last_content) > 0) {
                json_t *last_block = json_get(last_content, json_len(last_content) - 1);
                json_t *cc = json_object();
                json_set(cc, "type", json_string("ephemeral"));
                json_set(last_block, "cache_control", cc);
            }
        }
    }

    /* CACHE: system_and_3 strategy — mark last 3 non-system messages for cache.
     * Python prompt_caching.py apply_anthropic_cache_control().
     * We already marked the last user message above, so we scan backwards
     * and mark up to 2 more (indices 2 and 3 from end). */
    {
        int breakpoints = 0;
        int n_msgs = (int)json_len(msgs);
        for (int i = n_msgs - 2; i >= 0 && breakpoints < 2; i--) {
            json_t *msg = json_get(msgs, i);
            const char *r = json_get_str(msg, "role", "");
            if (strcmp(r, "system") == 0) continue; /* system handled separately */
            breakpoints++;

            json_t *content = json_obj_get(msg, "content");
            if (!content || json_len(content) == 0) continue;

            if (strcmp(r, "tool") == 0) {
                /* Tool messages: cache_control at message level */
                json_t *cc = json_object();
                json_set(cc, "type", json_string("ephemeral"));
                json_set(msg, "cache_control", cc);
            } else {
                /* User/assistant: cache_control on last content block */
                json_t *last_block = json_get(content, json_len(content) - 1);
                json_t *cc = json_object();
                json_set(cc, "type", json_string("ephemeral"));
                json_set(last_block, "cache_control", cc);
            }
        }
    }

    if (json_len(msgs) == 0) {
        json_free(msgs);
        /* At minimum, add a "hello" user message */
        json_t *dummy_msg = json_object();
        json_set(dummy_msg, "role", json_string("user"));
        json_t *dummy_content = json_array();
        json_append(dummy_content, text_block("Hello"));
        json_set(dummy_msg, "content", dummy_content);
        msgs = json_array();
        json_append(msgs, dummy_msg);
    }

    json_set(root, "messages", msgs);

    /* Set system field if we extracted any */
    if (has_system && system_text[0]) {
        /* P91: Prompt caching — first turn uses cache_control */
        if (p->system_cached) {
            /* Already cached — plain system string */
            json_set(root, "system", json_string(system_text));
        } else {
            /* First request — wrap with cache_control */
            json_t *sys_blocks = json_array();
            json_t *block = json_object();
            json_set(block, "type", json_string("text"));
            json_set(block, "text", json_string(system_text));
            json_t *cc = json_object();
            json_set(cc, "type", json_string("ephemeral"));
            json_set(block, "cache_control", cc);
            json_append(sys_blocks, block);
            json_set(root, "system", sys_blocks);
        }
    }

    /* v651b: apply ProviderProfile quirks (fixed_temperature, default_max_tokens) */
    apply_provider_profile(p, root);

    /* Serialize */
    char *body = json_serialize(root);
    json_free(root);
    return body;
}

/* ================================================================
 *  Response parsing
 * ================================================================ */

static provider_response_t *anthropic_parse_response(const provider_t *p,
                                                       const char *response_body) {
    (void)p;
    provider_response_t *resp = (provider_response_t *)calloc(1, sizeof(*resp));
    if (!resp) return NULL;

    char *err = NULL;
    json_t *root = json_parse(response_body, &err);
    if (!root) {
        resp->content = strdup("");
        free(err);
        return resp;
    }

    /* Check for error response */
    json_t *error_obj = json_obj_get(root, "error");
    if (error_obj) {
        const char *err_type = json_get_str(error_obj, "type", "unknown");
        const char *err_msg = json_get_str(error_obj, "message", "unknown error");
        resp->content = (char *)malloc(1024);
        if (resp->content)
            snprintf(resp->content, 1024, "Anthropic API error [%s]: %s", err_type, err_msg);
        json_free(root);
        return resp;
    }

    /* Usage */
    json_t *usage = json_obj_get(root, "usage");
    if (usage) {
        resp->input_tokens = (int)json_get_num(usage, "input_tokens", 0);
        resp->output_tokens = (int)json_get_num(usage, "output_tokens", 0);
    }

    /* Content array */
    json_t *content_arr = json_obj_get(root, "content");
    if (content_arr && json_len(content_arr) > 0) {
        /* First pass: count total text content for allocation.
         * Anthropic returns multiple content blocks:
         *   {"type":"text","text":"..."}
         *   {"type":"tool_use","id":"...","name":"...","input":{...}} */
        size_t text_len = 0;
        size_t thinking_len = 0;
        int tc_count = 0;
        size_t n = json_len(content_arr);
        for (size_t i = 0; i < n; i++) {
            json_t *block = json_get(content_arr, i);
            const char *type = json_get_str(block, "type", "");
            if (strcmp(type, "text") == 0) {
                const char *text = json_get_str(block, "text", "");
                text_len += strlen(text);
            } else if (strcmp(type, "thinking") == 0) {
                const char *text = json_get_str(block, "thinking", "");
                thinking_len += strlen(text);
            } else if (strcmp(type, "tool_use") == 0) {
                if (tc_count < 64) tc_count++;
            }
        }

        /* Allocate content buffer */
        if (text_len > 0) {
            resp->content = (char *)calloc(text_len + 1, 1);
            if (resp->content) {
                size_t pos = 0;
                for (size_t i = 0; i < n; i++) {
                    json_t *block = json_get(content_arr, i);
                    const char *type = json_get_str(block, "type", "");
                    if (strcmp(type, "text") == 0) {
                        const char *text = json_get_str(block, "text", "");
                        size_t add = strlen(text);
                        if (pos + add < text_len + 1) {
                            memcpy(resp->content + pos, text, add);
                            pos += add;
                        }
                    }
                }
                resp->content[pos] = '\0';
            }
        } else {
            resp->content = strdup("");
        }

        /* Extract reasoning from thinking blocks (B26) */
        if (thinking_len > 0) {
            resp->reasoning = (char *)calloc(thinking_len + 1, 1);
            if (resp->reasoning) {
                size_t pos = 0;
                for (size_t i = 0; i < n; i++) {
                    json_t *block = json_get(content_arr, i);
                    const char *type = json_get_str(block, "type", "");
                    if (strcmp(type, "thinking") == 0) {
                        const char *text = json_get_str(block, "thinking", "");
                        size_t add = strlen(text);
                        if (pos + add < thinking_len + 1) {
                            memcpy(resp->reasoning + pos, text, add);
                            pos += add;
                        }
                    }
                }
                resp->reasoning[pos] = '\0';
            }
        }

        /* Extract tool calls */
        if (tc_count > 0) {
            resp->tool_calls_count = 0;
            for (size_t i = 0; i < n && resp->tool_calls_count < 64; i++) {
                json_t *block = json_get(content_arr, i);
                const char *type = json_get_str(block, "type", "");
                if (strcmp(type, "tool_use") == 0) {
                    int idx = resp->tool_calls_count;
                    snprintf(resp->tool_calls[idx].id,
                             sizeof(resp->tool_calls[idx].id), "%s",
                             json_get_str(block, "id", ""));
                    snprintf(resp->tool_calls[idx].name,
                             sizeof(resp->tool_calls[idx].name), "%s",
                             json_get_str(block, "name", ""));

                    /* Serialize the input object back to JSON string */
                    json_t *input = json_obj_get(block, "input");
                    if (input) {
                        char *args = json_serialize(input);
                        if (args) {
                            snprintf(resp->tool_calls[idx].arguments,
                                     sizeof(resp->tool_calls[idx].arguments),
                                     "%s", args);
                            free(args);
                        }
                    }
                    resp->tool_calls_count++;
                }
            }
        }

        /* Check stop_reason for "end_turn" vs "tool_use" */
        const char *stop = json_get_str(root, "stop_reason", "");
        if (strcmp(stop, "tool_use") != 0 && resp->tool_calls_count > 0) {
            /* If stop_reason isn't tool_use but we have tool calls,
             * keep them (Anthropic stops on tool_use) */
        }
    }

    json_free(root);
    return resp;
}

/* ================================================================
 *  Streaming chunk parsing
 * ================================================================ */

static provider_response_t *anthropic_parse_stream_chunk(const provider_t *p,
                                                          const char *chunk) {
    (void)p;
    provider_response_t *resp = (provider_response_t *)calloc(1, sizeof(*resp));
    if (!resp) return NULL;

    /* Null-safe */
    if (!chunk) {
        resp->content = strdup("");
        return resp;
    }

    /* Anthropic SSE format:
     *   event: content_block_delta
     *   data: {"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"Hello"}}
     *
     *   event: content_block_start
     *   data: {"type":"content_block_start","index":0,"content_block":{"type":"text","text":""}}
     *
     *   event: message_start
     *   data: {"type":"message_start","message":{...}}
     *
     *   event: message_delta
     *   data: {"type":"message_delta","delta":{"stop_reason":"end_turn"},"usage":{"output_tokens":N}}
     */

    /* We handle two patterns:
     * 1. Event-lines followed by data-lines
     * 2. Direct "data: {...}" lines (some proxies simplify) */

    /* Handle three input formats used by different callers:
     * 1. Raw JSON (HTTP parser stripped "data: " prefix)
     * 2. Full SSE event+data line ("event: ...\ndata: {...}")
     * 3. Bare "data: {...}" line */
    const char *json_str = NULL;

    if (strncmp(chunk, "{", 1) == 0) {
        /* Raw JSON — HTTP streaming parser already stripped framing */
        json_str = chunk;
    } else if (strncmp(chunk, "event:", 6) == 0) {
        /* Full SSE: event: ...\ndata: {...} */
        const char *nl = strchr(chunk, '\n');
        if (nl) {
            const char *data_prefix = strstr(nl, "data: ");
            if (data_prefix)
                json_str = data_prefix + 6;
        }
    } else if (strncmp(chunk, "data: ", 6) == 0) {
        json_str = chunk + 6;
    }

    if (!json_str || !*json_str) {
        resp->content = strdup(chunk);
        return resp;
    }

    /* Check for event stream termination */
    if (strncmp(json_str, "[DONE]", 6) == 0) {
        resp->content = strdup("");
        return resp;
    }

    char *err = NULL;
    json_t *root = json_parse(json_str, &err);
    if (!root) {
        resp->content = strdup("");
        free(err);
        return resp;
    }

    /* Determine event type */
    const char *type = json_get_str(root, "type", "");

    if (strcmp(type, "content_block_delta") == 0) {
        json_t *delta = json_obj_get(root, "delta");
        if (delta) {
            const char *delta_type = json_get_str(delta, "type", "");
            if (strcmp(delta_type, "text_delta") == 0) {
                resp->content = strdup(json_get_str(delta, "text", ""));
            } else if (strcmp(delta_type, "input_json_delta") == 0) {
                /* Partial tool call arguments — we accumulate these
                 * in the streaming callback context, not per-chunk.
                 * Return empty content for now. */
                resp->content = strdup("");
            } else if (strcmp(delta_type, "thinking_delta") == 0) {
                /* B26: Anthropic thinking blocks — streaming reasoning */
                resp->reasoning = strdup(json_get_str(delta, "thinking", ""));
            } else {
                resp->content = strdup("");
            }
        } else {
            resp->content = strdup("");
        }
    } else if (strcmp(type, "content_block_start") == 0) {
        json_t *cb = json_obj_get(root, "content_block");
        if (cb) {
            const char *cb_type = json_get_str(cb, "type", "");
            if (strcmp(cb_type, "text") == 0) {
                resp->content = strdup(json_get_str(cb, "text", ""));
            } else {
                resp->content = strdup("");
            }
        } else {
            resp->content = strdup("");
        }
    } else if (strcmp(type, "message_start") == 0) {
        json_t *msg = json_obj_get(root, "message");
        if (msg) {
            json_t *usage = json_obj_get(msg, "usage");
            if (usage) {
                resp->input_tokens = (int)json_get_num(usage, "input_tokens", 0);
            }
        }
        resp->content = strdup("");
    } else if (strcmp(type, "message_delta") == 0) {
        json_t *delta = json_obj_get(root, "delta");
        json_t *usage = json_obj_get(root, "usage");
        if (usage)
            resp->output_tokens = (int)json_get_num(usage, "output_tokens", 0);

        /* Check stop_reason for tool_use */
        if (delta) {
            const char *stop = json_get_str(delta, "stop_reason", "");
            if (stop[0]) snprintf(resp->finish_reason, sizeof(resp->finish_reason), "%s", stop);
            if (strcmp(stop, "tool_use") == 0) {
                /* Signal: more tool calls may follow.
                 * The streaming callback accumulates these. */
            }
        }
        resp->content = strdup("");
    } else {
        /* ping or unknown — ignore */
        resp->content = strdup("");
    }

    json_free(root);
    return resp;
}

/* ================================================================
 *  Free response
 * ================================================================ */

static void anthropic_free_response(provider_response_t *resp) {
    if (!resp) return;
    free(resp->content);
    free(resp->reasoning);
    free(resp);
}

/* ================================================================
 *  Endpoint detection utilities — ported from Python anthropic_adapter.py
 * ================================================================ */

/* Port of Python anthropic_adapter.py:_is_oauth_token(). */
bool is_oauth_token(const char *key) {
    if (!key || !*key) return false;
    /* Regular Anthropic Console API keys — x-api-key auth, not OAuth */
    if (strncmp(key, "sk-ant-api", 10) == 0) return false;
    /* Anthropic-issued tokens (setup-tokens sk-ant-oat-*, managed keys) */
    if (strncmp(key, "sk-ant-", 7) == 0) return true;
    /* JWTs from Anthropic OAuth flow */
    if (strncmp(key, "eyJ", 3) == 0) return true;
    /* Claude Code OAuth access tokens */
    if (strncmp(key, "cc-", 3) == 0) return true;
    return false;
}

/* Port of Python anthropic_adapter.py:_normalize_base_url_text(). */
char *anthropic_normalize_base_url_text(const char *base_url) {
    if (!base_url || !*base_url) return strdup("");
    /* Strip leading/trailing whitespace */
    while (*base_url == ' ' || *base_url == '\t') base_url++;
    if (!*base_url) return strdup("");
    size_t len = strlen(base_url);
    while (len > 0 && (base_url[len-1] == ' ' || base_url[len-1] == '\t')) len--;
    return strndup(base_url, len);
}

/* Port of Python anthropic_adapter.py:_is_third_party_anthropic_endpoint(). */
bool anthropic_is_third_party_endpoint(const char *base_url) {
    char *norm = anthropic_normalize_base_url_text(base_url);
    if (!norm || !*norm) { free(norm); return false; }
    /* Lowercase for comparison */
    size_t len = strlen(norm);
    char *lower = malloc(len + 1);
    if (!lower) { free(norm); return false; }
    for (size_t i = 0; i < len; i++) lower[i] = tolower((unsigned char)norm[i]);
    lower[len] = '\0';
    /* Strip trailing slash */
    while (len > 0 && lower[len-1] == '/') lower[--len] = '\0';
    bool result = (strstr(lower, "anthropic.com") == NULL);
    free(lower);
    free(norm);
    return result;
}

/* Port of Python anthropic_adapter.py:_is_kimi_coding_endpoint(). */
bool is_kimi_coding_endpoint(const char *base_url) {
    char *norm = anthropic_normalize_base_url_text(base_url);
    if (!norm || !*norm) { free(norm); return false; }
    size_t len = strlen(norm);
    char *lower = malloc(len + 1);
    if (!lower) { free(norm); return false; }
    for (size_t i = 0; i < len; i++) lower[i] = tolower((unsigned char)norm[i]);
    lower[len] = '\0';
    while (len > 0 && lower[len-1] == '/') lower[--len] = '\0';
    bool result = (strncmp(lower, "https://api.kimi.com/coding", 27) == 0);
    free(lower);
    free(norm);
    return result;
}

static bool str_is_kimi_prefix(const char *m) {
    if (!m || !*m) return false;
    static const char *prefixes[] = {
        "kimi-", "kimi_", "moonshot-", "moonshot_",
        "k1.", "k1-", "k2.", "k2-", "k25", "k2.5", NULL
    };
    for (int i = 0; prefixes[i]; i++) {
        size_t plen = strlen(prefixes[i]);
        if (strncmp(m, prefixes[i], plen) == 0) return true;
    }
    return false;
}

/* Port of Python anthropic_adapter.py:_model_name_is_kimi_family(). */
bool model_name_is_kimi_family(const char *model) {
    if (!model || !*model) return false;
    const char *m = model;
    while (*m == ' ' || *m == '\t') m++;
    if (!*m) return false;
    size_t len = strlen(m);
    char *lower = malloc(len + 1);
    if (!lower) return false;
    for (size_t i = 0; i < len; i++) lower[i] = tolower((unsigned char)m[i]);
    lower[len] = '\0';
    /* Strip vendor prefix */
    char *slash = strrchr(lower, '/');
    char *prefix = slash ? slash + 1 : lower;
    /* Strip trailing whitespace */
    size_t plen = strlen(prefix);
    while (plen > 0 && (prefix[plen-1] == ' ' || prefix[plen-1] == '\t')) prefix[--plen] = '\0';
    bool result = str_is_kimi_prefix(prefix);
    free(lower);
    return result;
}

/* Port of Python anthropic_adapter.py:_is_kimi_family_endpoint(). */
bool is_kimi_family_endpoint(const char *base_url, const char *model) {
    if (is_kimi_coding_endpoint(base_url)) return true;
    /* Check known domains */
    if (base_url && *base_url) {
        if (url_host_matches(base_url, "api.kimi.com") ||
            url_host_matches(base_url, "moonshot.ai") ||
            url_host_matches(base_url, "moonshot.cn"))
            return true;
    }
    if (model_name_is_kimi_family(model)) return true;
    return false;
}

/* Port of Python anthropic_adapter.py:_is_deepseek_anthropic_endpoint(). */
bool anthropic_is_deepseek_endpoint(const char *base_url) {
    if (!base_url || !*base_url) return false;
    if (!url_host_matches(base_url, "api.deepseek.com")) return false;
    char *norm = anthropic_normalize_base_url_text(base_url);
    if (!norm || !*norm) { free(norm); return false; }
    size_t len = strlen(norm);
    char *lower = malloc(len + 1);
    if (!lower) { free(norm); return false; }
    for (size_t i = 0; i < len; i++) lower[i] = tolower((unsigned char)norm[i]);
    lower[len] = '\0';
    while (len > 0 && lower[len-1] == '/') lower[--len] = '\0';
    bool result = (strstr(lower, "/anthropic") != NULL);
    free(lower);
    free(norm);
    return result;
}

/* Port of Python anthropic_adapter.py:_requires_bearer_auth(). */
bool requires_bearer_auth(const char *base_url) {
    char *norm = anthropic_normalize_base_url_text(base_url);
    if (!norm || !*norm) { free(norm); return false; }
    size_t len = strlen(norm);
    char *lower = malloc(len + 1);
    if (!lower) { free(norm); return false; }
    for (size_t i = 0; i < len; i++) lower[i] = tolower((unsigned char)norm[i]);
    lower[len] = '\0';
    while (len > 0 && lower[len-1] == '/') lower[--len] = '\0';
    bool result = (
        (strncmp(lower, "https://api.minimax.io/anthropic", 33) == 0 ||
         strncmp(lower, "https://api.minimaxi.com/anthropic", 34) == 0 ||
         strstr(lower, "azure.com") != NULL)
    );
    free(lower);
    free(norm);
    return result;
}

/* Port of Python anthropic_adapter.py:_base_url_needs_context_1m_beta(). */
bool anthropic_base_url_needs_1m_beta(const char *base_url) {
    char *norm = anthropic_normalize_base_url_text(base_url);
    if (!norm || !*norm) { free(norm); return false; }
    size_t len = strlen(norm);
    char *lower = malloc(len + 1);
    if (!lower) { free(norm); return false; }
    for (size_t i = 0; i < len; i++) lower[i] = tolower((unsigned char)norm[i]);
    lower[len] = '\0';
    bool result = (strstr(lower, "azure.com") != NULL);
    free(lower);
    free(norm);
    return result;
}

/* Port of Python anthropic_adapter.py:_is_minimax_anthropic_endpoint(). */
bool anthropic_is_minimax_endpoint(const char *base_url) {
    char *norm = anthropic_normalize_base_url_text(base_url);
    if (!norm || !*norm) { free(norm); return false; }
    size_t len = strlen(norm);
    char *lower = malloc(len + 1);
    if (!lower) { free(norm); return false; }
    for (size_t i = 0; i < len; i++) lower[i] = tolower((unsigned char)norm[i]);
    lower[len] = '\0';
    while (len > 0 && lower[len-1] == '/') lower[--len] = '\0';
    bool result = (
        strncmp(lower, "https://api.minimax.io/anthropic", 33) == 0 ||
        strncmp(lower, "https://api.minimaxi.com/anthropic", 34) == 0
    );
    free(lower);
    free(norm);
    return result;
}

/* Port of Python anthropic_adapter.py:_is_azure_anthropic_endpoint(). */
bool is_azure_anthropic_endpoint(const char *base_url) {
    char *norm = anthropic_normalize_base_url_text(base_url);
    if (!norm || !*norm) { free(norm); return false; }
    size_t len = strlen(norm);
    char *lower = malloc(len + 1);
    if (!lower) { free(norm); return false; }
    for (size_t i = 0; i < len; i++) lower[i] = tolower((unsigned char)norm[i]);
    lower[len] = '\0';
    /* URL parsing: extract hostname */
    const char *host_start = NULL;
    const char *proto = strstr(lower, "://");
    if (proto) host_start = proto + 3; else host_start = lower;
    const char *path_start = strchr(host_start, '/');
    size_t host_len = path_start ? (size_t)(path_start - host_start) : strlen(host_start);
    /* Extract host portion for dot-padded matching */
    char host_buf[512];
    size_t hl = host_len < 511 ? host_len : 511;
    memcpy(host_buf, host_start, hl);
    host_buf[hl] = '\0';
    /* Check with dot-padded boundaries */
    char padded[520];
    snprintf(padded, sizeof(padded), ".%s.", host_buf);
    const char *path = path_start ? path_start : "";
    bool is_foundry = (strstr(padded, ".services.ai.azure.") != NULL);
    bool is_legacy = (strstr(padded, ".openai.azure.") != NULL);
    bool has_anthropic_path = (strstr(path, "/anthropic") != NULL);
    bool result = (is_foundry || is_legacy) && has_anthropic_path;
    free(lower);
    free(norm);
    return result;
}

/* Port of Python anthropic_adapter.py:_common_betas_for_base_url(). */
json_t *anthropic_common_betas_for_base_url(const char *base_url, bool drop_context_1m_beta) {
    json_t *betas = json_array();
    json_append(betas, json_string("interleaved-thinking-2025-05-14"));
    json_append(betas, json_string("fine-grained-tool-streaming-2025-05-14"));

    if (anthropic_base_url_needs_1m_beta(base_url) && !drop_context_1m_beta)
        json_append(betas, json_string("context-1m-2025-08-07"));

    if (anthropic_is_minimax_endpoint(base_url)) {
        /* Strip tool-streaming and context-1m for MiniMax */
        json_t *filtered = json_array();
        size_t n = json_len(betas);
        for (size_t i = 0; i < n; i++) {
            json_t *item = json_get(betas, i);
            if (item && item->type == JSON_STRING && item->str_val) {
                if (strcmp(item->str_val, "fine-grained-tool-streaming-2025-05-14") != 0 &&
                    strcmp(item->str_val, "context-1m-2025-08-07") != 0)
                    json_append(filtered, json_copy(item));
            }
        }
        json_free(betas);
        return filtered;
    }

    if (drop_context_1m_beta) {
        json_t *filtered = json_array();
        size_t n = json_len(betas);
        for (size_t i = 0; i < n; i++) {
            json_t *item = json_get(betas, i);
            if (item && item->type == JSON_STRING && item->str_val) {
                if (strcmp(item->str_val, "context-1m-2025-08-07") != 0)
                    json_append(filtered, json_copy(item));
            }
        }
        json_free(betas);
        return filtered;
    }

    return betas;
}

/* Port of Python anthropic_adapter.py:_is_bedrock_model_id(). */
bool is_bedrock_model_id(const char *model_id) {
    if (!model_id || !*model_id) return false;
    /* Bedrock model IDs: anthropic.claude-* */
    if (strncmp(model_id, "anthropic.claude-", 17) == 0) return true;
    /* Also check for common Bedrock ARN format */
    if (strstr(model_id, "bedrock") && strstr(model_id, "anthropic")) return true;
    return false;
}

/* PoP: anthropic_resolve_positive_max_tokens @ agent/anthropic_adapter.py:_resolve_positive_anthropic_max_tokens */
/* Port of Python anthropic_adapter.py:_resolve_positive_anthropic_max_tokens(). */
int anthropic_resolve_positive_max_tokens(int value) {
    if (value <= 0) return 0;
    hermes_log(LOG_DEBUG, "anthropic", "resolve_positive_max_tokens(%d) = %d", value, value);
    return value;
}

/* Port of Python anthropic_adapter.py:_resolve_anthropic_messages_max_tokens().
 * Resolve max_tokens budget for an Anthropic Messages call.
 * Prefers requested when positive; falls back to model's output ceiling.
 * Returns 4096 if neither resolves (shouldn't happen with current tables). */
int resolve_anthropic_messages_max_tokens(int requested, const char *model) {
    int resolved = anthropic_resolve_positive_max_tokens(requested);
    if (resolved > 0) return resolved;
    int fallback = anthropic_get_model_max_output(model);
    if (fallback > 0) return fallback;
    return 4096; /* safe fallback */
}

/* Port of Python anthropic_adapter.py:normalize_model_name().
 * Strips "anthropic/" prefix, converts dots to hyphens for Claude
 * models (unless preserve_dots is true or model is a Bedrock ID).
 */
char *normalize_model_name(const char *model, bool preserve_dots) {
    if (!model || !*model) return strdup("");

    /* Work on a copy since we may need to modify */
    char buf[256];
    size_t len = strlen(model);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, model, len);
    buf[len] = '\0';

    /* Strip "anthropic/" prefix (case-insensitive) */
    char lower_buf[256];
    for (size_t i = 0; i <= len; i++) lower_buf[i] = (buf[i] >= 'A' && buf[i] <= 'Z') ? buf[i] + 32 : buf[i];

    if (strncmp(lower_buf, "anthropic/", 10) == 0) {
        size_t shift = 10;
        memmove(buf, buf + shift, len - shift + 1);
        len -= shift;
        /* Recompute lower */
        for (size_t i = 0; i <= len; i++) lower_buf[i] = (buf[i] >= 'A' && buf[i] <= 'Z') ? buf[i] + 32 : buf[i];
    }

    if (!preserve_dots) {
        /* Bedrock model IDs (anthropic.claude-*, us.anthropic.claude-*)
         * must NOT have dots converted to hyphens. */
        if (is_bedrock_model_id(buf))
            return strdup(buf);

        /* Only convert dots to hyphens for Claude models */
        if (strncmp(lower_buf, "claude-", 7) == 0) {
            for (size_t i = 0; buf[i]; i++) {
                if (buf[i] == '.') buf[i] = '-';
            }
        }
    }

    return strdup(buf);
}

/* Port of Python anthropic_adapter.py:_sanitize_tool_id().
 * Replace characters not matching [a-zA-Z0-9_-] with underscores.
 */
char *anthropic_sanitize_tool_id(const char *tool_id) {
    if (!tool_id || !*tool_id) return strdup("tool_0");

    char *result = strdup(tool_id);
    if (!result) return strdup("tool_0");

    for (char *p = result; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') ||
              (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') ||
              *p == '_' || *p == '-')) {
            *p = '_';
        }
    }

    /* Ensure non-empty */
    if (!*result) {
        free(result);
        return strdup("tool_0");
    }

    return result;
}

/* Port of Python anthropic_adapter.py:resolve_anthropic_token().
 * Resolve Anthropic API key from environment variables.
 * Checks: ANTHROPIC_API_KEY, ANTHROPIC_KEY, CLAUDE_API_KEY, etc.
 */
char *resolve_anthropic_token(void) {
    /* Check environment variables in priority order */
    const char *keys[] = {
        "ANTHROPIC_API_KEY",
        "ANTHROPIC_KEY",
        "CLAUDE_API_KEY",
        NULL
    };
    for (int i = 0; keys[i]; i++) {
        const char *val = getenv(keys[i]);
        if (val && *val) return strdup(val);
    }

    /* Check for Claude Code credentials file (macOS keychain equivalent) */
    const char *home = getenv("HOME");
    if (home) {
        char path[512];
        int n = snprintf(path, sizeof(path), "%s/.claude/credentials.json", home);
        if (n > 0 && n < (int)sizeof(path)) {
            FILE *f = fopen(path, "r");
            if (f) {
                /* Simple file-read isn't worth implementing without json lib here.
                 * Just return NULL and let the caller fall through. */
                fclose(f);
            }
        }
    }

    return NULL;
}

/* Port of Python anthropic_adapter.py:is_claude_code_token_valid().
 * Check if Claude Code credentials have a non-expired access token.
 * Takes a parsed JSON object of credentials and returns true if valid.
 */
bool is_claude_code_token_valid(json_t *creds) {
    if (!creds) return false;

    double expires_at = json_get_num(creds, "expiresAt", 0.0);
    if (expires_at <= 0.0) {
        /* No expiry set (managed keys) — valid if token is present */
        const char *token = json_get_str(creds, "accessToken", NULL);
        return token && token[0] != '\0';
    }

    /* expiresAt is in milliseconds since epoch */
    time_t now_s = time(NULL);
    double now_ms = (double)now_s * 1000.0;
    /* Allow 60 seconds of buffer */
    return now_ms < (expires_at - 60000.0);
}

/* Port of Python anthropic_adapter.py:read_hermes_oauth_credentials().
 * Read Hermes-managed OAuth credentials from ~/.hermes/.anthropic_oauth.json.
 * Returns a parsed JSON object (caller must json_free), or NULL if not found.
 */
json_t *read_hermes_oauth_credentials(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return NULL;

    char path[1024];
    int n = snprintf(path, sizeof(path), "%s/.hermes/.anthropic_oauth.json", home);
    if (n <= 0 || n >= (int)sizeof(path)) return NULL;

    /* Parse JSON directly from file */
    char *err = NULL;
    json_t *root = json_parse_file(path, &err);
    if (!root) {
        free(err);
        return NULL;
    }

    /* Check for accessToken */
    const char *token = json_get_str(root, "accessToken", NULL);
    if (!token || token[0] == '\0') {
        json_free(root);
        return NULL;
    }

    return root;
}

/* Port of Python anthropic_adapter.py:_image_source_from_openai_url().
 * Convert an OpenAI-style image URL/data URL into Anthropic image source.
 * Returns a JSON object with type/url or type/media_type/data (caller must json_free).
 */
json_t *image_source_from_openai_url(const char *url) {
    if (!url || !*url) {
        json_t *r = json_object();
        json_object_set(r, "type", json_string("url"));
        json_object_set(r, "url", json_string(""));
        return r;
    }

    /* Skip leading/trailing whitespace */
    while (*url == ' ' || *url == '\t') url++;
    size_t len = strlen(url);
    while (len > 0 && (url[len-1] == ' ' || url[len-1] == '\t')) len--;

    /* data: URL — extract media type and payload */
    if (len >= 5 && strncmp(url, "data:", 5) == 0) {
        const char *comma = strchr(url + 5, ',');
        if (!comma) {
            json_t *r = json_object();
            json_object_set(r, "type", json_string("url"));
            json_object_set(r, "url", json_string(""));
            return r;
        }
        const char *data_start = comma + 1;

        /* Default media type */
        char media_type[64] = "image/jpeg";

        /* Parse mime type from "data:image/png;base64,..." */
        size_t hdr_len = (size_t)(comma - url);
        const char *mime_part = url + 5; /* after "data:" */
        const char *semi = memchr(mime_part, ';', hdr_len - 5);
        size_t mime_len = semi ? (size_t)(semi - mime_part) : (hdr_len - 5);
        if (mime_len > 0 && mime_len < sizeof(media_type) - 1) {
            char buf[64];
            memcpy(buf, mime_part, mime_len);
            buf[mime_len] = '\0';
            if (strncasecmp(buf, "image/", 6) == 0) {
                memcpy(media_type, buf, mime_len + 1);
            }
        }

        json_t *r = json_object();
        json_object_set(r, "type", json_string("base64"));
        json_object_set(r, "media_type", json_string(media_type));
        json_object_set(r, "data", json_string(data_start));
        return r;
    }

    /* Regular URL */
    {
        char url_buf[4096];
        size_t cp_len = len < sizeof(url_buf) - 1 ? len : sizeof(url_buf) - 1;
        memcpy(url_buf, url, cp_len);
        url_buf[cp_len] = '\0';
        json_t *r = json_object();
        json_object_set(r, "type", json_string("url"));
        json_object_set(r, "url", json_string(url_buf));
        return r;
    }
}

/* Port of Python anthropic_adapter.py:_evict_old_screenshots().
 * Keep only the most recent 3 computer-use screenshots in the message array.
 * Base64 images cost ~1,465 tokens each and accumulate across tool calls.
 * Mutates the JSON array in place.
 */
/* Port of Python: evict_old_screenshots */
void evict_old_screenshots(json_t *result) {
    if (!result || result->type != JSON_ARRAY) return;
    const int MAX_KEEP = 3;
    int image_count = 0;

    for (int i = (int)json_len(result) - 1; i >= 0; i--) {
        json_t *msg = json_get(result, i);
        if (!msg || msg->type != JSON_OBJECT) continue;

        json_t *content = json_obj_get(msg, "content");
        if (!content || content->type != JSON_ARRAY) continue;

        size_t n = json_len(content);
        for (size_t j = 0; j < n; j++) {
            json_t *block = json_get(content, j);
            if (!block || block->type != JSON_OBJECT) continue;

            const char *type = json_get_str(block, "type", "");
            if (strcmp(type, "tool_result") != 0) continue;

            json_t *inner = json_obj_get(block, "content");
            if (!inner || inner->type != JSON_ARRAY) continue;

            /* Check if this block has any image */
            bool has_image = false;
            size_t inner_n = json_len(inner);
            for (size_t k = 0; k < inner_n; k++) {
                json_t *b = json_get(inner, k);
                if (b && b->type == JSON_OBJECT) {
                    const char *bt = json_get_str(b, "type", "");
                    if (strcmp(bt, "image") == 0) {
                        has_image = true;
                        break;
                    }
                }
            }
            if (!has_image) continue;

            image_count++;
            if (image_count > MAX_KEEP) {
                /* Replace images with placeholder text */
                json_t *new_inner = json_array();
                for (size_t k = 0; k < inner_n; k++) {
                    json_t *b = json_get(inner, k);
                    if (b && b->type == JSON_OBJECT) {
                        const char *bt = json_get_str(b, "type", "");
                        if (strcmp(bt, "image") == 0) {
                            json_t *placeholder = json_object();
                            json_object_set(placeholder, "type", json_string("text"));
                            json_object_set(placeholder, "text",
                                json_string("[screenshot removed to save context]"));
                            json_append(new_inner, placeholder);
                        } else {
                            json_append(new_inner, json_copy(b));
                        }
                    } else {
                        json_append(new_inner, json_copy(b));
                    }
                }
                json_object_set(block, "content", new_inner);
                json_free(new_inner);
            }
        }
    }
}

/* Port of Python anthropic_adapter.py:_strip_orphaned_tool_blocks().
 * Strip tool_use blocks with no matching tool_result, and vice versa.
 * Context compression or session truncation can remove either side of a
 * tool-call pair. Anthropic rejects both orphans with HTTP 400.
 * Mutates the JSON array in place.
 */
void strip_orphaned_tool_blocks(json_t *result) {
    if (!result || result->type != JSON_ARRAY) return;

    /* Phase 1: collect tool_result IDs, strip orphaned tool_use blocks */
    /* Collect tool_use_id from tool_result blocks in user messages */
    size_t max_ids = 256;
    char **tool_result_ids = calloc(max_ids, sizeof(char *));
    size_t n_ids = 0;
    {
        size_t n = json_len(result);
        for (size_t i = 0; i < n; i++) {
            json_t *msg = json_get(result, i);
            if (!msg || msg->type != JSON_OBJECT) continue;
            const char *role = json_get_str(msg, "role", "");
            if (strcmp(role, "user") != 0) continue;
            json_t *content = json_obj_get(msg, "content");
            if (!content || content->type != JSON_ARRAY) continue;
            size_t cn = json_len(content);
            for (size_t j = 0; j < cn; j++) {
                json_t *block = json_get(content, j);
                if (!block || block->type != JSON_OBJECT) continue;
                const char *bt = json_get_str(block, "type", "");
                if (strcmp(bt, "tool_result") != 0) continue;
                const char *tid = json_get_str(block, "tool_use_id", NULL);
                if (tid && n_ids < max_ids)
                    tool_result_ids[n_ids++] = strdup(tid);
            }
        }
    }

    /* Strip orphaned tool_use from assistant messages */
    {
        size_t n = json_len(result);
        for (size_t i = 0; i < n; i++) {
            json_t *msg = json_get(result, i);
            if (!msg || msg->type != JSON_OBJECT) continue;
            const char *role = json_get_str(msg, "role", "");
            if (strcmp(role, "assistant") != 0) continue;
            json_t *content = json_obj_get(msg, "content");
            if (!content || content->type != JSON_ARRAY) continue;

            bool had_thinking = false;
            json_t *kept = json_array();
            size_t cn = json_len(content);
            for (size_t j = 0; j < cn; j++) {
                json_t *block = json_get(content, j);
                if (!block || block->type != JSON_OBJECT) {
                    json_append(kept, json_copy(block));
                    continue;
                }
                const char *bt = json_get_str(block, "type", "");
                if (strcmp(bt, "tool_use") == 0) {
                    const char *bid = json_get_str(block, "id", NULL);
                    bool matched = false;
                    for (size_t k = 0; k < n_ids; k++) {
                        if (bid && tool_result_ids[k] && strcmp(bid, tool_result_ids[k]) == 0) {
                            matched = true;
                            break;
                        }
                    }
                    if (!matched) {
                        /* Orphaned — check if message also has thinking blocks */
                        for (size_t t = 0; t < cn && !had_thinking; t++) {
                            json_t *tb = json_get(content, t);
                            if (tb && tb->type == JSON_OBJECT) {
                                const char *tt = json_get_str(tb, "type", "");
                                if (strcmp(tt, "thinking") == 0 || strcmp(tt, "redacted_thinking") == 0)
                                    had_thinking = true;
                            }
                        }
                        continue; /* drop orphaned tool_use */
                    }
                }
                json_append(kept, json_copy(block));
            }

            size_t kept_len = json_len(kept);
            if (kept_len < cn) {
                /* Something was stripped */
                if (had_thinking)
                    json_object_set(msg, "_thinking_signature_invalidated", json_bool(true));
                if (kept_len == 0)
                    json_append(kept, json_string("[tool call removed]"));
                json_object_set(msg, "content", kept);
            }
            json_free(kept);
        }
    }

    /* Phase 2: collect tool_use IDs, strip orphaned tool_result blocks */
    /* Reset for second pass */
    for (size_t k = 0; k < n_ids; k++) free(tool_result_ids[k]);
    n_ids = 0;

    /* Collect id from tool_use blocks in assistant messages */
    {
        size_t n = json_len(result);
        for (size_t i = 0; i < n; i++) {
            json_t *msg = json_get(result, i);
            if (!msg || msg->type != JSON_OBJECT) continue;
            const char *role = json_get_str(msg, "role", "");
            if (strcmp(role, "assistant") != 0) continue;
            json_t *content = json_obj_get(msg, "content");
            if (!content || content->type != JSON_ARRAY) continue;
            size_t cn = json_len(content);
            for (size_t j = 0; j < cn; j++) {
                json_t *block = json_get(content, j);
                if (!block || block->type != JSON_OBJECT) continue;
                const char *bt = json_get_str(block, "type", "");
                if (strcmp(bt, "tool_use") != 0) continue;
                const char *bid = json_get_str(block, "id", NULL);
                if (bid && n_ids < max_ids)
                    tool_result_ids[n_ids++] = strdup(bid);
            }
        }
    }

    /* Strip orphaned tool_result from user messages */
    {
        size_t n = json_len(result);
        for (size_t i = 0; i < n; i++) {
            json_t *msg = json_get(result, i);
            if (!msg || msg->type != JSON_OBJECT) continue;
            const char *role = json_get_str(msg, "role", "");
            if (strcmp(role, "user") != 0) continue;
            json_t *content = json_obj_get(msg, "content");
            if (!content || content->type != JSON_ARRAY) continue;

            json_t *kept = json_array();
            size_t cn = json_len(content);
            for (size_t j = 0; j < cn; j++) {
                json_t *block = json_get(content, j);
                if (!block || block->type != JSON_OBJECT) {
                    json_append(kept, json_copy(block));
                    continue;
                }
                const char *bt = json_get_str(block, "type", "");
                if (strcmp(bt, "tool_result") == 0) {
                    const char *tid = json_get_str(block, "tool_use_id", NULL);
                    bool matched = false;
                    for (size_t k = 0; k < n_ids; k++) {
                        if (tid && tool_result_ids[k] && strcmp(tid, tool_result_ids[k]) == 0) {
                            matched = true;
                            break;
                        }
                    }
                    if (!matched)
                        continue; /* drop orphaned tool_result */
                }
                json_append(kept, json_copy(block));
            }

            size_t kept_len = json_len(kept);
            if (kept_len < cn) {
                if (kept_len == 0)
                    json_append(kept, json_string("[tool result removed]"));
                json_object_set(msg, "content", kept);
            }
            json_free(kept);
        }
    }

    for (size_t k = 0; k < n_ids; k++) free(tool_result_ids[k]);
    free(tool_result_ids);
}

/* Port of Python anthropic_adapter.py:_merge_consecutive_roles().
 * Merge consecutive same-role messages to enforce Anthropic alternation.
 * Returns a new JSON array (caller must json_free).
 */
json_t *anthropic_merge_consecutive_roles(json_t *result) {
    json_t *fixed = json_array();
    if (!result || result->type != JSON_ARRAY) return fixed;

    size_t n = json_len(result);
    for (size_t i = 0; i < n; i++) {
        json_t *m = json_get(result, i);
        if (!m || m->type != JSON_OBJECT) { json_append(fixed, json_copy(m)); continue; }

        size_t fixed_n = json_len(fixed);
        if (fixed_n > 0) {
            json_t *last = json_get(fixed, fixed_n - 1);
            const char *last_role = json_get_str(last, "role", "");
            const char *cur_role = json_get_str(m, "role", "");

            if (strcmp(last_role, cur_role) == 0) {
                /* Same role — merge */
                if (strcmp(cur_role, "user") == 0) {
                    json_t *prev_content = json_obj_get(last, "content");
                    json_t *cur_content = json_obj_get(m, "content");

                    json_t *merged;
                    if (prev_content && cur_content &&
                        prev_content->type == JSON_STRING && cur_content->type == JSON_STRING) {
                        /* String + String → concatenate with newline */
                        const char *ps = json_get_str(last, "content", "");
                        const char *cs = json_get_str(m, "content", "");
                        char buf[65536];
                        snprintf(buf, sizeof(buf), "%s\n%s", ps, cs);
                        merged = json_string(buf);
                    } else if (prev_content && cur_content &&
                               prev_content->type == JSON_ARRAY && cur_content->type == JSON_ARRAY) {
                        /* Array + Array → concatenate */
                        merged = json_copy(prev_content);
                        size_t cn = json_len(cur_content);
                        for (size_t j = 0; j < cn; j++)
                            json_append(merged, json_copy(json_get(cur_content, j)));
                    } else {
                        /* Mixed types — normalize to arrays then concatenate */
                        merged = json_array();
                        if (prev_content) {
                            if (prev_content->type == JSON_STRING) {
                                json_t *tb = json_object();
                                json_object_set(tb, "type", json_string("text"));
                                json_object_set(tb, "text", json_copy(prev_content));
                                json_append(merged, tb);
                            } else if (prev_content->type == JSON_ARRAY) {
                                size_t cn = json_len(prev_content);
                                for (size_t j = 0; j < cn; j++)
                                    json_append(merged, json_copy(json_get(prev_content, j)));
                            }
                        }
                        if (cur_content) {
                            if (cur_content->type == JSON_STRING) {
                                json_t *tb = json_object();
                                json_object_set(tb, "type", json_string("text"));
                                json_object_set(tb, "text", json_copy(cur_content));
                                json_append(merged, tb);
                            } else if (cur_content->type == JSON_ARRAY) {
                                size_t cn = json_len(cur_content);
                                for (size_t j = 0; j < cn; j++)
                                    json_append(merged, json_copy(json_get(cur_content, j)));
                            }
                        }
                    }
                    json_object_set(last, "content", merged);
                    json_free(merged);
                } else {
                    /* Consecutive assistant messages */
                    /* Propagate signature-invalidation flag */
                    bool sig_inv = json_get_bool(m, "_thinking_signature_invalidated", false);
                    if (sig_inv)
                        json_object_set(last, "_thinking_signature_invalidated", json_bool(true));

                    /* Strip thinking blocks from second message's content */
                    json_t *cur_content = json_obj_get(m, "content");
                    json_t *prev_content = json_obj_get(last, "content");

                    json_t *stripped = NULL;
                    if (cur_content && cur_content->type == JSON_ARRAY) {
                        stripped = json_array();
                        size_t cn = json_len(cur_content);
                        for (size_t j = 0; j < cn; j++) {
                            json_t *b = json_get(cur_content, j);
                            if (b && b->type == JSON_OBJECT) {
                                const char *bt = json_get_str(b, "type", "");
                                if (strcmp(bt, "thinking") == 0 || strcmp(bt, "redacted_thinking") == 0)
                                    continue;
                            }
                            json_append(stripped, json_copy(b));
                        }
                        json_object_set(m, "content", stripped);
                        json_free(stripped);
                    }

                    /* Merge content */
                    json_t *merged;
                    if (prev_content && prev_content->type == JSON_ARRAY &&
                        cur_content && cur_content->type == JSON_ARRAY) {
                        merged = json_copy(prev_content);
                        size_t cn = json_len(cur_content);
                        for (size_t j = 0; j < cn; j++)
                            json_append(merged, json_copy(json_get(cur_content, j)));
                    } else {
                        /* Fallback: normalize to arrays */
                        merged = json_array();
                        if (prev_content) {
                            if (prev_content->type == JSON_STRING) {
                                json_t *tb = json_object();
                                json_object_set(tb, "type", json_string("text"));
                                json_object_set(tb, "text", json_copy(prev_content));
                                json_append(merged, tb);
                            } else if (prev_content->type == JSON_ARRAY) {
                                size_t cn = json_len(prev_content);
                                for (size_t j = 0; j < cn; j++)
                                    json_append(merged, json_copy(json_get(prev_content, j)));
                            }
                        }
                        if (cur_content) {
                            if (cur_content->type == JSON_STRING) {
                                json_t *tb = json_object();
                                json_object_set(tb, "type", json_string("text"));
                                json_object_set(tb, "text", json_copy(cur_content));
                                json_append(merged, tb);
                            } else if (cur_content->type == JSON_ARRAY) {
                                size_t cn = json_len(cur_content);
                                for (size_t j = 0; j < cn; j++)
                                    json_append(merged, json_copy(json_get(cur_content, j)));
                            }
                        }
                    }
                    json_object_set(last, "content", merged);
                    json_free(merged);
                }
                continue; /* skip current m — merged into last */
            }
        }
        json_append(fixed, json_copy(m));
    }
    return fixed;
}

/* ================================================================
 *  Message conversion helpers
 * ================================================================ */

/* Port of Python anthropic_adapter.py:_convert_content_part_to_anthropic().
 * Convert a single OpenAI-style content part (json_t*) to Anthropic format.
 *
 * Handles:
 *  - string values → {"type":"text","text":...}
 *  - input_text type → text block
 *  - image_url / input_image types → image block with source
 *  - Cache control passthrough
 *
 * Returns a json_t* object (caller must json_free), or NULL on input null.
 */
json_t *anthropic_convert_content_part_to_anthropic(const json_t *part) {
    if (!part) return NULL;

    json_t *block = NULL;

    if (part->type == JSON_STRING) {
        /* Plain string → text block */
        block = json_object();
        json_set(block, "type", json_string("text"));
        json_set(block, "text", json_string(part->str_val ? part->str_val : ""));
        return block;
    }

    if (part->type != JSON_OBJECT) {
        /* Non-string, non-object → serialize */
        char *s = json_serialize((json_t *)part);
        block = json_object();
        json_set(block, "type", json_string("text"));
        json_set(block, "text", json_string(s ? s : ""));
        free(s);
        return block;
    }

    const char *ptype = json_get_str(part, "type", "");

    if (strcmp(ptype, "input_text") == 0) {
        block = json_object();
        json_set(block, "type", json_string("text"));
        json_set(block, "text", json_copy(json_obj_get(part, "text")));
    } else if (strcmp(ptype, "image_url") == 0 || strcmp(ptype, "input_image") == 0) {
        json_t *image_value = json_obj_get(part, "image_url");
        const char *url = "";
        if (image_value && image_value->type == JSON_OBJECT) {
            url = json_get_str(image_value, "url", "");
        } else if (image_value && image_value->type == JSON_STRING) {
            url = image_value->str_val ? image_value->str_val : "";
        }
        json_t *source = image_source_from_openai_url(url);
        block = json_object();
        json_set(block, "type", json_string("image"));
        json_set(block, "source", source);
    } else {
        /* Pass through unknown types as-is */
        block = json_copy((json_t *)part);
    }

    /* Preserve cache_control if present and not already in block */
    json_t *cc = json_obj_get((json_t *)part, "cache_control");
    if (cc && cc->type == JSON_OBJECT) {
        json_t *existing = json_obj_get(block, "cache_control");
        if (!existing)
            json_set(block, "cache_control", json_copy(cc));
    }

    return block;
}

/* Port of Python anthropic_adapter.py:_convert_content_to_anthropic().
 * Convert an OpenAI-style multimodal content array to Anthropic blocks.
 * Calls anthropic_convert_content_part_to_anthropic for each element.
 * Returns new json array (caller must json_free).
 */
json_t *anthropic_convert_content_to_anthropic(const json_t *content) {
    json_t *result = json_array();
    if (!content || content->type != JSON_ARRAY) {
        /* Non-array content passed through as single text block */
        if (content) {
            json_t *block = anthropic_convert_content_part_to_anthropic(content);
            if (block) json_append(result, block);
        }
        return result;
    }

    size_t n = json_len(content);
    for (size_t i = 0; i < n; i++) {
        json_t *part = json_get(content, i);
        json_t *block = anthropic_convert_content_part_to_anthropic(part);
        if (block) json_append(result, block);
    }
    return result;
}

/* Port of Python anthropic_adapter.py:_content_parts_to_anthropic_blocks().
 * Convert OpenAI-style tool-message content parts to Anthropic tool_result
 * inner blocks. Filters to text and image types only — excludes tool_use
 * and other block types not valid inside tool_result content. */
json_t *anthropic_content_parts_to_anthropic_blocks(const json_t *parts) {
    json_t *out = json_array();
    if (!parts || parts->type != JSON_ARRAY)
        return out;

    size_t n = json_len(parts);
    for (size_t i = 0; i < n; i++) {
        json_t *part = json_get(parts, i);
        json_t *block = anthropic_convert_content_part_to_anthropic(part);
        if (!block) continue;

        const char *btype = json_get_str(block, "type", "");
        if (strcmp(btype, "text") == 0) {
            const char *text_val = json_get_str(block, "text", "");
            if (text_val && *text_val)
                json_append(out, block);
            else
                json_free(block);
        } else if (strcmp(btype, "image") == 0) {
            json_t *src = json_obj_get(block, "source");
            if (src && src->type == JSON_OBJECT)
                json_append(out, block);
            else
                json_free(block);
        } else {
            json_free(block);
        }
    }
    return out;
}

/* Port of Python anthropic_adapter.py:_convert_user_message().
 * Validate and convert a user message to Anthropic format.
 * Returns a new json object (caller must json_free).
 * Content is either a converted blocks array or a plain string. */
json_t *anthropic_convert_user_message(const json_t *content) {
    json_t *user_msg = json_object();
    json_set(user_msg, "role", json_string("user"));

    if (content && content->type == JSON_ARRAY) {
        json_t *blocks = anthropic_convert_content_to_anthropic(content);
        size_t n = json_len(blocks);
        bool all_empty = true;
        for (size_t i = 0; i < n; i++) {
            json_t *b = json_get(blocks, i);
            const char *text = json_get_str(b, "text", "");
            if (text && *text) { all_empty = false; break; }
        }
        if (!blocks || n == 0 || all_empty) {
            json_free(blocks);
            blocks = json_array();
            json_append(blocks, json_string("(empty message)"));
        }
        json_set(user_msg, "content", blocks);
    } else {
        const char *text = (content && content->type == JSON_STRING)
            ? json_get_str(content, NULL, "")
            : "";
        if (!text || !*text) text = "(empty message)";
        json_set(user_msg, "content", json_string(text));
    }
    return user_msg;
}

/* Port of Python anthropic_adapter.py:_extract_preserved_thinking_blocks().
 * Return Anthropic thinking blocks previously preserved on the message.
 * Reads reasoning_details array, filters for thinking/redacted_thinking types.
 * Returns new json array (caller must json_free). */
json_t *anthropic_extract_preserved_thinking_blocks(const json_t *message) {
    json_t *preserved = json_array();
    if (!message) return preserved;

    json_t *raw = json_obj_get(message, "reasoning_details");
    if (!raw || raw->type != JSON_ARRAY) return preserved;

    size_t n = json_len(raw);
    for (size_t i = 0; i < n; i++) {
        json_t *detail = json_get(raw, i);
        if (!detail || detail->type != JSON_OBJECT) continue;
        const char *btype = json_get_str(detail, "type", "");
        if (strcmp(btype, "thinking") != 0 && strcmp(btype, "redacted_thinking") != 0)
            continue;
        json_append(preserved, json_copy(detail));
    }
    return preserved;
}

/* Port of Python anthropic_adapter.py:_convert_assistant_message().
 * Convert an assistant message to Anthropic content blocks.
 * Handles thinking blocks, regular content, tool calls, and
 * reasoning_content injection for Kimi/DeepSeek endpoints.
 * Returns new json object (caller must json_free).
 *
 * Takes a raw message dict with optional fields:
 *   content, tool_calls, reasoning_details, reasoning_content,
 *   _extracted_thinking (cache_control passthrough on content). */
/* PoP: anthropic_apply_assistant_cache_control_to_last_cacheable_block @ agent/anthropic_adapter.py:_apply_assistant_cache_control_to_last_cacheable_block */
/* Port of Python agent/anthropic_adapter.py:_apply_assistant_cache_control_to_last_cacheable_block().
 * Mirrors the Python helper that walks `blocks` from the end and applies
 * `cache_control` (deep-copied) to the last block whose `type` is either
 * "text" or "tool_use". When cache_control is not a dict the call is a no-op
 * (matches Python's `if not isinstance(cache_control, dict): return`).
 * Mutates `blocks` in place; the caller still owns the array. */
void anthropic_apply_assistant_cache_control_to_last_cacheable_block(json_t *blocks,
                                                                      const json_t *cache_control) {
    if (!blocks || blocks->type != JSON_ARRAY) return;
    if (!cache_control || cache_control->type != JSON_OBJECT) return;

    size_t n = json_len(blocks);
    for (size_t i = n; i-- > 0; ) {
        json_t *block = json_get(blocks, i);
        if (!block || block->type != JSON_OBJECT) continue;
        const char *t = json_get_str(block, "type", "");
        if (t && (strcmp(t, "text") == 0 || strcmp(t, "tool_use") == 0)) {
            /* setdefault semantics: only attach if not already present — match Python's
             * `block.setdefault("cache_control", dict(cache_control))`. */
            if (!json_obj_get(block, "cache_control")) {
                json_set(block, "cache_control", json_copy(cache_control));
            }
            return;
        }
    }
}

json_t *anthropic_convert_assistant_message(const json_t *m) {
    json_t *assistant_msg = json_object();
    json_set(assistant_msg, "role", json_string("assistant"));

    /* Start with preserved thinking blocks */
    json_t *blocks = anthropic_extract_preserved_thinking_blocks(m);

    /* Convert content */
    json_t *content = json_obj_get(m, "content");
    if (content) {
        if (content->type == JSON_ARRAY) {
            json_t *converted = anthropic_convert_content_to_anthropic(content);
            if (converted && json_len(converted) > 0) {
                /* Extend blocks with converted content */
                size_t cn = json_len(converted);
                for (size_t i = 0; i < cn; i++)
                    json_append(blocks, json_copy(json_get(converted, i)));
                json_free(converted);
            }
        } else {
            const char *text = json_get_str(content, NULL, "");
            if (!text) text = "";
            json_t *tb = json_object();
            json_set(tb, "type", json_string("text"));
            json_set(tb, "text", json_string(text));
            json_append(blocks, tb);
        }
    }

    /* Convert tool_calls to tool_use blocks */
    json_t *tool_calls = json_obj_get(m, "tool_calls");
    if (tool_calls && tool_calls->type == JSON_ARRAY) {
        size_t tn = json_len(tool_calls);
        for (size_t i = 0; i < tn; i++) {
            json_t *tc = json_get(tool_calls, i);
            if (!tc || tc->type != JSON_OBJECT) continue;
            json_t *fn = json_obj_get(tc, "function");
            if (!fn || fn->type != JSON_OBJECT) continue;
            const char *name = json_get_str(fn, "name", "");
            const char *tid = json_get_str(tc, "id", "");

            /* Parse arguments string -> json */
            json_t *parsed_args = NULL;
            json_t *args = json_obj_get(fn, "arguments");
            if (args) {
                if (args->type == JSON_STRING) {
                    /* Parse JSON string */
                    char *err = NULL;
                    parsed_args = json_parse(args->str_val, &err);
                    if (err) { free(err); err = NULL; }
                    if (!parsed_args) parsed_args = json_object();
                } else {
                    parsed_args = json_copy(args);
                }
            }
            if (!parsed_args) parsed_args = json_object();

            json_t *tb = json_object();
            json_set(tb, "type", json_string("tool_use"));
            {
                char *sanitized = anthropic_sanitize_tool_id(tid);
                json_set(tb, "id", json_string(sanitized ? sanitized : ""));
                free(sanitized);
            }
            json_set(tb, "name", json_string(name));
            json_set(tb, "input", parsed_args);
            json_append(blocks, tb);
        }
    }

    /* Inject reasoning_content as thinking block (Kimi/DeepSeek) */
    json_t *reasoning = json_obj_get(m, "reasoning_content");
    if (reasoning && reasoning->type == JSON_STRING && reasoning->str_val) {
        /* Check if blocks already have thinking/redacted_thinking */
        bool has_thinking = false;
        size_t bn = json_len(blocks);
        for (size_t i = 0; i < bn; i++) {
            json_t *b = json_get(blocks, i);
            if (!b || b->type != JSON_OBJECT) continue;
            const char *bt = json_get_str(b, "type", "");
            if (strcmp(bt, "thinking") == 0 || strcmp(bt, "redacted_thinking") == 0) {
                has_thinking = true; break;
            }
        }
        if (!has_thinking) {
            json_t *tb = json_object();
            json_set(tb, "type", json_string("thinking"));
            json_set(tb, "thinking", json_string(reasoning->str_val));
            /* Prepend */
            json_t *new_blocks = json_array();
            json_append(new_blocks, tb);
            for (size_t i = 0; i < bn; i++)
                json_append(new_blocks, json_copy(json_get(blocks, i)));
            json_free(blocks);
            blocks = new_blocks;
        }
    }

    /* Empty guard — Anthropic rejects empty assistant content */
    if (json_len(blocks) == 0 && content && content->type == JSON_STRING) {
        const char *ct = content->str_val;
        if (ct && *ct) {
            json_t *tb = json_object();
            json_set(tb, "type", json_string("text"));
            json_set(tb, "text", json_string(ct));
            json_append(blocks, tb);
        }
    }
    if (json_len(blocks) == 0) {
        json_t *tb = json_object();
        json_set(tb, "type", json_string("text"));
        json_set(tb, "text", json_string("(empty)"));
        json_append(blocks, tb);
    }

    /* Python parity: apply cache_control to the last cacheable text/tool_use block. */
    anthropic_apply_assistant_cache_control_to_last_cacheable_block(blocks, json_obj_get(m, "cache_control"));

    json_set(assistant_msg, "content", blocks);
    return assistant_msg;
}

/* Port of Python anthropic_adapter.py:_normalize_tool_input_schema().
 * Normalize a tool input schema for Anthropic:
 *   1. Null/non-object → default object schema.
 *   2. Strip nullable unions from anyOf (remove type:null branches).
 *   3. Strip top-level oneOf/allOf/anyOf (Anthropic rejects them).
 *   4. Ensure type="object" has a properties dict.
 * Returns new json_t* (caller must json_free).
 *
 * NOTE: Uses direct struct access (c.keys, c.items, c.count) to iterate
 * object fields since libjson does not expose json_delete or json_get_key. */
static json_t *normalize_tool_input_schema(const json_t *schema) {
    if (!schema || schema->type != JSON_OBJECT) {
        json_t *def = json_object();
        json_set(def, "type", json_string("object"));
        json_set(def, "properties", json_object());
        return def;
    }

    /* Step 1: Start from a copy of the input. */
    json_t *normalized = json_copy((json_t *)schema);

    /* Step 2: Strip nullable unions from anyOf.
     * Look for anyOf arrays where one entry is {"type": "null"} and remove it. */
    json_t *anyof = json_obj_get(normalized, "anyOf");
    if (anyof && anyof->type == JSON_ARRAY && json_len(anyof) > 0) {
        json_t *cleaned = json_array();
        size_t n = json_len(anyof);
        for (size_t i = 0; i < n; i++) {
            json_t *elem = json_get(anyof, i);
            if (!elem || (elem->type == JSON_OBJECT &&
                strcmp(json_get_str(elem, "type", ""), "null") == 0))
                continue;
            json_append(cleaned, json_copy(elem));
        }
        if (json_len(cleaned) > 0) {
            json_set(normalized, "anyOf", cleaned);
        }
        /* If anyOf is now empty (only had nullable union), just leave it
         * — it will be stripped in Step 3's banned-key check below. */
    }

    /* Step 3: Rebuild the object without oneOf/allOf/anyOf keys.
     * libjson has no json_delete, so we rebuild by copying non-banned keys. */
    json_t *rebuilt = json_object();
    static const char *banned[] = {"oneOf", "allOf", "anyOf", NULL};
    for (size_t i = 0; i < normalized->c.count; i++) {
        const char *key = normalized->c.keys[i];
        if (!key) continue;
        bool is_banned = false;
        for (int b = 0; banned[b]; b++) {
            if (strcmp(key, banned[b]) == 0) { is_banned = true; break; }
        }
        if (!is_banned)
            json_set(rebuilt, key, json_copy(normalized->c.items[i]));
    }
    json_free(normalized);
    normalized = rebuilt;

    /* Ensure type field exists — default to "object" */
    const char *type_str = json_get_str(normalized, "type", "");
    if (!*type_str)
        json_set(normalized, "type", json_string("object"));

    /* Step 4: Ensure object types have a properties dict. */
    if (strcmp(json_get_str(normalized, "type", ""), "object") == 0) {
        json_t *props = json_obj_get(normalized, "properties");
        if (!props || props->type != JSON_OBJECT)
            json_set(normalized, "properties", json_object());
    }

    return normalized;
}

/* Port of Python anthropic_adapter.py:convert_tools_to_anthropic().
 * Convert OpenAI tool definitions to Anthropic format.
 * Tools param: JSON array of tool dicts. Returns new json array (caller must json_free). */
json_t *anthropic_convert_tools_to_anthropic(const json_t *tools) {
    if (!tools || tools->type != JSON_ARRAY || json_len(tools) == 0)
        return json_array();

    json_t *result = json_array();
    /* Simple set for dedup: just track seen names */
    size_t max_names = json_len(tools);
    char **seen_names = calloc(max_names, sizeof(char *));
    size_t n_seen = 0;

    size_t n = json_len(tools);
    for (size_t i = 0; i < n; i++) {
        json_t *t = json_get(tools, i);
        if (!t || t->type != JSON_OBJECT) continue;

        json_t *fn = json_obj_get(t, "function");
        if (!fn || fn->type != JSON_OBJECT) continue;

        const char *name = json_get_str(fn, "name", "");
        if (!*name) continue;

        /* Dedup: skip duplicate names */
        bool dup = false;
        for (size_t j = 0; j < n_seen; j++) {
            if (strcmp(seen_names[j], name) == 0) { dup = true; break; }
        }
        if (dup) continue;
        seen_names[n_seen++] = strdup(name);

        const char *desc = json_get_str(fn, "description", "");
        json_t *params = json_obj_get(fn, "parameters");

        json_t *input_schema;
        if (params)
            input_schema = normalize_tool_input_schema(params);
        else {
            input_schema = json_object();
            json_set(input_schema, "type", json_string("object"));
            json_set(input_schema, "properties", json_object());
        }

        json_t *anthropic_tool = json_object();
        json_set(anthropic_tool, "name", json_string(name));
        json_set(anthropic_tool, "description", json_string(desc ? desc : ""));
        json_set(anthropic_tool, "input_schema", input_schema);

        /* Forward cache_control */
        json_t *cc = json_obj_get(t, "cache_control");
        if (cc && cc->type == JSON_OBJECT)
            json_set(anthropic_tool, "cache_control", json_copy(cc));

        json_append(result, anthropic_tool);
    }

    for (size_t j = 0; j < n_seen; j++) free(seen_names[j]);
    free(seen_names);
    return result;
}

/* Port of Python anthropic_adapter.py:_convert_tool_message_to_result().
 * Convert a tool message to an Anthropic tool_result, merging consecutive
 * results into one user message.
 *
 * Mutates result in place — either appends a new user message or extends
 * the trailing user message's tool_result list. */
void convert_tool_message_to_result(json_t *result, const json_t *m) {
    if (!result || !m) return;

    json_t *content = json_obj_get(m, "content");
    json_t *multimodal_blocks = NULL;

    /* Check _multimodal flag */
    if (content && content->type == JSON_OBJECT) {
        json_t *mm = json_obj_get(content, "_multimodal");
        if (mm && mm->type == JSON_BOOL && mm->bool_val) {
            json_t *inner = json_obj_get(content, "content");
            if (inner) multimodal_blocks = anthropic_content_parts_to_anthropic_blocks(inner);
            /* Fallback text */
            if ((!multimodal_blocks || json_len(multimodal_blocks) == 0)) {
                json_t *text_summary = json_obj_get(content, "text_summary");
                if (text_summary) {
                    if (multimodal_blocks) json_free(multimodal_blocks);
                    multimodal_blocks = json_array();
                    json_t *tb = json_object();
                    json_set(tb, "type", json_string("text"));
                    json_set(tb, "text", json_copy(text_summary));
                    json_append(multimodal_blocks, tb);
                }
            }
        }
    }

    /* Check list content for image blocks */
    if (!multimodal_blocks && content && content->type == JSON_ARRAY) {
        json_t *converted = anthropic_content_parts_to_anthropic_blocks(content);
        if (converted) {
            size_t cn = json_len(converted);
            for (size_t i = 0; i < cn; i++) {
                json_t *b = json_get(converted, i);
                const char *bt = json_get_str(b, "type", "");
                if (strcmp(bt, "image") == 0) {
                    multimodal_blocks = converted;
                    break;
                }
            }
            if (!multimodal_blocks) json_free(converted);
        }
    }

    /* Back-compat: _anthropic_content_blocks stash */
    if (!multimodal_blocks) {
        json_t *stashed = json_obj_get(m, "_anthropic_content_blocks");
        if (stashed && stashed->type == JSON_ARRAY && json_len(stashed) > 0) {
            multimodal_blocks = json_array();
            bool has_text_content = (content && content->type == JSON_STRING);
            const char *text = has_text_content ? json_get_str(content, NULL, "") : "";
            if (has_text_content && text && *text) {
                json_t *tb = json_object();
                json_set(tb, "type", json_string("text"));
                json_set(tb, "text", json_string(text));
                json_append(multimodal_blocks, tb);
            }
            size_t sn = json_len(stashed);
            for (size_t i = 0; i < sn; i++)
                json_append(multimodal_blocks, json_copy(json_get(stashed, i)));
        }
    }

    /* Determine result_content */
    json_t *result_content = NULL;
    if (multimodal_blocks) {
        result_content = multimodal_blocks;
    } else if (content && content->type == JSON_STRING) {
        const char *ct = json_get_str(content, NULL, "");
        if (ct && *ct)
            result_content = json_string(ct);
        else
            result_content = json_string("(no output)");
    } else {
        result_content = json_string(content ? "(no output)" : "(no output)");
    }

    /* Build tool_result */
    const char *tid = json_get_str(m, "tool_call_id", "");
    char *sanitized = anthropic_sanitize_tool_id(tid);
    json_t *tool_result = json_object();
    json_set(tool_result, "type", json_string("tool_result"));
    json_set(tool_result, "tool_use_id", json_string(sanitized ? sanitized : "tool_0"));
    free(sanitized);
    json_set(tool_result, "content", result_content);

    /* Cache control passthrough */
    json_t *cc = json_obj_get(m, "cache_control");
    if (cc && cc->type == JSON_OBJECT)
        json_set(tool_result, "cache_control", json_copy(cc));

    /* Merge consecutive tool results into one user message */
    size_t rn = json_len(result);
    bool merged = false;
    if (rn > 0) {
        json_t *last = json_get(result, rn - 1);
        if (last && last->type == JSON_OBJECT) {
            const char *role = json_get_str(last, "role", "");
            json_t *last_content = json_obj_get(last, "content");
            if (strcmp(role, "user") == 0 &&
                last_content && last_content->type == JSON_ARRAY &&
                json_len(last_content) > 0) {
                json_t *first_block = json_get(last_content, 0);
                const char *ftype = json_get_str(first_block, "type", "");
                if (strcmp(ftype, "tool_result") == 0) {
                    json_append(last_content, tool_result);
                    merged = true;
                }
            }
        }
    }
    if (!merged) {
        json_t *user_msg = json_object();
        json_set(user_msg, "role", json_string("user"));
        json_t *content_arr = json_array();
        json_append(content_arr, tool_result);
        json_set(user_msg, "content", content_arr);
        json_append(result, user_msg);
    }
}

/* Port of Python anthropic_adapter.py:convert_messages_to_anthropic().
 * Convert OpenAI-format messages to Anthropic format.
 * Returns (system, anthropic_messages) via out-params.
 *
 * Iterates messages, dispatching by role:
 *   "system" → extracted to system_out (string or array)
 *   "assistant" → anthropic_convert_assistant_message()
 *   "tool" → convert_tool_message_to_result()
 *   "user" → anthropic_convert_user_message()
 * Post-processing: strip_orphaned, merge_roles, evict_screenshots.
 *
 * NOTE: manage_thinking_signatures() IS implemented below (line ~2505) and IS
 * called as post-processing below. This comment is historical — the feature
 * handles thinking block signatures for third-party/Kimi/DeepSeek endpoints. */
void convert_messages_to_anthropic(const json_t *messages,
                                              const char *base_url,
                                              const char *model,
                                              json_t **system_out,
                                              json_t **messages_out) {
    (void)base_url;
    (void)model;

    *system_out = NULL;
    *messages_out = json_array();

    if (!messages || messages->type != JSON_ARRAY)
        return;

    size_t n = json_len(messages);
    for (size_t i = 0; i < n; i++) {
        json_t *m = json_get(messages, i);
        if (!m || m->type != JSON_OBJECT) continue;

        const char *role = json_get_str(m, "role", "user");
        json_t *content = json_obj_get(m, "content");

        /* ── System message ── */
        if (strcmp(role, "system") == 0) {
            if (content && content->type == JSON_ARRAY) {
                /* Check for cache_control markers */
                bool has_cache = false;
                size_t cn = json_len(content);
                for (size_t j = 0; j < cn; j++) {
                    json_t *p = json_get(content, j);
                    if (p && p->type == JSON_OBJECT && json_obj_get(p, "cache_control")) {
                        has_cache = true;
                        break;
                    }
                }
                if (has_cache) {
                    /* Preserve content blocks with cache_control */
                    json_t *sys_arr = json_array();
                    for (size_t j = 0; j < cn; j++) {
                        json_t *p = json_get(content, j);
                        if (p && p->type == JSON_OBJECT)
                            json_append(sys_arr, json_copy(p));
                    }
                    *system_out = sys_arr;
                } else {
                    /* Join text blocks with newlines */
                    size_t total = 0;
                    for (size_t j = 0; j < cn; j++) {
                        json_t *p = json_get(content, j);
                        if (p && p->type == JSON_OBJECT) {
                            const char *t = json_get_str(p, "text", "");
                            if (*t) total += strlen(t) + 1;
                        }
                    }
                    char *combined = malloc(total + 1);
                    if (combined) {
                        combined[0] = '\0';
                        for (size_t j = 0; j < cn; j++) {
                            json_t *p = json_get(content, j);
                            if (p && p->type == JSON_OBJECT) {
                                const char *t = json_get_str(p, "text", "");
                                if (*t) {
                                    if (combined[0]) strcat(combined, "\n");
                                    strcat(combined, t);
                                }
                            }
                        }
                        *system_out = json_string(combined);
                        free(combined);
                    }
                }
            } else if (content && content->type == JSON_STRING) {
                *system_out = json_string(content->str_val ? content->str_val : "");
            }
            continue;
        }

        /* ── Assistant message ── */
        if (strcmp(role, "assistant") == 0) {
            json_t *converted = anthropic_convert_assistant_message(m);
            if (converted)
                json_append(*messages_out, converted);
            continue;
        }

        /* ── Tool message ── */
        if (strcmp(role, "tool") == 0) {
            convert_tool_message_to_result(*messages_out, m);
            continue;
        }

        /* ── User message (default) ── */
        json_t *user_msg = anthropic_convert_user_message(content ? content : json_string(""));
        if (user_msg)
            json_append(*messages_out, user_msg);
    }

    /* Post-processing */
    strip_orphaned_tool_blocks(*messages_out);
    json_t *merged = anthropic_merge_consecutive_roles(*messages_out);
    if (merged != *messages_out) {
        json_free(*messages_out);
        *messages_out = merged;
    }
    manage_thinking_signatures(*messages_out, base_url, model);
    evict_old_screenshots(*messages_out);
}

/* Port of Python anthropic_adapter.py:_manage_thinking_signatures().
 * Strip or preserve thinking blocks based on endpoint type.
 *
 * Mutates result in place. Thinking blocks are Anthropic-proprietary;
 * third-party endpoints cannot validate them. Kimi/DeepSeek endpoints
 * need unsigned blocks preserved for round-trip on replayed tool-call msgs.
 *
 * This function rebuilds content arrays and messages to work around the
 * absence of json_delete in libjson. */
void manage_thinking_signatures(json_t *result,
                                           const char *base_url,
                                           const char *model) {
    if (!result || result->type != JSON_ARRAY) return;

    bool is_third_party = anthropic_is_third_party_endpoint(base_url);
    bool preserve_unsigned = is_kimi_family_endpoint(base_url, model)
                          || anthropic_is_deepseek_endpoint(base_url);

    /* Find last assistant index (reverse scan) */
    ssize_t last_assistant_idx = -1;
    size_t n = json_len(result);
    for (ssize_t i = (ssize_t)n - 1; i >= 0; i--) {
        json_t *m = json_get(result, (size_t)i);
        if (m && m->type == JSON_OBJECT &&
            strcmp(json_get_str(m, "role", ""), "assistant") == 0) {
            last_assistant_idx = i;
            break;
        }
    }

    for (size_t idx = 0; idx < n; idx++) {
        json_t *m = json_get(result, idx);
        if (!m || m->type != JSON_OBJECT) continue;
        if (strcmp(json_get_str(m, "role", ""), "assistant") != 0) continue;

        json_t *content = json_obj_get(m, "content");
        if (!content || content->type != JSON_ARRAY) continue;

        size_t cn = json_len(content);
        json_t *new_content = json_array();

        if (preserve_unsigned) {
            /* Kimi / DeepSeek: strip signed, preserve unsigned. */
            for (size_t j = 0; j < cn; j++) {
                json_t *b = json_get(content, j);
                if (!b || b->type != JSON_OBJECT) {
                    if (b) json_append(new_content, json_copy(b));
                    continue;
                }
                const char *btype = json_get_str(b, "type", "");
                if (strcmp(btype, "thinking") != 0 &&
                    strcmp(btype, "redacted_thinking") != 0) {
                    json_append(new_content, json_copy(b));
                    continue;
                }
                /* Has signature or data → signed → strip */
                json_t *sig = json_obj_get(b, "signature");
                json_t *data = json_obj_get(b, "data");
                if (sig || data) continue;
                /* Unsigned → preserve */
                json_append(new_content, json_copy(b));
            }
        } else if (is_third_party || (ssize_t)idx != last_assistant_idx) {
            /* Third-party or non-latest: strip ALL thinking blocks. */
            for (size_t j = 0; j < cn; j++) {
                json_t *b = json_get(content, j);
                if (!b) continue;
                const char *btype = json_get_str(b, "type", "");
                if (strcmp(btype, "thinking") == 0 ||
                    strcmp(btype, "redacted_thinking") == 0)
                    continue;
                json_append(new_content, json_copy(b));
            }
        } else {
            /* Latest on direct Anthropic: keep signed, downgrade unsigned to text. */
            json_t *sig_invalidated = json_obj_get(m, "_thinking_signature_invalidated");
            bool signature_dead = sig_invalidated && sig_invalidated->type == JSON_BOOL
                                && sig_invalidated->bool_val;

            for (size_t j = 0; j < cn; j++) {
                json_t *b = json_get(content, j);
                if (!b || b->type != JSON_OBJECT) {
                    if (b) json_append(new_content, json_copy(b));
                    continue;
                }
                const char *btype = json_get_str(b, "type", "");
                if (strcmp(btype, "thinking") != 0 &&
                    strcmp(btype, "redacted_thinking") != 0) {
                    json_append(new_content, json_copy(b));
                    continue;
                }
                if (signature_dead) {
                    /* Convert thinking content to text */
                    const char *thinking_text = json_get_str(b, "thinking", "");
                    if (*thinking_text) {
                        json_t *tb = json_object();
                        json_set(tb, "type", json_string("text"));
                        json_set(tb, "text", json_string(thinking_text));
                        json_append(new_content, tb);
                    }
                    continue;
                }
                if (strcmp(btype, "redacted_thinking") == 0) {
                    /* Keep only if it has 'data' */
                    json_t *d = json_obj_get(b, "data");
                    if (d) json_append(new_content, json_copy(b));
                } else {
                    json_t *sig = json_obj_get(b, "signature");
                    if (sig) {
                        /* Has signature → keep */
                        json_append(new_content, json_copy(b));
                    } else {
                        /* No signature → downgrade to text */
                        const char *thinking_text = json_get_str(b, "thinking", "");
                        if (*thinking_text) {
                            json_t *tb = json_object();
                            json_set(tb, "type", json_string("text"));
                            json_set(tb, "text", json_string(thinking_text));
                            json_append(new_content, tb);
                        }
                    }
                }
            }
        }

        /* Empty guard */
        if (json_len(new_content) == 0) {
            json_free(new_content);
            new_content = json_array();
            json_t *tb = json_object();
            json_set(tb, "type", json_string("text"));
            json_set(tb, "text", json_string(
                preserve_unsigned ? "(empty)" :
                (is_third_party || (ssize_t)idx != last_assistant_idx) ?
                    "(thinking elided)" : "(empty)"));
            json_append(new_content, tb);
        }

        /* Strip cache_control from remaining thinking/redacted_thinking blocks
         * AND remove _thinking_signature_invalidated flag by rebuilding message
         * without that key. */
        json_t *cleaned_content = json_array();
        size_t nc = json_len(new_content);
        for (size_t j = 0; j < nc; j++) {
            json_t *b = json_get(new_content, j);
            if (!b) continue;
            if (b->type == JSON_OBJECT) {
                const char *btype = json_get_str(b, "type", "");
                if (strcmp(btype, "thinking") == 0 ||
                    strcmp(btype, "redacted_thinking") == 0) {
                    /* Rebuild block without cache_control */
                    json_t *cleaned = json_object();
                    for (size_t k = 0; k < b->c.count; k++) {
                        const char *kname = b->c.keys[k];
                        if (kname && strcmp(kname, "cache_control") != 0)
                            json_set(cleaned, kname, json_copy(b->c.items[k]));
                    }
                    json_append(cleaned_content, cleaned);
                    continue;
                }
            }
            json_append(cleaned_content, json_copy(b));
        }
        json_set(m, "content", cleaned_content);
        json_free(new_content);

        /* Rebuild message without _thinking_signature_invalidated */
        json_t *sig_flag = json_obj_get(m, "_thinking_signature_invalidated");
        if (sig_flag) {
            json_t *cleaned_msg = json_object();
            for (size_t k = 0; k < m->c.count; k++) {
                const char *kname = m->c.keys[k];
                if (kname && strcmp(kname, "_thinking_signature_invalidated") != 0)
                    json_set(cleaned_msg, kname, json_copy(m->c.items[k]));
            }
            /* Swap internal struct pointers to mutate m in place */
            json_t **old_items = m->c.items;
            char **old_keys = m->c.keys;
            size_t old_count = m->c.count;
            m->c.items = cleaned_msg->c.items;
            m->c.keys = cleaned_msg->c.keys;
            m->c.count = cleaned_msg->c.count;
            cleaned_msg->c.items = old_items;
            cleaned_msg->c.keys = old_keys;
            cleaned_msg->c.count = old_count;
            json_free(cleaned_msg);
        }
    }
}

/* Port of Python anthropic_adapter.py:build_anthropic_kwargs().
 * Build kwargs dict for the Anthropic Messages API call.
 * Converts messages, normalizes model, resolves max_tokens, handles
 * reasoning_config/thinking, tool_choice mapping, fast_mode, beta headers.
 *
 * Returns new json_t* dict (caller must json_free).
 *
 * NOTE: OAuth-specific Claude Code transforms (system prefix, tool name
 * prefixing, product name sanitization) are NOT implemented here — C
 * does not use the OAuth flow. */
/* PoP: anthropic_build_kwargs @ agent/transports/anthropic.py:build_kwargs */
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
    bool drop_context_1m_beta) {

    json_t *system = NULL;
    json_t *anthropic_messages = NULL;
    convert_messages_to_anthropic(messages, base_url, model,
                                             &system, &anthropic_messages);

    json_t *anthropic_tools = NULL;
    if (tools) {
        anthropic_tools = anthropic_convert_tools_to_anthropic(tools);
    }

    char *model_norm = normalize_model_name(model, preserve_dots);

    /* Resolve max_tokens */
    int effective_max_tokens = resolve_anthropic_messages_max_tokens(max_tokens, model);

    /* Clamp output cap to fit inside total context window */
    if (context_length > 0 && effective_max_tokens > context_length) {
        effective_max_tokens = context_length - 1;
        if (effective_max_tokens < 1) effective_max_tokens = 1;
    }

    /* Build kwargs */
    json_t *kwargs = json_object();
    json_set(kwargs, "model", json_string(model_norm ? model_norm : ""));
    json_set(kwargs, "messages", json_copy(anthropic_messages));
    json_set(kwargs, "max_tokens", json_number((double)effective_max_tokens));

    /* System prompt */
    if (system) {
        json_set(kwargs, "system", json_copy(system));
    }

    /* Tools + tool_choice */
    if (anthropic_tools && json_len(anthropic_tools) > 0) {
        json_set(kwargs, "tools", anthropic_tools);
        if (tool_choice) {
            json_t *tc = NULL;
            if (strcmp(tool_choice, "auto") == 0 || strcmp(tool_choice, "") == 0) {
                tc = json_object();
                json_set(tc, "type", json_string("auto"));
            } else if (strcmp(tool_choice, "required") == 0) {
                tc = json_object();
                json_set(tc, "type", json_string("any"));
            } else if (strcmp(tool_choice, "none") == 0) {
                /* Anthropic has no tool_choice "none" — omit tools entirely.
                 * Rebuild kwargs without the tools key since libjson has no delete. */
                json_t *stripped = json_object();
                for (size_t i = 0; i < kwargs->c.count; i++) {
                    const char *k = kwargs->c.keys[i];
                    if (k && strcmp(k, "tools") != 0)
                        json_set(stripped, k, json_copy(kwargs->c.items[i]));
                }
                json_free(kwargs);
                kwargs = stripped;
            } else {
                /* Specific tool name */
                tc = json_object();
                json_set(tc, "type", json_string("tool"));
                json_set(tc, "name", json_string(tool_choice));
            }
            if (tc) json_set(kwargs, "tool_choice", tc);
        }
    }

    /* reasoning_config → thinking parameter */
    bool is_kimi = is_kimi_family_endpoint(base_url, model);
    if (reasoning_config && reasoning_config->type == JSON_OBJECT && !is_kimi) {
        json_t *enabled = json_obj_get(reasoning_config, "enabled");
        bool reason_enabled = (!enabled || (enabled->type == JSON_BOOL && enabled->bool_val));

        if (reason_enabled) {
            char model_lower[256];
            size_t mlen = model ? strlen(model) : 0;
            if (mlen >= sizeof(model_lower)) mlen = sizeof(model_lower) - 1;
            for (size_t i = 0; i < mlen; i++)
                model_lower[i] = (model[i] >= 'A' && model[i] <= 'Z') ? model[i] + 32 : model[i];
            model_lower[mlen] = '\0';

            bool is_haiku = (strstr(model_lower, "haiku") != NULL);

            if (!is_haiku) {
                const char *effort = json_get_str(reasoning_config, "effort", "medium");

                if (supports_adaptive_thinking(model)) {
                    json_t *thinking = json_object();
                    json_set(thinking, "type", json_string("adaptive"));
                    json_set(thinking, "display", json_string("summarized"));
                    json_set(kwargs, "thinking", thinking);

                    /* Map effort */
                    const char *adaptive_effort = "medium";
                    if (strcmp(effort, "xhigh") == 0) adaptive_effort = "xhigh";
                    else if (strcmp(effort, "high") == 0) adaptive_effort = "high";
                    else if (strcmp(effort, "low") == 0) adaptive_effort = "low";

                    if (strcmp(adaptive_effort, "xhigh") == 0 && !supports_xhigh_effort(model))
                        adaptive_effort = "max";

                    json_t *output_config = json_object();
                    json_set(output_config, "type", json_string("output_config"));
                    json_set(output_config, "effort", json_string(adaptive_effort));
                    json_set(kwargs, "output_config", output_config);
                } else {
                    int budget = 8000;
                    if (strcmp(effort, "xhigh") == 0) budget = 32000;
                    else if (strcmp(effort, "high") == 0) budget = 16000;
                    else if (strcmp(effort, "low") == 0) budget = 4000;

                    json_t *thinking = json_object();
                    json_set(thinking, "type", json_string("enabled"));
                    json_set(thinking, "budget_tokens", json_number((double)budget));
                    json_set(kwargs, "thinking", thinking);
                    json_set(kwargs, "temperature", json_number(1.0));
                    int capped = effective_max_tokens > (budget + 4096) ? effective_max_tokens : (budget + 4096);
                    json_set(kwargs, "max_tokens", json_number((double)capped));
                }
            }
        }
    }

    /* Strip sampling params on models that forbid them (Opus 4.7+) */
    if (forbids_sampling_params(model)) {
        json_t *stripped = json_object();
        for (size_t i = 0; i < kwargs->c.count; i++) {
            const char *k = kwargs->c.keys[i];
            if (!k) continue;
            if (strcmp(k, "temperature") == 0 || strcmp(k, "top_p") == 0 || strcmp(k, "top_k") == 0)
                continue;
            json_set(stripped, k, json_copy(kwargs->c.items[i]));
        }
        json_free(kwargs);
        kwargs = stripped;
    }

    /* extra_body: fast_mode speed */
    if (fast_mode) {
        json_t *extra = json_object();
        json_set(extra, "speed", json_string("fast"));
        json_set(kwargs, "extra_body", extra);
    }

    /* Beta headers */
    json_t *betas = anthropic_common_betas_for_base_url(base_url, drop_context_1m_beta);
    if (betas && json_len(betas) > 0) {
        json_set(kwargs, "anthropic_beta_headers", json_copy(betas));
    }
    json_free(betas);

    free(model_norm);
    if (system) json_free(system);
    if (anthropic_messages) json_free(anthropic_messages);
    if (anthropic_tools && json_len(anthropic_tools) == 0) json_free(anthropic_tools);

    return kwargs;
}

/* Port of Python anthropic_adapter.py:_generate_pkce().
 * Generate PKCE code_verifier and S256 code_challenge.
 * Returns newly allocated strings via out-params (caller must free). */
/* Port of Python: generate_pkce */
void generate_pkce(char **verifier_out, char **challenge_out) {
    if (verifier_out) *verifier_out = NULL;
    if (challenge_out) *challenge_out = NULL;
    char *verifier = crypto_pkce_verifier();
    if (!verifier) return;
    char *challenge = crypto_pkce_challenge(verifier);
    if (!challenge) { free(verifier); return; }
    if (verifier_out) *verifier_out = verifier; else free(verifier);
    if (challenge_out) *challenge_out = challenge; else free(challenge);
}

/* Port of Python anthropic_adapter.py:refresh_anthropic_oauth_pure().
 * Refresh an Anthropic OAuth token without mutating credential files.
 * Returns new json_t* dict with access_token, refresh_token, expires_at_ms,
 * or NULL on failure. Caller must json_free.
 *
 * Tries multiple token endpoints with fallback. Uses libhttp directly. */
json_t *anthropic_refresh_oauth(const char *refresh_token, bool use_json) {
    if (!refresh_token || !*refresh_token) return NULL;

    const char *client_id = "9d1c250a-e61b-44d9-88ed-5944d1962f5e";
    const char *endpoints[] = {
        "https://platform.claude.com/v1/oauth/token",
        "https://console.anthropic.com/v1/oauth/token",
        NULL
    };

    json_t *last_result = NULL;

    for (int e = 0; endpoints[e]; e++) {
        char body[4096];
        if (use_json) {
            json_t *payload = json_object();
            json_set(payload, "grant_type", json_string("refresh_token"));
            json_set(payload, "refresh_token", json_string(refresh_token));
            json_set(payload, "client_id", json_string(client_id));
            char *serialized = json_serialize(payload);
            json_free(payload);
            if (!serialized) continue;
            snprintf(body, sizeof(body), "%s", serialized);
            free(serialized);
        } else {
            char encoded_r[1024], encoded_c[256];
            /* Simple URL encoding (no special chars expected in tokens) */
            snprintf(encoded_r, sizeof(encoded_r), "%s", refresh_token);
            snprintf(encoded_c, sizeof(encoded_c), "%s", client_id);
            snprintf(body, sizeof(body),
                     "grant_type=refresh_token&refresh_token=%s&client_id=%s",
                     encoded_r, encoded_c);
        }

        http_t *h = http_new(10); /* 10s timeout */
        if (!h) continue;

        char hdr_buf[512];
        snprintf(hdr_buf, sizeof(hdr_buf),
                 "Content-Type: %s\r\nUser-Agent: claude-cli/0.0.0 (external, cli)\r\n",
                 use_json ? "application/json" : "application/x-www-form-urlencoded");

        http_resp_t *resp = http_request(h, HTTP_POST, endpoints[e], hdr_buf, body, strlen(body));
        if (resp && resp->status == 200 && resp->body && *resp->body) {
            json_t *result = json_parse(resp->body, NULL);
            if (result && result->type == JSON_OBJECT) {
                const char *access = json_get_str(result, "access_token", "");
                if (*access) {
                    /* Add expires_at_ms */
                    time_t now = time(NULL);
                    double expires_in = json_get_num(result, "expires_in", 3600.0);
                    json_set(result, "expires_at_ms",
                             json_number((double)((long long)now * 1000LL + (long long)(expires_in * 1000.0))));
                    if (last_result) json_free(last_result);
                    last_result = json_copy(result);
                }
                json_free(result);
            }
        }
        if (resp) http_resp_free(resp);
        http_free(h);

        if (last_result) break; /* Success — stop trying endpoints */
    }

    return last_result;
}

/* Port of Python anthropic_adapter.py:_refresh_oauth_token().
 * Attempt to refresh an expired OAuth token from credential dict.
 * Reads refreshToken from creds, calls anthropic_refresh_oauth,
 * returns new access_token string or NULL. Caller must free. */
char *anthropic_refresh_oauth_token(json_t *creds) {
    if (!creds || creds->type != JSON_OBJECT) return NULL;
    const char *refresh = json_get_str(creds, "refreshToken", "");
    if (!*refresh) return NULL;

    json_t *refreshed = anthropic_refresh_oauth(refresh, false);
    if (!refreshed) return NULL;

    const char *access = json_get_str(refreshed, "access_token", "");
    char *result = (*access) ? strdup(access) : NULL;
    json_free(refreshed);
    return result;
}

/* PoP: anthropic_to_plain_data @ agent/anthropic_adapter.py:_to_plain_data */
/* Port of Python anthropic_adapter.py:_to_plain_data().
 * Recursively convert SDK objects to plain data structures.
 * In C, there are no Pydantic/SDK objects — values are already plain JSON.
 * Returns json_copy of the input for JSON types, or json_string for other types. */
json_t *anthropic_to_plain_data(const json_t *value) {
    if (!value) {
        hermes_log(LOG_DEBUG, "anthropic", "to_plain_data: NULL input");
        return json_null();
    }
    json_t *copy = json_copy((json_t *)value);
    hermes_log(LOG_DEBUG, "anthropic", "to_plain_data: copied JSON value");
    return copy;
}

/* PoP: anthropic_get_sdk @ agent/anthropic_adapter.py:_get_anthropic_sdk */
/* Port of Python anthropic_adapter.py:_get_anthropic_sdk().
 * Returns the anthropic SDK reference. In C, no Python SDK exists;
 * returns a version-identifying string constant. */
const char *anthropic_get_sdk(void) {
    static const char *sdk = "anthropic-c-provider/" HERMES_VERSION;
    hermes_log(LOG_DEBUG, "anthropic", "get_sdk = %s", sdk);
    return sdk;
}

/* Port of Python anthropic_adapter.py:_detect_claude_code_version().
 * Detect Claude Code version by running `claude --version` via popen.
 * Returns malloc'd string or NULL on failure. Caller must free. */
char *anthropic_detect_claude_code_version(void) {
    static const char *cmds[] = {"claude", "claude-code", NULL};
    for (int i = 0; cmds[i]; i++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "%s --version 2>/dev/null", cmds[i]);
        FILE *fp = popen(cmd, "r");
        if (!fp) continue;
        char buf[256];
        if (!fgets(buf, sizeof(buf), fp)) { pclose(fp); continue; }
        int rc = pclose(fp);
        if (rc != 0) continue;
        char *nl = strchr(buf, '\n');
        if (nl) *nl = '\0';
        char *space = strchr(buf, ' ');
        if (space) *space = '\0';
        if (buf[0] >= '0' && buf[0] <= '9')
            return strdup(buf);
    }
    return NULL;
}

/* Port of Python anthropic_adapter.py:_get_claude_code_version().
 * Get Claude Code version. Uses static cache after first detect.
 * Returns "0.0.0" fallback if detection fails. */
const char *anthropic_get_claude_code_version(void) {
    static char *cached = NULL;
    if (cached) return cached;
    cached = anthropic_detect_claude_code_version();
    if (!cached) cached = strdup("0.0.0");
    return cached;
}

/* Port of Python anthropic_adapter.py:_build_anthropic_client_with_bearer_hook().
 * Build httpx client with bearer auth hook for Foundry Entra ID.
 * C uses libhttp directly per-call with anthropic_build_headers() for auth;
 * returns a JSON config string describing the provider setup.
 * Caller must free the returned string. */
char *anthropic_build_client_with_bearer_hook(const char *token_provider,
                                               const char *base_url,
                                               double timeout,
                                               bool drop_context_1m_beta) {
    (void)token_provider;
    (void)base_url;
    (void)timeout;
    (void)drop_context_1m_beta;
    json_t *cfg = json_object();
    json_set(cfg, "provider", json_string("anthropic"));
    json_set(cfg, "mode", json_string("bearer_hook"));
    json_set(cfg, "note", json_string("C uses anthropic_build_headers() per-call; bearer auth handled inline"));
    char *out = json_serialize(cfg);
    json_free(cfg);
    return out;
}

/* Port of Python anthropic_adapter.py:build_anthropic_client().
 * Configure an Anthropic provider client. C uses provider_ops_t +
 * per-call http_request(). Returns JSON config string describing
 * the resolved provider setup. Caller must free. */
char *anthropic_build_client(const char *api_key,
                              const char *base_url,
                              double timeout,
                              bool drop_context_1m_beta) {
    (void)api_key;
    (void)timeout;
    json_t *cfg = json_object();
    json_set(cfg, "provider", json_string("anthropic"));
    json_set(cfg, "base_url", json_string(base_url ? base_url : ""));
    json_set(cfg, "mode", json_string("per_call_http"));
    /* Detect Azure endpoint */
    if (base_url) {
        bool is_azure = is_azure_anthropic_endpoint(base_url);
        json_set(cfg, "is_azure", json_bool(is_azure));
    }
    /* Common betas */
    json_t *betas = anthropic_common_betas_for_base_url(base_url, drop_context_1m_beta);
    json_set(cfg, "betas", betas ? betas : json_array());
    char *out = json_serialize(cfg);
    json_free(cfg);
    return out;
}

/* Port of Python anthropic_adapter.py:build_anthropic_bedrock_client().
 * Configure Bedrock provider. C uses provider_bedrock.c for AWS SigV4.
 * Returns JSON config string. Caller must free. */
char *anthropic_build_bedrock_client(const char *region) {
    json_t *cfg = json_object();
    json_set(cfg, "provider", json_string("bedrock"));
    json_set(cfg, "region", json_string(region ? region : ""));
    json_set(cfg, "mode", json_string("provider_bedrock_c"));
    json_t *betas = json_array();
    json_append(betas, json_string("context-1m-2025-08-07"));
    json_set(cfg, "betas", betas);
    char *out = json_serialize(cfg);
    json_free(cfg);
    return out;
}

/* Port of Python anthropic_adapter.py:_read_claude_code_credentials_from_keychain().
 * Read Claude Code OAuth credentials from macOS Keychain.
 * Runs `security find-generic-password` via popen on Darwin.
 * Returns json_t* dict with accessToken/refreshToken/expiresAt, or NULL. */
json_t *anthropic_read_creds_from_keychain(void) {
#ifdef __APPLE__
    FILE *fp = popen("security find-generic-password -s \"Claude Code-credentials\" -w 2>/dev/null", "r");
    if (!fp) return NULL;
    char buf[8192];
    size_t len = fread(buf, 1, sizeof(buf) - 1, fp);
    int rc = pclose(fp);
    if (rc != 0 || len == 0) return NULL;
    buf[len] = '\0';

    json_t *parsed = json_parse(buf, NULL);
    if (!parsed || parsed->type != JSON_OBJECT) {
        if (parsed) json_free(parsed);
        return NULL;
    }
    json_t *oauth = json_obj_get(parsed, "claudeAiOauth");
    if (!oauth || oauth->type != JSON_OBJECT) {
        json_free(parsed);
        return NULL;
    }
    const char *access = json_get_str(oauth, "accessToken", "");
    if (!*access) { json_free(parsed); return NULL; }

    json_t *result = json_object();
    json_set(result, "accessToken", json_string(access));
    json_set(result, "refreshToken", json_string(json_get_str(oauth, "refreshToken", "")));
    json_set(result, "expiresAt", json_copy(json_obj_get(oauth, "expiresAt")));
    json_set(result, "source", json_string("keychain"));
    json_free(parsed);
    return result;
#else
    return NULL;
#endif
}

/* PoP: anthropic_read_claude_code_creds_from_file @ agent/anthropic_adapter.py:_read_claude_code_credentials_from_file */
/* Port of Python agent/anthropic_adapter.py:_read_claude_code_credentials_from_file().
 * Read Claude Code OAuth credentials from ~/.claude/.credentials.json.
 * Mirrors the Python helper that builds the file path via Path.home() / ".claude" / ".credentials.json",
 * gracefully returns NULL when the file is missing, unparseable, has no claudeAiOauth object,
 * or has an empty accessToken.
 * Returns json_t* dict with accessToken/refreshToken/expiresAt/source, or NULL. */
json_t *anthropic_read_claude_code_creds_from_file(void) {
    const char *home = getenv("HOME");
    if (!home || !*home) return NULL;
    char cred_path[4096];
    snprintf(cred_path, sizeof(cred_path), "%s/.claude/.credentials.json", home);

    /* Existence check — Python: if not cred_path.exists(): return None */
    struct stat st;
    if (stat(cred_path, &st) != 0 || !S_ISREG(st.st_mode)) return NULL;

    /* Load + parse JSON, silently ignore malformed files / IO errors —
     * Python catches (json.JSONDecodeError, OSError, IOError) and logs at debug. */
    json_t *data = json_parse_file(cred_path, NULL);
    if (!data || data->type != JSON_OBJECT) {
        if (data) json_free(data);
        return NULL;
    }

    json_t *oauth = json_obj_get(data, "claudeAiOauth");
    if (!oauth || oauth->type != JSON_OBJECT) {
        json_free(data);
        return NULL;
    }
    const char *access = json_get_str(oauth, "accessToken", "");
    if (!access || !*access) {
        json_free(data);
        return NULL;
    }

    json_t *result = json_object();
    json_set(result, "accessToken", json_string(access));
    json_set(result, "refreshToken", json_string(json_get_str(oauth, "refreshToken", "")));
    json_t *expires = json_obj_get(oauth, "expiresAt");
    if (expires) {
        json_set(result, "expiresAt", json_copy(expires));
    } else {
        json_set(result, "expiresAt", json_int(0));
    }
    json_set(result, "source", json_string("claude_code_credentials_file"));
    json_free(data);
    return result;
}

/* PoP: anthropic_read_claude_code_creds @ agent/anthropic_adapter.py:read_claude_code_credentials */
/* Port of Python anthropic_adapter.py:read_claude_code_credentials().
 * Read refreshable Claude Code credentials, preferring macOS Keychain on Darwin
 * and falling back to ~/.claude/.credentials.json elsewhere.
 * Returns json_t* dict with accessToken/refreshToken/expiresAt/source, or NULL. */
json_t *anthropic_read_claude_code_creds(void) {
    /* Try macOS Keychain first */
    json_t *kc = anthropic_read_creds_from_keychain();
    if (kc) return kc;

    /* Fall back to ~/.claude/.credentials.json via the dedicated helper */
    return anthropic_read_claude_code_creds_from_file();
}

/* Port of Python anthropic_adapter.py:_write_claude_code_credentials().
 * Write refreshed credentials to ~/.claude/.credentials.json.
 * Uses atomic write: temp file → fsync → rename.
 * Preserves existing claudeAiOauth fields and scopes. */
void anthropic_write_claude_code_creds(const char *access_token,
                                        const char *refresh_token,
                                        long long expires_at_ms,
                                        json_t *scopes) {
    const char *home = getenv("HOME");
    if (!home || !access_token) return;

    char dir[4096], path[4096], tmp_path[4160];
    snprintf(dir, sizeof(dir), "%s/.claude", home);
    snprintf(path, sizeof(path), "%s/.claude/.credentials.json", home);
    snprintf(tmp_path, sizeof(tmp_path), "%s/.claude/.credentials.tmp.XXXXXX", home);

    /* Read existing data to preserve other fields */
    json_t *existing = json_parse_file(path, NULL);
    if (!existing) existing = json_object();

    /* Build oauth_data */
    json_t *oauth_data = json_object();
    json_set(oauth_data, "accessToken", json_string(access_token));
    json_set(oauth_data, "refreshToken", json_string(refresh_token ? refresh_token : ""));
    json_set(oauth_data, "expiresAt", json_number((double)expires_at_ms));

    if (scopes && scopes->type == JSON_ARRAY) {
        json_set(oauth_data, "scopes", json_copy(scopes));
    } else {
        /* Preserve previously-stored scopes */
        json_t *old_oauth = json_obj_get(existing, "claudeAiOauth");
        if (old_oauth && old_oauth->type == JSON_OBJECT) {
            json_t *old_scopes = json_obj_get(old_oauth, "scopes");
            if (old_scopes) json_set(oauth_data, "scopes", json_copy(old_scopes));
        }
    }

    json_set(existing, "claudeAiOauth", oauth_data);

    /* Ensure directory exists */
    char mkcmd[4160];
    snprintf(mkcmd, sizeof(mkcmd), "mkdir -p '%s'", dir);
    (void)system(mkcmd);

    /* Atomic write via mkstemp */
    int fd = mkstemp(tmp_path);
    if (fd < 0) { json_free(existing); return; }
    /* Set 0o600 permissions */
    fchmod(fd, S_IRUSR | S_IWUSR);

    char *serialized = json_serialize_pretty(existing, 2);
    if (!serialized) { close(fd); unlink(tmp_path); json_free(existing); return; }

    ssize_t written = write(fd, serialized, strlen(serialized));
    free(serialized);
    if (written < 0) { close(fd); unlink(tmp_path); json_free(existing); return; }
    fsync(fd);
    close(fd);

    rename(tmp_path, path);
    json_free(existing);
}

/* Port of Python anthropic_adapter.py:_resolve_claude_code_token_from_credentials().
 * Resolve token from Claude Code credential files, refreshing if needed.
 * If creds is NULL, reads from credential sources first.
 * Returns malloc'd token string or NULL. Caller must free. */
char *anthropic_resolve_creds_token(json_t *creds) {
    bool own_creds = false;
    if (!creds) {
        creds = anthropic_read_claude_code_creds();
        own_creds = true;
    }
    if (!creds) return NULL;

    char *result = NULL;
    if (is_claude_code_token_valid(creds)) {
        const char *token = json_get_str(creds, "accessToken", "");
        if (*token) result = strdup(token);
    } else {
        /* Expired — attempt refresh */
        result = anthropic_refresh_oauth_token(creds);
    }

    if (own_creds) json_free(creds);
    return result;
}

/* Port of Python anthropic_adapter.py:_prefer_refreshable_claude_code_token().
 * Prefer Claude Code creds when a persisted env OAuth token would shadow refresh.
 * If env_token is OAuth and creds has a refreshToken, resolve from creds.
 * Returns malloc'd token string or NULL. Caller must free. */
char *anthropic_prefer_refreshable_token(const char *env_token, json_t *creds) {
    if (!env_token || !*env_token) return NULL;
    if (!is_oauth_token(env_token)) return NULL;
    if (!creds || creds->type != JSON_OBJECT) return NULL;

    json_t *rt = json_obj_get(creds, "refreshToken");
    if (!rt || rt->type != JSON_STRING) return NULL;
    const char *refresh = rt->str_val;
    if (!refresh || !*refresh) return NULL;

    char *resolved = anthropic_resolve_creds_token(creds);
    if (resolved && strcmp(resolved, env_token) != 0)
        return resolved;
    free(resolved);
    return NULL;
}

/* Port of Python anthropic_adapter.py:run_oauth_setup_token().
 * Run 'claude setup-token' interactively and return the resulting token.
 * Checks credential files and env vars after subprocess completes.
 * Returns malloc'd token string or NULL. Caller must free. */
char *anthropic_run_oauth_setup(void) {
    /* Check if claude binary exists */
    FILE *fp = popen("which claude 2>/dev/null", "r");
    if (!fp) return NULL;
    char buf[1024];
    bool found = fgets(buf, sizeof(buf), fp) != NULL;
    pclose(fp);
    if (!found) return NULL;

    /* Run claude setup-token (interactive) */
    int rc = system("claude setup-token 2>&1");
    (void)rc;

    /* Check credential files after subprocess */
    json_t *creds = anthropic_read_claude_code_creds();
    if (creds) {
        if (is_claude_code_token_valid(creds)) {
            const char *token = json_get_str(creds, "accessToken", "");
            if (*token) { json_free(creds); return strdup(token); }
        }
        json_free(creds);
    }

    /* Check env vars */
    const char *env_vars[] = {"CLAUDE_CODE_OAUTH_TOKEN", "ANTHROPIC_TOKEN", NULL};
    for (int i = 0; env_vars[i]; i++) {
        const char *val = getenv(env_vars[i]);
        if (val && *val) return strdup(val);
    }

    return NULL;
}

/* Port of Python anthropic_adapter.py:run_hermes_oauth_login_pure().
 * Run Hermes-native OAuth PKCE flow with browser interaction.
 * Generates PKCE, prints auth URL, opens browser, reads code from stdin,
 * exchanges authorization code for token via HTTP POST.
 * Returns json_t* dict with access_token/refresh_token/expires_at_ms, or NULL.
 * Caller must json_free. */
json_t *anthropic_run_oauth_login(void) {
    const char *client_id = "9d1c250a-e61b-44d9-88ed-5944d1962f5e";
    const char *token_url = "https://console.anthropic.com/v1/oauth/token";
    const char *redirect_uri = "https://console.anthropic.com/oauth/code/callback";
    const char *scopes = "org:create_api_key user:profile user:inference";

    /* Generate PKCE */
    char *verifier = NULL, *challenge = NULL;
    generate_pkce(&verifier, &challenge);
    if (!verifier || !challenge) {
        free(verifier); free(challenge);
        return NULL;
    }

    /* Generate a random state for CSRF protection */
    char oauth_state[65];
    {
        unsigned char rnd[32];
        FILE *urandom = fopen("/dev/urandom", "r");
        if (!urandom || fread(rnd, 1, 32, urandom) != 32) {
            if (urandom) fclose(urandom);
            free(verifier); free(challenge);
            return NULL;
        }
        fclose(urandom);
        for (int i = 0; i < 32; i++)
            snprintf(oauth_state + i * 2, 3, "%02x", rnd[i]);
        oauth_state[64] = '\0';
    }

    /* Build auth URL */
    char auth_url[4096];
    snprintf(auth_url, sizeof(auth_url),
        "https://claude.ai/oauth/authorize"
        "?code=true&client_id=%s&response_type=code"
        "&redirect_uri=%s&scope=%s"
        "&code_challenge=%s&code_challenge_method=S256"
        "&state=%s",
        client_id, redirect_uri, scopes, challenge, oauth_state);

    printf("\n");
    printf("Authorize Hermes with your Claude Pro/Max subscription.\n");
    printf("\n");
    printf("╭─ Claude Pro/Max Authorization ────────────────────╮\n");
    printf("│                                                   │\n");
    printf("│  Open this link in your browser:                  │\n");
    printf("╰───────────────────────────────────────────────────╯\n");
    printf("\n");
    printf("  %s\n", auth_url);
    printf("\n");

    /* Try opening browser */
    {
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "xdg-open '%s' 2>/dev/null || open '%s' 2>/dev/null", auth_url, auth_url);
        int bc = system(cmd);
        if (bc == 0)
            printf("  (Browser opened automatically)\n");
    }

    printf("\n");
    printf("After authorizing, you'll see a code. Paste it below.\n");
    printf("\n");
    printf("Authorization code: ");

    char auth_code[4096];
    if (!fgets(auth_code, sizeof(auth_code), stdin)) {
        free(verifier); free(challenge);
        return NULL;
    }
    /* Strip trailing newline */
    size_t clen = strlen(auth_code);
    while (clen > 0 && (auth_code[clen - 1] == '\n' || auth_code[clen - 1] == '\r'))
        auth_code[--clen] = '\0';

    if (!*auth_code) {
        printf("No code entered.\n");
        free(verifier); free(challenge);
        return NULL;
    }

    /* Parse code#state format */
    char *hashed = strchr(auth_code, '#');
    char *received_state = NULL;
    if (hashed) {
        *hashed = '\0';
        received_state = hashed + 1;
    }

    /* Validate state (CSRF protection) */
    if (received_state && strcmp(received_state, oauth_state) != 0) {
        printf("OAuth state mismatch — possible CSRF, aborting.\n");
        free(verifier); free(challenge);
        return NULL;
    }

    /* Exchange authorization code for token */
    json_t *payload = json_object();
    json_set(payload, "grant_type", json_string("authorization_code"));
    json_set(payload, "client_id", json_string(client_id));
    json_set(payload, "code", json_string(auth_code));
    json_set(payload, "state", json_string(received_state ? received_state : ""));
    json_set(payload, "redirect_uri", json_string(redirect_uri));
    json_set(payload, "code_verifier", json_string(verifier));
    free(verifier); free(challenge);

    char *payload_str = json_serialize(payload);
    json_free(payload);
    if (!payload_str) return NULL;

    http_t *h = http_new(15);
    json_t *result = NULL;
    if (h) {
        char hdrs[512];
        snprintf(hdrs, sizeof(hdrs),
            "Content-Type: application/json\r\nUser-Agent: claude-cli/%s (external, cli)\r\n",
            anthropic_get_claude_code_version());

        http_resp_t *resp = http_request(h, HTTP_POST, token_url, hdrs,
                                          payload_str, strlen(payload_str));
        if (resp && resp->status == 200 && resp->body && *resp->body) {
            json_t *parsed = json_parse(resp->body, NULL);
            if (parsed && parsed->type == JSON_OBJECT) {
                const char *access = json_get_str(parsed, "access_token", "");
                if (*access) {
                    result = json_object();
                    json_set(result, "access_token", json_string(access));
                    json_set(result, "refresh_token",
                             json_string(json_get_str(parsed, "refresh_token", "")));
                    double expires_in = json_get_num(parsed, "expires_in", 3600.0);
                    time_t now = time(NULL);
                    json_set(result, "expires_at_ms",
                             json_number((double)((long long)now * 1000LL +
                                                  (long long)(expires_in * 1000.0))));
                }
                json_free(parsed);
            }
        }
        if (resp) http_resp_free(resp);
        http_free(h);
    }
    free(payload_str);

    if (!result)
        printf("Token exchange failed.\n");

    return result;
}

/* ================================================================
 *  Provider Operations Table
 * ================================================================ */

const provider_ops_t PROVIDER_OPS_ANTHROPIC = {
    .build_url = anthropic_build_url,
    .build_headers = anthropic_build_headers,
    .build_request_body = anthropic_build_request_body,
    .parse_response = anthropic_parse_response,
    .parse_stream_chunk = anthropic_parse_stream_chunk,
    .free_response = anthropic_free_response,
    .name = "anthropic"
};
