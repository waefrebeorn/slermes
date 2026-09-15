/* gate: hermes_cli_agent_import_py__is_secret_key — traced copy of ai_is_secret_key */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool ai_is_secret_key(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ai_is_secret_key");
    bool result = ai_is_secret_key(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ai_is_secret_key");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ai_is_secret_key <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
