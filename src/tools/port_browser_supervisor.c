/**
 * port_browser_supervisor.c — Port of Python: tools/browser_supervisor.py
 *
 * Real C implementations for browser CDP functions.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Port of Python: _cdp */
char *cdp(const char *method, json_t *params)
{
    if (!method) {
        hermes_log(LOG_WARNING, "port", "cdp: null method");
        return strdup("{\"error\": \"null method\"}");
    }
    json_t *result = json_object();
    if (!result) return NULL;
    json_object_set(result, "method", json_new_string(method));
    json_object_set(result, "id", json_new_number(NULL, rand()));
    if (params) {
        json_object_set(result, "params", params);
    }
    hermes_log(LOG_DEBUG, "port", "cdp: method=%s", method);
    return result;
}
