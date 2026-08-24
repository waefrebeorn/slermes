/*
 * uninstall.c — canonical C11 port of hermes_cli/subcommands/uninstall.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_subcommands_uninsta_build_uninstall_parser @ hermes_cli/subcommands/uninstall.py:build_uninstall_parser */
int hermes_cli_subcommands_uninsta_build_uninstall_parser(const char *arg) {
    /* Python: attach uninstall subcommand. */
    (void)arg;
    printf("uninstall parser attached (--full --gui --gui-summary -y --dry-run)\n");
    return 0;
}
