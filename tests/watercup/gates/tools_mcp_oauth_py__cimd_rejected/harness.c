/* gate: tools_mcp_oauth_py__cimd_rejected — traced copy of mcpo_cimd_rejected */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool mcpo_cimd_rejected(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcpo_cimd_rejected");
    bool result = mcpo_cimd_rejected(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcpo_cimd_rejected");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcpo_cimd_rejected <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
