/* gate: tools_mcp_oauth_py___make_redirect_handler — traced copy of mcpo_u_make_redirect_handler */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mcpo_u_make_redirect_handler(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcpo_u_make_redirect_handler");
    int result = mcpo_u_make_redirect_handler(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcpo_u_make_redirect_handler");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcpo_u_make_redirect_handler <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
