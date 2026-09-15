/* gate: tools_mcp_oauth_py___is_valid_cimd_url — traced copy of mcpo_is_valid_cimd_url */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool mcpo_is_valid_cimd_url(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcpo_is_valid_cimd_url");
    bool result = mcpo_is_valid_cimd_url(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcpo_is_valid_cimd_url");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcpo_is_valid_cimd_url <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
