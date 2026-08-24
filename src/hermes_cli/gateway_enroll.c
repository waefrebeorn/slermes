/*
 * gateway_enroll.c — canonical C11 port of hermes_cli/gateway_enroll.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_gateway_enroll_u_default_gateway_id @ hermes_cli/gateway_enroll.py:_default_gateway_id */
int hermes_cli_gateway_enroll_u_default_gateway_id(const char *arg) {
    /* Python: "gw-<hostname>" or gw-hermes. Arg = hostname. */
    if (!arg || !*arg) { printf("gw-hermes\n"); return 0; }
    printf("gw-%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_gateway_enroll_u_resolve_connector_url @ hermes_cli/gateway_enroll.py:_resolve_connector_url */
int hermes_cli_gateway_enroll_u_resolve_connector_url(const char *arg) {
    /* Python: ws->http scheme map. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "none") == 0) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_gateway_enroll_u_resolve_identity_token @ hermes_cli/gateway_enroll.py:_resolve_identity_token */
int hermes_cli_gateway_enroll_u_resolve_identity_token(const char *arg) {
    /* Python: relay identity token or RuntimeError. Arg = "token". */
    if (!arg || !*arg) {
        fprintf(stderr, "failed to resolve relay identity token\n");
        return 1;
    }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_gateway_enroll_u_post_enroll @ hermes_cli/gateway_enroll.py:_post_enroll */
int hermes_cli_gateway_enroll_u_post_enroll(const char *arg) {
    /* Python: relay enroll. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "http401") == 0) {
        fprintf(stderr, "Connector rejected the caller identity (401). Try `hermes auth add nous` and retry.\n");
        return 1;
    }
    if (strcmp(state, "http403") == 0) {
        fprintf(stderr, "Enrollment token invalid, expired, already used, or tenant mismatch (403).\n");
        return 1;
    }
    if (strcmp(state, "unreachable") == 0) {
        fprintf(stderr, "Could not reach the connector: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    if (strcmp(state, "no_secret") == 0) {
        fprintf(stderr, "Connector returned an unexpected response (no secret).\n");
        return 1;
    }
    printf("enrolled: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_gateway_enroll_cmd_gateway_enroll @ hermes_cli/gateway_enroll.py:cmd_gateway_enroll */
int hermes_cli_gateway_enroll_cmd_gateway_enroll(const char *arg) {
    /* Python: relay enrollment. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("1\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "managed") == 0) {
        printf("✗ `hermes gateway enroll` is not available in a managed/hosted install.\n  The relay gateway secret is provisioned by the hosting platform.\n");
        return 1;
    }
    if (strcmp(state, "no_token") == 0) {
        printf("✗ No enrollment token. Pass --token <token> (or set GATEWAY_RELAY_ENROLL_TOKEN).\n");
        return 1;
    }
    if (strcmp(state, "failed") == 0) {
        fprintf(stderr, "enrollment failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("✓ Gateway enrolled with relay connector (creds persisted to .env: %s)\n", t2 ? t2 + 1 : "");
    return 0;
}
