/*
 * hermes_tools_mcp_server.c — Hermes tools as MCP server over stdio.
 *
 * Exposes a curated subset of Hermes tools to the codex app-server via
 * stdio MCP. Codex registers it as a normal MCP server (per codex config).
 *
 * Maps to Python agent/transports/hermes_tools_mcp_server.py (233 lines).
 *
 * Protocol: newline-delimited JSON-RPC 2.0 over stdin/stdout.
 *   - initialize / initialized
 *   - tools/list
 *   - tools/call
 *   - ping
 *   - notifications/initialized
 */


/* PoP: MCP tools server (C infrastructure) */

#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MCP_MAX_LINE 131072

/* ================================================================
 *  Exposed tools — curated subset (matches Python EXPOSED_TOOLS)
 * ================================================================ */

static const char *EXPOSED_TOOLS[] = {
    "web_search",
    "web_extract",
    "browser_navigate",
    "browser_click",
    "browser_type",
    "browser_press",
    "browser_snapshot",
    "browser_scroll",
    "browser_back",
    "browser_get_images",
    "browser_console",
    "browser_vision",
    "vision_analyze",
    "image_generate",
    "skill_view",
    "skills_list",
    "text_to_speech",
    "kanban_complete",
    "kanban_block",
    "kanban_comment",
    "kanban_heartbeat",
    "kanban_show",
    "kanban_list",
    "kanban_create",
    "kanban_unblock",
    "kanban_link",
    NULL,
};

/* Check if a tool name is in the exposed set */
static bool is_exposed(const char *name) {
    if (!name) return false;
    for (int i = 0; EXPOSED_TOOLS[i]; i++) {
        if (strcmp(name, EXPOSED_TOOLS[i]) == 0) return true;
    }
    return false;
}

/* ================================================================
 *  MCP response helpers
 * ================================================================ */

static void send_response(const char *id_str, json_t *result) {
    json_t *resp = json_object();
    json_set(resp, "jsonrpc", json_string("2.0"));

    /* Parse id — may be string or number */
    if (id_str) {
        json_t *id_val = json_parse(id_str, NULL);
        if (id_val) {
            json_set(resp, "id", id_val);
        } else {
            json_set(resp, "id", json_string(id_str));
        }
    } else {
        json_set(resp, "id", json_null());
    }

    json_set(resp, "result", result);

    char *s = json_serialize(resp);
    if (s) {
        printf("%s\n", s);
        fflush(stdout);
        free(s);
    }
    json_free(resp);
}

static void send_error(const char *id_str, int code, const char *message) {
    json_t *resp = json_object();
    json_set(resp, "jsonrpc", json_string("2.0"));

    if (id_str) {
        json_t *id_val = json_parse(id_str, NULL);
        if (id_val) {
            json_set(resp, "id", id_val);
        } else {
            json_set(resp, "id", json_string(id_str));
        }
    } else {
        json_set(resp, "id", json_null());
    }

    json_t *err = json_object();
    json_set(err, "code", json_number((double)code));
    json_set(err, "message", json_string(message ? message : "Unknown error"));
    json_set(resp, "error", err);

    char *s = json_serialize(resp);
    if (s) {
        printf("%s\n", s);
        fflush(stdout);
        free(s);
    }
    json_free(resp);
}

/* Port of Python agent/memory_provider.py:initialize(). */
/* ================================================================
 *  MCP method handlers
 * ================================================================ */

static void handle_initialize(const char *id_str, json_t *params) {
    (void)params;

    json_t *caps = json_object();
    json_t *tools_cap = json_object();
    json_set(caps, "tools", tools_cap);

    json_t *server_info = json_object();
    json_set(server_info, "name", json_string("hermes-tools"));
    json_set(server_info, "version", json_string(HERMES_VERSION));

    json_t *result = json_object();
    json_set(result, "protocolVersion", json_string("2025-03-26"));
    json_set(result, "capabilities", caps);
    json_set(result, "serverInfo", server_info);

    send_response(id_str, result);
}

static void handle_tools_list(const char *id_str, json_t *params) {
    (void)params;

    json_t *tools = json_array();

    for (int i = 0; EXPOSED_TOOLS[i]; i++) {
        const char *name = EXPOSED_TOOLS[i];

        /* Get tool schema from registry */
        const char *schema = registry_get_schema(name);
        const char *description = NULL;

        /* Look up description from registry */
        tool_registry_t *reg = get_registry();
        if (reg) {
            for (size_t j = 0; j < reg->count; j++) {
                if (strcmp(reg->tools[j].name, name) == 0) {
                    description = reg->tools[j].description;
                    break;
                }
            }
        }

        json_t *tool = json_object();
        json_set(tool, "name", json_string(name));
        json_set(tool, "description",
                 json_string(description ? description : ""));

        if (schema && schema[0]) {
            json_t *schema_obj = json_parse(schema, NULL);
            if (schema_obj) {
                json_set(tool, "inputSchema", schema_obj);
            } else {
                json_t *empty = json_object();
                json_set(empty, "type", json_string("object"));
                json_set(empty, "properties", json_object());
                json_set(tool, "inputSchema", empty);
            }
        } else {
            json_t *empty = json_object();
            json_set(empty, "type", json_string("object"));
            json_set(empty, "properties", json_object());
            json_set(tool, "inputSchema", empty);
        }

        json_array_append(tools, tool);
    }

    json_t *result = json_object();
    json_set(result, "tools", tools);

    send_response(id_str, result);
}

static void handle_tools_call(const char *id_str, json_t *params) {
    if (!params) {
        send_error(id_str, -32602, "Missing params");
        return;
    }

    const char *tool_name = json_get_str(params, "name", "");
    if (!tool_name || !tool_name[0]) {
        send_error(id_str, -32602, "Missing tool name");
        return;
    }

    if (!is_exposed(tool_name)) {
        send_error(id_str, -32601, "Tool not found in hermes-tools");
        return;
    }

    /* Extract arguments */
    json_t *args = json_obj_get(params, "arguments");
    char *args_json = args ? json_serialize(args) : strdup("{}");

    /* Dispatch via Hermes tool registry */
    char *result_str = registry_dispatch(tool_name, args_json, "");
    free(args_json);

    if (!result_str) {
        send_error(id_str, -32603, "Tool dispatch returned NULL");
        return;
    }

    /* Build MCP tool result: {"content": [{"type": "text", "text": ...}]} */
    json_t *content_arr = json_array();
    json_t *content_item = json_object();
    json_set(content_item, "type", json_string("text"));
    json_set(content_item, "text", json_string(result_str));
    json_array_append(content_arr, content_item);

    json_t *result = json_object();
    json_set(result, "content", content_arr);

    send_response(id_str, result);
    free(result_str);
}

static void handle_ping(const char *id_str) {
    json_t *result = json_object();
    send_response(id_str, result);
}

/* ================================================================
 *  Main request dispatcher
 * ================================================================ */

static void handle_line(const char *line) {
    if (!line || !line[0]) return;

    json_t *req = json_parse(line, NULL);
    if (!req) {
        send_error(NULL, -32700, "Parse error: invalid JSON");
        return;
    }

    const char *method = json_get_str(req, "method", "");
    json_t *id_node = json_obj_get(req, "id");
    json_t *params = json_obj_get(req, "params");

    /* Serialize id for response */
    char *id_str = NULL;
    if (id_node) {
        id_str = json_serialize(id_node);
    }

    if (strcmp(method, "initialize") == 0) {
        handle_initialize(id_str, params);
    } else if (strcmp(method, "tools/list") == 0) {
        handle_tools_list(id_str, params);
    } else if (strcmp(method, "tools/call") == 0) {
        handle_tools_call(id_str, params);
    } else if (strcmp(method, "ping") == 0) {
        handle_ping(id_str);
    } else if (strcmp(method, "notifications/initialized") == 0) {
        /* No response for notifications */
    } else {
        send_error(id_str, -32601, "Method not found");
    }

    free(id_str);
    json_free(req);
}

/* ================================================================
 *  Entry point
 * ================================================================ */

int hermes_tools_mcp_server_main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    /* Quiet mode: keep Hermes' own banners off stdout (which is the MCP wire) */
    /* Note: HERMES_QUIET and HERMES_REDACT_SECRETS would be set via env */

    char *line = NULL;
    size_t line_cap = 0;
    ssize_t n;

    /* Use stdio line reading */
    while ((n = getline(&line, &line_cap, stdin)) != -1) {
        /* Strip trailing newline */
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) {
            line[--n] = '\0';
        }
        if (n > 0) {
            handle_line(line);
        }
    }

    free(line);
    return 0;
}
