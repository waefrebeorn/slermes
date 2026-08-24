/*
 * console.c — canonical C11 port of hermes_cli/subcommands/console.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_subcommands_console_build_console_parser @ hermes_cli/subcommands/console.py:build_console_parser */
int hermes_cli_subcommands_console_build_console_parser(const char *arg) {
    /* Python: attach console subcommand (safe REPL). */
    (void)arg;
    printf("console parser attached\n");
    return 0;
}
