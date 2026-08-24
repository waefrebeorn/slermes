/*
 * vercel_auth.c — canonical C11 port of hermes_cli/vercel_auth.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_vercel_auth_u_present @ hermes_cli/vercel_auth.py:_present */
int hermes_cli_vercel_auth_u_present(const char *arg) {
    /* Python: bool(os.getenv(name)).
     * Arg = env var name. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *val = getenv(arg);
    printf("%d\n", val && *val ? 1 : 0);
    return 0;
}

/* PoP: hermes_cli_vercel_auth_describe_vercel_auth @ hermes_cli/vercel_auth.py:describe_vercel_auth */
int hermes_cli_vercel_auth_describe_vercel_auth(const char *arg) {
    /* Python: return Vercel auth status without exposing secrets.
     * Arg = state summary (pre-computed by caller). */
    if (!arg || !*arg) { printf("status: unknown\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}
