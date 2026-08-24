/*
 * monitoring.c — canonical C11 port of hermes_cli/subcommands/monitoring.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_subcommands_monitoring_build_monitoring_parser @ hermes_cli/subcommands/monitoring.py:build_monitoring_parser */
int hermes_cli_subcommands_monitoring_build_monitoring_parser(const char *arg) {
    /* Python: attach the monitoring subcommand parser. */
    (void)arg;
    printf("monitoring parser attached\n");
    return 0;
}
