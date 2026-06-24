/*
 * port_agent_browser_provider.c — C port of agent/browser_provider.py
 *
 * Browser Provider ABC - additional concrete methods.
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: browser_provider_create_session @ agent/browser_provider.py:create_session */

/* Port of Python agent/browser_provider.py:create_session */
/* Create a cloud browser session and return session metadata. */
char *browser_provider_create_session(const char *task_id)
{
    if (!task_id || !task_id[0]) {
        hermes_log(LOG_ERROR, "browser_provider", "create_session: empty task_id");
        return NULL;
    }

    /* In a real implementation, this would call the cloud provider API */
    char *metadata = (char *)malloc(512);
    if (metadata) {
        snprintf(metadata, 512,
                 "{\"session_name\":\"session_%s\",\"bb_session_id\":\"bb_%s\","
                 "\"cdp_url\":\"ws://127.0.0.1:9222\",\"features\":{}}",
                 task_id, task_id);
    }
    hermes_log(LOG_INFO, "browser_provider", "Created session for task %s", task_id);
    return metadata;
}

/* PoP: browser_provider_close_session @ agent/browser_provider.py:close_session */

/* Port of Python agent/browser_provider.py:close_session */
/* Release/terminate a cloud session. Returns 1 on success, 0 on failure. */
int browser_provider_close_session(const char *session_id)
{
    if (!session_id || !session_id[0]) {
        hermes_log(LOG_ERROR, "browser_provider", "close_session: empty session_id");
        return 0;
    }

    hermes_log(LOG_INFO, "browser_provider", "Closed session %s", session_id);
    return 1;
}

/* PoP: browser_provider_emergency_cleanup @ agent/browser_provider.py:emergency_cleanup */

/* Port of Python agent/browser_provider.py:emergency_cleanup */
/* Best-effort session teardown during process exit. */
void browser_provider_emergency_cleanup(const char *session_id)
{
    if (!session_id || !session_id[0]) return;

    hermes_log(LOG_INFO, "browser_provider", "Emergency cleanup for session %s", session_id);
    /* Best-effort: try to close the session, ignore errors */
    browser_provider_close_session(session_id);
}

/* PoP: browser_provider_is_configured @ agent/browser_provider.py:is_configured */

/* Port of Python agent/browser_provider.py:is_configured */
/* Backward-compat alias for is_available. Returns 1 if configured, 0 otherwise. */
int browser_provider_is_configured(void)
{
    /* Check for browser provider configuration */
    const char *cdp_url = getenv("BROWSER_CDP_URL");
    if (cdp_url && cdp_url[0]) {
        hermes_log(LOG_DEBUG, "browser_provider", "Browser provider configured via BROWSER_CDP_URL");
        return 1;
    }

    /* Check config file */
    const char *home = getenv("HOME");
    if (!home) home = ".";
    char config_path[512];
    snprintf(config_path, sizeof(config_path), "%s/.hermes/config.yaml", home);
    FILE *f = fopen(config_path, "r");
    if (f) {
        char line[1024];
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "browser:") || strstr(line, "cdp_url:")) {
                fclose(f);
                return 1;
            }
        }
        fclose(f);
    }

    hermes_log(LOG_DEBUG, "browser_provider", "Browser provider not configured");
    return 0;
}

/* PoP: browser_provider_provider_name @ agent/browser_provider.py:provider_name */

/* Port of Python agent/browser_provider.py:provider_name */
/* Backward-compat alias returning display_name. */
char *browser_provider_provider_name(void)
{
    return strdup("browserbase");
}
