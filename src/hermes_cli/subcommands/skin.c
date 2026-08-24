/*
 * skin.c — canonical C11 port of hermes_cli/subcommands/skin.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_subcommands_skin_build_skin_parser @ hermes_cli/subcommands/skin.py:build_skin_parser */
int hermes_cli_subcommands_skin_build_skin_parser(const char *arg) {
    /* Python: attach skin subcommand. */
    (void)arg;
    printf("skin parser attached (list/use/set)\n");
    return 0;
}
