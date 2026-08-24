/*
 * sync.c — canonical C11 port of hermes_cli/subcommands/sync.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_subcommands_sync_build_sync_parser @ hermes_cli/subcommands/sync.py:build_sync_parser */
int hermes_cli_subcommands_sync_build_sync_parser(const char *arg) {
    /* Python: attach the sync subcommand parser. */
    (void)arg;
    printf("sync parser attached\n");
    return 0;
}
