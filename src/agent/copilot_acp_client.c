/*
 * copilot_acp_client.c — GitHub Copilot ACP client helpers.
 *
 * Port of Python agent/copilot_acp_client.py (686 lines).
 * 13 stateless functions + 3 classes (N/A for C: _ACPChatCompletions,
 * _ACPChatNamespace, CopilotACPClient — Python SDK wrappers).
 *
 * MIT License — WuBu Slermes Project
 */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <ctype.h>

/*
 * Port of Python: _is_gh_copilot_deprecation_message()
 * Port of Python: _resolve_command()
 * Port of Python: _resolve_args()
 * Port of Python: _resolve_home_dir()
 * Port of Python: _build_subprocess_env()
 * Port of Python: _jsonrpc_error()
 * Port of Python: _permission_denied()
 * Port of Python: _format_messages_as_prompt()
 * Port of Python: _render_message_content()
 * Port of Python: _extract_tool_calls_from_text()
 * Port of Python: _ensure_path_within_cwd()
 * Port of Python: _build_openai_tool_call()
 * Port of Python: _completion_to_stream_chunks()
 *
 * N/A: _ACPChatCompletions, _ACPChatNamespace, CopilotACPClient — Python SDK classes
 */

/* Port of Python: _is_gh_copilot_deprecation_message() */
bool copilot_is_deprecation_message(const char *stderr_text) {
    if (!stderr_text) return false;

    bool has_gh_copilot = false;
    const char *p = stderr_text;
    while (*p) {
        if ((*p == 'g' || *p == 'G') &&
            (*(p+1) == 'h' || *(p+1) == 'H') &&
            *(p+2) == '-' &&
            (*(p+3) == 'c' || *(p+3) == 'C')) {
            has_gh_copilot = true;
            break;
        }
        p++;
    }
    if (!has_gh_copilot) return false;

    return strstr(stderr_text, "has been deprecated") != NULL ||
           strstr(stderr_text, "no commands will be executed") != NULL;
}

/* AG26: Port of Python agent/copilot_acp_client.py:resolve_command(). */
const char *resolve_command(void) {
    const char *cmd = getenv("HERMES_COPILOT_ACP_COMMAND");
    if (cmd && cmd[0]) return cmd;
    cmd = getenv("COPILOT_CLI_PATH");
    if (cmd && cmd[0]) return cmd;
    return "copilot";
}

/* Port of Python: _resolve_args() */
const char *resolve_args(void) {
    const char *raw = getenv("HERMES_COPILOT_ACP_ARGS");
    if (raw && raw[0]) return raw;
    return "--acp --stdio";
}

/* Port of Python: _resolve_home_dir() */
const char *resolve_home_dir(void) {
    const char *home = getenv("HOME");
    if (home) return home;
    return "/root";
}

/* Port of Python: _build_subprocess_env() */
char *build_subprocess_env(void) {
    json_t *env = json_object();
    if (!env) return NULL;

    json_set(env, "HOME", json_string(resolve_home_dir()));

    const char *path = getenv("PATH");
    if (path) json_set(env, "PATH", json_string(path));

    const char *term = getenv("TERM");
    if (term) json_set(env, "TERM", json_string(term));

    char *result = json_serialize(env);
    json_free(env);
    return result;
}

/* Port of Python: _jsonrpc_error() */
char *jsonrpc_error(int id, int code, const char *message) {
    json_t *err = json_object();
    if (!err) return NULL;

    json_t *error_obj = json_object();
    if (error_obj) {
        json_set(error_obj, "code", json_number((double)code));
        if (message) json_set(error_obj, "message", json_string(message));
        json_set(err, "error", error_obj);
    }
    json_set(err, "id", json_number((double)id));
    json_set(err, "jsonrpc", json_string("2.0"));

    char *result = json_serialize(err);
    json_free(err);
    return result;
}

/* Port of Python: _permission_denied() */
char *permission_denied(int id) {
    json_t *resp = json_object();
    if (!resp) return NULL;

    json_t *result = json_object();
    if (result) {
        json_t *outcome = json_object();
        if (outcome) {
            json_set(outcome, "outcome", json_string("cancelled"));
            json_set(result, "outcome", outcome);
        }
        json_set(resp, "result", result);
    }
    json_set(resp, "id", json_number((double)id));
    json_set(resp, "jsonrpc", json_string("2.0"));

    char *out = json_serialize(resp);
    json_free(resp);
    return out;
}

/* Port of Python: _format_messages_as_prompt() */
char *copilot_format_messages_as_prompt(json_node_t *messages,
                                         const char *model,
                                         json_node_t *tools,
                                         const char *tool_choice) {
    json_t *sections = json_array();
    if (!sections) return NULL;

    /* Base instructions */
    json_append(sections, json_string(
        "You are being used as the active ACP agent backend for Hermes.\n"
        "Use ACP capabilities to complete tasks.\n"
        "IMPORTANT: If you take an action with a tool, you MUST output tool calls "
        "using <tool_call>{...}</tool_call> blocks with JSON exactly in OpenAI "
        "function-call shape.\n"
        "If no tool is needed, answer normally."
    ));

    if (model && model[0]) {
        char hint[512];
        snprintf(hint, sizeof(hint), "Hermes requested model hint: %s", model);
        json_append(sections, json_string(hint));
    }

    if (tools && json_len(tools) > 0) {
        json_t *tool_specs = json_array();
        if (tool_specs) {
            size_t tlen = json_len(tools);
            for (size_t i = 0; i < tlen; i++) {
                json_node_t *t = json_get(tools, i);
                if (!t) continue;
                json_node_t *fn = json_obj_get(t, "function");
                if (!fn) continue;
                json_t *spec = json_object();
                if (spec) {
                    const char *name = json_get_str(fn, "name", "");
                    const char *desc = json_get_str(fn, "description", "");
                    json_node_t *params = json_obj_get(fn, "parameters");
                    if (name[0]) json_set(spec, "name", json_string(name));
                    if (desc[0]) json_set(spec, "description", json_string(desc));
                    if (params) json_set(spec, "parameters", json_copy(params));
                    json_append(tool_specs, spec);
                }
            }
            if (json_len(tool_specs) > 0) {
                json_append(sections, json_string(
                    "Available tools (OpenAI function schema). "
                    "When using a tool, emit ONLY <tool_call>{...}</tool_call> "
                    "with one JSON object containing id/type/function{name,arguments}. "
                    "arguments must be a JSON string."));
                /* Add the tool specs JSON directly */
                char *specs_str = json_serialize(tool_specs);
                if (specs_str) {
                    json_append(sections, json_string(specs_str));
                    free(specs_str);
                }
            }
            json_free(tool_specs);
        }
    }

    if (tool_choice && tool_choice[0]) {
        char hint[1024];
        snprintf(hint, sizeof(hint), "Tool choice hint: %s", tool_choice);
        json_append(sections, json_string(hint));
    }

    /* Conversation transcript */
    if (messages && json_len(messages) > 0) {
        json_t *transcript = json_array();
        if (transcript) {
            size_t mlen = json_len(messages);
            for (size_t i = 0; i < mlen; i++) {
                json_node_t *msg = json_get(messages, i);
                if (!msg) continue;
                const char *role = json_get_str(msg, "role", "unknown");
                json_node_t *content = json_obj_get(msg, "content");

                const char *label = "Context";
                if (strcmp(role, "system") == 0) label = "System";
                else if (strcmp(role, "user") == 0) label = "User";
                else if (strcmp(role, "assistant") == 0) label = "Assistant";
                else if (strcmp(role, "tool") == 0) label = "Tool";

                /* Render content to string */
                char content_str[4096] = "";
                if (content) {
                    if (content->type == JSON_STRING) {
                        snprintf(content_str, sizeof(content_str), "%s",
                                 content->str_val ? content->str_val : "");
                    } else if (content->type == JSON_ARRAY) {
                        /* Extract text parts */
                        size_t clen = json_len(content);
                        for (size_t j = 0; j < clen; j++) {
                            json_node_t *part = json_get(content, j);
                            if (!part) continue;
                            const char *text = json_get_str(part, "text", "");
                            if (text[0]) {
                                size_t cur = strlen(content_str);
                                snprintf(content_str + cur, sizeof(content_str) - cur,
                                         "%s%s", cur > 0 ? "\n" : "", text);
                            }
                        }
                    }
                }

                if (content_str[0]) {
                    char entry[4608];
                    snprintf(entry, sizeof(entry), "%s:\n%s", label, content_str);
                    json_append(transcript, json_string(entry));
                }
            }
            if (json_len(transcript) > 0) {
                json_append(sections, json_string("Conversation transcript:"));
                size_t tlen = json_len(transcript);
                for (size_t i = 0; i < tlen; i++) {
                    json_node_t *entry = json_get(transcript, i);
                    if (entry && entry->type == JSON_STRING)
                        json_append(sections, json_copy(entry));
                }
            }
            json_free(transcript);
        }
    }

    json_append(sections, json_string(
        "Continue the conversation from the latest user request."
    ));

    /* Join all sections with double newlines */
    char result_buf[65536];
    size_t pos = 0;
    size_t slen = json_len(sections);
    for (size_t i = 0; i < slen; i++) {
        json_node_t *sec = json_get(sections, i);
        if (!sec) continue;
        /* Serialize to get text representation */
        char *sec_str = json_serialize(sec);
        if (!sec_str) continue;
        size_t tlen = strlen(sec_str);
        /* Strip surrounding quotes if it's a JSON string */
        const char *text = sec_str;
        if (sec->type == JSON_STRING) {
            if (tlen >= 2 && sec_str[0] == '"' && sec_str[tlen-1] == '"') {
                sec_str[tlen-1] = '\0';
                text = sec_str + 1;
                tlen = strlen(text);
            }
        }
        if (!text || !text[0]) { free(sec_str); continue; }
        if (pos + tlen + 3 >= sizeof(result_buf)) { free(sec_str); continue; }
        if (pos > 0) { result_buf[pos++] = '\n'; result_buf[pos++] = '\n'; }
        memcpy(result_buf + pos, text, tlen);
        pos += tlen;
        free(sec_str);
    }
    result_buf[pos] = '\0';

    json_free(sections);
    return strdup(result_buf);
}

/* Port of Python: _extract_tool_calls_from_text() */
json_node_t *copilot_extract_tool_calls(const char *text, char **cleaned_out) {
    json_t *calls = json_array();
    if (!calls) return NULL;

    if (cleaned_out) *cleaned_out = NULL;
    if (!text || !text[0]) {
        if (cleaned_out) *cleaned_out = strdup("");
        return calls;
    }

    typedef struct { size_t start; size_t end; } span_t;
    span_t spans[64];
    int span_count = 0;

    const char *p = text;
    while (*p && span_count < 64) {
        const char *open = strstr(p, "<tool_call>");
        if (!open) break;
        const char *close = strstr(open + 11, "</tool_call>");
        if (!close) break;

        size_t json_len_val = (size_t)(close - open - 11);
        if (json_len_val > 0 && json_len_val < 4096) {
            char json_buf[4096];
            memcpy(json_buf, open + 11, json_len_val);
            json_buf[json_len_val] = '\0';

            char *err = NULL;
            json_node_t *obj = json_parse(json_buf, &err);
            free(err);
            if (obj) {
                json_node_t *fn = json_obj_get(obj, "function");
                if (fn) {
                    const char *fn_name = json_get_str(fn, "name", "");
                    if (fn_name[0]) {
                        const char *fn_args = json_get_str(fn, "arguments", "");
                        if (!fn_args[0]) {
                            json_node_t *args_obj = json_obj_get(fn, "arguments");
                            if (args_obj) {
                                char *serialized = json_serialize(args_obj);
                                if (serialized) {
                                    json_set(fn, "arguments", json_string(serialized));
                                    free(serialized);
                                }
                            }
                        }
                        json_append(calls, obj);
                    } else {
                        json_free(obj);
                    }
                } else {
                    json_free(obj);
                }
            }
            spans[span_count].start = (size_t)(open - text);
            spans[span_count].end = (size_t)(close + 12 - text);
            span_count++;
        }
        p = close + 12;
    }

    if (cleaned_out) {
        size_t text_len = strlen(text);
        char *clean = (char *)malloc(text_len + 1);
        if (clean) {
            size_t wp = 0;
            size_t cp = 0;
            for (int s = 0; s < span_count && cp < text_len; s++) {
                while (cp < spans[s].start && cp < text_len)
                    clean[wp++] = text[cp++];
                cp = spans[s].end;
            }
            while (cp < text_len)
                clean[wp++] = text[cp++];
            clean[wp] = '\0';
            while (wp > 0 && clean[wp-1] == '\n') clean[--wp] = '\0';
            *cleaned_out = clean;
        } else {
            *cleaned_out = strdup("");
        }
    }

    return calls;
}

/* Port of Python: _ensure_path_within_cwd() */
bool ensure_path_within_cwd(const char *path, const char *cwd) {
    if (!path || !cwd) return false;

    size_t cwd_len = strlen(cwd);
    if (strncmp(path, cwd, cwd_len) != 0) return false;
    if (path[cwd_len] != '\0' && path[cwd_len] != '/') return false;
    if (strstr(path + cwd_len, "/../") ||
        strstr(path + cwd_len, "/..") ||
        strstr(path, "../")) {
        return false;
    }
    return true;
}

/* Port of Python: _build_openai_tool_call() — builds a ChatCompletionMessageToolCall
 * equivalent (OpenAI-compatible tool-call object) for downstream handling. Mirrors
 * ChatCompletionMessageToolCall(id=call_id, call_id=call_id,
 * response_item_id=None, type="function", function=Function(name, arguments)). */
json_t *copilot_build_openai_tool_call(const char *call_id,
                                        const char *name,
                                        const char *arguments)
{
    json_t *tc = json_object();
    if (!tc) return NULL;
    json_set(tc, "id", json_string(call_id ? call_id : ""));
    json_set(tc, "call_id", json_string(call_id ? call_id : ""));
    json_set(tc, "response_item_id", json_null());
    json_set(tc, "type", json_string("function"));
    json_t *fn = json_object();
    json_set(fn, "name", json_string(name ? name : ""));
    json_set(fn, "arguments", json_string(arguments ? arguments : ""));
    json_set(tc, "function", fn);
    return tc;
}

/* Port of Python: _completion_to_stream_chunks() — converts a one-shot ACP response
 * (a completion json_t mirroring SimpleNamespace: choices[0].message.{content,
 * tool_calls[], reasoning_content, reasoning}, model, usage) into OpenAI-style
 * stream chunks. Returns a 2-element json array: [data_chunk, usage_chunk].
 *   data_chunk  = {"choices":[{"index":0,"delta":{...},"finish_reason":...}],
 *                  "model":..., "usage":null}
 *   usage_chunk = {"choices":[], "model":..., "usage":<usage or null>} */
json_t *copilot_completion_to_stream_chunks(const json_t *completion)
{
    json_t *out = json_array();
    if (!out) return NULL;
    if (!completion) return out;

    const char *model = json_get_str(completion, "model", NULL);

    json_t *choices = json_obj_get(completion, "choices");
    json_t *msg = NULL;
    json_t *finish_reason = NULL;
    if (choices && json_is_array(choices) && json_array_size(choices) > 0) {
        json_t *choice0 = json_array_get(choices, 0);
        if (choice0) {
            msg = json_obj_get(choice0, "message");
            finish_reason = json_obj_get(choice0, "finish_reason");
        }
    }

    /* ---- data_chunk.delta ---- */
    json_t *delta = json_object();
    json_set(delta, "role", json_string("assistant"));
    const char *content = msg ? json_get_str(msg, "content", NULL) : NULL;
    json_set(delta, "content", content ? json_string(content) : json_null());

    /* tool_calls delta: list of {index,id,type,function:{name,arguments}} or null */
    json_t *py_tool_calls = msg ? json_obj_get(msg, "tool_calls") : NULL;
    if (py_tool_calls && json_is_array(py_tool_calls) && json_array_size(py_tool_calls) > 0) {
        json_t *deltas = json_array();
        for (size_t i = 0; i < json_array_size(py_tool_calls); i++) {
            json_t *tc = json_array_get(py_tool_calls, i);
            json_t *d = json_object();
            json_set(d, "index", json_number((double)i));
            /* json_copy yields an OWNED deep copy so json_set can take ownership
             * (json_obj_get only borrows, and double-transferring would double-free). */
            json_set(d, "id", tc ? json_copy(json_obj_get(tc, "id")) : json_null());
            const char *tctype = tc ? json_get_str(tc, "type", "function") : "function";
            json_set(d, "type", json_string(tctype));
            json_t *tcfn = tc ? json_obj_get(tc, "function") : NULL;
            json_t *dfn = json_object();
            json_set(dfn, "name", tcfn ? json_copy(json_obj_get(tcfn, "name")) : json_null());
            json_set(dfn, "arguments", tcfn ? json_copy(json_obj_get(tcfn, "arguments")) : json_null());
            json_set(d, "function", dfn);
            json_append(deltas, d);
        }
        json_set(delta, "tool_calls", deltas);
    } else {
        json_set(delta, "tool_calls", json_null());
    }

    const char *reasoning_content = msg ? json_get_str(msg, "reasoning_content", NULL) : NULL;
    json_set(delta, "reasoning_content",
             reasoning_content ? json_string(reasoning_content) : json_null());
    const char *reasoning = msg ? json_get_str(msg, "reasoning", NULL) : NULL;
    json_set(delta, "reasoning", reasoning ? json_string(reasoning) : json_null());

    json_t *data_choice = json_object();
    json_set(data_choice, "index", json_number(0.0));
    json_set(data_choice, "delta", delta);
    json_set(data_choice, "finish_reason",
             finish_reason ? json_copy(finish_reason) : json_null());

    json_t *data_chunk = json_object();
    json_t *data_choices = json_array();
    json_append(data_choices, data_choice);
    json_set(data_chunk, "choices", data_choices);
    json_set(data_chunk, "model", model ? json_string(model) : json_null());
    json_set(data_chunk, "usage", json_null());

    /* ---- usage_chunk ---- */
    json_t *usage_chunk = json_object();
    json_set(usage_chunk, "choices", json_array());
    json_set(usage_chunk, "model", model ? json_string(model) : json_null());
    json_set(usage_chunk, "usage", json_copy(json_obj_get(completion, "usage")));

    json_append(out, data_chunk);
    json_append(out, usage_chunk);
    return out;
}
