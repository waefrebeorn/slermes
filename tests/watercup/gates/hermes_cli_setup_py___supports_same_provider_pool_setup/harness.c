/* gate: hermes_cli_setup_py___supports_same_provider_pool_setup — traced copy of stp_supports_same_provider_pool_setup */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool stp_supports_same_provider_pool_setup(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("stp_supports_same_provider_pool_setup");
    bool result = stp_supports_same_provider_pool_setup(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("stp_supports_same_provider_pool_setup");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s stp_supports_same_provider_pool_setup <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
