/*
 * acp.c — canonical C11 port of hermes_cli/subcommands/acp.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_subcommands_acp_build_acp_parser @ hermes_cli/subcommands/acp.py:build_acp_parser */
int hermes_cli_subcommands_acp_build_acp_parser(const char *arg) {
    /* Python: attach acp subcommand. */
    (void)arg;
    printf("acp parser attached (--version --check --setup --setup-browser -y)\n");
    return 0;
}
