/* gate: hermes_cli_gateway_windows_py___install_startup_fallback — traced copy of gw_u_install_startup_fallback */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gw_u_install_startup_fallback(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_u_install_startup_fallback");
    int result = gw_u_install_startup_fallback(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_u_install_startup_fallback");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_u_install_startup_fallback <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
