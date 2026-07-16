#ifndef HERMES_WEB_DASHBOARD_H
#define HERMES_WEB_DASHBOARD_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Shared session token (defined in web_dashboard.c, validated by the
 * web_server auth helpers). Mirrors Python web_server._SESSION_TOKEN. */
#define SESSION_TOKEN_LEN 256
extern char g_session_token[SESSION_TOKEN_LEN];

/* Returns true if the raw HTTP header block carries a valid dashboard
 * session token (X-Hermes-Session-Token or legacy Bearer). */
bool ws_has_valid_session_token(const char *headers);

/* Returns true iff the dashboard auth gate must be active for `host`
 * (loopback binds are trusted; non-loopback always requires auth). */
bool ws_should_require_auth(const char *host, bool allow_public);

/* True if the Host header targets the interface we bound to
 * (DNS-rebinding defence, GHSA-ppp5-vxwm-4cf7). */
bool ws_is_accepted_host(const char *host_header, const char *bound_host);

#ifdef __cplusplus
}
#endif

/**
 * Initialize the web dashboard server.
 * Reads config from env vars (DASHBOARD_HOST, DASHBOARD_PORT, HERMES_WEB_DIST).
 */
void dashboard_init(void);

/**
 * Start the web dashboard server in a background thread.
 * Returns true on success.
 */
bool dashboard_start(void);

/**
 * Stop the web dashboard server.
 */
void dashboard_stop(void);

/**
 * Check if the dashboard server is running.
 */
bool dashboard_is_running(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_WEB_DASHBOARD_H */
