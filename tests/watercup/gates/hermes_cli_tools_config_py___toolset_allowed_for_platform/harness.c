/* gate: hermes_cli_tools_config_py___toolset_allowed_for_platform — traced copy of toolset_allowed_for_platform */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int toolset_allowed_for_platform(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("toolset_allowed_for_platform");
    int result = toolset_allowed_for_platform(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("toolset_allowed_for_platform");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s toolset_allowed_for_platform <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
