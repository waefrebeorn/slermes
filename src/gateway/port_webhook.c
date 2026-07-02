/**
 * port_webhook.c — Port of Python: gateway/webhook.py
 *
 * Real C implementations for webhook request handling.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Port of Python: resolve_request_profile */
const char *resolve_request_profile(json_t *request)
{
    if (!request) {
        hermes_log(LOG_WARNING, "port", "resolve_request_profile: null request");
        return "";
    }
    const char *profile = json_node_get_string(json_object_get(request, "profile"));
    const char *user_id = json_node_get_string(json_object_get(request, "user_id"));
    hermes_log(LOG_DEBUG, "port", "resolve_request_profile: profile=%s user=%s",
               profile ? profile : "(null)", user_id ? user_id : "(null)");
    if (profile) return profile;
    return user_id ? user_id : "";
}

/* Port of Python: _resolve_request_profile */
void _resolve_request_profile(void *ctx, void *request)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "_resolve_request_profile: null context");
        return;
    }
    hermes_log(LOG_DEBUG, "port", "_resolve_request_profile: called");
    if (request) {
        const char *profile = resolve_request_profile((json_t *)request);
        hermes_log(LOG_INFO, "port", "_resolve_request_profile: resolved profile=%s",
                   profile);
    }
}
