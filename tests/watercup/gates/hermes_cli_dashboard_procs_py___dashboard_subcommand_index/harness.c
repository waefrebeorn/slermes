/* gate: hermes_cli_dashboard_procs_py___dashboard_subcommand_index — traced copy of dashboard_procs_dashboard_subcommand_index */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int dashboard_procs_dashboard_subcommand_index(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("dashboard_procs_dashboard_subcommand_index");
    int result = dashboard_procs_dashboard_subcommand_index(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("dashboard_procs_dashboard_subcommand_index");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s dashboard_procs_dashboard_subcommand_index <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
