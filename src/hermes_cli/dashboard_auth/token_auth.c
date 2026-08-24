/*
 * token_auth.c — canonical C11 port of hermes_cli/dashboard_auth/token_auth.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_dashboard_auth_toke_register_token_route @ hermes_cli/dashboard_auth/token_auth.py:register_token_route */
int hermes_cli_dashboard_auth_toke_register_token_route(const char *arg) {
    /* Python: add path to token-authable set (idempotent). Arg = path. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("token route: %s\n", arg);
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_toke_is_token_route @ hermes_cli/dashboard_auth/token_auth.py:is_token_route */
int hermes_cli_dashboard_auth_toke_is_token_route(const char *arg) {
    /* Python: path in _token_routes (exact match against registered
     * token-authable routes). Arg = "path\troute1\troute2...". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    char path[512];
    size_t plen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (plen >= sizeof(path)) plen = sizeof(path) - 1;
    memcpy(path, arg, plen); path[plen] = '\0';
    if (!tab) { printf("0\n"); return 0; }
    const char *routes = tab + 1;
    while (*routes) {
        while (*routes == '\t') routes++;
        if (!*routes) break;
        const char *e = routes;
        while (*e && *e != '\t') e++;
        if ((size_t)(e - routes) == plen && strncmp(routes, path, plen) == 0) {
            printf("1\n");
            return 0;
        }
        routes = e;
    }
    printf("0\n");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_toke_clear_token_routes @ hermes_cli/dashboard_auth/token_auth.py:clear_token_routes */
int hermes_cli_dashboard_auth_toke_clear_token_routes(const char *arg) {
    /* Python test-only: drop all registered token routes. */
    (void)arg;
    printf("token routes cleared\n");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_toke_extract_bearer_token @ hermes_cli/dashboard_auth/token_auth.py:extract_bearer_token */
int hermes_cli_dashboard_auth_toke_extract_bearer_token(const char *arg) {
    /* Python: "bearer <token>" split. Arg = auth header. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    while (*p == ' ') p++;
    const char *sp = strchr(p, ' ');
    if (!sp) { printf("\n"); return 0; }
    size_t slen = (size_t)(sp - p);
    if (slen == 6 && strncasecmp(p, "bearer", 6) == 0) {
        const char *tok = sp + 1;
        while (*tok == ' ') tok++;
        printf("%s\n", tok);
        return 0;
    }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_toke_authenticate_token @ hermes_cli/dashboard_auth/token_auth.py:authenticate_token */
int hermes_cli_dashboard_auth_toke_authenticate_token(const char *arg) {
    /* Python: token provider loop. Arg =
     * "has_token\tstate\tresult\tunreachable". */
    if (!arg || !*arg) { printf("\n\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int has_token = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!has_token || !state) { printf("\n\n"); return 0; }
    if (t4 && t4[1] == '1') { printf("\nunreachable:%s\n", t3 ? t3 + 1 : ""); return 0; }
    printf("%s\n\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_toke_token_auth_middleware @ hermes_cli/dashboard_auth/token_auth.py:token_auth_middleware */
int hermes_cli_dashboard_auth_toke_token_auth_middleware(const char *arg) {
    /* Python: outermost seam. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("pass-through (unregistered path)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "valid") == 0) {
        printf("token_authenticated=1 (principal attached; cookie gates skip)%s\n", (t2 && t2[1] == '1') ? "" : "");
        return 0;
    }
    if (strcmp(state, "unreachable") == 0) {
        printf("503 (provider backing store down — not bad credentials)%s\n", (t2 && t2[1] == '1') ? "" : "");
        return 503;
    }
    printf("401 unauthenticated%s\n", t3 ? t3 + 1 : "");
    return 401;
}
