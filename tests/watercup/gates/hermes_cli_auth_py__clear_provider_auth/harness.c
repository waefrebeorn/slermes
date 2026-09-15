/* gate: hermes_cli_auth_py__clear_provider_auth — traced copy of clear_provider_auth */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool clear_provider_auth(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("clear_provider_auth");
    bool result = clear_provider_auth(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("clear_provider_auth");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s clear_provider_auth <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
