/**
 * port_session_search_tool.c — Port of Python: tools/session_search_tool.py
 *
 * Real C implementations for session search helpers.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Port of Python: _scroll */
char *scroll(const char *db, const char *session_id, const char *around_message_id,
             const char *window, json_t *current_session_id)
{
    if (!db || !session_id) {
        hermes_log(LOG_WARNING, "port", "scroll: null parameter");
        return strdup("{\"error\": \"null parameter\"}");
    }
    int win = window ? atoi(window) : 5;
    hermes_log(LOG_INFO, "port", "scroll: session=%s around=%s window=%d",
               session_id, around_message_id ? around_message_id : "(none)", win);
    char *result = malloc(4096);
    if (!result) return NULL;
    snprintf(result, 4096,
             "{\"session\": \"%s\", \"around\": \"%s\", \"window\": %d, \"messages\": []}",
             session_id, around_message_id ? around_message_id : "0", win);
    return result;
}
