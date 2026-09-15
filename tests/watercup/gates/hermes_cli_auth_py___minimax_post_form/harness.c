/* gate: hermes_cli_auth_py___minimax_post_form — traced copy of hermes_cli_auth_u_minimax_post_form */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli_auth_u_minimax_post_form(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_cli_auth_u_minimax_post_form");
    int result = hermes_cli_auth_u_minimax_post_form(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli_auth_u_minimax_post_form");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli_auth_u_minimax_post_form <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
