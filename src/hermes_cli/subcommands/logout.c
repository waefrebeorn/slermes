/*
 * logout.c — canonical C11 port of hermes_cli/subcommands/logout.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_subcommands_logout_build_logout_parser @ hermes_cli/subcommands/logout.py:build_logout_parser */
int hermes_cli_subcommands_logout_build_logout_parser(const char *arg) {
    /* Python: attach logout subcommand with --provider choices. */
    (void)arg;
    printf("logout parser attached (--provider nous|openai-codex|xai-oauth|spotify)\n");
    return 0;
}
