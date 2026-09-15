/* gate: tools_environments_local_py___is_hermes_internal_secret — traced copy of env_is_internal_secret */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool env_is_internal_secret(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("env_is_internal_secret");
    bool result = env_is_internal_secret(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("env_is_internal_secret");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s env_is_internal_secret <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
