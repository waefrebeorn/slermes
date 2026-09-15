/* gate: hermes_cli_auth_py__is_known_auth_provider — traced copy of is_known_auth_provider */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool is_known_auth_provider(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("is_known_auth_provider");
    bool result = is_known_auth_provider(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_known_auth_provider");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_known_auth_provider <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
