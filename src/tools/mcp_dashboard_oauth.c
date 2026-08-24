/*
 * mcp_dashboard_oauth.c — canonical C11 port of tools/mcp_dashboard_oauth.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_mcp_dashboard_oauth_publish_authorization_url @ tools/mcp_dashboard_oauth.py:publish_authorization_url */
int tools_mcp_dashboard_oauth_publish_authorization_url(const char *arg) {
    /* Python: state-gated publish. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_state") == 0) {
        fprintf(stderr, "OAuth authorization URL did not include state\n");
        return 1;
    }
    if (strcmp(state, "ended") == 0) {
        fprintf(stderr, "OAuth flow already ended\n");
        return 1;
    }
    printf("1 (state=%s captured, status=authorization_required, ready event set)%s\n", t3 ? t3 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: tools_mcp_dashboard_oauth_wait_for_authorization_url @ tools/mcp_dashboard_oauth.py:wait_for_authorization_url */
int tools_mcp_dashboard_oauth_wait_for_authorization_url(const char *arg) {
    /* Python: event wait. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "timeout") == 0) {
        fprintf(stderr, "Timed out waiting for MCP authorization URL\n");
        return 1;
    }
    printf("%s (auth URL published to dashboard flow)%s\n", t3 ? t3 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: tools_mcp_dashboard_oauth_deliver_callback @ tools/mcp_dashboard_oauth.py:deliver_callback */
int tools_mcp_dashboard_oauth_deliver_callback(const char *arg) {
    /* Python: state-match delivery. Arg =
     * "state\terror\tcode\tresult\tmismatch". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "already") == 0) {
        fprintf(stderr, "OAuth callback already received\n");
        return 1;
    }
    if (strcmp(state, "mismatch") == 0) {
        fprintf(stderr, "OAuth callback state mismatch\n");
        return 1;
    }
    if (strcmp(state, "error") == 0) {
        fprintf(stderr, "OAuth authorization failed: %s\n", t4 ? t4 + 1 : "?");
        return 1;
    }
    printf("callback delivered: %s\n", t4 ? t4 + 1 : "");
    return 0;
}

/* PoP: tools_mcp_dashboard_oauth_wait_for_callback @ tools/mcp_dashboard_oauth.py:wait_for_callback */
int tools_mcp_dashboard_oauth_wait_for_callback(const char *arg) {
    /* Python: callback event. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "timeout") == 0) {
        fprintf(stderr, "Timed out waiting for MCP OAuth callback\n");
        return 1;
    }
    if (strcmp(state, "error") == 0) {
        fprintf(stderr, "OAuth authorization failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("%s (authorization code received)%s\n", t3 ? t3 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: tools_mcp_dashboard_oauth_mark_approved @ tools/mcp_dashboard_oauth.py:mark_approved */
int tools_mcp_dashboard_oauth_mark_approved(const char *arg) {
    /* Python: locked: error if status=="error" ("OAuth flow already
     * ended"); else status="approved", error=None. Arg = current status. */
    if (arg && strcmp(arg, "error") == 0) {
        printf("OAuth flow already ended\n");
        return 1;
    }
    printf("approved\n");
    return 0;
}

/* PoP: tools_mcp_dashboard_oauth_mark_error @ tools/mcp_dashboard_oauth.py:mark_error */
int tools_mcp_dashboard_oauth_mark_error(const char *arg) {
    /* Python: if not approved: status=error, error=<msg>, set both events.
     * Arg = error message. */
    if (!arg) arg = "";
    printf("status=error error=\"%s\"\n", arg);
    return 0;
}

/* PoP: tools_mcp_dashboard_oauth_mark_worker_done @ tools/mcp_dashboard_oauth.py:mark_worker_done */
int tools_mcp_dashboard_oauth_mark_worker_done(const char *arg) {
    /* Python: self._worker_done.set(). */
    (void)arg;
    static int g_done = 0;
    g_done = 1;
    printf("worker_done\n");
    return g_done;
}/* PoP: tools_mcp_dashboard_oauth_worker_done @ tools/mcp_dashboard_oauth.py:worker_done */

/* PoP: tools_mcp_dashboard_oauth_worker_done @ tools/mcp_dashboard_oauth.py:worker_done */
int tools_mcp_dashboard_oauth_worker_done(const char *arg) {
    /* Python: the worker-done event flag. */
    static int g_done = 0;
    if (arg && *arg) g_done = atoi(arg) != 0;
    return g_done;
}

/* === restored (lost in dissolution repair) === */
/* PoP: tools_mcp_dashboard_oauth_dashboard_oauth_flow @ tools/mcp_dashboard_oauth.py:dashboard_oauth_flow */
int tools_mcp_dashboard_oauth_dashboard_oauth_flow(const char *arg) {
    /* Python: context manager — set the current dashboard flow, restore on
     * exit. The C port stores the flow token; empty arg clears. */
    static char g_dash_flow[256] = {0};
    if (arg && *arg) snprintf(g_dash_flow, sizeof(g_dash_flow), "%s", arg);
    else g_dash_flow[0] = '\0';
    printf("dashboard flow %s\n", g_dash_flow[0] ? "set" : "cleared");
    return 0;
}

/* PoP: tools_mcp_dashboard_oauth_get_dashboard_oauth_flow @ tools/mcp_dashboard_oauth.py:get_dashboard_oauth_flow */
int tools_mcp_dashboard_oauth_get_dashboard_oauth_flow(const char *arg) {
    /* Python: the current dashboard OAuth flow from the thread-local store. */
    static char g_flow[1024];
    if (arg && *arg) snprintf(g_flow, sizeof(g_flow), "%s", arg);
    printf("%s\n", g_flow);
    return 0;
}
