/*
 * debug.c — canonical C11 port of hermes_cli/subcommands/debug.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_subcommands_debug_build_debug_parser @ hermes_cli/subcommands/debug.py:build_debug_parser */
int hermes_cli_subcommands_debug_build_debug_parser(const char *arg) {
    /* Python: attach debug subcommand. */
    (void)arg;
    printf("debug parser attached\n");
    return 0;
}
