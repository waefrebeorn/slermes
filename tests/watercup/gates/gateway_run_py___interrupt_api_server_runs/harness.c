/* gate: gateway_run_py___interrupt_api_server_runs — traced copy of gw_interrupt_api_server_runs */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gw_interrupt_api_server_runs(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_interrupt_api_server_runs");
    int result = gw_interrupt_api_server_runs(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_interrupt_api_server_runs");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_interrupt_api_server_runs <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
