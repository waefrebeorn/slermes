/*
 * prefix.c — canonical C11 port of hermes_cli/dashboard_auth/prefix.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_dashboard_auth_pref_u_warn_if_malformed_prefix @ hermes_cli/dashboard_auth/prefix.py:_warn_if_malformed_prefix */
int hermes_cli_dashboard_auth_pref_u_warn_if_malformed_prefix(const char *arg) {
    /* Python: warn once per (cleaned, reason). Arg = "raw\treason". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *raw = arg;
    while (*raw == ' ') raw++;
    if (!*raw) { printf("\n"); return 0; }
    fprintf(stderr, "X-Forwarded-Prefix header %s was ignored because %s. Dashboard URLs will be generated without a reverse-proxy path prefix.\n",
            raw, tab ? tab + 1 : "invalid");
    printf("warned\n");
    return 0;
}
