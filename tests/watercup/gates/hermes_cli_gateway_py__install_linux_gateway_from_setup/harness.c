/* gate: hermes_cli_gateway_py__install_linux_gateway_from_setup — traced copy of cgw_install_linux_gateway_from_setup */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cgw_install_linux_gateway_from_setup(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cgw_install_linux_gateway_from_setup");
    int result = cgw_install_linux_gateway_from_setup(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cgw_install_linux_gateway_from_setup");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cgw_install_linux_gateway_from_setup <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
