/* gate: tools_mcp_oauth_py___has_cached_client_info — traced copy of mcpo_has_cached_client_info */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool mcpo_has_cached_client_info(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcpo_has_cached_client_info");
    bool result = mcpo_has_cached_client_info(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcpo_has_cached_client_info");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcpo_has_cached_client_info <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
