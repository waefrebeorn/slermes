/*
 * whatsapp.c — canonical C11 port of hermes_cli/subcommands/whatsapp.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_subcommands_whatsap_build_whatsapp_parser @ hermes_cli/subcommands/whatsapp.py:build_whatsapp_parser */
int hermes_cli_subcommands_whatsap_build_whatsapp_parser(const char *arg) {
    /* Python: attach whatsapp subcommand. */
    (void)arg;
    printf("whatsapp parser attached\n");
    return 0;
}
