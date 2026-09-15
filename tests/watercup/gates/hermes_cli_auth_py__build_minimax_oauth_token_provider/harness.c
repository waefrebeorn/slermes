/* gate: hermes_cli_auth_py__build_minimax_oauth_token_provider — traced copy of auth_build_minimax_oauth_token_provider */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int auth_build_minimax_oauth_token_provider(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("auth_build_minimax_oauth_token_provider");
    int result = auth_build_minimax_oauth_token_provider(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("auth_build_minimax_oauth_token_provider");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auth_build_minimax_oauth_token_provider <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
