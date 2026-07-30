/*
 * port_web_server_extra.c — Port of Python hermes_cli/web_server.py
 * NA_ASYNC: cron_fire_webhook
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Port of Python: cron_fire_webhook */
bool cron_fire_webhook(const char* webhook_url, json_t* payload)
{
    if (!webhook_url) return false;
    hermes_log(LOG_DEBUG, "port", "cron_fire_webhook: url=%s", webhook_url);

    /* Convert payload to JSON string */
    char* payload_str = payload ? json_serialize(payload) : NULL;
    if (!payload_str) {
        payload_str = strdup("{}");
    }

    /* Use curl to send the webhook */
    char cmd[8192];
    snprintf(cmd, sizeof(cmd),
             "curl -s -X POST -H 'Content-Type: application/json' "
             "-d '%s' '%s' 2>/dev/null",
             payload_str, webhook_url);

    free(payload_str);

    int ret = system(cmd);
    if (ret != 0) {
        hermes_log(LOG_WARNING, "port", "cron_fire_webhook: curl failed for %s", webhook_url);
        return false;
    }

    return true;
}
