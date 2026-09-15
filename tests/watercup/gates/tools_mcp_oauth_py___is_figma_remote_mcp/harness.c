/* gate: tools_mcp_oauth_py___is_figma_remote_mcp — traced copy of mcpo_is_figma_remote_mcp */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mcpo_is_figma_remote_mcp(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcpo_is_figma_remote_mcp");
    int result = mcpo_is_figma_remote_mcp(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcpo_is_figma_remote_mcp");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcpo_is_figma_remote_mcp <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
