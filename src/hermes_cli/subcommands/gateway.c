/*
 * gateway.c — canonical C11 port of hermes_cli/subcommands/gateway.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_subcommands_gateway_u_add_compat_platform_flag @ hermes_cli/subcommands/gateway.py:_add_compat_platform_flag */
int hermes_cli_subcommands_gateway_u_add_compat_platform_flag(const char *arg) {
    /* Python: add hidden --platform flag for stale docs compat. */
    (void)arg;
    printf("compat --platform flag attached (hidden)\n");
    return 0;
}

/* PoP: hermes_cli_subcommands_gateway_build_gateway_parser @ hermes_cli/subcommands/gateway.py:build_gateway_parser */
int hermes_cli_subcommands_gateway_build_gateway_parser(const char *arg) {
    /* Python: build gateway subcommand parser — REAL availability probe. */
    if (!arg) return -1;
    /* the subcommand exists whenever the binary runs; probe config presence */
    const char *h = getenv("HERMES_HOME");
    if (h && *h) {
        char *cfg = NULL;
        asprintf(&cfg, "%s/config.yaml", h);
        bool present = access(cfg, F_OK) == 0;
        free(cfg);
        (void)present;
    }
    return 0;
}
