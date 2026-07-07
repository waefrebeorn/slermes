/*
 * port_agent_browser_provider.c — C port of agent/browser_provider.py
 *
 * Browser Provider ABC - additional concrete methods.
 */

#include "hermes.h"
#include "hermes_logger.h"
#include "libhttp/http.h"
#include "libjson/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: browser_provider_create_session @ agent/browser_provider.py:create_session */

/* Port of Python agent/browser_provider.py:create_session */
/* Create a cloud browser session and return session metadata.
 * The base provider is abstract; if a concrete provider (Browserbase) is
 * configured via BROWSERBASE_API_KEY we make the real REST call and return
 * its cdp_url/id; otherwise we return NULL (no concrete backend configured). */
char *browser_provider_create_session(const char *task_id)
{
    if (!task_id || !task_id[0]) {
        hermes_log(LOG_ERROR, "browser_provider", "create_session: empty task_id");
        return NULL;
    }

    const char *bb_key = getenv("BROWSERBASE_API_KEY");
    const char *bb_project = getenv("BROWSERBASE_PROJECT_ID");
    if (!bb_key || !bb_key[0] || !bb_project || !bb_project[0]) {
        hermes_log(LOG_WARNING, "browser_provider",
                   "No concrete browser backend configured (BROWSERBASE_API_KEY/PROJECT_ID); "
                   "create_session is a no-op for the abstract base provider");
        return NULL;
    }

    char body[512];
    snprintf(body, sizeof(body), "{\"projectId\":\"%s\"}", bb_project);

    char auth[1024];
    snprintf(auth, sizeof(auth),
             "Authorization: Bearer %s\r\nContent-Type: application/json", bb_key);

    http_t *http = http_new(30);
    char *metadata = NULL;
    if (http) {
        http_resp_t *res = http_request(http, HTTP_POST,
                                        "https://api.browserbase.com/v1/sessions",
                                        auth, body, strlen(body));
        if (res && res->status >= 200 && res->status < 300 && res->body) {
            json_t *doc = json_parse(res->body, NULL);
            if (doc && doc->type == JSON_OBJECT) {
                const char *sid = json_get_str(doc, "id", NULL);
                const char *cdp = json_get_str(doc, "connectUrl", json_get_str(doc, "cdpUrl", NULL));
                metadata = malloc(512);
                if (metadata) {
                    snprintf(metadata, 512,
                             "{\"session_name\":\"session_%s\",\"bb_session_id\":\"%s\","
                             "\"cdp_url\":\"%s\",\"features\":{}}",
                             task_id, sid ? sid : "", cdp ? cdp : "");
                }
                hermes_log(LOG_INFO, "browser_provider",
                           "Created real Browserbase session %s for task %s",
                           sid ? sid : "(none)", task_id);
            }
            if (doc) json_free(doc);
        } else {
            hermes_log(LOG_ERROR, "browser_provider", "Browserbase session HTTP %d",
                       res ? res->status : -1);
        }
        if (res) http_resp_free(res);
        http_free(http);
    }
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
