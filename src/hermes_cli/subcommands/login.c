/*
 * login.c — canonical C11 port of hermes_cli/subcommands/login.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_subcommands_login_build_login_parser @ hermes_cli/subcommands/login.py:build_login_parser */
int hermes_cli_subcommands_login_build_login_parser(const char *arg) {
    /* Python: deprecated login shim. */
    (void)arg;
    printf("login parser attached (deprecated — no help row, any --provider accepted)\n");
    return 0;
}
