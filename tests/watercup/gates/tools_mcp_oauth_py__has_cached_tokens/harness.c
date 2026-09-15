/* gate: tools_mcp_oauth_py__has_cached_tokens — traced copy of mcpo_has_cached_tokens */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mcpo_has_cached_tokens(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcpo_has_cached_tokens");
    int result = mcpo_has_cached_tokens(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcpo_has_cached_tokens");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcpo_has_cached_tokens <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
