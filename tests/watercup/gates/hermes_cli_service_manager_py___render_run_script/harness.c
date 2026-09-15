/* gate: hermes_cli_service_manager_py___render_run_script — traced copy of hermes_cli_service_manager_u_render_run_script */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli_service_manager_u_render_run_script(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_cli_service_manager_u_render_run_script");
    int result = hermes_cli_service_manager_u_render_run_script(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli_service_manager_u_render_run_script");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli_service_manager_u_render_run_script <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
