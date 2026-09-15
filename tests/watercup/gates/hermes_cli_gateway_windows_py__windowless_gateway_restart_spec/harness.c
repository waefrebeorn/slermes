/* gate: hermes_cli_gateway_windows_py__windowless_gateway_restart_spec — traced copy of gw_windowless_gateway_restart_spec */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gw_windowless_gateway_restart_spec(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_windowless_gateway_restart_spec");
    int result = gw_windowless_gateway_restart_spec(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_windowless_gateway_restart_spec");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_windowless_gateway_restart_spec <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
