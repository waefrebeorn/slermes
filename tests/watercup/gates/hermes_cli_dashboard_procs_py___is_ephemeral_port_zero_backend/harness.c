/* gate: hermes_cli_dashboard_procs_py___is_ephemeral_port_zero_backend — traced copy of dashboard_procs_is_ephemeral_port_zero_backend */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int dashboard_procs_is_ephemeral_port_zero_backend(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("dashboard_procs_is_ephemeral_port_zero_backend");
    int result = dashboard_procs_is_ephemeral_port_zero_backend(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("dashboard_procs_is_ephemeral_port_zero_backend");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s dashboard_procs_is_ephemeral_port_zero_backend <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
