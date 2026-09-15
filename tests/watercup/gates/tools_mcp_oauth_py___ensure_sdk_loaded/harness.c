/* gate: tools_mcp_oauth_py___ensure_sdk_loaded — traced copy of mcpo_ensure_sdk_loaded */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mcpo_ensure_sdk_loaded(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcpo_ensure_sdk_loaded");
    int result = mcpo_ensure_sdk_loaded(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcpo_ensure_sdk_loaded");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcpo_ensure_sdk_loaded <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
