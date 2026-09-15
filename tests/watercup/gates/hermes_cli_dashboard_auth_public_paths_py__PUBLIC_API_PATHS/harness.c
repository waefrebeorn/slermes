/* gate: hermes_cli_dashboard_auth_public_paths_py__PUBLIC_API_PATHS — traced copy of dapaths_is_public */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool dapaths_is_public(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("dapaths_is_public");
    bool result = dapaths_is_public(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("dapaths_is_public");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s dapaths_is_public <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
