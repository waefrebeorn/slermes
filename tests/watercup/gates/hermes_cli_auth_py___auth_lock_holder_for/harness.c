/* gate: hermes_cli_auth_py___auth_lock_holder_for — traced copy of auth_u_auth_lock_holder_for */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int auth_u_auth_lock_holder_for(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("auth_u_auth_lock_holder_for");
    int result = auth_u_auth_lock_holder_for(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("auth_u_auth_lock_holder_for");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auth_u_auth_lock_holder_for <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
