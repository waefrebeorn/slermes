/* gate: hermes_cli_gateway_py___prepare_profile_gateway_update_restart — traced copy of cgw_u_prepare_profile_gateway_update_restart */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cgw_u_prepare_profile_gateway_update_restart(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cgw_u_prepare_profile_gateway_update_restart");
    int result = cgw_u_prepare_profile_gateway_update_restart(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cgw_u_prepare_profile_gateway_update_restart");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cgw_u_prepare_profile_gateway_update_restart <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
