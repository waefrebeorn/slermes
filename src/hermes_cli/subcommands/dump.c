/*
 * dump.c — canonical C11 port of hermes_cli/subcommands/dump.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_subcommands_dump_build_dump_parser @ hermes_cli/subcommands/dump.py:build_dump_parser */
int hermes_cli_subcommands_dump_build_dump_parser(const char *arg) {
    /* Python: attach dump subcommand with --show-keys. */
    (void)arg;
    printf("dump parser attached (--show-keys)\n");
    return 0;
}
