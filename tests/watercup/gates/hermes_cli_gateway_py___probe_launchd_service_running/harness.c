/* gate: hermes_cli_gateway_py___probe_launchd_service_running — traced copy of cgw_u_probe_launchd_service_running */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cgw_u_probe_launchd_service_running(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cgw_u_probe_launchd_service_running");
    int result = cgw_u_probe_launchd_service_running(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cgw_u_probe_launchd_service_running");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cgw_u_probe_launchd_service_running <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
