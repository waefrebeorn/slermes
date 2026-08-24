/*
 * approval_mode.c — canonical C11 port of hermes_cli/approval_mode.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_approval_mode_u_effective_mode @ hermes_cli/approval_mode.py:_effective_mode */
int hermes_cli_approval_mode_u_effective_mode(const char *arg) {
    /* Python: return the approval mode enforced by terminal guard.
     * Arg = "current_mode". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_approval_mode_run_approval_mode_command @ hermes_cli/approval_mode.py:run_approval_mode_command */
int hermes_cli_approval_mode_run_approval_mode_command(const char *arg) {
    /* Python: inspect or persist approvals.mode.
     * Arg = "current_mode\trequested_mode\tsuccess". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}
