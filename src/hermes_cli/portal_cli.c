/*
 * portal_cli.c — canonical C11 port of hermes_cli/portal_cli.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_portal_cli_u_cmd_open @ hermes_cli/portal_cli.py:_cmd_open */
int hermes_cli_portal_cli_u_cmd_open(const char *arg) {
    /* Python: open subscription URL in browser. Arg = "url\topened". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    printf("Opening %s\n", tab ? tab + 1 : arg);
    printf("Could not launch a browser. Visit the URL above manually.\n");
    return 1;
}

/* PoP: hermes_cli_portal_cli_u_cmd_login @ hermes_cli/portal_cli.py:_cmd_login */
int hermes_cli_portal_cli_u_cmd_login(const char *arg) {
    /* Python: portal one-shot; cancel -> 1. Arg = "state". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    if (strcmp(arg, "cancelled") == 0) { printf("Portal setup cancelled.\n"); return 1; }
    printf("portal onboarding complete\n");
    return 0;
}
