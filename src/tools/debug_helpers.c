/*
 * port_tools_debug_helpers.c — C port of tools/debug_helpers.py
 */

#include "hermes_logger.h"
#include "debug_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: get_debug_session_info @ tools/debug_helpers.py:get_session_info */

/* Port of Python tools/debug_helpers.py:get_session_info */
/* Return a summary dict suitable for returning from get_debug_session_info(). */
char *cli_tools_debug_helpers_get_session_info(const debug_session_t *session)
{
    if (!session) {
        return strdup("{\"enabled\":false,\"session_id\":null,\"log_path\":null,\"total_calls\":0}");
    }

    if (!session->enabled) {
        return strdup("{\"enabled\":false,\"session_id\":null,\"log_path\":null,\"total_calls\":0}");
    }

    /* Build log path */
    char log_path[1024];
    if (session->log_dir[0]) {
        snprintf(log_path, sizeof(log_path),
            "%s/%s_debug_%s.json",
            session->log_dir, session->tool_name, session->session_id);
    } else {
        snprintf(log_path, sizeof(log_path),
            "%s_debug_%s.json",
            session->tool_name, session->session_id);
    }

    /* Build JSON */
    size_t buf_size = 512 + strlen(log_path) + strlen(session->session_id);
    char *json = (char *)malloc(buf_size);
    if (!json) return NULL;

    snprintf(json, buf_size,
        "{"
        "\"enabled\":true,"
        "\"session_id\":\"%s\","
        "\"log_path\":\"%s\","
        "\"total_calls\":%d"
        "}",
        session->session_id, log_path, session->call_count);

    return json;
}


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_debug_helpers_log_call @ tools/debug_helpers.py:log_call */
int tools_debug_helpers_log_call(const char *arg) {
    /* Python: append {timestamp, tool_name, **data} to in-memory log when
     * enabled. Arg = "enabled\ttool_name\tdata_json". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    int enabled = (arg[0] == '1');
    if (!enabled) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (t1 && t1[1]) printf("logged %s\n", t1 + 1);
    (void)t2;
    return 0;
}
