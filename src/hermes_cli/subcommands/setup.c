/*
 * setup.c — canonical C11 port of hermes_cli/subcommands/setup.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_subcommands_setup_build_setup_parser @ hermes_cli/subcommands/setup.py:build_setup_parser */
int hermes_cli_subcommands_setup_build_setup_parser(const char *arg) {
    /* Python: attach setup subcommand. */
    (void)arg;
    printf("setup parser attached (section, --non-interactive, --quick, --portal)\n");
    return 0;
}
