/* gate: hermes_cli_dashboard_procs_py___valid_lockfile_payload — traced copy of dashboard_procs_valid_lockfile_payload */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int dashboard_procs_valid_lockfile_payload(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("dashboard_procs_valid_lockfile_payload");
    int result = dashboard_procs_valid_lockfile_payload(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("dashboard_procs_valid_lockfile_payload");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s dashboard_procs_valid_lockfile_payload <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
