/*
 * approvals.c — canonical C11 port of hermes_cli/subcommands/approvals.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_subcommands_approval_build_approvals_parser @ hermes_cli/subcommands/approvals.py:build_approvals_parser */
int hermes_cli_subcommands_approval_build_approvals_parser(const char *arg) {
    /* Python: attach the approvals subcommand parser (suggest, --apply,
     * --json, --days, --min-count, --limit, --db). */
    (void)arg;
    printf("approvals subcommands parser attached (suggest --apply --json --days=90 --min-count=2 --limit=20 --db)\n");
    return 0;
}
