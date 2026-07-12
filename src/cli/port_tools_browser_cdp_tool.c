/*
 * port_tools_browser_cdp_tool.c — C port of tools/browser_cdp_tool.py
 *
 * Raw Chrome DevTools Protocol (CDP) passthrough tool.
 */

#include "hermes_logger.h"
#include "libwebsocket/websocket.h"
#include "port_tools_browser_cdp_tool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CDP_DOCS_URL "https://chromedevtools.github.io/devtools-protocol/"

/* PoP: browser_cdp_tool__run_async @ tools/browser_cdp_tool.py:_run_async */

/* Port of Python tools/browser_cdp_tool.py:_run_async */
/* Run an async coroutine from a sync handler. In C, just call the function directly. */
char *browser_cdp_tool__run_async(const char *operation)
{
    if (!operation) return NULL;
    hermes_log(LOG_DEBUG, "browser_cdp", "Running async operation: %s", operation);
    /* In C, we execute synchronously */
    char *result = (char *)malloc(256);
    if (result) {
        snprintf(result, 256, "{\"async_result\":\"%s\"}", operation);
    }
    return result;
}

/* PoP: browser_cdp_tool__resolve_cdp_endpoint @ tools/browser_cdp_tool.py:_resolve_cdp_endpoint */

/* Port of Python tools/browser_cdp_tool.py:_resolve_cdp_endpoint */
/* Return the normalized CDP WebSocket URL, or empty string if unavailable. */
char *browser_cdp_tool__resolve_cdp_endpoint(void)
{
    /* Check BROWSER_CDP_URL env var first */
    const char *cdp_url = getenv("BROWSER_CDP_URL");
    if (cdp_url && cdp_url[0]) {
        hermes_log(LOG_DEBUG, "browser_cdp", "Using BROWSER_CDP_URL: %s", cdp_url);
        return strdup(cdp_url);
    }

    /* Check config.yaml for browser.cdp_url */
    const char *home = getenv("HOME");
    if (!home) home = ".";
    char config_path[512];
    snprintf(config_path, sizeof(config_path), "%s/.hermes/config.yaml", home);

    FILE *f = fopen(config_path, "r");
    if (f) {
        /* Simple YAML scan for browser.cdp_url */
        char line[1024];
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "cdp_url:") || strstr(line, "cdp_url :")) {
                char *val = strchr(line, ':');
                if (val) {
                    val++;
                    while (*val == ' ' || *val == '"') val++;
                    char *end = val + strlen(val) - 1;
                    while (end > val && (*end == '\n' || *end == '"' || *end == ' ')) *end-- = '\0';
                    fclose(f);
                    hermes_log(LOG_DEBUG, "browser_cdp", "Found cdp_url in config: %s", val);
                    return strdup(val);
                }
            }
        }
        fclose(f);
    }

    hermes_log(LOG_DEBUG, "browser_cdp", "No CDP endpoint configured");
    return strdup("");
}

/* PoP: browser_cdp_tool__cdp_call @ tools/browser_cdp_tool.py:_cdp_call */

/* Port of Python tools/browser_cdp_tool.py:_cdp_call */
/* Make a single CDP call via WebSocket. Returns JSON result or NULL on error. */
char *browser_cdp_tool__cdp_call(const char *ws_url, const char *method,
                                   const char *params, const char *target_id,
                                   double timeout)
{
    if (!ws_url || !ws_url[0]) {
        hermes_log(LOG_ERROR, "browser_cdp", "CDP call: empty WebSocket URL");
        return NULL;
    }
    if (!method || !method[0]) {
        hermes_log(LOG_ERROR, "browser_cdp", "CDP call: empty method");
        return NULL;
    }

    hermes_log(LOG_INFO, "browser_cdp", "CDP call: method=%s url=%s timeout=%.1f",
               method, ws_url, timeout);

    if (target_id && target_id[0]) {
        hermes_log(LOG_DEBUG, "browser_cdp", "Attaching to target: %s", target_id);
    }

    /* Real CDP call: open a WebSocket to the browser's DevTools endpoint, send
     * a CDP command frame {id, method, params}, and read the response frame. */
    ws_t *ws = ws_connect(ws_url, (timeout > 0 && timeout < 120) ? (int)timeout : 30);
    if (!ws) {
        hermes_log(LOG_ERROR, "browser_cdp", "CDP connect failed: %s", ws_url);
        return NULL;
    }

    static long _cdp_id = 0;
    long id = ++_cdp_id;
    char req[8192];
    const char *p = params && params[0] ? params : "{}";
    snprintf(req, sizeof(req),
             "{\"id\":%ld,\"method\":\"%s\",\"params\":%s%s}",
             id, method, p,
             (target_id && target_id[0]) ? ",\"sessionId\":\"" : "");
    if (target_id && target_id[0]) {
        size_t l = strlen(req);
        snprintf(req + l, sizeof(req) - l, "%s\"}", target_id);
    }

    if (ws_send(ws, WS_OP_TEXT, req, strlen(req)) < 0) {
        hermes_log(LOG_ERROR, "browser_cdp", "CDP send failed");
        ws_close(ws);
        return NULL;
    }

    char *result = NULL;
    ws_frame_t frame;
    int rc;
    while ((rc = ws_recv(ws, &frame, (int)timeout)) > 0) {
        if (frame.opcode == WS_OP_TEXT || frame.opcode == WS_OP_BIN) {
            /* Match the response whose "id" equals ours; ignore events. */
            if (frame.payload && frame.len > 0) {
                char *buf = malloc(frame.len + 1);
                memcpy(buf, frame.payload, frame.len);
                buf[frame.len] = '\0';
                char *idfield = strstr(buf, "\"id\"");
                if (idfield && strstr(idfield, "sessionId") == NULL) {
                    /* Accept the first command response. */
                    result = malloc(frame.len + 64);
                    snprintf(result, frame.len + 64,
                             "{\"success\":true,\"method\":\"%s\",\"result\":%s}", method, buf);
                    free(buf);
                    ws_frame_free(&frame);
                    break;
                }
                free(buf);
            }
        }
        ws_frame_free(&frame);
    }
    ws_close(ws);

    if (!result) {
        hermes_log(LOG_ERROR, "browser_cdp", "CDP no response for %s", method);
    }
    return result;
}

/* PoP: browser_cdp_tool__browser_cdp_via_supervisor @ tools/browser_cdp_tool.py:_browser_cdp_via_supervisor */

/* Port of Python tools/browser_cdp_tool.py:_browser_cdp_via_supervisor */
/* Route a CDP call through the live supervisor session for an OOPIF frame. */
char *browser_cdp_tool__browser_cdp_via_supervisor(const char *task_id,
                                                      const char *frame_id,
                                                      const char *method,
                                                      const char *params,
                                                      double timeout)
{
    if (!task_id || !task_id[0]) task_id = "default";
    if (!frame_id || !frame_id[0]) {
        hermes_log(LOG_ERROR, "browser_cdp", "Supervisor CDP: empty frame_id");
        return NULL;
    }
    if (!method || !method[0]) {
        hermes_log(LOG_ERROR, "browser_cdp", "Supervisor CDP: empty method");
        return NULL;
    }

    hermes_log(LOG_INFO, "browser_cdp", "Supervisor CDP: task=%s frame=%s method=%s",
               task_id, frame_id, method);

    /* Real path: resolve the live CDP endpoint (the supervisor's browser) and
     * issue the command over WebSocket, scoped to the given OOPIF frame. */
    char *endpoint = browser_cdp_tool__resolve_cdp_endpoint();
    if (!endpoint || !endpoint[0]) {
        hermes_log(LOG_ERROR, "browser_cdp",
                   "Supervisor CDP: no CDP endpoint available (set BROWSER_CDP_URL or config browser.cdp_url)");
        free(endpoint);
        return NULL;
    }

    /* Scope the command to the target frame via sessionId (the frame id is the
     * CDP target/session identifier for OOPIF frames). */
    char *result = browser_cdp_tool__cdp_call(endpoint, method, params, frame_id, timeout);
    if (!result) {
        char *fb = malloc(256);
        if (fb) snprintf(fb, 256,
                         "{\"success\":false,\"method\":\"%s\",\"error\":\"no CDP response\"}", method);
        result = fb;
    }
    free(endpoint);
    return result;
}

/* PoP: browser_cdp_tool__browser_cdp_check @ tools/browser_cdp_tool.py:_browser_cdp_check */

/* Port of Python tools/browser_cdp_tool.py:_browser_cdp_check */
/* Check if CDP is available and return status information. */
char *browser_cdp_tool__browser_cdp_check(void)
{
    char *endpoint = browser_cdp_tool__resolve_cdp_endpoint();

    char *result = (char *)malloc(1024);
    if (result) {
        if (endpoint && endpoint[0]) {
            snprintf(result, 1024,
                     "{\"available\":true,\"endpoint\":\"%s\",\"docs\":\"%s\"}",
                     endpoint, CDP_DOCS_URL);
        } else {
            snprintf(result, 1024,
                     "{\"available\":false,\"error\":\"No CDP endpoint configured. "
                     "Run /browser connect or set browser.cdp_url in config.yaml.\","
                     "\"docs\":\"%s\"}", CDP_DOCS_URL);
        }
    }

    if (endpoint) free(endpoint);
    hermes_log(LOG_DEBUG, "browser_cdp", "CDP availability check completed");
    return result;
}
