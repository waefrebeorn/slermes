/* gate: hermes_cli_gateway_py___launchd_error_indicates_unloaded — traced copy of cgw_u_launchd_error_indicates_unloaded */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cgw_u_launchd_error_indicates_unloaded(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cgw_u_launchd_error_indicates_unloaded");
    int result = cgw_u_launchd_error_indicates_unloaded(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cgw_u_launchd_error_indicates_unloaded");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cgw_u_launchd_error_indicates_unloaded <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
