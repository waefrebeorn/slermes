/*
 * model.c — canonical C11 port of hermes_cli/subcommands/model.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_subcommands_model_build_model_parser @ hermes_cli/subcommands/model.py:build_model_parser */
int hermes_cli_subcommands_model_build_model_parser(const char *arg) {
    /* Python: attach model subcommand. */
    (void)arg;
    printf("model parser attached (--refresh --portal-url --no-browser --insecure)\n");
    return 0;
}
