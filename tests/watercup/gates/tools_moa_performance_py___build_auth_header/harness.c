/* gate: tools_moa_performance_py___build_auth_header — traced copy of tools_moa_performance_u_build_auth_header */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_moa_performance_u_build_auth_header(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_moa_performance_u_build_auth_header");
    int result = tools_moa_performance_u_build_auth_header(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_moa_performance_u_build_auth_header");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_moa_performance_u_build_auth_header <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
