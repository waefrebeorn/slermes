/* gate: hermes_cli_banner_py___is_full_sha — traced copy of banner_is_full_sha */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool banner_is_full_sha(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("banner_is_full_sha");
    bool result = banner_is_full_sha(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("banner_is_full_sha");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s banner_is_full_sha <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
