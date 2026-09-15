/* gate: hermes_cli_gateway_py___windows_gateway_should_absorb_console_controls — traced copy of cgw_u_windows_gateway_should_absorb_console_controls */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cgw_u_windows_gateway_should_absorb_console_controls(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cgw_u_windows_gateway_should_absorb_console_controls");
    int result = cgw_u_windows_gateway_should_absorb_console_controls(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cgw_u_windows_gateway_should_absorb_console_controls");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cgw_u_windows_gateway_should_absorb_console_controls <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
