/* gate: hermes_cli_auth_py___refresh_minimax_oauth_state — traced copy of auth_u_refresh_minimax_oauth_state */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int auth_u_refresh_minimax_oauth_state(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("auth_u_refresh_minimax_oauth_state");
    int result = auth_u_refresh_minimax_oauth_state(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("auth_u_refresh_minimax_oauth_state");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auth_u_refresh_minimax_oauth_state <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
