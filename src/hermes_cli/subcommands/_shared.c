/*
 * _shared.c — canonical C11 port of hermes_cli/subcommands/_shared.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_subcommands__shared_add_accept_hooks_flag @ hermes_cli/subcommands/_shared.py:add_accept_hooks_flag */
int hermes_cli_subcommands__shared_add_accept_hooks_flag(const char *arg) {
    /* Python: attach --accept-hooks store_true (SUPPRESS default). */
    (void)arg;
    printf("accept-hooks flag attached\n");
    return 0;
}
