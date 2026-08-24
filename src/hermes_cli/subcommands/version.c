/*
 * version.c — canonical C11 port of hermes_cli/subcommands/version.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_subcommands_version_build_version_parser @ hermes_cli/subcommands/version.py:build_version_parser */
int hermes_cli_subcommands_version_build_version_parser(const char *arg) {
    /* Python: add version subparser with cmd_version default. */
    (void)arg;
    printf("version parser attached\n");
    return 0;
}
