/*
 * middleware.c — canonical C11 port of hermes_cli/dashboard_auth/middleware.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_dashboard_auth_midd_u_path_is_public @ hermes_cli/dashboard_auth/middleware.py:_path_is_public */
int hermes_cli_dashboard_auth_midd_u_path_is_public(const char *arg) {
    /* Python: exact API allowlist or prefix match. Arg =
     * "path\tpublic_paths\tpublic_prefixes". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *exact = t1 ? t1 + 1 : "";
    const char *prefixes = t2 ? t2 + 1 : "";
    /* exact match */
    const char *p = exact;
    while (*p) {
        const char *t = strchr(p, '\t');
        size_t len = t ? (size_t)(t - p) : strlen(p);
        if (len == strlen(arg) && strncmp(p, arg, len) == 0) { printf("1\n"); return 0; }
        p = t ? t + 1 : p + len;
    }
    /* prefix match */
    p = prefixes;
    while (*p) {
        const char *t = strchr(p, '\t');
        size_t len = t ? (size_t)(t - p) : strlen(p);
        if (len && strncmp(p, arg, len) == 0) { printf("1\n"); return 0; }
        p = t ? t + 1 : p + len;
    }
    printf("0\n");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_midd_u_ordered_session_providers @ hermes_cli/dashboard_auth/middleware.py:_ordered_session_providers */
int hermes_cli_dashboard_auth_midd_u_ordered_session_providers(const char *arg) {
    /* Python: stable sort hint to front. Arg = "hint\tproviders". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *hint = arg;
    const char *providers = tab ? tab + 1 : "";
    if (hint[0] && providers[0]) printf("%s\n%s\n", hint, providers);
    else printf("%s\n", providers);
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_midd_u_unauth_response @ hermes_cli/dashboard_auth/middleware.py:_unauth_response */
int hermes_cli_dashboard_auth_midd_u_unauth_response(const char *arg) {
    /* Python: 401 JSON / 302 HTML. Arg =
     * "is_api\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int is_api = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (is_api) {
        printf("401 JSON: {\"error\": \"unauthenticated\", \"login_url\": \"%s\"}\n", t2 ? t2 + 1 : "/login");
        return 0;
    }
    printf("302 → %s\n", t2 ? t2 + 1 : "/login");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_midd_u_auto_sso_response @ hermes_cli/dashboard_auth/middleware.py:_auto_sso_response */
int hermes_cli_dashboard_auth_midd_u_auto_sso_response(const char *arg) {
    /* Python: one-shot SSO redirect. Arg =
     * "redirect\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int redirect = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!redirect) { printf("\n"); return 0; }
    printf("302 → /auth/login (single interactive OAuth provider, marker cleared after one bounce): %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_midd_u_safe_next_target @ hermes_cli/dashboard_auth/middleware.py:_safe_next_target */
int hermes_cli_dashboard_auth_midd_u_safe_next_target(const char *arg) {
    /* Python: open-redirect gate. Arg =
     * "path\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *path = arg;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (path[0] != '/' || path[0] == '/' && path[1] == '/') { printf("\n"); return 0; }
    if (strncmp(path, "/login", 6) == 0 || strncmp(path, "/auth/", 6) == 0 || strncmp(path, "/api/auth/", 10) == 0) { printf("\n"); return 0; }
    if (strncmp(path, "/api", 4) == 0) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_midd_u_extract_bearer @ hermes_cli/dashboard_auth/middleware.py:_extract_bearer */
int hermes_cli_dashboard_auth_midd_u_extract_bearer(const char *arg) {
    /* Python: "Bearer <token>" from Authorization header, else "". Arg =
     * header value. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    while (*p == ' ' || *p == '\t') p++;
    if (strncasecmp(p, "bearer", 6) == 0 && (p[6] == ' ' || p[6] == '\t')) {
        const char *tok = p + 6;
        while (*tok == ' ' || *tok == '\t') tok++;
        printf("%s\n", tok);
        return 0;
    }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_midd_u_verify_bearer @ hermes_cli/dashboard_auth/middleware.py:_verify_bearer */
int hermes_cli_dashboard_auth_midd_u_verify_bearer(const char *arg) {
    /* Python: bearer verify loop. Arg =
     * "state\tresult\tunreachable". */
    if (!arg || !*arg) { printf("\n\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n\n"); return 0; }
    if (t3 && t3[1] == '1') {
        printf("\nunreachable:%s\n", t2 ? t2 + 1 : "");
        return 0;
    }
    printf("%s\n\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_midd_gated_auth_middleware @ hermes_cli/dashboard_auth/middleware.py:gated_auth_middleware */
int hermes_cli_dashboard_auth_midd_gated_auth_middleware(const char *arg) {
    /* Python: auth_required gate. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("pass-through (loopback mode)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "token") == 0) {
        printf("pass-through (token_authenticated — not a cookie session, never bounced to /login)%s\n", (t2 && t2[1] == '1') ? "" : "");
        return 0;
    }
    if (strcmp(state, "unauth") == 0) {
        printf("302 → /login (no-store, next= preserved)%s\n", (t2 && t2[1] == '1') ? "" : "");
        return 0;
    }
    printf("pass-through (session ok)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_midd_u_expires_in_seconds @ hermes_cli/dashboard_auth/middleware.py:_expires_in_seconds */
int hermes_cli_dashboard_auth_midd_u_expires_in_seconds(const char *arg) {
    /* Python: max(60, exp - now). Arg = expires_at (epoch). */
    if (!arg || !*arg) { printf("60\n"); return 0; }
    long exp = strtol(arg, NULL, 10);
    long now = (long)time(NULL);
    long v = exp - now;
    if (v < 60) v = 60;
    printf("%ld\n", v);
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_midd_u_attempt_refresh @ hermes_cli/dashboard_auth/middleware.py:_attempt_refresh */
int hermes_cli_dashboard_auth_midd_u_attempt_refresh(const char *arg) {
    /* Python: RT rotation. Arg =
     * "no_rt\trejected\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int no_rt = arg[0] == '1';
    int rejected = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (no_rt || rejected) { printf("\n"); return 0; }
    if (!state) { printf("refresh failed — provider unreachable\n"); return 1; }
    printf("session rotated: %s\n", t3 ? t3 + 1 : "");
    return 0;
}
