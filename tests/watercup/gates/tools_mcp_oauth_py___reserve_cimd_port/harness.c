/* gate: tools_mcp_oauth_py___reserve_cimd_port — traced copy of mcpo_reserve_cimd_port */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mcpo_reserve_cimd_port(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcpo_reserve_cimd_port");
    int result = mcpo_reserve_cimd_port(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcpo_reserve_cimd_port");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcpo_reserve_cimd_port <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
