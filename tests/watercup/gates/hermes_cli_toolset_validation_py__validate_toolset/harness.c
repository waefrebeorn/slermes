/* gate: hermes_cli_toolset_validation_py__validate_toolset — traced copy of toolset_validation_is_known_toolset */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool toolset_validation_is_known_toolset(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("toolset_validation_is_known_toolset");
    bool result = toolset_validation_is_known_toolset(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("toolset_validation_is_known_toolset");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s toolset_validation_is_known_toolset <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
