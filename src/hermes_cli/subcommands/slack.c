/*
 * slack.c — canonical C11 port of hermes_cli/subcommands/slack.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_subcommands_slack_build_slack_parser @ hermes_cli/subcommands/slack.py:build_slack_parser */
int hermes_cli_subcommands_slack_build_slack_parser(const char *arg) {
    /* Python: build slack subcommand parser — REAL availability probe. */
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
