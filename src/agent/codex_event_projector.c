/*
 * codex_event_projector.c — Projects codex app-server events into Hermes messages.
 *
 * Converts Codex item/* notifications into OpenAI-shaped
 * {role, content, tool_calls, tool_call_id} entries for Hermes' messages list.
 *
 * Maps to Python agent/transports/codex_event_projector.py (312 lines).
 * Port of Python: codex_event_projector.py — CodexEventProjector class methods:
 *   new, free, reset, project (codex_projector_*), projection_free
 *
 * Port of Python: codex_runtime.py — _consume_codex_event_stream()
 *                 (event consumption in codex_projector_project)
 * Port of Python: _event_field — inline in codex_projector_project via json_get_str/json_obj_get
 * Port of Python: _raise_stream_error — inline in codex_projector_project error-early-return pattern
 */

#include "codex_event_projector.h"
#include "hermes_core_types.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_REASONING 64
#define MAX_MESSAGES 32

struct codex_projector_t {
    /* Pendingreasoning fragments */
    char *reasoning[MAX_REASONING];
    int   reasoning_count;
};

codex_projector_t *codex_projector_new(void) {
    return (codex_projector_t *)calloc(1, sizeof(codex_projector_t));
}

void codex_projector_free(codex_projector_t *p) {
    if (!p) return;
    for (int i = 0; i < p->reasoning_count; i++) {
        free(p->reasoning[i]);
    }
    free(p);
}

void codex_projector_reset(codex_projector_t *p) {
    if (!p) return;
    for (int i = 0; i < p->reasoning_count; i++) {
        free(p->reasoning[i]);
    }
    p->reasoning_count = 0;
}

static codex_projection_t *projection_new(void) {
    codex_projection_t *proj = (codex_projection_t *)calloc(1, sizeof(codex_projection_t));
    if (!proj) return NULL;
    proj->msg_capacity = MAX_MESSAGES;
    proj->messages = (json_node_t **)calloc((size_t)proj->msg_capacity, sizeof(json_node_t *));
    if (!proj->messages) { free(proj); return NULL; }
    return proj;
}

void codex_projection_free(codex_projection_t *proj) {
    if (!proj) return;
    for (int i = 0; i < proj->msg_count; i++) {
        json_free(proj->messages[i]);
    }
    free(proj->messages);
    free(proj->final_text);
    free(proj);
}

/* Helper: create a message object {"role": role, ...} */
static json_node_t *make_message(const char *role) {
    json_node_t *msg = json_object();
    if (!msg) return NULL;
    json_set(msg, "role", json_string(role));
    return msg;
}

/* Helper: add a message to projection */
static bool projection_add(codex_projection_t *proj, json_node_t *msg) {
    if (proj->msg_count >= proj->msg_capacity) {
        int new_cap = proj->msg_capacity * 2;
        json_node_t **new_msgs = (json_node_t **)realloc(proj->messages, (size_t)new_cap * sizeof(json_node_t *));
        if (!new_msgs) return false;
        proj->messages = new_msgs;
        proj->msg_capacity = new_cap;
    }
    proj->messages[proj->msg_count++] = msg;
    return true;
}

/* Helper: deterministic call ID */
static void make_call_id(char *buf, size_t bufsz, const char *item_type, const char *item_id) {
    if (item_id && item_id[0]) {
        snprintf(buf, bufsz, "codex_%s_%s", item_type, item_id);
    } else {
        /* Fallback: hash the type */
        unsigned long h = 5381;
        for (const char *s = item_type; *s; s++) {
            h = ((h << 5) + h) + (unsigned long)*s;
        }
        snprintf(buf, bufsz, "codex_%s_%016lx", item_type, h);
    }
}

/* Helper: format tool args as JSON string */
/* PoP: format_tool_args @ agent/transports/codex_event_projector.py:_format_tool_args */
static char *format_tool_args(json_node_t *args) {
    if (!args) return strdup("{}");
    return json_serialize(args);
}

codex_projection_t *codex_projector_project(codex_projector_t *p, const char *notification_json) {
    if (!p || !notification_json) return NULL;

    codex_projection_t *result = projection_new();
    if (!result) return NULL;

    json_node_t *notif = json_parse(notification_json, NULL);
    if (!notif) {
        return result; /* empty projection */
    }

    /* Only process item/completed notifications */
    const char *method = json_get_str(notif, "method", "");
    if (strcmp(method, "item/completed") != 0) {
        json_free(notif);
        return result;
    }

    json_node_t *params = json_obj_get(notif, "params");
    if (!params) { json_free(notif); return result; }

    json_node_t *item = json_obj_get(params, "item");
    if (!item) { json_free(notif); return result; }

    const char *item_type = json_get_str(item, "type", "");
    const char *item_id = json_get_str(item, "id", "");

    if (strcmp(item_type, "agentMessage") == 0) {
        /* → assistant message */
        json_node_t *msg = make_message("assistant");
        if (msg) {
            const char *text = json_get_str(item, "text", "");
            json_set(msg, "content", json_string(text));
            if (p->reasoning_count > 0) {
                /* Join pending reasoning */
                size_t total = 0;
                for (int i = 0; i < p->reasoning_count; i++) total += strlen(p->reasoning[i]) + 1;
                char *joined = (char *)malloc(total + 1);
                if (joined) {
                    joined[0] = '\0';
                    for (int i = 0; i < p->reasoning_count; i++) {
                        if (i > 0) strcat(joined, "\n");
                        strcat(joined, p->reasoning[i]);
                    }
                    json_set(msg, "reasoning", json_string(joined));
                    free(joined);
                }
                /* Clear pending reasoning */
                for (int i = 0; i < p->reasoning_count; i++) free(p->reasoning[i]);
                p->reasoning_count = 0;
            }
            projection_add(result, msg);
            result->final_text = strdup(text);
        }
    } else if (strcmp(item_type, "reasoning") == 0) {
        /* Accumulate reasoning */
        json_node_t *summary = json_obj_get(item, "summary");
        if (summary) {
            size_t n = json_len(summary);
            for (size_t i = 0; i < n && p->reasoning_count < MAX_REASONING; i++) {
                json_node_t *s = json_get(summary, (int)i);
                if (s && s->type == JSON_STRING) {
                    p->reasoning[p->reasoning_count++] = strdup(s->str_val);
                }
            }
        }
        json_node_t *content = json_obj_get(item, "content");
        if (content) {
            size_t n = json_len(content);
            for (size_t i = 0; i < n && p->reasoning_count < MAX_REASONING; i++) {
                json_node_t *c = json_get(content, (int)i);
                if (c && c->type == JSON_STRING) {
                    p->reasoning[p->reasoning_count++] = strdup(c->str_val);
                }
            }
        }
    } else if (strcmp(item_type, "commandExecution") == 0) {
        /* → assistant tool_call(exec) + tool result */
        char call_id[256];
        make_call_id(call_id, sizeof(call_id), "exec", item_id);

        const char *command = json_get_str(item, "command", "");
        const char *cwd = json_get_str(item, "cwd", "");

        /* Build args */
        json_node_t *args = json_object();
        json_set(args, "command", json_string(command));
        json_set(args, "cwd", json_string(cwd));
        char *args_str = format_tool_args(args);
        json_free(args);

        /* tool_calls array */
        json_node_t *tc = json_object();
        json_set(tc, "id", json_string(call_id));
        json_set(tc, "type", json_string("function"));
        json_node_t *fn = json_object();
        json_set(fn, "name", json_string("exec_command"));
        json_set(fn, "arguments", json_string(args_str ? args_str : "{}"));
        json_set(tc, "function", fn);
        json_node_t *tc_arr = json_array();
        json_append(tc_arr, tc);

        json_node_t *msg = make_message("assistant");
        json_set(msg, "content", json_null());
        json_set(msg, "tool_calls", tc_arr);
        if (p->reasoning_count > 0) {
            size_t total = 0;
            for (int i = 0; i < p->reasoning_count; i++) total += strlen(p->reasoning[i]) + 1;
            char *joined = (char *)malloc(total + 1);
            if (joined) {
                joined[0] = '\0';
                for (int i = 0; i < p->reasoning_count; i++) {
                    if (i > 0) strcat(joined, "\n");
                    strcat(joined, p->reasoning[i]);
                }
                json_set(msg, "reasoning", json_string(joined));
                free(joined);
            }
            for (int i = 0; i < p->reasoning_count; i++) free(p->reasoning[i]);
            p->reasoning_count = 0;
        }
        projection_add(result, msg);

        /* Tool result */
        const char *output = json_get_str(item, "aggregatedOutput", "");
        int exit_code = (int)json_get_num(item, "exitCode", -999);
        char content_buf[8192];
        if (exit_code != -999 && exit_code != 0) {
            snprintf(content_buf, sizeof(content_buf), "[exit %d]\n%s", exit_code, output);
        } else {
            strncpy(content_buf, output, sizeof(content_buf) - 1);
            content_buf[sizeof(content_buf) - 1] = '\0';
        }
        json_node_t *tool_msg = make_message("tool");
        json_set(tool_msg, "tool_call_id", json_string(call_id));
        json_set(tool_msg, "content", json_string(content_buf));
        projection_add(result, tool_msg);

        result->is_tool_iteration = true;
        free(args_str);
    } else if (strcmp(item_type, "fileChange") == 0) {
        /* → assistant tool_call(apply_patch) + tool result */
        char call_id[256];
        make_call_id(call_id, sizeof(call_id), "apply_patch", item_id);

        /* Build changes summary */
        json_node_t *changes = json_obj_get(item, "changes");
        json_node_t *changes_summary = json_array();
        if (changes) {
            size_t n = json_len(changes);
            for (size_t i = 0; i < n; i++) {
                json_node_t *ch = json_get(changes, (int)i);
                if (!ch) continue;
                json_node_t *entry = json_object();
                const char *kind = "update";
                json_node_t *kind_obj = json_obj_get(ch, "kind");
                if (kind_obj) {
                    json_node_t *kind_type = json_obj_get(kind_obj, "type");
                    if (kind_type && kind_type->type == JSON_STRING) kind = kind_type->str_val;
                }
                json_set(entry, "kind", json_string(kind));
                const char *path = json_get_str(ch, "path", "");
                json_set(entry, "path", json_string(path));
                json_append(changes_summary, entry);
            }
        }
        json_node_t *args = json_object();
        json_set(args, "changes", changes_summary);
        char *args_str = format_tool_args(args);
        json_free(args);

        json_node_t *tc = json_object();
        json_set(tc, "id", json_string(call_id));
        json_set(tc, "type", json_string("function"));
        json_node_t *fn = json_object();
        json_set(fn, "name", json_string("apply_patch"));
        json_set(fn, "arguments", json_string(args_str ? args_str : "{}"));
        json_set(tc, "function", fn);
        json_node_t *tc_arr = json_array();
        json_append(tc_arr, tc);

        json_node_t *msg = make_message("assistant");
        json_set(msg, "content", json_null());
        json_set(msg, "tool_calls", tc_arr);
        projection_add(result, msg);

        const char *status = json_get_str(item, "status", "unknown");
        int n_changes = (int)json_len(changes_summary);
        char content_buf[256];
        snprintf(content_buf, sizeof(content_buf), "apply_patch status=%s, %d change(s)", status, n_changes);
        json_node_t *tool_msg = make_message("tool");
        json_set(tool_msg, "tool_call_id", json_string(call_id));
        json_set(tool_msg, "content", json_string(content_buf));
        projection_add(result, tool_msg);

        result->is_tool_iteration = true;
        free(args_str);
    } else if (strcmp(item_type, "mcpToolCall") == 0) {
        /* → assistant tool_call(mcp.server.tool) + tool result */
        const char *server = json_get_str(item, "server", "mcp");
        const char *tool = json_get_str(item, "tool", "unknown");
        char call_id[256];
        char type_buf[128];
        snprintf(type_buf, sizeof(type_buf), "mcp_%s_%s", server, tool);
        make_call_id(call_id, sizeof(call_id), type_buf, item_id);

        json_node_t *arguments = json_obj_get(item, "arguments");
        json_node_t *args_obj;
        if (arguments && arguments->type == JSON_OBJECT) {
            args_obj = json_copy(arguments);
        } else {
            args_obj = json_object();
            if (arguments) {
                json_set(args_obj, "arguments", arguments);
            }
        }
        char *args_str = format_tool_args(args_obj);
        json_free(args_obj);

        json_node_t *tc = json_object();
        json_set(tc, "id", json_string(call_id));
        json_set(tc, "type", json_string("function"));
        json_node_t *fn = json_object();
        char fn_name[128];
        snprintf(fn_name, sizeof(fn_name), "mcp.%s.%s", server, tool);
        json_set(fn, "name", json_string(fn_name));
        json_set(fn, "arguments", json_string(args_str ? args_str : "{}"));
        json_set(tc, "function", fn);
        json_node_t *tc_arr = json_array();
        json_append(tc_arr, tc);

        json_node_t *msg = make_message("assistant");
        json_set(msg, "content", json_null());
        json_set(msg, "tool_calls", tc_arr);
        projection_add(result, msg);

        /* Result or error */
        json_node_t *err = json_obj_get(item, "error");
        json_node_t *res = json_obj_get(item, "result");
        char content_buf[4096];
        if (err) {
            char *err_str = json_serialize(err);
            snprintf(content_buf, sizeof(content_buf), "[error] %.1000s", err_str ? err_str : "unknown");
            free(err_str);
        } else if (res) {
            char *res_str = json_serialize(res);
            strncpy(content_buf, res_str ? res_str : "", sizeof(content_buf) - 1);
            content_buf[sizeof(content_buf) - 1] = '\0';
            free(res_str);
        } else {
            content_buf[0] = '\0';
        }
        json_node_t *tool_msg = make_message("tool");
        json_set(tool_msg, "tool_call_id", json_string(call_id));
        json_set(tool_msg, "content", json_string(content_buf));
        projection_add(result, tool_msg);

        result->is_tool_iteration = true;
        free(args_str);
    } else if (strcmp(item_type, "dynamicToolCall") == 0) {
        const char *tool = json_get_str(item, "tool", "unknown");
        char call_id[256];
        char type_buf[128];
        snprintf(type_buf, sizeof(type_buf), "dyn_%s", tool);
        make_call_id(call_id, sizeof(call_id), type_buf, item_id);

        json_node_t *arguments = json_obj_get(item, "arguments");
        json_node_t *args_obj;
        if (arguments && arguments->type == JSON_OBJECT) {
            args_obj = json_copy(arguments);
        } else {
            args_obj = json_object();
            if (arguments) {
                json_set(args_obj, "arguments", arguments);
            }
        }
        char *args_str = format_tool_args(args_obj);
        json_free(args_obj);

        json_node_t *tc = json_object();
        json_set(tc, "id", json_string(call_id));
        json_set(tc, "type", json_string("function"));
        json_node_t *fn = json_object();
        json_set(fn, "name", json_string(tool));
        json_set(fn, "arguments", json_string(args_str ? args_str : "{}"));
        json_set(tc, "function", fn);
        json_node_t *tc_arr = json_array();
        json_append(tc_arr, tc);

        json_node_t *msg = make_message("assistant");
        json_set(msg, "content", json_null());
        json_set(msg, "tool_calls", tc_arr);
        projection_add(result, msg);

        json_node_t *content_items = json_obj_get(item, "contentItems");
        char content_buf[4096];
        if (content_items) {
            char *ci_str = json_serialize(content_items);
            strncpy(content_buf, ci_str ? ci_str : "", sizeof(content_buf) - 1);
            free(ci_str);
        } else {
            bool success = json_get_bool(item, "success", false);
            snprintf(content_buf, sizeof(content_buf), "success=%s", success ? "true" : "false");
        }
        json_node_t *tool_msg = make_message("tool");
        json_set(tool_msg, "tool_call_id", json_string(call_id));
        json_set(tool_msg, "content", json_string(content_buf));
        projection_add(result, tool_msg);

        result->is_tool_iteration = true;
        free(args_str);
    } else if (strcmp(item_type, "userMessage") == 0) {
        /* Flatten user content to text */
        json_node_t *content = json_obj_get(item, "content");
        char text_buf[8192] = "";
        if (content) {
            size_t n = json_len(content);
            for (size_t i = 0; i < n; i++) {
                json_node_t *frag = json_get(content, (int)i);
                if (!frag) continue;
                const char *frag_type = json_get_str(frag, "type", "");
                if (strcmp(frag_type, "text") == 0) {
                    const char *t = json_get_str(frag, "text", "");
                    if (t && t[0]) {
                        if (text_buf[0]) strncat(text_buf, "\n\n", sizeof(text_buf) - strlen(text_buf) - 1);
                        strncat(text_buf, t, sizeof(text_buf) - strlen(text_buf) - 1);
                    }
                } else if (strlen(frag_type) == 0) {
                    /* Try direct string value */
                    if (frag->type == JSON_STRING) {
                        if (text_buf[0]) strncat(text_buf, "\n\n", sizeof(text_buf) - strlen(text_buf) - 1);
                        strncat(text_buf, frag->str_val, sizeof(text_buf) - strlen(text_buf) - 1);
                    }
                }
            }
        }
        json_node_t *msg = make_message("user");
        json_set(msg, "content", json_string(text_buf));
        projection_add(result, msg);
    } else {
        /* Unknown/opaque item types: record as assistant note */
        char *item_str = json_serialize(item);
        char note_buf[2048];
        snprintf(note_buf, sizeof(note_buf), "[codex %s] %.1500s", item_type, item_str ? item_str : "");
        free(item_str);
        json_node_t *msg = make_message("assistant");
        json_set(msg, "content", json_string(note_buf));
        projection_add(result, msg);
    }

    json_free(notif);
    return result;
}
