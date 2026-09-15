/* gate: tools_mcp_oauth_py__load_oauth_metadata — traced copy of mcpo_load_oauth_metadata */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mcpo_load_oauth_metadata(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcpo_load_oauth_metadata");
    int result = mcpo_load_oauth_metadata(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcpo_load_oauth_metadata");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcpo_load_oauth_metadata <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
