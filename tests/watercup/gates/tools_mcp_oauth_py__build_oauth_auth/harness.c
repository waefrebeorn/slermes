/* gate: tools_mcp_oauth_py__build_oauth_auth — traced copy of mcpo_build_oauth_auth */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mcpo_build_oauth_auth(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcpo_build_oauth_auth");
    int result = mcpo_build_oauth_auth(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcpo_build_oauth_auth");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcpo_build_oauth_auth <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
