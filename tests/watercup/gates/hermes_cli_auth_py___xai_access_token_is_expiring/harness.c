/* gate: hermes_cli_auth_py___xai_access_token_is_expiring — traced copy of auth_u_xai_access_token_is_expiring */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int auth_u_xai_access_token_is_expiring(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("auth_u_xai_access_token_is_expiring");
    int result = auth_u_xai_access_token_is_expiring(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("auth_u_xai_access_token_is_expiring");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auth_u_xai_access_token_is_expiring <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
