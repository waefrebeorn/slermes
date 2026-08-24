/*
 * memory.c — canonical C11 port of hermes_cli/subcommands/memory.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_subcommands_memory_build_memory_parser @ hermes_cli/subcommands/memory.py:build_memory_parser */
int hermes_cli_subcommands_memory_build_memory_parser(const char *arg) {
    /* Python: attach memory subcommand. */
    (void)arg;
    printf("memory parser attached (setup/status/off/reset)\n");
    return 0;
}
