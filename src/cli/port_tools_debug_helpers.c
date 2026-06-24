/*
 * port_tools_debug_helpers.c — C port of tools/debug_helpers.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include "debug_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_debug_helpers_get_session_info @ tools/debug_helpers.py:get_session_info */

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
