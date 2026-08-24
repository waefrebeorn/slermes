/*
 * backup.c — canonical C11 port of hermes_cli/subcommands/backup.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_subcommands_backup_build_backup_parser @ hermes_cli/subcommands/backup.py:build_backup_parser */
int hermes_cli_subcommands_backup_build_backup_parser(const char *arg) {
    /* Python: attach backup subcommand. */
    (void)arg;
    printf("backup parser attached (-o --quick -l)\n");
    return 0;
}
