/*
 * codex_runtime.c — Port of Python agent/codex_runtime.py
 *
 * Python API -> C implementation mapping:
 *   run_codex_app_server_turn()         -> codex_session_create/run in codex_app_server_session.c
 *   _consume_codex_event_stream()       -> codex_session_handle_event in codex_app_server_session.c
 *   run_codex_stream()                  -> consolidated in codex_app_server_session.c
 *   run_codex_create_stream_fallback()  -> codex_app_server_session.c
 *
 * Codex app server runtime is implemented in codex_app_server_session.c.
 * This file is a name-parity wrapper.
 */

#include "hermes_agent.h"
#include "hermes_json.h"
#include <string.h>
#include <stdlib.h>

/* PoP: codex_note_to_tool_progress @ agent/codex_runtime.py:_codex_note_to_tool_progress */
char *codex_note_to_tool_progress(const char *note_json)
{
    if (!note_json || !*note_json) return NULL;

    json_t *note = json_parse(note_json, NULL);
    if (!note || note->type != JSON_OBJECT) {
        if (note) json_free(note);
        return NULL;
    }

    json_t *method = json_object_get(note, "method");
    if (!method || method->type != JSON_STRING ||
        strcmp(json_node_get_string(method), "item/started") != 0) {
        json_free(note);
        return NULL;
    }

    json_t *params = json_object_get(note, "params");
    if (!params || params->type != JSON_OBJECT) {
        json_free(note);
        return NULL;
    }

    json_t *item = json_object_get(params, "item");
    if (!item || item->type != JSON_OBJECT) {
        json_free(note);
        return NULL;
    }

    json_t *type_node = json_object_get(item, "type");
    if (!type_node || type_node->type != JSON_STRING) {
        json_free(note);
        return NULL;
    }

    const char *item_type = json_node_get_string(type_node);
    json_t *result = json_object();

    if (strcmp(item_type, "commandExecution") == 0) {
        json_t *cmd = json_object_get(item, "command");
        const char *command = (cmd && cmd->type == JSON_STRING) ? json_node_get_string(cmd) : "";
        json_set(result, "tool_name", json_string("exec_command"));
        json_set(result, "preview", json_string(command));
        json_t *args = json_object();
        json_set(args, "command", json_string(command));
        json_t *cwd = json_object_get(item, "cwd");
        if (cwd && cwd->type == JSON_STRING)
            json_set(args, "cwd", json_copy(cwd));
        json_set(result, "args", args);
    } else if (strcmp(item_type, "fileChange") == 0) {
        json_t *changes = json_object_get(item, "changes");
        json_set(result, "tool_name", json_string("apply_patch"));
        if (changes && changes->type == JSON_ARRAY) {
            /* Count elements */
            size_t n = 0;
            while (json_array_get(changes, n) != NULL) n++;

            char preview[512] = "file changes";
            if (n > 0) {
                size_t pos = 0;
                for (size_t i = 0; i < n && i < 3 && pos < 480; i++) {
                    json_t *change = json_array_get(changes, i);
                    json_t *path_node = change ? json_object_get(change, "path") : NULL;
                    if (path_node && path_node->type == JSON_STRING) {
                        const char *p = json_node_get_string(path_node);
                        if (i > 0) { preview[pos++] = ','; preview[pos++] = ' '; }
                        size_t plen = strlen(p);
                        if (pos + plen < 480) {
                            memcpy(preview + pos, p, plen);
                            pos += plen;
                        }
                    }
                }
                if (n > 3) {
                    int rem = snprintf(preview + pos, sizeof(preview) - pos, ", +%zu more", n - 3);
                    if (rem > 0) pos += (size_t)rem;
                }
                preview[pos] = '\0';
            }
            json_set(result, "preview", json_string(preview));
            json_set(result, "args", json_copy(changes));
        } else {
            json_set(result, "preview", json_string("file changes"));
            json_set(result, "args", json_object());
        }
    } else if (strcmp(item_type, "mcpToolCall") == 0) {
        json_t *tool_name_node = json_object_get(item, "toolName");
        const char *tool_name = (tool_name_node && tool_name_node->type == JSON_STRING)
            ? json_node_get_string(tool_name_node) : "mcp";
        json_set(result, "tool_name", json_string(tool_name));
        json_t *args_node = json_object_get(item, "args");
        json_set(result, "preview", json_string(tool_name));
        json_set(result, "args", args_node ? json_copy(args_node) : json_object());
    } else {
        json_free(result);
        json_free(note);
        return NULL;
    }

    char *out = json_serialize(result);
    json_free(result);
    json_free(note);
    return out;
}