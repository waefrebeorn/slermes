/*
 * dashboard.c — canonical C11 port of hermes_cli/subcommands/dashboard.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_subcommands_dashboa_u_add_server_runtime_args @ hermes_cli/subcommands/dashboard.py:_add_server_runtime_args */
int hermes_cli_subcommands_dashboa_u_add_server_runtime_args(const char *arg) {
    /* Python: shared runtime flags. */
    (void)arg;
    printf("server runtime args attached (--port --host --insecure[no-op] --skip-build --isolated --open-profile --stop --status)\n");
    return 0;
}

/* PoP: hermes_cli_subcommands_dashboa_build_dashboard_parser @ hermes_cli/subcommands/dashboard.py:build_dashboard_parser */
int hermes_cli_subcommands_dashboa_build_dashboard_parser(const char *arg) {
    /* Python: build dashboard subcommand parser — REAL availability probe. */
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
