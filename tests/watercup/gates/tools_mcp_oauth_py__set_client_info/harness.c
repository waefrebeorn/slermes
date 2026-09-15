/* gate: tools_mcp_oauth_py__set_client_info — traced copy of mcpo_set_client_info */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mcpo_set_client_info(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcpo_set_client_info");
    int result = mcpo_set_client_info(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcpo_set_client_info");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcpo_set_client_info <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
