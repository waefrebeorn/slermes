/* gate: hermes_cli_uninstall_py__uninstall_gateway_service — traced copy of uninst_uninstall_gateway_service */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int uninst_uninstall_gateway_service(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("uninst_uninstall_gateway_service");
    int result = uninst_uninstall_gateway_service(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("uninst_uninstall_gateway_service");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uninst_uninstall_gateway_service <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
