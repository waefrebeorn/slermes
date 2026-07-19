/*
 * port_close_terminal_tool.c — Faithful C port of tools/close_terminal_tool.py.
 *
 * Closes the read-only terminal tab mirroring a background process in the
 * Hermes desktop GUI. Does NOT kill the process (use process(action='kill')).
 * Routes through the process registry's injected on_close sink (the desktop
 * gateway wires it to emit terminal.close). Gated on HERMES_DESKTOP so it
 * never appears outside the GUI — matching Python's check_fn.
 *
 * Self-contained: depends only on process_registry (for request_close_terminal)
 * and the registry (for tool registration). No god headers.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "registry.h"
#include "process_registry.h"
#include "port_close_terminal_tool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

/* PoP: check_close_terminal_requirements @ tools/close_terminal_tool.py:check_close_terminal_requirements */
bool check_close_terminal_requirements(void) {
    const char *v = getenv("HERMES_DESKTOP");
    if (!v || !*v) return false;
    char buf[8];
    snprintf(buf, sizeof(buf), "%s", v);
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    return strcmp(buf, "1") == 0 || strcmp(buf, "true") == 0 || strcmp(buf, "yes") == 0;
}

/* PoP: close_terminal_tool @ tools/close_terminal_tool.py:close_terminal_tool
 * Handler signature matches registry_register_ex: args_json is the tool's
 * argument object ({"process_id": "..."}), task_id is the active task. */
char *close_terminal_tool(const char *args_json, const char *task_id) {
    (void)task_id;
    const char *pid_raw = "";
    char *pid = NULL;            /* owned copy when non-empty */
    if (args_json && *args_json) {
        json_node_t *a = json_parse(args_json, NULL);
        if (a) {
            pid_raw = json_object_get_string(a, "process_id", "");
            if (pid_raw && *pid_raw) pid = strdup(pid_raw);
            json_free(a);
        }
    }
    if (!pid || !*pid) {
        free(pid);
        return strdup("{\"success\":false,\"error\":\"process_id is required (the background "
                      "process whose tab to close).\"}");
    }
    char *result = process_registry_request_close_terminal(pid);
    /* The registry returns {status:...}; normalize to the tool's {success:...}
     * envelope so callers get a consistent shape. */
    json_node_t *j = json_parse(result, NULL);
    if (!j) { free(pid); return result; } /* return raw on parse failure */
    const char *status = json_object_get_string(j, "status", "");
    char *out;
    if (strcmp(status, "ok") == 0) {
        const char *closed = json_object_get_string(j, "closed", "");
        const char *note = json_object_get_string(j, "note", "");
        size_t n = 96 + strlen(closed) + strlen(note);
        out = malloc(n);
        snprintf(out, n, "{\"success\":true,\"closed\":\"%s\",\"note\":\"%s\"}", closed, note);
    } else {
        const char *err = json_object_get_string(j, "error", "close failed");
        size_t n = 64 + strlen(err);
        out = malloc(n);
        snprintf(out, n, "{\"success\":false,\"error\":\"%s\"}", err);
    }
    json_free(j);
    free(result);
    free(pid);
    return out;
}

/* PoP: registry.register @ tools/close_terminal_tool.py:registry.register(name="close_terminal") */
void registry_init_close_terminal(void) {
    bool ok = registry_register_ex("close_terminal",
        "Close the read-only terminal tab for one of your background processes in the Hermes "
        "desktop GUI (the tabs mirroring terminal(background=true) runs). This does NOT kill the "
        "process — it only drops the tab/view; the output keeps buffering and the user can reopen "
        "it from the status stack. Use it to tidy up when a background process's live terminal is "
        "no longer worth showing. To actually stop the process, use process(action='kill') instead.",
        "{"
        "\"type\":\"object\","
        "\"properties\":{"
          "\"process_id\":{\"type\":\"string\",\"description\":\"The background process's session id (from terminal(background=true) output or process(action='list')) whose tab should be closed.\"}"
        "},"
        "\"required\":[\"process_id\"]"
        "}",
        "terminal",
        close_terminal_tool);
    (void)ok;
    /* Mirror Python's check_fn: only available in the desktop GUI. */
    registry_set_check_fn("close_terminal", check_close_terminal_requirements);
}
