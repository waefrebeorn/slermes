/* gate: hermes_cli_gateway_py__launch_detached_gateway_restart_by_cmdline — traced copy of gwc2_launch_detached_gateway_restart_by_cmdline */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gwc2_launch_detached_gateway_restart_by_cmdline(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gwc2_launch_detached_gateway_restart_by_cmdline");
    int result = gwc2_launch_detached_gateway_restart_by_cmdline(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gwc2_launch_detached_gateway_restart_by_cmdline");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gwc2_launch_detached_gateway_restart_by_cmdline <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
