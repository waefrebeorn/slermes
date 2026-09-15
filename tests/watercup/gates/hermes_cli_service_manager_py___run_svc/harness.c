/* gate: hermes_cli_service_manager_py___run_svc — traced copy of hermes_cli_service_manager_u_run_svc */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli_service_manager_u_run_svc(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_cli_service_manager_u_run_svc");
    int result = hermes_cli_service_manager_u_run_svc(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli_service_manager_u_run_svc");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli_service_manager_u_run_svc <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
