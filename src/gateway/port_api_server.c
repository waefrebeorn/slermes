/**
 * port_api_server.c — Port of Python: gateway/platforms/api_server.py
 *
 * Real C implementations for API server handlers.
 */

#include "hermes_logger.h"
#include "hermes_gateway_webhook.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Port of Python: handle_cron_fire */
void handle_cron_fire(json_t *request)
{
    if (!request) {
        hermes_log(LOG_WARNING, "port", "handle_cron_fire: null request");
        return;
    }
    const char *job_id = json_node_get_string(json_object_get(request, "job_id"));
    hermes_log(LOG_INFO, "port", "handle_cron_fire: job=%s",
               job_id ? job_id : "(unknown)");

    if (job_id) {
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "hermes cron run '%s' 2>&1", job_id);
        FILE *fp = popen(cmd, "r");
        if (fp) {
            char line[1024];
            while (fgets(line, sizeof(line), fp)) {
                hermes_log(LOG_DEBUG, "port", "cron: %s", line);
            }
            pclose(fp);
        } else {
            hermes_log(LOG_ERROR, "port", "handle_cron_fire: failed to run job %s", job_id);
        }
    }
}

/* Port of Python: _handle_cron_fire */
void _handle_cron_fire(void *ctx)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "_handle_cron_fire: null context");
        return;
    }
    hermes_log(LOG_INFO, "port", "_handle_cron_fire: dispatching cron event");
    handle_cron_fire((json_t *)ctx);
}
