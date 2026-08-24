/*
 * cookies.c — canonical C11 port of hermes_cli/dashboard_auth/cookies.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_dashboard_auth_cook_set_session_provider_cookie @ hermes_cli/dashboard_auth/cookies.py:set_session_provider_cookie */
int hermes_cli_dashboard_auth_cook_set_session_provider_cookie(const char *arg) {
    /* Python: set_cookie(provider routing hint); no-op when empty. Arg =
     * "provider\tname\tuse_https\tprefix". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    if (!t1 || !t1[1]) { printf("0\n"); return 0; }
    char cookie[128];
    size_t nlen = (size_t)(t1 - arg);
    if (nlen >= sizeof(cookie)) nlen = sizeof(cookie) - 1;
    memcpy(cookie, arg, nlen); cookie[nlen] = '\0';
    const char *name = t1 + 1;
    size_t nlen2 = t2 ? (size_t)(t2 - t1 - 1) : strlen(name);
    char cn[128];
    if (nlen2 >= sizeof(cn)) nlen2 = sizeof(cn) - 1;
    memcpy(cn, name, nlen2); cn[nlen2] = '\0';
    int https = t2 ? (t2[1] == '1') : 0;
    const char *prefix = t3 ? t3 + 1 : "";
    printf("set-cookie %s%s=%s%s\n", prefix, cn, cookie,
           https ? " (secure)" : "");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_cook_read_session_cookies @ hermes_cli/dashboard_auth/cookies.py:read_session_cookies */
int hermes_cli_dashboard_auth_cook_read_session_cookies(const char *arg) {
    /* Python: (at, rt) from cookie request helpers, either may be None.
     * Arg = "access\trefresh" tokens (tab-separated). */
    if (!arg || !*arg) { printf("\n\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("%s\n\n", arg); return 0; }
    printf("%.*s\n%s\n", (int)(tab - arg), arg, tab + 1);
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_cook_read_session_provider @ hermes_cli/dashboard_auth/cookies.py:read_session_provider */
int hermes_cli_dashboard_auth_cook_read_session_provider(const char *arg) {
    /* Python: _read_with_fallback(request, SESSION_PROVIDER_COOKIE) — the
     * provider routing hint associated with the session cookies. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_cook_read_pkce_cookie @ hermes_cli/dashboard_auth/cookies.py:read_pkce_cookie */
int hermes_cli_dashboard_auth_cook_read_pkce_cookie(const char *arg) {
    /* Python: _read_with_fallback(request, PKCE_COOKIE). Arg = cookie
     * value (empty = missing). */
    printf("%s\n", arg ? arg : "");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_cook_read_sso_attempt_cookie @ hermes_cli/dashboard_auth/cookies.py:read_sso_attempt_cookie */
int hermes_cli_dashboard_auth_cook_read_sso_attempt_cookie(const char *arg) {
    /* Python: _read_with_fallback(request, SSO_ATTEMPT_COOKIE) — return the
     * auto-SSO marker value if present (any variant), else None. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}
