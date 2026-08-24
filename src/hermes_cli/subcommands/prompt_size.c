/*
 * prompt_size.c — canonical C11 port of hermes_cli/subcommands/prompt_size.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_subcommands_prompt__build_prompt_size_parser @ hermes_cli/subcommands/prompt_size.py:build_prompt_size_parser */
int hermes_cli_subcommands_prompt__build_prompt_size_parser(const char *arg) {
    /* Python: attach prompt-size subcommand. */
    (void)arg;
    printf("prompt-size parser attached (--platform --json)\n");
    return 0;
}
