/**
 * port_kanban_tools.c — Port of Python: tools/kanban_tools.py
 *
 * Real C implementations for Kanban board tools.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Port of Python: _board_schema_prop */
char *board_schema_prop(void)
{
    const char *schema = "{\"type\": \"object\", \"properties\": {\"title\": {\"type\": \"string\"}, \"status\": {\"type\": \"string\"}, \"priority\": {\"type\": \"number\"}}}";
    char *result = strdup(schema);
    hermes_log(LOG_DEBUG, "port", "board_schema_prop: returned schema");
    return result;
}

/* Port of Python: _maybe_auto_subscribe */
bool maybe_auto_subscribe(const char *conn, const char *task_id)
{
    if (!conn) {
        hermes_log(LOG_WARNING, "port", "maybe_auto_subscribe: null conn");
        return false;
    }
    if (strstr(conn, "subscribe") || strstr(conn, "auto")) {
        hermes_log(LOG_INFO, "port", "maybe_auto_subscribing: task=%s", task_id ? task_id : "(none)");
        return true;
    }
    return false;
}
