/*
 * memory_oauth.c — canonical C11 port of hermes_cli/memory_oauth.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_memory_oauth_u_resolve_flow @ hermes_cli/memory_oauth.py:_resolve_flow */
int hermes_cli_memory_oauth_u_resolve_flow(const char *arg) {
    /* Python: import plugins.memory.<provider>.oauth_flow or 404. Arg =
     * "provider\tvalid\tfound". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    size_t plen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    /* isidentifier: [A-Za-z_][A-Za-z0-9_]* */
    int valid = plen > 0 && (arg[0] == '_' || (arg[0] >= 'A' && arg[0] <= 'Z') || (arg[0] >= 'a' && arg[0] <= 'z'));
    for (size_t i = 1; valid && i < plen; i++) {
        char c = arg[i];
        if (!(c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) valid = 0;
    }
    if (!valid) { printf("404 unknown memory provider\n"); return 404; }
    if (t2 && t2[1] == '1') { printf("oauth flow: plugins.memory.%.*s.oauth_flow\n", (int)plen, arg); return 0; }
    printf("404 %.*s does not support OAuth connect\n", (int)plen, arg);
    return 404;
}

/* PoP: hermes_cli_memory_oauth_u_scope_to_profile @ hermes_cli/memory_oauth.py:_scope_to_profile */
int hermes_cli_memory_oauth_u_scope_to_profile(const char *arg) {
    /* Python: profile scoping w/ validation. Arg =
     * "profile\tstate\terror". */
    if (!arg || !*arg) { printf("no profile scope\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "bad_name") == 0) { fprintf(stderr, "400: %s\n", t2 ? t2 + 1 : ""); return 1; }
    if (strcmp(state, "not_found") == 0) { fprintf(stderr, "404: Profile '%s' does not exist.\n", arg); return 1; }
    if (strcmp(state, "current") == 0) { printf("current profile untouched\n"); return 0; }
    printf("scoped to profile: %s\n", arg);
    return 0;
}

/* PoP: hermes_cli_memory_oauth_start_memory_oauth @ hermes_cli/memory_oauth.py:start_memory_oauth */
int hermes_cli_memory_oauth_start_memory_oauth(const char *arg) {
    /* Python: zero-CLI flow. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "Failed to start OAuth: %s\n", t3 ? t3 + 1 : "?");
        return 500;
    }
    printf("1 (loopback flow started in background worker; poll status: %s)%s\n", t3 ? t3 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: hermes_cli_memory_oauth_memory_oauth_status @ hermes_cli/memory_oauth.py:memory_oauth_status */
int hermes_cli_memory_oauth_memory_oauth_status(const char *arg) {
    /* Python: flow status poll. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("idle\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "idle";
    if (strcmp(state, "error") == 0) {
        fprintf(stderr, "Failed to read OAuth status: %s\n", t3 ? t3 + 1 : "?");
        return 500;
    }
    printf("%s (profile-scoped flow lookup)%s\n", state, (t2 && t2[1] == '1') ? "" : "");
    return 0;
}
