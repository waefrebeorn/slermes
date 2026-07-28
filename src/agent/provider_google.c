/*
 * provider_google.c — Google Gemini API provider.
 * Supports Gemini models via generativelanguage.googleapis.com.
 *
 * Key differences from OpenAI:
 *  - x-goog-api-key header
 *  - Endpoint: /v1beta/models/{model}:generateContent
 *  - "contents" array with "user"/"model" roles (not "assistant")
 *  - Parts array for content blocks
 *  - functionCall / functionResponse parts for tool calls
 *  - Separate system_instruction field
 *  - functionDeclarations tool format
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "provider.h"
#include "provider_profile.h"
#include "base64.h"
#include "hermes_transport_common.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ================================================================
 *  Gemini native adapter helpers
 * ================================================================ */

/* Detect free-tier quota exhaustion in Gemini error responses */
static bool is_free_tier_quota_error(const char *error_message) {
    if (!error_message) return false;
    /* Python: "free_tier" in error_message.lower() */
    const char *p = error_message;
    while (*p) {
        if ((*p == 'f' || *p == 'F') &&
            strncasecmp(p, "free_tier", 9) == 0)
            return true;
        p++;
    }
    return false;
}

/* ================================================================
 *  URL building
 * ================================================================ */

static char *google_build_url(const provider_t *p, const char *base_url) {
    if (!base_url || !*base_url)
        base_url = "https://generativelanguage.googleapis.com/v1beta";

    const char *model = p->model[0] ? p->model : "gemini-2.0-flash";

    /* If URL already includes :generateContent, use as-is */
    if (strstr(base_url, ":generateContent") || strstr(base_url, ":streamGenerateContent"))
        return strdup(base_url);

    size_t base_len = strlen(base_url);
    size_t model_len = strlen(model);
    char *url = (char *)malloc(base_len + model_len + 40);
    if (!url) return NULL;

    /* Strip trailing slash to avoid //models */
    while (base_len > 0 && base_url[base_len-1] == '/') base_len--;

    snprintf(url, base_len + model_len + 40, "%.*s/models/%s:generateContent",
             (int)base_len, base_url, model);

    return url;
}

/* ================================================================
 *  Headers
 * ================================================================ */

char *google_build_headers(const provider_t *p, const char *api_key) {
    (void)p;
    char *headers = (char *)malloc(1024);
    if (!headers) return NULL;

    if (api_key && *api_key) {
        snprintf(headers, 1024,
            "x-goog-api-key: %s\r\n"
            "Content-Type: application/json\r\n"
            "Accept: application/json",
            api_key);
    } else {
        snprintf(headers, 1024,
            "Content-Type: application/json\r\n"
            "Accept: application/json");
    }
    return headers;
}

/* ================================================================
 *  Request body building
 * ================================================================ */

static char *google_build_request_body(const provider_t *p,
                                        const message_t **messages,
                                        size_t msg_count,
                                        json_t *tools_json,
                                        bool streaming) {
    (void)p;
    json_t *root = json_object();
    if (!root) return NULL;

    /* Generation config */
    json_t *gen_config = json_object();
    int max_tok = p->config.max_tokens > 0 ? p->config.max_tokens : 4096;
    json_set(gen_config, "maxOutputTokens", json_number(max_tok));
    if (p->config.temperature >= 0.0f)
        json_set(gen_config, "temperature", json_number(p->config.temperature));
    if (p->config.top_p > 0.0f && p->config.top_p < 1.0f)
        json_set(gen_config, "topP", json_number(p->config.top_p));
    if (p->config.stop_count > 0) {
        json_t *stop_arr = json_array();
        for (int i = 0; i < p->config.stop_count && i < HERMES_STOP_SEQUENCES_MAX; i++)
            if (p->config.stop_sequences[i][0])
                json_append(stop_arr, json_string(p->config.stop_sequences[i]));
        if (json_len(stop_arr) > 0) json_set(gen_config, "stopSequences", stop_arr);
        else json_free(stop_arr);
    }
    json_set(root, "generationConfig", gen_config);

    /* B30: generation_config depth — top_k, candidate_count */
    if (p->config.top_k > 0)
        json_set(gen_config, "topK", json_number(p->config.top_k));
    if (p->config.candidate_count > 0)
        json_set(gen_config, "candidateCount", json_number(p->config.candidate_count));

    /* B28/L05: extra_body — merge arbitrary JSON fields into request body */
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

    /* B29: Google safety settings — parsed from JSON array string */
    if (p->config.safety_settings[0]) {
        json_t *ss = json_parse(p->config.safety_settings, NULL);
        if (ss && ss->type == JSON_ARRAY) {
            json_set(root, "safetySettings", ss);
        } else if (ss) {
            json_free(ss);
        }
    }

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
        json_set(root, "parallel_tool_calls", json_bool(false));

    /* System instruction (separate from contents) */
    char system_text[4096] = "";
    bool has_system = false;

    /* Tools (convert from OpenAI format to Google format) */
    /* OpenAI: {"type":"function","function":{"name":"...","description":"...","parameters":{...}}} */
    /* Google: {"functionDeclarations":[{"name":"...","description":"...","parameters":{...}}]} */
    if (tools_json && json_len(tools_json) > 0) {
        size_t n = json_len(tools_json);
        /* Count how many have function definitions */
        int fd_count = 0;
        for (size_t i = 0; i < n; i++) {
            json_t *ot = json_get(tools_json, i);
            json_t *fn = json_obj_get(ot, "function");
            if (fn) fd_count++;
        }

        if (fd_count > 0) {
            json_t *tools_arr = json_array();
            json_t *decls = json_array();
            for (size_t i = 0; i < n; i++) {
                json_t *ot = json_get(tools_json, i);
                json_t *fn = json_obj_get(ot, "function");
                if (!fn) continue;

                json_t *fd = json_object();
                json_set(fd, "name", json_copy(json_obj_get(fn, "name")));
                json_set(fd, "description", json_copy(json_obj_get(fn, "description")));

                /* Map OpenAI "parameters" to Google "parameters" (same name, same structure) */
                json_t *params = json_obj_get(fn, "parameters");
                if (params)
                    json_set(fd, "parameters", json_copy(params));

                json_append(decls, fd);
            }
            /* tools: [{"functionDeclarations": [...]}] — array of objects with functionDeclarations key */
            json_t *fd_obj = json_object();
            json_set(fd_obj, "functionDeclarations", decls);
            json_append(tools_arr, fd_obj);
            json_set(root, "tools", tools_arr);
        }
    }

    /* Contents array */
    json_t *contents = json_array();
    if (!contents) { json_free(root); return NULL; }

    /* Track tool call IDs mapping: generated tool_call_id -> function name
     * For Google, functionResponse needs the function name (not an ID), so
     * we store the mapping from tool_call_id to function name */
    int tc_map_count = 0;
    struct {
        char id[64];
        char name[128];
    } tc_map[128];

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

        /* For Google, we need to build "contents" array entries.
         * Roles: "user" and "model" (not "assistant", not "tool").
         * Tool results: functionResponse parts in a user message.
         * Tool calls: functionCall parts in a model message. */

        if (msg->role == MSG_USER) {
            json_t *content = json_object();
            json_set(content, "role", json_string("user"));
            json_t *parts = json_array();

            if (msg->tool_call_id) {
                /* This is a tool result wrapped as user message.
                 * Find the function name from the tool_call_id. */
                const char *fn_name = NULL;
                for (int k = 0; k < tc_map_count; k++) {
                    if (strcmp(tc_map[k].id, msg->tool_call_id) == 0) {
                        fn_name = tc_map[k].name;
                        break;
                    }
                }
                if (!fn_name) fn_name = msg->tool_name ? msg->tool_name : "unknown";

                json_t *fr = json_object();
                json_t *resp_obj = json_object();
                /* Parse result as JSON object if possible */
                char *err = NULL;
                json_t *parsed = NULL;
                if (msg->content)
                    parsed = json_parse(msg->content, &err);
                if (parsed) {
                    json_set(resp_obj, "result", parsed);
                } else {
                    json_set(resp_obj, "result", json_string(msg->content ? msg->content : ""));
                    free(err);
                }
                json_set(fr, "functionResponse", resp_obj);
                /* Set name at outer level of functionResponse part */
                /* Google format: {functionResponse: {name: "...", response: {...}}} */
                json_t *fr_wrapped = json_object();
                json_set(fr_wrapped, "name", json_string(fn_name));
                json_set(fr_wrapped, "response", resp_obj);
                /* Put functionResponse into a part */
                json_t *part = json_object();
                json_set(part, "functionResponse", json_copy(fr_wrapped));
                json_append(parts, part);

                json_free(fr_wrapped);
            } else {
                json_t *part = json_object();
                json_set(part, "text", json_string(msg->content ? msg->content : ""));
                json_append(parts, part);
            }

            json_set(content, "parts", parts);
            json_append(contents, content);
        }
        else if (msg->role == MSG_ASSISTANT) {
            json_t *content = json_object();
            json_set(content, "role", json_string("model"));
            json_t *parts = json_array();

            /* Text part */
            if (msg->content && msg->content[0]) {
                json_t *part = json_object();
                json_set(part, "text", json_string(msg->content));
                json_append(parts, part);
            }

            /* Function call parts */
            for (int j = 0; j < msg->tool_calls_count; j++) {
                /* Store in map for tool result lookup */
                if (tc_map_count < 128) {
                    snprintf(tc_map[tc_map_count].id, sizeof(tc_map[tc_map_count].id),
                             "%s", msg->tool_calls[j].id);
                    snprintf(tc_map[tc_map_count].name, sizeof(tc_map[tc_map_count].name),
                             "%s", msg->tool_calls[j].name);
                    tc_map_count++;
                }

                json_t *fc = json_object();
                json_set(fc, "name", json_string(msg->tool_calls[j].name));

                /* Parse args as JSON object */
                char *err = NULL;
                json_t *args = json_parse(msg->tool_calls[j].arguments, &err);
                if (args) {
                    json_set(fc, "args", args);
                } else {
                    json_set(fc, "args", json_object());
                    free(err);
                }

                json_t *part = json_object();
                json_set(part, "functionCall", fc);
                json_append(parts, part);
            }

            json_set(content, "parts", parts);
            json_append(contents, content);
        }
        else if (msg->role == MSG_TOOL) {
            /* MSG_TOOL is a tool result. For Google API, this goes
             * into a "user" role content with functionResponse parts. */
            json_t *content = json_object();
            json_set(content, "role", json_string("user"));
            json_t *parts = json_array();

            const char *fn_name = NULL;
            for (int k = 0; k < tc_map_count; k++) {
                if (strcmp(tc_map[k].id, msg->tool_call_id) == 0) {
                    fn_name = tc_map[k].name;
                    break;
                }
            }
            if (!fn_name) fn_name = msg->tool_name ? msg->tool_name : "unknown";

            json_t *fr_resp = json_object();
            json_t *fr_part = json_object();
            /* Parse result content as JSON */
            char *err = NULL;
            json_t *parsed = json_parse(msg->content ? msg->content : "{}", &err);
            if (parsed) {
                json_set(fr_resp, "response", parsed);
            } else {
                json_t *resp_obj = json_object();
                json_set(resp_obj, "result", json_string(msg->content ? msg->content : ""));
                json_set(fr_resp, "response", resp_obj);
                free(err);
            }
            json_set(fr_part, "name", json_string(fn_name));
            /* Merge name into response */
            json_t *final_resp = json_obj_get(fr_resp, "response");
            if (final_resp) {
                /* Google format expects: {functionResponse: {name: "fn", response: {...}}} */
                json_t *part = json_object();
                json_t *fr = json_object();
                json_set(fr, "name", json_string(fn_name));
                json_set(fr, "response", json_copy(final_resp));
                json_set(part, "functionResponse", fr);
                json_append(parts, part);
            }

            json_set(content, "parts", parts);
            json_append(contents, content);
        }
    }

    if (json_len(contents) == 0) {
        /* Dummy content */
        json_t *dummy = json_object();
        json_set(dummy, "role", json_string("user"));
        json_t *parts = json_array();
        json_t *part = json_object();
        json_set(part, "text", json_string("Hello"));
        json_append(parts, part);
        json_set(dummy, "parts", parts);
        json_append(contents, dummy);
    }

    json_set(root, "contents", contents);

    /* System instruction */
    if (has_system && system_text[0]) {
        json_t *si = json_object();
        json_t *si_parts = json_array();
        json_t *si_part = json_object();
        json_set(si_part, "text", json_string(system_text));
        json_append(si_parts, si_part);
        json_set(si, "parts", si_parts);
        json_set(root, "systemInstruction", si);
    }

    /* Streaming — Google uses a different endpoint for streaming */
    (void)streaming;

    /* v651b: apply ProviderProfile quirks (fixed_temperature, default_max_tokens) */
    apply_provider_profile(p, root);

    char *body = json_serialize(root);
    json_free(root);
    return body;
}

/* Port of Python agent/shell_hooks.py:_parse_response(). */
/* ================================================================
 *  Response parsing
 * ================================================================ */

static provider_response_t *google_parse_response(const provider_t *p,
                                                    const char *response_body) {
    (void)p;
    provider_response_t *resp = (provider_response_t *)calloc(1, sizeof(*resp));
    if (!resp) return NULL;

    char *err = NULL;
    json_t *root = json_parse(response_body, &err);
    if (!root) {
        resp->content = (char *)malloc(256);
        if (resp->content)
            snprintf(resp->content, 256, "JSON parse error: %s", err ? err : "unknown");
        free(err);
        return resp;
    }

    /* Check for error */
    json_t *error_obj = json_obj_get(root, "error");
    if (error_obj) {
        const char *err_msg = json_get_str(error_obj, "message", "unknown error");
        int status = (int)json_get_num(error_obj, "code", 0);
        resp->content = (char *)malloc(1024);
        if (resp->content) {
            if (status == 429 && is_free_tier_quota_error(err_msg)) {
                snprintf(resp->content, 1024,
                    "Google API free-tier quota exhausted: %s\n\n"
                    "Your API key is on the free tier (<= 250 requests/day). "
                    "Enable billing at https://aistudio.google.com/apikey",
                    err_msg);
            } else {
                snprintf(resp->content, 1024, "Google API error: %s", err_msg);
            }
        }
        json_free(root);
        return resp;
    }

    /* Usage metadata */
    json_t *usage = json_obj_get(root, "usageMetadata");
    if (usage) {
        resp->input_tokens = (int)json_get_num(usage, "promptTokenCount", 0);
        resp->output_tokens = (int)json_get_num(usage, "candidatesTokenCount", 0);
    }

    /* Candidates[0].content.parts[] */
    json_t *candidates = json_obj_get(root, "candidates");
    if (candidates && json_len(candidates) > 0) {
        json_t *candidate = json_get(candidates, 0);
        json_t *content = json_obj_get(candidate, "content");
        if (content) {
            json_t *parts = json_obj_get(content, "parts");
            if (parts && json_len(parts) > 0) {
                size_t n = json_len(parts);

                /* First pass: count text length and tool calls */
                size_t text_len = 0;
                int tc_count = 0;
                for (size_t i = 0; i < n; i++) {
                    json_t *part = json_get(parts, i);
                    if (json_obj_get(part, "text")) {
                        const char *t = json_get_str(part, "text", "");
                        text_len += strlen(t);
                    }
                    if (json_obj_get(part, "functionCall")) {
                        if (tc_count < 64) tc_count++;
                    }
                }

                /* Allocate and fill text */
                if (text_len > 0) {
                    resp->content = (char *)calloc(text_len + 1, 1);
                    if (resp->content) {
                        size_t pos = 0;
                        for (size_t i = 0; i < n; i++) {
                            json_t *part = json_get(parts, i);
                            if (json_obj_get(part, "text")) {
                                const char *t = json_get_str(part, "text", "");
                                size_t add = strlen(t);
                                if (pos + add <= text_len) {
                                    memcpy(resp->content + pos, t, add);
                                    pos += add;
                                }
                            }
                        }
                        resp->content[pos] = '\0';
                    }
                } else {
                    resp->content = strdup("");
                }

                /* Extract tool calls (functionCall parts) */
                if (tc_count > 0) {
                    resp->tool_calls_count = 0;
                    for (size_t i = 0; i < n && resp->tool_calls_count < 64; i++) {
                        json_t *part = json_get(parts, i);
                        json_t *fc = json_obj_get(part, "functionCall");
                        if (!fc) continue;

                        int idx = resp->tool_calls_count;
                        const char *fn_name = json_get_str(fc, "name", "");
                        snprintf(resp->tool_calls[idx].name,
                                 sizeof(resp->tool_calls[idx].name), "%s", fn_name);

                        /* Generate a tool call ID (Google doesn't provide one) */
                        static int g_tc_counter = 0;
                        snprintf(resp->tool_calls[idx].id,
                                 sizeof(resp->tool_calls[idx].id), "call_google_%d",
                                 ++g_tc_counter);

                        /* Serialize args to JSON string */
                        json_t *args = json_obj_get(fc, "args");
                        if (args) {
                            char *args_str = json_serialize(args);
                            if (args_str) {
                                snprintf(resp->tool_calls[idx].arguments,
                                         sizeof(resp->tool_calls[idx].arguments),
                                         "%s", args_str);
                                free(args_str);
                            }
                        }

                        resp->tool_calls_count++;
                    }
                }
            }
        }

        /* Check finish reason */
        const char *finish = json_get_str(candidate, "finishReason", "");
        const char *mapped = transport_map_finish_reason("google", finish);
        snprintf(resp->finish_reason, sizeof(resp->finish_reason), "%s", mapped);

        /* Handle blocked content (finishReason=SAFETY/BLOCKLIST/etc with no content text) */
        if (!resp->content && mapped && strcmp(mapped, "content_filter") == 0) {
            /* Extract safety ratings for user feedback */
            resp->content = strdup("[Content blocked by Google safety filters]");
        }

        /* If finishReason is "STOP" but we have function calls,
         * that's normal for Google — keep tool calls. */
    }

    json_free(root);
    return resp;
}

/* ================================================================
 *  Streaming chunk parsing
 * ================================================================ */

static provider_response_t *google_parse_stream_chunk(const provider_t *p,
                                                       const char *chunk) {
    (void)p;
    provider_response_t *resp = (provider_response_t *)calloc(1, sizeof(*resp));
    if (!resp) return NULL;

    /* Null-safe */
    if (!chunk) {
        resp->content = strdup("");
        return resp;
    }

    /* Strip "data: " prefix if present (Google SSE) */
    const char *json_str = chunk;
    if (strncmp(chunk, "data: ", 6) == 0)
        json_str = chunk + 6;

    if (!json_str || !*json_str) {
        resp->content = strdup(chunk);
        return resp;
    }

    /* Skip "[DONE]" */
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

    /* Google streaming response structure:
     * {
     *   "candidates": [{
     *     "content": {
     *       "parts": [{"text": "chunk"}]
     *     }
     *   }],
     *   "usageMetadata": {...}
     * } */

    json_t *candidates = json_obj_get(root, "candidates");
    if (candidates && json_len(candidates) > 0) {
        json_t *candidate = json_get(candidates, 0);

        /* Check finish reason FIRST (before content, since final chunk
         * may have finishReason + text content simultaneously) */
        const char *finish = json_get_str(candidate, "finishReason", NULL);
        if (finish) {
            const char *mapped = transport_map_finish_reason("google", finish);
            snprintf(resp->finish_reason, sizeof(resp->finish_reason), "%s", mapped);
            /* Also extract text if present before signaling end */
        }

        json_t *content = json_obj_get(candidate, "content");
        if (content) {
            json_t *parts = json_obj_get(content, "parts");
            if (parts && json_len(parts) > 0) {
                json_t *part = json_get(parts, 0);
                /* Text delta */
                const char *text = json_get_str(part, "text", NULL);
                if (text) {
                    resp->content = strdup(text);
                    if (finish) {
                        /* Final chunk with both finishReason and text */
                        json_free(root);
                        return resp;
                    }
                    json_free(root);
                    return resp;
                }
                /* functionCall delta (may be partial or first chunk) */
                json_t *fc = json_obj_get(part, "functionCall");
                if (fc) {
                    /* Return empty content — streaming accumulates these */
                    resp->content = strdup("");
                    json_free(root);
                    return resp;
                }
            }
        }

        if (finish) {
            /* finishReason but no content → signal end of stream */
            resp->content = strdup("");
            json_free(root);
            return resp;
        }
    }

    /* Usage metadata in last chunk */
    json_t *usage = json_obj_get(root, "usageMetadata");
    if (usage) {
        resp->input_tokens = (int)json_get_num(usage, "promptTokenCount", 0);
        resp->output_tokens = (int)json_get_num(usage, "candidatesTokenCount", 0);
    }

    resp->content = strdup("");
    json_free(root);
    return resp;
}

/* ================================================================
 *  Free response
 * ================================================================ */

static void google_free_response(provider_response_t *resp) {
    if (!resp) return;
    free(resp->content);
    free(resp->reasoning);
    free(resp);
}

/* ================================================================
 *  Google provider utility functions — ported from Python
 *  gemini_native_adapter.py
 * ================================================================ */

/* Port of Python agent/gemini_native_adapter.py:is_native_gemini_base_url().
 * Check if a base URL speaks Gemini's native REST API.
 * Returns true when the URL contains "generativelanguage.googleapis.com"
 * and does NOT end with "/openai" (OpenAI-compat endpoint). */
bool google_is_native_base_url(const char *base_url) {
    if (!base_url || !*base_url) return false;

    /* Normalize: strip, lowercase */
    char buf[512];
    size_t len = 0;
    const char *p = base_url;
    while (*p && len < sizeof(buf) - 1) {
        buf[len++] = (char)tolower((unsigned char)*p);
        p++;
    }
    buf[len] = '\0';

    /* Trim trailing whitespace */
    while (len > 0 && buf[len - 1] == ' ') buf[--len] = '\0';
    /* Trim trailing slashes */
    while (len > 0 && buf[len - 1] == '/') buf[--len] = '\0';

    if (len == 0) return false;
    if (!strstr(buf, "generativelanguage.googleapis.com")) return false;

    /* Check it doesn't end with /openai (the OpenAI-compat endpoint) */
    if (len >= 7 && strcmp(buf + len - 7, "/openai") == 0) return false;

    return true;
}

/* Port of Python gemini_native_adapter._coerce_content_to_text().
 * Extracts text from a Gemini message content value.
 * Handles: NULL/JSON_NULL → "", string → copy, array → join text parts,
 * object with type=="text" → extract text field.
 * Returns malloc'd string, caller must free. */
char *google_coerce_content_to_text(const json_t *content) {
    if (!content || content->type == JSON_NULL) return strdup("");

    /* String: return content directly */
    if (content->type == JSON_STRING)
        return strdup(content->str_val ? content->str_val : "");

    /* Array: iterate parts, collect text pieces */
    if (content->type == JSON_ARRAY) {
        char *pieces[256];
        int n = 0;
        size_t total = 0;

        for (size_t i = 0; i < content->c.count && n < 256; i++) {
            json_t *item = content->c.items[i];
            if (!item) continue;

            if (item->type == JSON_STRING) {
                const char *s = item->str_val ? item->str_val : "";
                pieces[n] = strdup(s);
                if (pieces[n]) { total += strlen(s); n++; }
            } else if (item->type == JSON_OBJECT) {
                const char *type_str = json_get_str(item, "type", "");
                if (strcmp(type_str, "text") == 0) {
                    const char *text = json_get_str(item, "text", "");
                    pieces[n] = strdup(text);
                    if (pieces[n]) { total += strlen(text); n++; }
                }
            }
        }

        if (n == 0) return strdup("");

        size_t needed = total + (n > 0 ? (size_t)(n - 1) : 0) + 1;
        char *result = (char *)malloc(needed);
        if (!result) {
            for (int i = 0; i < n; i++) free(pieces[i]);
            return NULL;
        }
        result[0] = '\0';
        for (int i = 0; i < n; i++) {
            if (i > 0) strcat(result, "\n");
            strcat(result, pieces[i]);
            free(pieces[i]);
        }
        return result;
    }

    return strdup("");
}

/* Port of Python gemini_native_adapter._tool_call_extra_signature().
 * Extracts thought signature from tool_call.extra_content.google(.thought_signature)
 * or tool_call.extra_content.thought_signature. Returns malloc'd string or NULL. */
char *google_tool_call_extra_signature(const json_t *tool_call) {
    if (!tool_call) return NULL;
    json_t *extra = json_obj_get(tool_call, "extra_content");
    if (!extra) return NULL;

    json_t *google = json_obj_get(extra, "google");
    if (!google) google = json_obj_get(extra, "thought_signature");
    if (!google) return NULL;

    if (google->type == JSON_OBJECT) {
        json_t *sig = json_obj_get(google, "thought_signature");
        if (!sig) sig = json_obj_get(google, "thoughtSignature");
        if (sig && sig->type == JSON_STRING && sig->str_val && sig->str_val[0])
            return strdup(sig->str_val);
    } else if (google->type == JSON_STRING && google->str_val && google->str_val[0]) {
        return strdup(google->str_val);
    }
    return NULL;
}

/* Port of Python gemini_native_adapter._translate_tool_call_to_gemini().
 * Translates an OpenAI-format tool_call to a Gemini functionCall part.
 * Returns a json_t object: {functionCall: {name, args}} with optional thoughtSignature. */
json_t *translate_tool_call_to_gemini(const json_t *tool_call) {
    json_t *fc = json_object();
    json_t *fn = NULL;
    const char *name = "";
    const char *args_raw = "";

    if (tool_call) {
        fn = json_obj_get(tool_call, "function");
        if (fn) {
            name = json_get_str(fn, "name", "");
            args_raw = json_get_str(fn, "arguments", "");
        }
    }

    json_set(fc, "name", json_string(name));

    /* Parse arguments as JSON — use args_raw already extracted above */
    if (args_raw && *args_raw) {
        char *err = NULL;
        json_t *args = json_parse(args_raw, &err);
        if (args) {
            json_set(fc, "args", args);
        } else {
            json_set(fc, "args", json_object());
            free(err);
        }
    } else {
        json_set(fc, "args", json_object());
    }

    json_t *result = json_object();
    json_set(result, "functionCall", fc);

    /* Optional thought signature */
    char *sig = google_tool_call_extra_signature(tool_call);
    if (sig) {
        json_set(result, "thoughtSignature", json_string(sig));
        free(sig);
    }

    return result;
}

/* Port of Python gemini_native_adapter._translate_tool_result_to_gemini().
 * Translates a tool-result message (role="tool" or "function") to a Gemini
 * functionResponse part.
 * @param message JSON object with tool_call_id, name, content keys
 * @param tool_name_by_call_id Optional JSON object mapping call_id→name (may be NULL)
 * Returns json_t: {functionResponse: {name: "...", response: {...}}}
 * Caller must json_free() the result. */
json_t *translate_tool_result_to_gemini(const json_t *message, const json_t *tool_name_by_call_id) {
    json_t *fr = json_object();

    /* Tool name resolution: message.name > tool_name_by_call_id[tool_call_id] > tool_call_id > "tool" */
    const char *name = NULL;
    if (message) name = json_get_str(message, "name", NULL);

    if (!name || !*name) {
        const char *tool_call_id = message ? json_get_str(message, "tool_call_id", NULL) : NULL;
        if (tool_call_id && *tool_call_id && tool_name_by_call_id) {
            json_t *mapped = json_obj_get(tool_name_by_call_id, tool_call_id);
            if (mapped && mapped->type == JSON_STRING && mapped->str_val && mapped->str_val[0])
                name = mapped->str_val;
            else
                name = tool_call_id;
        } else {
            name = (tool_call_id && *tool_call_id) ? tool_call_id : "tool";
        }
    }
    json_set(fr, "name", json_string(name));

    /* Content: coerce to text, then try JSON parse */
    json_t *content_json = message ? json_obj_get(message, "content") : NULL;
    char *text = google_coerce_content_to_text(content_json);

    json_t *response = NULL;
    if (text && *text) {
        /* Skip leading whitespace to check for JSON object/array start */
        const char *p = text;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (*p == '{' || *p == '[') {
            char *err = NULL;
            json_t *parsed = json_parse(text, &err);
            if (parsed) {
                if (parsed->type == JSON_OBJECT)
                    response = parsed;
                else
                    json_free(parsed); /* non-dict JSON → fall through to text wrap */
            } else {
                free(err);
            }
        }
    }

    if (!response) {
        response = json_object();
        json_set(response, "output", json_string(text ? text : ""));
    }

    json_set(fr, "response", response);
    free(text);

    json_t *result = json_object();
    json_set(result, "functionResponse", fr);
    return result;
}

/* Port of Python gemini_native_adapter._translate_tools_to_gemini().
 * Translates OpenAI-format tool definitions array to Gemini
 * functionDeclarations array.
 * Input: [{"type": "function", "function": {"name": "...", ...}}]
 * Output: [{"functionDeclarations": [{"name": "...", ...}]}]
 * Returns empty array [] if no valid tools found.
 * Caller must json_free() the result. */
json_t *google_translate_tools_to_gemini(const json_t *tools) {
    if (!tools || tools->type != JSON_ARRAY)
        return json_array();

    json_t *declarations = json_array();

    for (size_t i = 0; i < tools->c.count; i++) {
        json_t *tool = tools->c.items[i];
        if (!tool || tool->type != JSON_OBJECT) continue;

        /* Extract {function: {name, description, parameters}} */
        json_t *fn = json_obj_get(tool, "function");
        if (!fn || fn->type != JSON_OBJECT) continue;

        const char *name = json_get_str(fn, "name", NULL);
        if (!name || !*name) continue;

        json_t *decl = json_object();
        json_set(decl, "name", json_string(name));

        /* Optional description */
        const char *desc = json_get_str(fn, "description", NULL);
        if (desc && *desc)
            json_set(decl, "description", json_string(desc));

        /* Optional parameters — deep copy to avoid ownership issues */
        json_t *params = json_obj_get(fn, "parameters");
        if (params && params->type == JSON_OBJECT)
            json_set(decl, "parameters", json_copy(params));

        json_append(declarations, decl);
    }

    if (declarations->c.count == 0) {
        json_free(declarations);
        return json_array();
    }

    json_t *wrapper = json_object();
    json_set(wrapper, "functionDeclarations", declarations);
    json_t *result = json_array();
    json_append(result, wrapper);
    return result;
}

/* Port of Python gemini_native_adapter._translate_tool_choice_to_gemini().
 * Translates OpenAI tool_choice to Gemini tool_config.
 * OpenAI: "auto" / "required" / "none" / {"function": {"name": "..."}}
 * Gemini: {"functionCallingConfig": {"mode": "AUTO"|"ANY"|"NONE"}}
 * Returns NULL for None/unknown inputs. Caller must json_free() result. */
json_t *google_translate_tool_choice_to_gemini(const json_t *tool_choice) {
    if (!tool_choice) return NULL;

    if (tool_choice->type == JSON_STRING) {
        const char *s = tool_choice->str_val ? tool_choice->str_val : "";
        const char *mode = NULL;
        if (strcmp(s, "auto") == 0) mode = "AUTO";
        else if (strcmp(s, "required") == 0) mode = "ANY";
        else if (strcmp(s, "none") == 0) mode = "NONE";
        else return NULL;

        json_t *config = json_object();
        json_set(config, "mode", json_string(mode));
        json_t *result = json_object();
        json_set(result, "functionCallingConfig", config);
        return result;
    }

    if (tool_choice->type == JSON_OBJECT) {
        json_t *fn = json_obj_get(tool_choice, "function");
        if (fn) {
            const char *name = json_get_str(fn, "name", NULL);
            if (name && *name) {
                json_t *names = json_array();
                json_append(names, json_string(name));
                json_t *config = json_object();
                json_set(config, "mode", json_string("ANY"));
                json_set(config, "allowedFunctionNames", names);
                json_t *result = json_object();
                json_set(result, "functionCallingConfig", config);
                return result;
            }
        }
    }

    return NULL;
}

/* Port of Python gemini_native_adapter._normalize_thinking_config().
 * Normalizes thinking config to Gemini-compatible format.
 * Accepts thinkingBudget/thinking_budget (int), includeThoughts/include_thoughts (bool),
 * thinkingLevel/thinking_level (string, stripped+lowered).
 * Returns NULL for None/unknown/empty inputs. Caller must json_free() result. */
json_t *google_normalize_thinking_config(const json_t *config) {
    if (!config || config->type != JSON_OBJECT || config->c.count == 0)
        return NULL;

    json_t *result = json_object();
    bool has_any = false;

    /* Budget: thinkingBudget or thinking_budget */
    json_t *v = json_obj_get(config, "thinkingBudget");
    if (!v) v = json_obj_get(config, "thinking_budget");
    if (v && v->type == JSON_NUMBER) {
        json_set(result, "thinkingBudget", json_number(v->num_val));
        has_any = true;
    }

    /* Include thoughts: includeThoughts or include_thoughts */
    v = json_obj_get(config, "includeThoughts");
    if (!v) v = json_obj_get(config, "include_thoughts");
    if (v && v->type == JSON_BOOL) {
        json_set(result, "includeThoughts", json_bool(v->bool_val));
        has_any = true;
    }

    /* Thinking level: thinkingLevel or thinking_level */
    v = json_obj_get(config, "thinkingLevel");
    if (!v) v = json_obj_get(config, "thinking_level");
    if (v && v->type == JSON_STRING && v->str_val && v->str_val[0]) {
        /* Strip leading whitespace and lowercase */
        const char *s = v->str_val;
        while (*s == ' ') s++;
        if (*s) {
            char *lower = strdup(s);
            if (lower) {
                for (char *p = lower; *p; p++) *p = (char)tolower((unsigned char)*p);
                /* Strip trailing space */
                size_t len = strlen(lower);
                while (len > 0 && lower[len - 1] == ' ') lower[--len] = '\0';
                if (*lower) {
                    json_set(result, "thinkingLevel", json_string(lower));
                    has_any = true;
                }
                free(lower);
            }
        }
    }

    if (!has_any) {
        json_free(result);
        return NULL;
    }

    return result;
}

/* Port of Python gemini_native_adapter._extract_multimodal_parts().
 * Extracts multimodal parts from message content for Gemini.
 * - Non-array content → text parts via google_coerce_content_to_text()
 * - Array content → iterates items:
 *   - string → {"text": string}
 *   - dict type="text" → {"text": text}
 *   - dict type="image_url" → {"inlineData": {"mimeType": ..., "data": ...}}
 * Returns json_t* array of parts. Empty array if no valid parts.
 * Caller must json_free(). */
json_t *google_extract_multimodal_parts(const json_t *content) {
    json_t *parts = json_array();

    if (!content || content->type == JSON_NULL)
        return parts;

    if (content->type != JSON_ARRAY) {
        char *text = google_coerce_content_to_text(content);
        if (text && *text) {
            json_t *part = json_object();
            json_set(part, "text", json_string(text));
            json_append(parts, part);
        }
        free(text);
        return parts;
    }

    for (size_t i = 0; i < content->c.count; i++) {
        json_t *item = content->c.items[i];
        if (!item) continue;

        if (item->type == JSON_STRING) {
            json_t *part = json_object();
            json_set(part, "text", json_string(item->str_val ? item->str_val : ""));
            json_append(parts, part);

        } else if (item->type == JSON_OBJECT) {
            const char *ptype = json_get_str(item, "type", "");

            if (strcmp(ptype, "text") == 0) {
                const char *text = json_get_str(item, "text", "");
                if (text && *text) {
                    json_t *part = json_object();
                    json_set(part, "text", json_string(text));
                    json_append(parts, part);
                }

            } else if (strcmp(ptype, "image_url") == 0) {
                json_t *image_url = json_obj_get(item, "image_url");
                if (!image_url) continue;
                const char *url = json_get_str(image_url, "url", "");
                if (!url || strncmp(url, "data:", 5) != 0) continue;

                /* Parse data: URL: data:[mime][;base64],<data> */
                const char *comma = strchr(url, ',');
                if (!comma) continue;

                /* Extract MIME type from header */
                char mime[128] = "image/png";
                const char *semi = (const char *)memchr(url + 5, ';',
                    (size_t)(comma - url - 5));
                if (semi) {
                    size_t mime_len = (size_t)(semi - url - 5);
                    if (mime_len > 0 && mime_len < sizeof(mime)) {
                        memcpy(mime, url + 5, mime_len);
                        mime[mime_len] = '\0';
                    }
                }

                /* Decode then re-encode to normalize (matches Python behavior) */
                size_t decoded_len = 0;
                unsigned char *decoded = base64_decode(comma + 1, &decoded_len);
                if (decoded) {
                    char *encoded = base64_encode(decoded, decoded_len);
                    if (encoded) {
                        json_t *inline_data = json_object();
                        json_set(inline_data, "mimeType", json_string(mime));
                        json_set(inline_data, "data", json_string(encoded));
                        json_t *part = json_object();
                        json_set(part, "inlineData", inline_data);
                        json_append(parts, part);
                        free(encoded);
                    }
                    free(decoded);
                }
            }
        }
    }

    return parts;
}

/* Port of Python gemini_native_adapter._tool_call_extra_from_part().
 * Reverse of google_tool_call_extra_signature(): extracts thoughtSignature
 * from a Gemini part and wraps as {google: {thought_signature: sig}}.
 * Returns NULL if no signature found. Caller must json_free(). */
json_t *google_tool_call_extra_from_part(const json_t *part) {
    if (!part || part->type != JSON_OBJECT) return NULL;

    const char *sig = json_get_str(part, "thoughtSignature", NULL);
    if (!sig || !*sig) return NULL;

    json_t *google = json_object();
    json_set(google, "thought_signature", json_string(sig));
    json_t *result = json_object();
    json_set(result, "google", google);
    return result;
}

/* Port of Python gemini_native_adapter._build_gemini_contents().
 * Translates OpenAI-format messages array to Gemini contents[] + systemInstruction.
 * Iterates messages by role:
 *   - system → accumulates into system_instruction.text
 *   - tool/function → translate_tool_result wrapped as user role
 *   - assistant/user → extract_multimodal_parts + translate_tool_calls
 * Returns json_t object with "contents" array and optional "systemInstruction".
 * Caller must json_free(). */
json_t *google_build_gemini_contents(const json_t *messages) {
    json_t *result = json_object();
    json_t *contents = json_array();
    json_t *tool_name_by_call_id = json_object();
    char *system_texts[64];
    int n_system = 0;
    size_t system_total = 0;

    if (!messages || messages->type != JSON_ARRAY) {
        json_set(result, "contents", contents);
        json_free(tool_name_by_call_id);
        return result;
    }

    for (size_t i = 0; i < messages->c.count; i++) {
        json_t *msg = messages->c.items[i];
        if (!msg || msg->type != JSON_OBJECT) continue;

        const char *role = json_get_str(msg, "role", "user");

        /* System messages: accumulate text */
        if (strcmp(role, "system") == 0) {
            json_t *content = json_obj_get(msg, "content");
            char *text = google_coerce_content_to_text(content);
            if (text && *text && n_system < 64) {
                system_texts[n_system] = text;
                system_total += strlen(text);
                n_system++;
            } else {
                free(text);
            }
            continue;
        }

        /* Tool/function results: translate as functionResponse */
        if (strcmp(role, "tool") == 0 || strcmp(role, "function") == 0) {
            json_t *tool_part = translate_tool_result_to_gemini(msg, tool_name_by_call_id);
            json_t *content_obj = json_object();
            json_t *parts = json_array();
            json_append(parts, tool_part);
            json_set(content_obj, "role", json_string("user"));
            json_set(content_obj, "parts", parts);
            json_append(contents, content_obj);
            continue;
        }

        /* User/assistant messages */
        const char *gemini_role = (strcmp(role, "assistant") == 0) ? "model" : "user";
        json_t *parts = json_array();

        /* Extract multimodal parts from content */
        json_t *content_val = json_obj_get(msg, "content");
        json_t *content_parts = google_extract_multimodal_parts(content_val);
        if (content_parts) {
            for (size_t j = 0; j < content_parts->c.count; j++)
                json_append(parts, json_copy(content_parts->c.items[j]));
            json_free(content_parts);
        }

        /* Translate tool_calls */
        json_t *tool_calls = json_obj_get(msg, "tool_calls");
        if (tool_calls && tool_calls->type == JSON_ARRAY) {
            for (size_t j = 0; j < tool_calls->c.count; j++) {
                json_t *tc = tool_calls->c.items[j];
                if (!tc || tc->type != JSON_OBJECT) continue;

                /* Build tool_name_by_call_id map */
                const char *tc_id = json_get_str(tc, "id", NULL);
                if (!tc_id) tc_id = json_get_str(tc, "call_id", NULL);
                if (tc_id && *tc_id) {
                    json_t *fn = json_obj_get(tc, "function");
                    if (fn) {
                        const char *fn_name = json_get_str(fn, "name", NULL);
                        if (fn_name && *fn_name)
                            json_set(tool_name_by_call_id, tc_id, json_string(fn_name));
                    }
                }

                json_t *tc_part = translate_tool_call_to_gemini(tc);
                json_append(parts, tc_part);
            }
        }

        if (parts->c.count > 0) {
            json_t *content_obj = json_object();
            json_set(content_obj, "role", json_string(gemini_role));
            json_set(content_obj, "parts", parts);
            json_append(contents, content_obj);
        } else {
            json_free(parts);
        }
    }

    json_set(result, "contents", contents);

    /* Build system instruction from accumulated system texts */
    if (n_system > 0) {
        size_t needed = system_total + (size_t)(n_system > 0 ? n_system - 1 : 0) + 1;
        char *joined = (char *)malloc(needed);
        if (joined) {
            joined[0] = '\0';
            for (int i = 0; i < n_system; i++) {
                if (i > 0) strcat(joined, "\n");
                strcat(joined, system_texts[i]);
                free(system_texts[i]);
            }
            /* Strip trailing whitespace */
            size_t len = strlen(joined);
            while (len > 0 && (joined[len - 1] == ' ' || joined[len - 1] == '\n' || joined[len - 1] == '\t'))
                joined[--len] = '\0';

            if (*joined) {
                json_t *text_part = json_object();
                json_set(text_part, "text", json_string(joined));
                json_t *si_parts = json_array();
                json_append(si_parts, text_part);
                json_t *si = json_object();
                json_set(si, "parts", si_parts);
                json_set(result, "systemInstruction", si);
            }
            free(joined);
        } else {
            for (int i = 0; i < n_system; i++) free(system_texts[i]);
        }
    }

    json_free(tool_name_by_call_id);
    return result;
}

/* Port of Python gemini_native_adapter.py:gemini_http_error().
 * Generate structured error info from a Gemini API HTTP response.
 * Returns JSON object with: code, message, retry_after, details.
 * Caller must free with json_free(). */

/* Helper: get json_t value from an object by string key */
static json_t *json_obj_get_key(json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT || !key) return NULL;
    for (size_t i = 0; i < obj->c.count; i++) {
        if (obj->c.keys && obj->c.keys[i] && strcmp(obj->c.keys[i], key) == 0)
            return obj->c.items[i];
    }
    return NULL;
}

/* Port of Python agent/gemini_cloudcode_adapter.py:_gemini_http_error(). */
json_t *gemini_http_error(int status_code, const char *body_text) {
    json_t *result = json_object();
    if (!result) return NULL;

    /* Code */
    char code_buf[64];
    if (status_code == 401)
        snprintf(code_buf, sizeof(code_buf), "gemini_unauthorized");
    else if (status_code == 429)
        snprintf(code_buf, sizeof(code_buf), "gemini_rate_limited");
    else if (status_code == 404)
        snprintf(code_buf, sizeof(code_buf), "gemini_model_not_found");
    else
        snprintf(code_buf, sizeof(code_buf), "gemini_http_%d", status_code);
    json_set(result, "code", json_string(code_buf));

    /* Default message */
    char message[4096];
    if (body_text && *body_text) {
        size_t bt_len = strlen(body_text);
        if (bt_len > 500) bt_len = 500;
        snprintf(message, sizeof(message), "Gemini returned HTTP %d: %.*s",
                 status_code, (int)bt_len, body_text);
    } else {
        snprintf(message, sizeof(message), "Gemini returned HTTP %d", status_code);
    }

    /* Parse body for structured error info */
    char err_status[256] = "";
    char err_message[2048] = "";
    char reason[256] = "";
    bool is_free_tier = false;

    if (body_text && *body_text) {
        json_t *body = json_parse(body_text, NULL);
        if (body && body->type == JSON_OBJECT) {
            json_t *err_obj = json_obj_get_key(body, "error");
            if (err_obj && err_obj->type == JSON_OBJECT) {
                json_t *s = json_obj_get_key(err_obj, "status");
                if (s && s->type == JSON_STRING && s->str_val)
                    snprintf(err_status, sizeof(err_status), "%s", s->str_val);

                json_t *m = json_obj_get_key(err_obj, "message");
                if (m && m->type == JSON_STRING && m->str_val)
                    snprintf(err_message, sizeof(err_message), "%s", m->str_val);

                /* Build detailed message */
                if (err_message[0]) {
                    snprintf(message, sizeof(message),
                             "Gemini HTTP %d (%s): %s",
                             status_code,
                             err_status[0] ? err_status : "error",
                             err_message);
                }

                /* Check for free_tier in message */
                char *lower_msg = strdup(err_message[0] ? err_message : "");
                if (lower_msg) {
                    for (char *p = lower_msg; *p; p++) *p = (char)tolower((unsigned char)*p);
                    if (strstr(lower_msg, "free_tier")) is_free_tier = true;
                    free(lower_msg);
                }

                /* Extract details */
                json_t *details_arr = json_obj_get_key(err_obj, "details");
                if (details_arr && details_arr->type == JSON_ARRAY) {
                    for (size_t i = 0; i < details_arr->c.count; i++) {
                        json_t *detail = details_arr->c.items[i];
                        if (!detail || detail->type != JSON_OBJECT) continue;

                        json_t *type_url = json_obj_get_key(detail, "@type");
                        if (!type_url || type_url->type != JSON_STRING) continue;
                        if (!type_url->str_val) continue;

                        if (strstr(type_url->str_val, "google.rpc.ErrorInfo")) {
                            json_t *r = json_obj_get_key(detail, "reason");
                            if (r && r->type == JSON_STRING && r->str_val)
                                snprintf(reason, sizeof(reason), "%s", r->str_val);
                        }
                    }
                }
            }
        }
        json_free(body);
    }

    /* Free-tier quota exhaustion -> append actionable guidance */
    if (status_code == 429 && is_free_tier) {
        size_t mlen = strlen(message);
        snprintf(message + mlen, sizeof(message) - mlen,
                 "\n\nYour Google API key is on the free tier (<= 250 requests/day "
                 "for gemini-2.5-flash). Hermes typically makes 3-10 API calls per "
                 "user turn, so the free tier is exhausted in a handful of messages "
                 "and cannot sustain an agent session. Enable billing on your Google "
                 "Cloud project and regenerate the key in a billing-enabled project: "
                 "https://aistudio.google.com/apikey");
    }

    json_set(result, "message", json_string(message));
    json_set(result, "status_code", json_number(status_code));
    json_set(result, "retry_after", json_null());

    /* Details sub-object */
    json_t *details = json_object();
    if (err_status[0]) json_set(details, "status", json_string(err_status));
    if (reason[0])    json_set(details, "reason", json_string(reason));
    if (err_message[0]) json_set(details, "message", json_string(err_message));
    json_set(result, "details", details);

    return result;
}

/* ================================================================
 *  Tier probing
 * ================================================================ */

/* Port of Python gemini_native_adapter.py:probe_gemini_tier().
 * Probe a Google AI Studio API key and return its tier.
 * Returns one of: "free", "paid", "unknown".
 * "unknown" means the probe failed; callers should proceed without blocking.
 * Internal function — uses http library directly to make a probe request
 * to the Gemini generateContent endpoint.
 */
const char *google_probe_gemini_tier(const char *api_key,
                                      const char *base_url,
                                      const char *model,
                                      int timeout_sec) {
    if (!api_key || !*api_key) return "unknown";

    /* Normalize base_url */
    const char *default_url = "https://generativelanguage.googleapis.com/v1beta";
    const char *burl = (base_url && *base_url) ? base_url : default_url;

    /* Strip /openai suffix if present (Python does this) */
    size_t blen = strlen(burl);
    char norm_url[512];
    size_t npos = 0;
    {
        size_t cp = blen < sizeof(norm_url) - 1 ? blen : sizeof(norm_url) - 1;
        memcpy(norm_url, burl, cp);
        norm_url[cp] = '\0';
        /* Strip trailing slash */
        while (cp > 0 && norm_url[cp-1] == '/') { norm_url[cp-1] = '\0'; cp--; }
        /* Strip /openai suffix */
        if (cp >= 7 && strcasecmp(norm_url + cp - 7, "/openai") == 0) {
            norm_url[cp - 7] = '\0';
            cp -= 7;
        }
        npos = cp;
    }

    /* Model */
    const char *mod = (model && *model) ? model : "gemini-2.5-flash";

    /* Build URL: {base}/models/{model}:generateContent */
    char url[512];
    int url_len = snprintf(url, sizeof(url), "%.*s/models/%s:generateContent",
                           (int)npos, norm_url, mod);
    if (url_len <= 0 || url_len >= (int)sizeof(url)) return "unknown";

    /* Build simplified payload: {"contents":[{"role":"user","parts":[{"text":"hi"}]}],"generationConfig":{"maxOutputTokens":1}} */
    json_t *payload = json_new_object();
    if (!payload) return "unknown";
    json_t *contents = json_new_array();
    json_t *msg = json_new_object();
    json_object_set(msg, "role", json_new_string("user"));
    json_t *parts = json_new_array();
    json_t *part = json_new_object();
    json_object_set(part, "text", json_new_string("hi"));
    json_array_append(parts, part);
    json_object_set(msg, "parts", parts);
    json_array_append(contents, msg);
    json_object_set(payload, "contents", contents);
    json_t *gc = json_new_object();
    json_object_set(gc, "maxOutputTokens", json_new_number(1));
    json_object_set(payload, "generationConfig", gc);
    char *payload_str = json_serialize(payload);
    json_free(payload);

    if (!payload_str) return "unknown";

    /* Create HTTP client with timeout */
    int to = (timeout_sec > 0) ? timeout_sec : 10;
    http_t *h = http_new(to);
    if (!h) { free(payload_str); return "unknown"; }

    /* POST with API key as query parameter (matching Python's params={"key": key}) */
    char url_with_key[768];
    snprintf(url_with_key, sizeof(url_with_key), "%s?key=%s", url, api_key);

    http_resp_t *resp = http_post_json(h, url_with_key, payload_str);
    free(payload_str);

    if (!resp) {
        http_free(h);
        return "unknown";
    }

    const char *result = "unknown";

    /* Check x-ratelimit-limit-requests-per-day header in response headers */
    if (resp->headers && *resp->headers) {
        const char *rl_key = "x-ratelimit-limit-requests-per-day";
        const char *hdr = resp->headers;
        size_t rl_len = strlen(rl_key);

        while (*hdr) {
            /* Skip leading whitespace/newlines */
            while (*hdr == '\r' || *hdr == '\n' || *hdr == ' ') hdr++;
            if (!*hdr) break;

            /* Check for header match (case-insensitive) */
            bool match = true;
            for (size_t i = 0; i < rl_len; i++) {
                if ((hdr[i] | 32) != (rl_key[i])) { match = false; break; }
            }
            if (match && hdr[rl_len] == ':') {
                const char *val = hdr + rl_len + 1;
                while (*val == ' ') val++;
                int rpd_val = 0;
                const char *vp = val;
                while (*vp >= '0' && *vp <= '9') { rpd_val = rpd_val * 10 + (*vp - '0'); vp++; }
                if (vp > val) {
                    result = (rpd_val <= 1000) ? "free" : "paid";
                }
                break;
            }

            const char *nl = strchr(hdr, '\n');
            if (nl) hdr = nl + 1;
            else break;
        }
    }

    /* If no rate limit header found, check status code */
    if (strcmp(result, "unknown") == 0) {
        if (resp->status == 429) {
            /* Check body for free_tier */
            if (resp->body) {
                size_t bl = strlen(resp->body);
                char *lower_body = (char *)malloc(bl + 1);
                if (lower_body) {
                    for (size_t i = 0; i < bl; i++)
                        lower_body[i] = (char)tolower((unsigned char)resp->body[i]);
                    lower_body[bl] = '\0';
                    if (strstr(lower_body, "free_tier"))
                        result = "free";
                    else
                        result = "paid";
                    free(lower_body);
                }
            }
        } else if (resp->status >= 200 && resp->status < 300) {
            result = "paid";
        }
    }

    /* Fix: result is "unknown" string literal if unset */
    if (!result) result = "unknown";

    http_resp_free(resp);
    http_free(h);
    return result;
}


/* Port of Python gemini_native_adapter.py:is_free_tier_quota_error().
 * Return True when a Gemini 429 message indicates free-tier exhaustion.
 * Public wrapper — callers outside provider_google.c use this instead of
 * the static is_free_tier_quota_error() above. */
bool google_is_free_tier_quota_error(const char *error_message) {
    if (!error_message) return false;
    const char *p = error_message;
    while (*p) {
        if ((*p == 'f' || *p == 'F') &&
            strncasecmp(p, "free_tier", 9) == 0)
            return true;
        p++;
    }
    return false;
}

/* Port of Python gemini_native_adapter.py:build_gemini_request().
 * Build a Gemini native API request JSON from OpenAI-style arguments.
 * Returns a JSON string (caller must free).
 * Args are passed as individual params, not a single JSON object,
 * matching Python's keyword-argument convention. */
char *google_build_gemini_request(const json_t *messages,
                                   const json_t *tools,
                                   const json_t *tool_choice,
                                   double temperature,
                                   int max_tokens,
                                   double top_p,
                                   const json_t *stop,
                                   const json_t *thinking_config) {
    json_t *request = json_new_object();
    if (!request) return NULL;

    /* Build contents via the shared helper */
    json_t *contents_result = google_build_gemini_contents(messages);
    if (contents_result) {
        /* Merge contents and systemInstruction from the helper result */
        json_t *c = json_obj_get(contents_result, "contents");
        if (c) json_object_set(request, "contents", json_copy(c));
        json_t *si = json_obj_get(contents_result, "systemInstruction");
        if (si) json_object_set(request, "systemInstruction", json_copy(si));
        json_free(contents_result);
    }

    /* Translate tools */
    json_t *gemini_tools = google_translate_tools_to_gemini(tools);
    if (gemini_tools && json_len(gemini_tools) > 0)
        json_object_set(request, "tools", gemini_tools);
    else
        json_free(gemini_tools);

    /* Translate tool_choice */
    json_t *tool_config = google_translate_tool_choice_to_gemini(tool_choice);
    if (tool_config)
        json_object_set(request, "toolConfig", tool_config);

    /* Generation config */
    json_t *gen_config = json_new_object();
    if (temperature >= 0.0)
        json_object_set(gen_config, "temperature", json_new_number(temperature));
    if (max_tokens > 0)
        json_object_set(gen_config, "maxOutputTokens", json_new_number((double)max_tokens));
    if (top_p > 0.0 && top_p < 1.0)
        json_object_set(gen_config, "topP", json_new_number(top_p));
    if (stop) {
        if (stop->type == JSON_ARRAY)
            json_object_set(gen_config, "stopSequences", json_copy(stop));
        else if (stop->type == JSON_STRING)
            json_object_set(gen_config, "stopSequences", json_new_string(stop->str_val));
    }

    /* Normalize thinking config */
    json_t *normalized = google_normalize_thinking_config(thinking_config);
    if (normalized)
        json_object_set(gen_config, "thinkingConfig", normalized);

    if (json_len(gen_config) > 0)
        json_object_set(request, "generationConfig", gen_config);
    else
        json_free(gen_config);

    char *body = json_serialize(request);
    json_free(request);
    return body;
}

/* Port of Python gemini_native_adapter.py:_map_gemini_finish_reason().
 * Map Google Gemini finishReason to OpenAI-compatible string.
 * Internal helper for translate_gemini_response and translate_stream_event. */
static const char *map_gemini_finish_reason(const char *reason) {
    if (!reason || !*reason) return "stop";
    /* Mapping from gemini_native_adapter.py _map_gemini_finish_reason */
    if (strcasecmp(reason, "STOP") == 0) return "stop";
    if (strcasecmp(reason, "MAX_TOKENS") == 0) return "length";
    if (strcasecmp(reason, "SAFETY") == 0) return "content_filter";
    if (strcasecmp(reason, "RECITATION") == 0) return "content_filter";
    return "stop";
}

/* Port of Python gemini_native_adapter.py:_empty_response().
 * Build an empty chat-completion response for the given model.
 * Returns JSON string (caller must free). */
char *google_empty_response(const char *model) {
    json_t *usage = json_new_object();
    json_object_set(usage, "prompt_tokens", json_new_number(0));
    json_object_set(usage, "completion_tokens", json_new_number(0));
    json_object_set(usage, "total_tokens", json_new_number(0));

    json_t *choice = json_new_object();
    json_object_set(choice, "index", json_new_number(0));
    json_object_set(choice, "finish_reason", json_new_string("stop"));
    json_object_set(choice, "content", json_new_string(""));

    json_t *root = json_new_object();
    json_object_set(root, "model", json_new_string(model ? model : "unknown"));
    json_object_set(root, "usage", usage);
    json_t *choices = json_new_array();
    json_array_append(choices, choice);
    json_object_set(root, "choices", choices);

    char *body = json_serialize(root);
    json_free(root);
    return body;
}

/* Port of Python agent/gemini_cloudcode_adapter.py:_translate_gemini_response(). */
/* Port of Python gemini_native_adapter.py:translate_gemini_response().
 * Translates a Gemini native API response dict into an OpenAI-compatible
 * chat completion response JSON string.
 * Returns malloc'd JSON string (caller must free). */
char *google_translate_gemini_response(const json_t *resp, const char *model) {
    if (!resp || resp->type != JSON_OBJECT)
        return google_empty_response(model);

    json_t *candidates = json_obj_get(resp, "candidates");
    if (!candidates || candidates->type != JSON_ARRAY || json_len(candidates) == 0)
        return google_empty_response(model);

    json_t *cand = json_get(candidates, 0);
    if (!cand || cand->type != JSON_OBJECT)
        return google_empty_response(model);

    json_t *content_obj = json_obj_get(cand, "content");
    json_t *parts = NULL;
    if (content_obj && content_obj->type == JSON_OBJECT)
        parts = json_obj_get(content_obj, "parts");

    /* Collect text, reasoning, and tool calls from parts */
    json_t *text_parts = json_new_array();
    json_t *tc_arr = json_new_array();
    json_t *reasoning_arr = json_new_array();
    int part_index = 0;

    if (parts && parts->type == JSON_ARRAY) {
        for (size_t i = 0; i < parts->c.count; i++) {
            json_t *part = parts->c.items[i];
            if (!part || part->type != JSON_OBJECT) continue;

            const char *text = json_get_str(part, "text", NULL);
            json_t *fc = json_obj_get(part, "functionCall");

            if (part->type == JSON_OBJECT) {
                json_t *thought_val = json_obj_get(part, "thought");
                bool is_thought = thought_val && thought_val->type == JSON_BOOL && thought_val->bool_val;
                if (is_thought && text)
                    json_array_append(reasoning_arr, json_new_string(text));
                else if (text)
                    json_array_append(text_parts, json_new_string(text));
            }

            if (fc && fc->type == JSON_OBJECT) {
                const char *fn_name = json_get_str(fc, "name", "");
                json_t *args = json_obj_get(fc, "args");
                char *args_str = args ? json_serialize(args) : strdup("{}");

                json_t *tc = json_new_object();
                json_object_set(tc, "name", json_new_string(fn_name));
                json_object_set(tc, "arguments", json_new_string(args_str ? args_str : "{}"));
                json_object_set(tc, "index", json_new_number((double)part_index));
                json_object_set(tc, "id", json_new_string(""));

                /* Extra content from thoughtSignature */
                json_t *extra = google_tool_call_extra_from_part(part);
                if (extra)
                    json_object_set(tc, "extra_content", extra);

                json_array_append(tc_arr, tc);
                free(args_str);
                part_index++;
            }
        }
    }

    /* Build finish_reason */
    const char *finish_reason = "stop";
    const char *fr_raw = json_get_str(cand, "finishReason", NULL);
    if (tc_arr->c.count > 0)
        finish_reason = "tool_calls";
    else if (fr_raw)
        finish_reason = map_gemini_finish_reason(fr_raw);

    /* Build content string from text_parts */
    char *content_str = NULL;
    if (text_parts->c.count > 0) {
        size_t total = 0;
        for (size_t i = 0; i < text_parts->c.count; i++) {
            json_t *tp = text_parts->c.items[i];
            if (tp && tp->type == JSON_STRING && tp->str_val)
                total += strlen(tp->str_val);
        }
        total += (text_parts->c.count > 0 ? text_parts->c.count - 1 : 0) + 1;
        content_str = (char *)calloc(1, total);
        if (content_str) {
            for (size_t i = 0; i < text_parts->c.count; i++) {
                json_t *tp = text_parts->c.items[i];
                if (tp && tp->type == JSON_STRING && tp->str_val) {
                    if (i > 0) strcat(content_str, "\n");
                    strcat(content_str, tp->str_val);
                }
            }
        }
    }

    /* Reasoning string */
    char *reasoning_str = NULL;
    if (reasoning_arr->c.count > 0) {
        size_t total = 0;
        for (size_t i = 0; i < reasoning_arr->c.count; i++) {
            json_t *rp = reasoning_arr->c.items[i];
            if (rp && rp->type == JSON_STRING && rp->str_val)
                total += strlen(rp->str_val);
        }
        total += 1;
        reasoning_str = (char *)calloc(1, total);
        if (reasoning_str) {
            for (size_t i = 0; i < reasoning_arr->c.count; i++) {
                json_t *rp = reasoning_arr->c.items[i];
                if (rp && rp->type == JSON_STRING && rp->str_val)
                    strcat(reasoning_str, rp->str_val);
            }
        }
    }

    /* Usage metadata */
    json_t *usage_meta = json_obj_get(resp, "usageMetadata");
    int prompt_tokens = 0, completion_tokens = 0, total_tokens = 0, cached_tokens = 0;
    if (usage_meta && usage_meta->type == JSON_OBJECT) {
        prompt_tokens = (int)json_get_num(usage_meta, "promptTokenCount", 0);
        completion_tokens = (int)json_get_num(usage_meta, "candidatesTokenCount", 0);
        total_tokens = (int)json_get_num(usage_meta, "totalTokenCount", 0);
        cached_tokens = (int)json_get_num(usage_meta, "cachedContentTokenCount", 0);
    }

    json_t *result = json_new_object();
    json_object_set(result, "model", json_new_string(model ? model : "unknown"));
    json_object_set(result, "content", json_new_string(content_str ? content_str : ""));
    if (reasoning_str)
        json_object_set(result, "reasoning", json_new_string(reasoning_str));
    if (tc_arr->c.count > 0)
        json_object_set(result, "tool_calls", tc_arr);
    json_object_set(result, "finish_reason", json_new_string(finish_reason));
    json_object_set(result, "prompt_tokens", json_new_number((double)prompt_tokens));
    json_object_set(result, "completion_tokens", json_new_number((double)completion_tokens));
    json_object_set(result, "total_tokens", json_new_number((double)total_tokens));
    json_object_set(result, "cached_tokens", json_new_number((double)cached_tokens));

    char *body = json_serialize(result);
    json_free(result);
    free(content_str);
    free(reasoning_str);
    json_free(text_parts);
    json_free(tc_arr);
    json_free(reasoning_arr);
    return body;
}

/* Port of Python agent/gemini_cloudcode_adapter.py:_translate_stream_event(). */
/* Port of Python gemini_native_adapter.py:translate_stream_event().
 * Port of Python gemini_native_adapter.py:_make_stream_chunk() — chunk creation is inlined here.
 * Translate a Gemini SSE stream event into a list of OpenAI-compatible
 * stream chunk JSON strings (separated by newlines).
 * Caller must free the returned string.
 * Simpler C port: returns a single JSON array string with all chunks for this event. */
char *google_translate_stream_event(const json_t *event, const char *model) {
    if (!event || event->type != JSON_OBJECT)
        return strdup("[]");

    json_t *candidates = json_obj_get(event, "candidates");
    if (!candidates || candidates->type != JSON_ARRAY || json_len(candidates) == 0)
        return strdup("[]");

    json_t *cand = json_get(candidates, 0);
    if (!cand || cand->type != JSON_OBJECT)
        return strdup("[]");

    json_t *content_obj = json_obj_get(cand, "content");
    json_t *parts = NULL;
    if (content_obj && content_obj->type == JSON_OBJECT)
        parts = json_obj_get(content_obj, "parts");

    json_t *chunks = json_new_array();
    if (!chunks) return strdup("[]");

    if (parts && parts->type == JSON_ARRAY) {
        for (size_t i = 0; i < parts->c.count; i++) {
            json_t *part = parts->c.items[i];
            if (!part || part->type != JSON_OBJECT) continue;

            const char *text = json_get_str(part, "text", NULL);
            json_t *fc = json_obj_get(part, "functionCall");
            json_t *thought_val = json_obj_get(part, "thought");
            bool is_thought = thought_val && thought_val->type == JSON_BOOL && thought_val->bool_val;

            if (is_thought && text) {
                json_t *chunk = json_new_object();
                json_object_set(chunk, "content", json_new_string(""));
                json_object_set(chunk, "reasoning", json_new_string(text));
                json_object_set(chunk, "model", json_new_string(model ? model : ""));
                json_array_append(chunks, chunk);
            } else if (text) {
                json_t *chunk = json_new_object();
                json_object_set(chunk, "content", json_new_string(text));
                json_object_set(chunk, "model", json_new_string(model ? model : ""));
                json_array_append(chunks, chunk);
            }

            if (fc && fc->type == JSON_OBJECT) {
                const char *fn_name = json_get_str(fc, "name", "");
                json_t *args = json_obj_get(fc, "args");
                char *args_str = args ? json_serialize(args) : strdup("{}");

                json_t *chunk = json_new_object();
                json_object_set(chunk, "content", json_new_string(""));
                json_t *tc_delta = json_new_object();
                json_object_set(tc_delta, "index", json_new_number((double)i));
                json_object_set(tc_delta, "name", json_new_string(fn_name));
                json_object_set(tc_delta, "arguments", json_new_string(args_str ? args_str : "{}"));
                json_object_set(chunk, "tool_call_delta", tc_delta);
                json_object_set(chunk, "model", json_new_string(model ? model : ""));
                json_array_append(chunks, chunk);
                free(args_str);
            }
        }
    }

    /* Finish reason chunk */
    const char *fr_raw = json_get_str(cand, "finishReason", NULL);
    if (fr_raw && *fr_raw) {
        const char *mapped = map_gemini_finish_reason(fr_raw);
        json_t *finish_chunk = json_new_object();
        json_object_set(finish_chunk, "finish_reason", json_new_string(mapped));
        json_object_set(finish_chunk, "model", json_new_string(model ? model : ""));
        /* Add usage from this event if available */
        json_t *usage_meta = json_obj_get(event, "usageMetadata");
        if (usage_meta && usage_meta->type == JSON_OBJECT) {
            json_t *usage = json_new_object();
            json_object_set(usage, "prompt_tokens",
                json_new_number(json_get_num(usage_meta, "promptTokenCount", 0)));
            json_object_set(usage, "completion_tokens",
                json_new_number(json_get_num(usage_meta, "candidatesTokenCount", 0)));
            json_object_set(usage, "total_tokens",
                json_new_number(json_get_num(usage_meta, "totalTokenCount", 0)));
            json_object_set(usage, "cached_tokens",
                json_new_number(json_get_num(usage_meta, "cachedContentTokenCount", 0)));
            json_object_set(finish_chunk, "usage", usage);
        }
        json_array_append(chunks, finish_chunk);
    }

    char *body = json_serialize(chunks);
    json_free(chunks);
    return body;
}

/* ================================================================ */

const provider_ops_t PROVIDER_OPS_GOOGLE = {
    .build_url = google_build_url,
    .build_headers = google_build_headers,
    .build_request_body = google_build_request_body,
    .parse_response = google_parse_response,
    .parse_stream_chunk = google_parse_stream_chunk,
    .free_response = google_free_response,
    .name = "google"
};

/* Port of Python gemini_native_adapter.py:_iter_sse_events().
 * Parse a Gemini SSE response body into a JSON array of parsed events.
 * Each event is a JSON object parsed from a "data: {...}" line.
 * Returns json_t* array (caller must json_free), or NULL on failure. */
json_t *google_iter_sse_events(const char *response_body) {
    if (!response_body) return NULL;

    json_t *events = json_array();
    if (!events) return NULL;

    const char *p = response_body;
    while (*p) {
        /* Skip empty/whitespace lines */
        if (*p == '\n' || *p == '\r') { p++; continue; }

        /* Look for "data: " prefix */
        const char *data_prefix = "data: ";
        if (strncmp(p, data_prefix, strlen(data_prefix)) != 0) {
            p++;
            continue;
        }

        p += strlen(data_prefix);

        /* Find end of this SSE event (double newline or end of string) */
        const char *end = strstr(p, "\n\n");
        if (!end) {
            /* No more events — parse remaining as single line */
            end = p + strlen(p);
        }

        /* Try to parse the data as JSON */
        size_t len = (size_t)(end - p);
        if (len > 0) {
            char *json_str = malloc(len + 1);
            if (json_str) {
                memcpy(json_str, p, len);
                json_str[len] = '\0';
                json_t *parsed = json_parse(json_str, NULL);
                if (parsed) {
                    json_append(events, parsed);
                }
                free(json_str);
            }
        }

        p = end;
        /* Skip past the \n\n separator */
        if (*p == '\n') p++;
        if (*p == '\n') p++;
    }

    return events;
}

/* Port of Python agent/gemini_cloudcode_adapter.py:wrap_code_assist_request().
 * Build the Code Assist API request envelope wrapping an inner Gemini request. */
char *wrap_code_assist_request(const char *project_id, const char *model,
                                const char *inner_request_json,
                                const char *user_prompt_id) {
    json_t *envelope = json_object();
    if (!envelope) return NULL;
    json_set(envelope, "project", json_string(project_id ? project_id : ""));
    json_set(envelope, "model", json_string(model ? model : ""));
    json_set(envelope, "user_prompt_id", json_string(
        (user_prompt_id && user_prompt_id[0]) ? user_prompt_id : "auto-gen"));
    json_t *inner = NULL;
    if (inner_request_json) {
        inner = json_parse(inner_request_json, NULL);
    }
    json_set(envelope, "request", inner ? inner : json_object());
    char *result = json_serialize(envelope);
    json_free(envelope);
    return result;
}

/* ================================================================
 *  gemini_native_adapter.py gaps (rg=6 -> 0)
 *  Faithful port of GeminiNativeClient + module-level helpers.
 *  Reuses the existing google_* streaming primitives.
 * ================================================================ */

/* PoP: gemini_native_adapter_bare_gemini_model_id @ agent/gemini_native_adapter.py:bare_gemini_model_id */
char *gemini_native_adapter_bare_gemini_model_id(const char *model) {
    /* Strip Gemini's own provider prefix from an aggregator-style model id. */
    if (!model) return strdup("");
    char *name = strdup(model);
    if (!name) return strdup("");
    /* trim trailing/leading whitespace */
    char *s = name, *e = name + strlen(name);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n')) *--e = '\0';
    while (*s == ' ' || *s == '\t') s++;
    char *lowered = strdup(s);
    if (lowered) {
        for (char *p = lowered; *p; p++) *p = (char)tolower((unsigned char)*p);
        if (strncmp(lowered, "google/", 7) == 0) {
            memmove(s, s + 7, strlen(s + 7) + 1);
        } else if (strncmp(lowered, "gemini/", 7) == 0) {
            memmove(s, s + 7, strlen(s + 7) + 1);
        }
        free(lowered);
    }
    /* re-trim after strip; if empty, fall back to original name */
    while (*s == ' ') s++;
    char *out;
    if (*s == '\0') out = strdup(model);
    else out = strdup(s);
    free(name);
    return out;
}

/* PoP: handle @ gateway/platforms/yuanbao.py:handle */
/* Opaque native client handle (mirrors GeminiNativeClient: api_key, base_url,
 * _default_headers). _default_headers is currently empty in C (the Python
 * default is {}), so only api_key/base_url are material. */
typedef struct gemini_native_client {
    char *api_key;
    char *base_url;
} gemini_native_client_t;

/* PoP: gemini_native_adapter__enter @ agent/gemini_native_adapter.py:__enter__ */
gemini_native_client_t *gemini_native_adapter__enter(const char *api_key,
                                                      const char *base_url) {
    gemini_native_client_t *c = calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->api_key = strdup(api_key ? api_key : "");
    c->base_url = strdup(base_url ? base_url : "");
    return c; /* Python __enter__ returns self */
}

/* PoP: gemini_native_adapter__exit @ agent/gemini_native_adapter.py:__exit__ */
void gemini_native_adapter__exit(gemini_native_client_t *c) {
    /* Python __exit__ calls self.close(); the C client holds no open socket,
     * so this frees the handle (the real teardown). */
    if (!c) return;
    free(c->api_key);
    free(c->base_url);
    free(c);
}

/* PoP: gemini_native_adapter__headers @ agent/gemini_native_adapter.py:_headers */
char *gemini_native_adapter__headers(const gemini_native_client_t *c) {
    /* Mirrors Python: Content-Type/Accept/x-goog-api-key/User-Agent, then
     * update(self._default_headers) — which is empty here. Reuses the same
     * header construction as google_build_headers. */
    if (!c) return NULL;
    char *h = google_build_headers(NULL, c->api_key);
    if (!h) return NULL;
    /* Append the native User-Agent (Python sets "hermes-agent (gemini-native)"). */
    size_t n = strlen(h);
    char *out = realloc(h, n + 64);
    if (!out) return h;
    snprintf(out + n, 64, "\r\nUser-Agent: hermes-agent (gemini-native)");
    return out;
}

/* PoP: gemini_native_adapter__advance_stream_iterator @ agent/gemini_native_adapter.py:_advance_stream_iterator */
/* Advance through a json_t* array of SSE events. Sets *done=1 at end.
 * Returns the next event (borrowed, do not free) or NULL when done. */
json_t *gemini_native_adapter__advance_stream_iterator(json_t *events,
                                                        size_t *idx, int *done) {
    if (done) *done = 0;
    if (!events || events->type != JSON_ARRAY || !idx) return NULL;
    if (*idx >= (size_t)json_len(events)) {
        if (done) *done = 1;
        return NULL;
    }
    json_t *ev = json_get(events, (int)*idx);
    (*idx)++;
    if (!ev) {
        if (done) *done = 1;
    }
    return ev;
}

/* PoP: gemini_native_adapter__stream_completion @ agent/gemini_native_adapter.py:_stream_completion */
/* Perform the SSE streaming completion request and return a json_t* ARRAY of
 * translated chunk-JSON strings (one entry per streamed chunk), or NULL.
 * Mirrors Python: POST to {base_url}/models/{bare}:streamGenerateContent?alt=sse,
 * iter_sse_events(), then translate_stream_event() per event. */
json_t *gemini_native_adapter__stream_completion(const gemini_native_client_t *c,
                                                  const char *model,
                                                  const char *request_json,
                                                  int timeout_sec) {
    if (!c || !model || !request_json) return NULL;
    char *bare = gemini_native_adapter_bare_gemini_model_id(model);
    if (!bare) return NULL;

    char url[1024];
    snprintf(url, sizeof(url), "%s/models/%s:streamGenerateContent?alt=sse",
             c->base_url && *c->base_url ? c->base_url : "https://generativelanguage.googleapis.com/v1beta",
             bare);
    free(bare);

    char url_with_key[1152];
    snprintf(url_with_key, sizeof(url_with_key), "%s?key=%s", url,
             c->api_key && *c->api_key ? c->api_key : "");

    int to = (timeout_sec > 0) ? timeout_sec : 30;
    http_t *h = http_new(to);
    if (!h) return NULL;

    /* Streaming body: the Python request is already a Gemini request JSON. */
    http_resp_t *resp = http_post_json(h, url_with_key, request_json);
    if (!resp || !resp->body) {
        http_free(h);
        return NULL;
    }

    json_t *events = google_iter_sse_events(resp->body);
    json_t *chunks = json_array();
    if (events && chunks) {
        size_t idx = 0;
        int done = 0;
        while (!done) {
            json_t *ev = gemini_native_adapter__advance_stream_iterator(events, &idx, &done);
            if (!ev) break;
            char *chunk_str = google_translate_stream_event(ev, model);
            if (chunk_str) {
                json_append(chunks, json_new_string(chunk_str));
                free(chunk_str);
            }
        }
    }
    if (events) json_free(events);
    http_free(h);
    return chunks;
}
