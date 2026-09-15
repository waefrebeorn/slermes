/* gate: agent_secret_scope_py___is_global_env — traced copy of secret_scope_is_global_env */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool secret_scope_is_global_env(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("secret_scope_is_global_env");
    bool result = secret_scope_is_global_env(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("secret_scope_is_global_env");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s secret_scope_is_global_env <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
