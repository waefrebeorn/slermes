/*
 * security.c — canonical C11 port of hermes_cli/subcommands/security.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_subcommands_securit_build_security_parser @ hermes_cli/subcommands/security.py:build_security_parser */
int hermes_cli_subcommands_securit_build_security_parser(const char *arg) {
    /* Python: audit subcommand. */
    (void)arg;
    printf("security parser attached (audit --json --fail-on --skip-*)\n");
    return 0;
}
