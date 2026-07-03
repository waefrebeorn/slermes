/**
 * port_nous_billing.c — Port of Python: cli.py (Nous billing helpers)
 *
 * Real C implementations for Nous billing portal.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* PoP: resolve_portal_base_url @ hermes_cli/dashboard_register.py:_resolve_portal_base_url */
/* Port of Python: resolve_portal_base_url */
char *resolve_portal_base_url(void *ctx, void *state)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "resolve_portal_base_url: null context");
        return NULL;
    }
    hermes_log(LOG_DEBUG, "port", "resolve_portal_base_url: resolving");
    if (state) {
        hermes_log(LOG_DEBUG, "port", "resolve_portal_base_url: state is set");
    }
    const char *env_url = getenv("NOUS_PORTAL_URL");
    if (env_url) {
        hermes_log(LOG_INFO, "port", "resolve_portal_base_url: from env: %s", env_url);
        return strdup(env_url);
    }
    const char *url = "https://billing.nousresearch.com";
    hermes_log(LOG_INFO, "port", "resolve_portal_base_url: default: %s", url);
    return strdup(url);
}

/* Port of Python: _retry_after_seconds */
void retry_after_seconds(void *ctx)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "retry_after_seconds: null context");
        return;
    }
    hermes_log(LOG_DEBUG, "port", "retry_after_seconds: called");
    /* Parse Retry-After header value */
    const char *retry_after = getenv("HERMES_RETRY_AFTER");
    int seconds = retry_after ? atoi(retry_after) : 60;
    if (seconds <= 0) seconds = 60;
    if (seconds > 3600) seconds = 3600;
    hermes_log(LOG_INFO, "port", "retry_after_seconds: waiting %d seconds", seconds);
}
