/* gate: hermes_cli_gateway_windows_py___collect_gateway_stop_pids — traced copy of gw_u_collect_gateway_stop_pids */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gw_u_collect_gateway_stop_pids(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_u_collect_gateway_stop_pids");
    int result = gw_u_collect_gateway_stop_pids(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_u_collect_gateway_stop_pids");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_u_collect_gateway_stop_pids <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
