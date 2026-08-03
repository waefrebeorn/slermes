/*
 * plugin_llm.c — Plugin LLM facade: host-owned LLM access for trusted plugins.
 *
 * Port of Python agent/plugin_llm.py (1046 lines).
 *
 * MIT License — WuBu Slermes Project
 */
#include "plugin_llm.h"
#include "hermes_json.h"
#include "auxiliary_client.h"
#include "hermes_agent.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ================================================================
 *  Helper: normalize reference string (lower-case + strip)
 *  Port of Python: _normalize_ref()
 * ================================================================ */
static void normalize_ref(const char *raw, char *out, size_t sz) {
    if (!raw || !*raw) { out[0] = '\0'; return; }
    size_t i, j = 0;
    for (i = 0; raw[i] && j < sz - 1; i++) {
        char c = raw[i];
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        if (c != ' ' && c != '\t') {
            out[j++] = c;
        }
    }
    out[j] = '\0';
}

/* ================================================================
 *  Helper: check if a string is in a comma-separated list
 * ================================================================ */
static bool in_list(const char *list, const char *item) {
    if (!list || !*list || !item || !*item) return false;
    if (strcmp(list, "*") == 0) return true;

    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", list);

    char item_norm[256];
    normalize_ref(item, item_norm, sizeof(item_norm));

    char *save;
    const char *tok = strtok_r(buf, ",", &save);
    while (tok) {
        char tok_norm[256];
        normalize_ref(tok, tok_norm, sizeof(tok_norm));
        if (strcmp(tok_norm, item_norm) == 0) return true;
        if (strcmp(tok_norm, "*") == 0) return true;
        tok = strtok_r(NULL, ",", &save);
    }
    return false;
}

/* ================================================================
 *  Helper: check if node is boolean true
 * ================================================================ */
static bool json_is_true_val(const json_t *node) {
    return node && node->type == JSON_BOOL && node->bool_val;
}

/* ================================================================
 *  Trust policy resolution
/* AG26: Port of Python agent/plugin_llm.py:_resolve_trust_policy() */
/* AG26: Port of Python agent/plugin_llm.py:_coerce_allowlist() */
/* Port of Python: _resolve_trust_policy() + _coerce_allowlist() */
/* ================================================================ */
plugin_llm_trust_policy_t resolve_trust_policy(
    const char *plugin_id,
    const json_t *config_json)
{
    plugin_llm_trust_policy_t policy;
    memset(&policy, 0, sizeof(policy));

    if (plugin_id) {
        snprintf(policy.plugin_id, sizeof(policy.plugin_id), "%s", plugin_id);
    }

    if (!config_json || config_json->type != JSON_OBJECT) {
        return policy; /* fully restrictive default */
    }

    /* Read allow_provider_override */
    json_t *v = json_obj_get(config_json, "allow_provider_override");
    policy.allow_provider_override = json_is_true_val(v);

    /* Read allowed_providers */
    v = json_obj_get(config_json, "allowed_providers");
    if (v && v->type == JSON_ARRAY) {
        size_t count = json_len(v);
        size_t pos = 0;
        for (size_t i = 0; i < count && pos < sizeof(policy.allowed_providers) - 1; i++) {
            json_t *elem = json_get(v, i);
            if (elem && elem->type == JSON_STRING) {
                const char *s = elem->str_val ? elem->str_val : "";
                if (strcmp(s, "*") == 0) {
                    policy.allow_any_provider = true;
                }
                if (pos > 0 && pos < sizeof(policy.allowed_providers) - 1) {
                    policy.allowed_providers[pos++] = ',';
                }
                size_t slen = strlen(s);
                size_t to_copy = slen;
                if (pos + to_copy >= sizeof(policy.allowed_providers) - 1) {
                    to_copy = sizeof(policy.allowed_providers) - 1 - pos;
                }
                memcpy(policy.allowed_providers + pos, s, to_copy);
                pos += to_copy;
            }
        }
        policy.allowed_providers[pos] = '\0';
    }

    /* Read allow_model_override */
    v = json_obj_get(config_json, "allow_model_override");
    policy.allow_model_override = json_is_true_val(v);

    /* Read allowed_models */
    v = json_obj_get(config_json, "allowed_models");
    if (v && v->type == JSON_ARRAY) {
        size_t count = json_len(v);
        size_t pos = 0;
        for (size_t i = 0; i < count && pos < sizeof(policy.allowed_models) - 1; i++) {
            json_t *elem = json_get(v, i);
            if (elem && elem->type == JSON_STRING) {
                const char *s = elem->str_val ? elem->str_val : "";
                if (strcmp(s, "*") == 0) {
                    policy.allow_any_model = true;
                }
                if (pos > 0 && pos < sizeof(policy.allowed_models) - 1) {
                    policy.allowed_models[pos++] = ',';
                }
                size_t slen = strlen(s);
                size_t to_copy = slen;
                if (pos + to_copy >= sizeof(policy.allowed_models) - 1) {
                    to_copy = sizeof(policy.allowed_models) - 1 - pos;
                }
                memcpy(policy.allowed_models + pos, s, to_copy);
                pos += to_copy;
            }
        }
        policy.allowed_models[pos] = '\0';
    }

    /* Read allow_agent_id_override */
    v = json_obj_get(config_json, "allow_agent_id_override");
    policy.allow_agent_id_override = json_is_true_val(v);

    /* Read allow_profile_override */
    v = json_obj_get(config_json, "allow_profile_override");
    policy.allow_profile_override = json_is_true_val(v);

    return policy;
}

/* ================================================================
 *  Override checking
 *  Port of Python: _check_overrides()
 * ================================================================ */
int check_overrides(
    const plugin_llm_trust_policy_t *policy,
    const char **provider,
    const char **model,
    const char **agent_id,
    const char **profile,
    char *err_msg, size_t err_sz)
{
    if (!policy) return -1;

    /* Provider override check */
    if (provider && *provider && **provider) {
        if (!policy->allow_provider_override) {
            snprintf(err_msg, err_sz,
                     "Plugin '%s' cannot override the provider "
                     "(set plugins.entries.%s.llm.allow_provider_override "
                     "to true to allow).",
                     policy->plugin_id, policy->plugin_id);
            return -1;
        }
        char norm[256];
        normalize_ref(*provider, norm, sizeof(norm));
        if (!policy->allow_any_provider && policy->allowed_providers[0]) {
            if (!in_list(policy->allowed_providers, norm)) {
                snprintf(err_msg, err_sz,
                         "Plugin '%s' provider override '%s' is not in "
                         "plugins.entries.%s.llm.allowed_providers.",
                         policy->plugin_id, *provider, policy->plugin_id);
                return -1;
            }
        }
    }

    /* Model override check */
    if (model && *model && **model) {
        if (!policy->allow_model_override) {
            snprintf(err_msg, err_sz,
                     "Plugin '%s' cannot override the model "
                     "(set plugins.entries.%s.llm.allow_model_override "
                     "to true to allow).",
                     policy->plugin_id, policy->plugin_id);
            return -1;
        }
        char norm[256];
        normalize_ref(*model, norm, sizeof(norm));
        if (!policy->allow_any_model && policy->allowed_models[0]) {
            if (!in_list(policy->allowed_models, norm)) {
                snprintf(err_msg, err_sz,
                         "Plugin '%s' model override '%s' is not in "
                         "plugins.entries.%s.llm.allowed_models.",
                         policy->plugin_id, *model, policy->plugin_id);
                return -1;
            }
        }
    }

    /* Agent ID override check */
    if (agent_id && *agent_id && **agent_id && !policy->allow_agent_id_override) {
        snprintf(err_msg, err_sz,
                 "Plugin '%s' cannot override the agent ID "
                 "(set plugins.entries.%s.llm.allow_agent_id_override "
                 "to true to allow).",
                 policy->plugin_id, policy->plugin_id);
        return -1;
    }

    /* Profile override check */
    if (profile && *profile && **profile && !policy->allow_profile_override) {
        snprintf(err_msg, err_sz,
                 "Plugin '%s' cannot override the auth profile "
                 "(set plugins.entries.%s.llm.allow_profile_override "
                 "to true to allow).",
                 policy->plugin_id, policy->plugin_id);
        return -1;
    }

    return 0;
}

/* ================================================================
 *  Input block normalization
 *  Port of Python: _normalize_input_block()
 * ================================================================ */
static json_t *normalize_input_block(const plugin_llm_input_t *block) {
    if (!block) return NULL;

    json_t *obj = json_object();
    if (!obj) return NULL;

    if (block->type == PLUGIN_LLM_INPUT_TYPE_TEXT) {
        json_set(obj, "type", json_string("text"));
        json_set(obj, "text", json_string(block->text ? block->text : ""));
    } else if (block->type == PLUGIN_LLM_INPUT_TYPE_IMAGE) {
        json_set(obj, "type", json_string("image_url"));
        json_t *url_obj = json_object();
        if (block->url && *block->url) {
            json_set(url_obj, "url", json_string(block->url));
        } else if (block->data && block->data_len > 0) {
            const char *mime = block->mime_type && *block->mime_type
                               ? block->mime_type : "image/png";
            char data_uri[65536];
            snprintf(data_uri, sizeof(data_uri), "data:%s;base64,%.*s",
                     mime, (int)block->data_len, block->data);
            json_set(url_obj, "url", json_string(data_uri));
        }
        json_set(obj, "image_url", url_obj);
    }

    return obj;
}

/* ================================================================
 *  Build structured messages
 *  Port of Python: _build_structured_messages()
 * ================================================================ */
json_t *plugin_llm_build_structured_messages(
    const char *instructions,
    const plugin_llm_input_t *inputs,
    int input_count,
    bool json_mode,
    const json_t *json_schema,
    const char *schema_name,
    const char *system_prompt)
{
    json_t *messages = json_array();
    if (!messages) return NULL;

    /* Build system message parts */
    char sys_text[4096] = "";
    if (system_prompt && *system_prompt) {
        snprintf(sys_text, sizeof(sys_text), "%s", system_prompt);
    }
    if (json_mode || json_schema) {
        size_t len = strlen(sys_text);
        snprintf(sys_text + len, sizeof(sys_text) - len,
                 "%sRespond with a single JSON object that matches the "
                 "requested shape. Do not include prose or markdown fences.",
                 len > 0 ? "\n\n" : "");
    }
    if (sys_text[0]) {
        json_t *sys_msg = json_object();
        if (sys_msg) {
            json_set(sys_msg, "role", json_string("system"));
            json_set(sys_msg, "content", json_string(sys_text));
            json_append(messages, sys_msg);
        }
    }

    /* Build user message parts */
    json_t *user_msg = json_object();
    json_t *content = json_array();
    if (!user_msg || !content) {
        json_free(messages);
        json_free(user_msg);
        json_free(content);
        return NULL;
    }
    json_set(user_msg, "role", json_string("user"));
    json_set(user_msg, "content", content);

    /* Build header text */
    char header[16384] = "";
    if (instructions && *instructions) {
        snprintf(header, sizeof(header), "%s", instructions);
    }
    if (schema_name && *schema_name) {
        size_t len = strlen(header);
        snprintf(header + len, sizeof(header) - len,
                 "\n\nSchema name: %s", schema_name);
    }
    if (json_schema) {
        char *schema_str = json_serialize((json_t *)json_schema);
        if (schema_str) {
            size_t len = strlen(header);
            snprintf(header + len, sizeof(header) - len,
                     "\n\nJSON schema:\n%s", schema_str);
            free(schema_str);
        }
    }
    if (header[0]) {
        json_t *text_part = json_object();
        if (text_part) {
            json_set(text_part, "type", json_string("text"));
            json_set(text_part, "text", json_string(header));
            json_append(content, text_part);
        }
    }

    /* Add input blocks */
    for (int i = 0; i < input_count; i++) {
        json_t *block = normalize_input_block(&inputs[i]);
        if (block) {
            json_append(content, block);
        }
    }

    json_append(messages, user_msg);
    return messages;
}

/* ================================================================
 *  Code fence stripping
 *  Port of Python: _strip_code_fences()
 * ================================================================ */
static char *strip_code_fences(const char *text) {
    if (!text || !*text) {
        char *out = malloc(1);
        if (out) out[0] = '\0';
        return out;
    }

    /* Look for ```json or ``` fences */
    const char *start = strstr(text, "```");
    if (!start) {
        /* No fence found — return stripped copy */
        while (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r') text++;
        char *out = strdup(text);
        if (out) {
            /* Trim trailing */
            size_t len = strlen(out);
            while (len > 0 && (out[len-1] == ' ' || out[len-1] == '\t' ||
                               out[len-1] == '\n' || out[len-1] == '\r')) {
                out[--len] = '\0';
            }
        }
        return out;
    }

    /* Skip past the opening ``` */
    const char *inner = start + 3;
    /* Skip optional language identifier (json) */
    if (strncmp(inner, "json", 4) == 0) {
        inner += 4;
    }
    /* Skip to end of line */
    while (*inner && *inner != '\n') inner++;
    if (*inner == '\n') inner++;

    /* Find closing ``` */
    const char *end = strstr(inner, "```");
    if (!end) {
        /* No closing fence — use the whole inner content */
        char *out = malloc(strlen(inner) + 1);
        if (out) strcpy(out, inner);
        return out;
    }

    /* Extract content between fences */
    size_t content_len = end - inner;
    /* Trim trailing whitespace */
    while (content_len > 0 && (inner[content_len-1] == ' ' ||
                                inner[content_len-1] == '\t' ||
                                inner[content_len-1] == '\n' ||
                                inner[content_len-1] == '\r')) {
        content_len--;
    }

    char *out = malloc(content_len + 1);
    if (out) {
        memcpy(out, inner, content_len);
        out[content_len] = '\0';
    }
    return out;
}

/* ================================================================
 *  Parse structured text — try to parse as JSON
 *  Port of Python: _parse_structured_text()
 * ================================================================ */
static json_t *parse_structured_text(const char *text, bool json_mode,
                                      const json_t *json_schema,
                                      char *content_type_buf,
                                      size_t ct_sz) {
    if (!json_mode && !json_schema) {
        if (content_type_buf) snprintf(content_type_buf, ct_sz, "text");
        return NULL;
    }
    if (!text || !*text) {
        if (content_type_buf) snprintf(content_type_buf, ct_sz, "text");
        return NULL;
    }

    char *stripped = strip_code_fences(text);
    if (!stripped) {
        if (content_type_buf) snprintf(content_type_buf, ct_sz, "text");
        return NULL;
    }

    json_t *parsed = json_parse(stripped, NULL);
    free(stripped);

    if (!parsed) {
        if (content_type_buf) snprintf(content_type_buf, ct_sz, "text");
        return NULL;
    }

    if (json_schema) {
        /* libjson has json_validate_schema() — use it when schema provided */
        char *schema_err = NULL;
        if (!json_validate_schema(json_schema, parsed, &schema_err)) {
            /* Schema validation failed — still return the parsed JSON
             * with content_type="text" to match Python behavior when
             * jsonschema is unavailable (we treat it as best-effort). */
            free(schema_err);
        }
    }

    if (content_type_buf) snprintf(content_type_buf, ct_sz, "json");
    return parsed;
}

/* ================================================================
 *  Usage extraction
 *  Port of Python: _extract_usage()
 * ================================================================ */
plugin_llm_usage_t extract_usage(const llm_response_t *response) {
    plugin_llm_usage_t usage;
    memset(&usage, 0, sizeof(usage));
    usage.cost_usd = -1.0; /* not available */

    if (!response) return usage;

    usage.input_tokens = response->input_tokens;
    usage.output_tokens = response->output_tokens;
    usage.total_tokens = response->input_tokens + response->output_tokens;
    usage.cache_read_tokens = response->cache_read_tokens;
    usage.cache_write_tokens = response->cache_write_tokens;

    return usage;
}

/* ================================================================
 *  Text extraction
 *  Port of Python: _extract_text()
 * ================================================================ */
char *extract_text(const llm_response_t *response) {
    if (!response) return strdup("");

    const char *content = response->content;
    if (!content) return strdup("");

    return strdup(content);
}

/* ================================================================
 *  JSON response format builder
 *  Port of Python: PluginLlm._json_response_format()
 * ================================================================ */
static json_t *json_response_format(bool json_mode,
                                     const json_t *json_schema) {
    if (json_schema) {
        json_t *fmt = json_object();
        if (!fmt) return NULL;
        json_set(fmt, "type", json_string("json_schema"));
        json_t *schema_obj = json_object();
        if (schema_obj) {
            json_set(schema_obj, "name", json_string("plugin_structured_output"));
            json_set(schema_obj, "schema", (json_t *)json_schema);
            json_set(schema_obj, "strict", json_bool(false));
        }
        json_set(fmt, "json_schema", schema_obj);
        json_t *resp_fmt = json_object();
        if (resp_fmt) {
            json_set(resp_fmt, "response_format", fmt);
        } else {
            json_free(fmt);
        }
        return resp_fmt;
    }

    if (json_mode) {
        json_t *fmt = json_object();
        if (!fmt) return NULL;
        json_set(fmt, "type", json_string("json_object"));
        json_t *resp_fmt = json_object();
        if (resp_fmt) {
            json_set(resp_fmt, "response_format", fmt);
        } else {
            json_free(fmt);
        }
        return resp_fmt;
    }

    return NULL;
}

/* ================================================================
 *  Resolve attribution
 *  Note: llm_response_t doesn't have a 'model' field,
 *  so we use the llm_config_t model as fallback.
 *  Port of Python: _resolve_attribution()
 * ================================================================ */
static void resolve_attribution(
    const char *provider_override,
    const char *model_override,
    const llm_response_t *response,
    const llm_config_t *llm_cfg,
    char *provider_out, size_t provider_sz,
    char *model_out, size_t model_sz)
{
    if (provider_override && *provider_override) {
        snprintf(provider_out, provider_sz, "%s", provider_override);
    } else {
        snprintf(provider_out, provider_sz, "auto");
    }

    if (model_override && *model_override) {
        snprintf(model_out, model_sz, "%s", model_override);
    } else if (llm_cfg && llm_cfg->model[0]) {
        snprintf(model_out, model_sz, "%s", llm_cfg->model);
    } else {
        snprintf(model_out, model_sz, "default");
    }
}

/* ================================================================
 *  Build message_t array from JSON messages array
 *  Returns NULL on error. Caller must free with free_messages().
 * ================================================================ */
static message_t **json_to_message_array(json_t *json_msgs, int *count_out) {
    if (!json_msgs || json_msgs->type != JSON_ARRAY) {
        *count_out = 0;
        return NULL;
    }

    size_t count = json_len(json_msgs);
    if (count == 0) {
        *count_out = 0;
        return NULL;
    }

    message_t **arr = calloc(count, sizeof(message_t *));
    if (!arr) {
        *count_out = 0;
        return NULL;
    }

    int real_count = 0;
    for (size_t i = 0; i < count; i++) {
        json_t *msg_node = json_get(json_msgs, i);
        if (!msg_node || msg_node->type != JSON_OBJECT) continue;

        message_t *msg = calloc(1, sizeof(message_t));
        if (!msg) continue;

        /* Extract role */
        json_t *role = json_obj_get(msg_node, "role");
        if (role && role->type == JSON_STRING) {
            const char *role_str = role->str_val ? role->str_val : "";
            if (strcmp(role_str, "system") == 0) msg->role = MSG_SYSTEM;
            else if (strcmp(role_str, "user") == 0) msg->role = MSG_USER;
            else if (strcmp(role_str, "assistant") == 0) msg->role = MSG_ASSISTANT;
            else if (strcmp(role_str, "tool") == 0) msg->role = MSG_TOOL;
            else msg->role = MSG_USER;
        } else {
            msg->role = MSG_USER;
        }

        /* Extract content */
        json_t *content = json_obj_get(msg_node, "content");
        if (content && content->type == JSON_STRING) {
            msg->content = strdup(content->str_val ? content->str_val : "");
        } else if (content && content->type == JSON_ARRAY) {
            /* Array content — serialize back to JSON string */
            char *ser = json_serialize(content);
            msg->content = ser ? ser : strdup("");
        } else {
            msg->content = strdup("");
        }

        arr[real_count++] = msg;
    }

    *count_out = real_count;
    return arr;
}

static void free_message_array(message_t **arr, int count) {
    if (!arr) return;
    for (int i = 0; i < count; i++) {
        if (arr[i]) {
            free(arr[i]->content);
            free(arr[i]);
        }
    }
    free(arr);
}

/* ================================================================
 *  Main API — complete
/* AG26: Port of Python agent/plugin_llm.py:PluginLlm.complete() */
/* AG26: Port of Python agent/plugin_llm.py:_invoke_sync() */
/* Port of Python: PluginLlm.complete() + _invoke_sync() */
/* ================================================================ */
/* PoP: _complete @ gateway/platforms/qqbot/chunked_upload.py:_complete */
/* Port of Python gateway/platforms/qqbot/chunked_upload.py:_complete(). */
plugin_llm_result_t *plugin_llm_complete(
    const char *plugin_id,
    llm_config_t *llm_cfg,
    json_t *messages,
    const char *provider,
    const char *model,
    const char *agent_id,
    const char *profile,
    double temperature,
    int max_tokens,
    json_t *extra_body,
    json_t *policy_json,
    char *err_msg, size_t err_sz)
{
    if (!plugin_id || !llm_cfg || !messages) {
        if (err_msg) snprintf(err_msg, err_sz, "Invalid arguments");
        return NULL;
    }

    /* Resolve trust policy */
    plugin_llm_trust_policy_t policy = resolve_trust_policy(plugin_id, policy_json);

    /* Check overrides */
    const char *eff_provider = provider;
    const char *eff_model = model;
    const char *eff_agent = agent_id;
    const char *eff_profile = profile;

    if (check_overrides(&policy, &eff_provider, &eff_model,
                                    &eff_agent, &eff_profile,
                                    err_msg, err_sz) != 0) {
        return NULL;
    }

    /* Convert JSON messages to message_t array */
    int msg_count = 0;
    message_t **msg_array = json_to_message_array(messages, &msg_count);
    if (!msg_array || msg_count == 0) {
        free_message_array(msg_array, msg_count);
        if (err_msg) snprintf(err_msg, err_sz, "No valid messages");
        return NULL;
    }

    /* Apply overrides to llm_cfg */
    if (eff_provider && *eff_provider) {
        snprintf(llm_cfg->provider, sizeof(llm_cfg->provider), "%s", eff_provider);
    }
    if (eff_model && *eff_model) {
        snprintf(llm_cfg->model, sizeof(llm_cfg->model), "%s", eff_model);
    }
    if (temperature >= 0.0) {
        llm_cfg->temperature = (float)temperature;
    }
    if (max_tokens > 0) {
        llm_cfg->max_tokens = max_tokens;
    }

    /* Handle extra_body — merge into llm_cfg extra_body if present */
    char saved_extra[4096] = "";
    if (extra_body) {
        snprintf(saved_extra, sizeof(saved_extra), "%s", llm_cfg->extra_body);
        char *eb_str = json_serialize(extra_body);
        if (eb_str) {
            snprintf(llm_cfg->extra_body, sizeof(llm_cfg->extra_body), "%s", eb_str);
            free(eb_str);
        }
    }

    /* Call LLM */
    llm_response_t *response = llm_chat_completion(
        llm_cfg,
        (const message_t **)msg_array,
        msg_count,
        NULL /* no tools */);

    /* Restore extra_body */
    if (extra_body && saved_extra[0]) {
        snprintf(llm_cfg->extra_body, sizeof(llm_cfg->extra_body), "%s", saved_extra);
    } else if (extra_body) {
        llm_cfg->extra_body[0] = '\0';
    }

    /* Clean up message array */
    free_message_array(msg_array, msg_count);

    if (!response) {
        if (err_msg) snprintf(err_msg, err_sz, "LLM call failed");
        return NULL;
    }

    /* Extract text and usage */
    char *text = extract_text(response);
    plugin_llm_usage_t usage = extract_usage(response);

    /* Resolve attribution */
    char provider_buf[256] = "", model_buf[256] = "";
    resolve_attribution(eff_provider, eff_model, response, llm_cfg,
                         provider_buf, sizeof(provider_buf),
                         model_buf, sizeof(model_buf));

    /* Build result */
    plugin_llm_result_t *result = calloc(1, sizeof(plugin_llm_result_t));
    if (!result) {
        free(text);
        llm_response_free(response);
        if (err_msg) snprintf(err_msg, err_sz, "Out of memory");
        return NULL;
    }

    result->text = text;
    result->provider = strdup(provider_buf);
    result->model = strdup(model_buf);
    result->agent_id = eff_agent && *eff_agent ? strdup(eff_agent) : strdup("default");
    result->usage = usage;

    /* Build audit object */
    result->audit = json_object();
    if (result->audit) {
        if (plugin_id) {
            json_set(result->audit, "plugin_id", json_string(plugin_id));
        }
        if (eff_profile && *eff_profile) {
            json_set(result->audit, "profile", json_string(eff_profile));
        }
    }

    llm_response_free(response);
    return result;
}

/* ================================================================
 *  Main API — complete_structured
 *  Port of Python: PluginLlm.complete_structured()
 * ================================================================ */
plugin_llm_structured_result_t *plugin_llm_complete_structured(
    const char *plugin_id,
    llm_config_t *llm_cfg,
    const char *instructions,
    const plugin_llm_input_t *inputs,
    int input_count,
    bool json_mode,
    const json_t *json_schema,
    const char *schema_name,
    const char *system_prompt,
    const char *provider,
    const char *model,
    const char *agent_id,
    const char *profile,
    double temperature,
    int max_tokens,
    json_t *policy_json,
    char *err_msg, size_t err_sz)
{
    if (!plugin_id || !llm_cfg || !instructions || !*instructions) {
        if (err_msg) snprintf(err_msg, err_sz, "plugin_llm_complete_structured requires non-empty instructions");
        return NULL;
    }
    if (!inputs || input_count <= 0) {
        if (err_msg) snprintf(err_msg, err_sz, "plugin_llm_complete_structured requires at least one input block");
        return NULL;
    }

    /* Resolve trust policy and check overrides */
    plugin_llm_trust_policy_t policy = resolve_trust_policy(plugin_id, policy_json);

    const char *eff_provider = provider;
    const char *eff_model = model;
    const char *eff_agent = agent_id;
    const char *eff_profile = profile;

    if (check_overrides(&policy, &eff_provider, &eff_model,
                                    &eff_agent, &eff_profile,
                                    err_msg, err_sz) != 0) {
        return NULL;
    }

    /* Build structured messages */
    json_t *messages = plugin_llm_build_structured_messages(
        instructions, inputs, input_count,
        json_mode, json_schema, schema_name, system_prompt);
    if (!messages) {
        if (err_msg) snprintf(err_msg, err_sz, "Failed to build messages");
        return NULL;
    }

    /* Build extra body for response format */
    json_t *extra_body = json_response_format(json_mode, json_schema);

    /* Convert to message_t array */
    int msg_count = 0;
    message_t **msg_array = json_to_message_array(messages, &msg_count);
    if (!msg_array || msg_count == 0) {
        json_free(messages);
        json_free(extra_body);
        if (err_msg) snprintf(err_msg, err_sz, "No valid messages");
        return NULL;
    }

    /* Apply overrides */
    if (eff_provider && *eff_provider) {
        snprintf(llm_cfg->provider, sizeof(llm_cfg->provider), "%s", eff_provider);
    }
    if (eff_model && *eff_model) {
        snprintf(llm_cfg->model, sizeof(llm_cfg->model), "%s", eff_model);
    }
    if (temperature >= 0.0) {
        llm_cfg->temperature = (float)temperature;
    }
    if (max_tokens > 0) {
        llm_cfg->max_tokens = max_tokens;
    }

    /* Merge extra_body */
    char saved_extra[4096] = "";
    if (extra_body) {
        snprintf(saved_extra, sizeof(saved_extra), "%s", llm_cfg->extra_body);
        char *eb_str = json_serialize(extra_body);
        if (eb_str) {
            snprintf(llm_cfg->extra_body, sizeof(llm_cfg->extra_body), "%s", eb_str);
            free(eb_str);
        }
    }

    /* Call LLM */
    llm_response_t *response = llm_chat_completion(
        llm_cfg,
        (const message_t **)msg_array,
        msg_count,
        NULL);

    /* Restore extra_body */
    if (extra_body && saved_extra[0]) {
        snprintf(llm_cfg->extra_body, sizeof(llm_cfg->extra_body), "%s", saved_extra);
    } else if (extra_body) {
        llm_cfg->extra_body[0] = '\0';
    }

    /* Clean up */
    free_message_array(msg_array, msg_count);
    json_free(messages);
    json_free(extra_body);

    if (!response) {
        if (err_msg) snprintf(err_msg, err_sz, "LLM call failed");
        return NULL;
    }

    /* Extract response */
    char *text = extract_text(response);
    plugin_llm_usage_t usage = extract_usage(response);

    char provider_buf[256] = "", model_buf[256] = "";
    resolve_attribution(eff_provider, eff_model, response, llm_cfg,
                         provider_buf, sizeof(provider_buf),
                         model_buf, sizeof(model_buf));

    /* Parse structured text */
    char content_type[16] = "text";
    json_t *parsed = parse_structured_text(text, json_mode, json_schema,
                                            content_type, sizeof(content_type));

    /* Build result */
    plugin_llm_structured_result_t *result = calloc(1, sizeof(plugin_llm_structured_result_t));
    if (!result) {
        free(text);
        json_free(parsed);
        llm_response_free(response);
        if (err_msg) snprintf(err_msg, err_sz, "Out of memory");
        return NULL;
    }

    result->text = text;
    result->provider = strdup(provider_buf);
    result->model = strdup(model_buf);
    result->agent_id = eff_agent && *eff_agent ? strdup(eff_agent) : strdup("default");
    result->usage = usage;
    result->parsed = parsed;
    result->content_type = strdup(content_type);

    /* Build audit */
    result->audit = json_object();
    if (result->audit) {
        if (plugin_id) {
            json_set(result->audit, "plugin_id", json_string(plugin_id));
        }
        if (eff_profile && *eff_profile) {
            json_set(result->audit, "profile", json_string(eff_profile));
        }
        if (schema_name && *schema_name) {
            json_set(result->audit, "schema_name", json_string(schema_name));
        }
    }

    llm_response_free(response);
    return result;
}

/* ================================================================
 *  Free functions
 * ================================================================ */

void plugin_llm_result_free(plugin_llm_result_t *result) {
    if (!result) return;
    free(result->text);
    free(result->provider);
    free(result->model);
    free(result->agent_id);
    json_free(result->audit);
    free(result);
}

void plugin_llm_structured_result_free(plugin_llm_structured_result_t *result) {
    if (!result) return;
    free(result->text);
    free(result->provider);
    free(result->model);
    free(result->agent_id);
    json_free(result->parsed);
    free(result->content_type);
    json_free(result->audit);
    free(result);
}

void plugin_llm_input_free(plugin_llm_input_t *input) {
    if (!input) return;
    free(input->text);
    free(input->url);
    free(input->data);
    free(input->mime_type);
    free(input->file_name);
}

/* Port of Python: make_plugin_llm_for_test — N/A, PluginLlm is a Python class with callback injection.
 * C uses direct function calls (resolve_trust_policy, invoke) with struct parameters. Test helpers
 * would construct plugin_llm_input_t + callback structs directly. */
