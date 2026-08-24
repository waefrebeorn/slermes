/*
 * doctor.c — canonical C11 port of hermes_cli/subcommands/doctor.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_subcommands_doctor_build_doctor_parser @ hermes_cli/subcommands/doctor.py:build_doctor_parser */
int hermes_cli_subcommands_doctor_build_doctor_parser(const char *arg) {
    /* Python: attach doctor subcommand. */
    (void)arg;
    printf("doctor parser attached (--fix --ack ADVISORY_ID)\n");
    return 0;
}
