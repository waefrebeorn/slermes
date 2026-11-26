/* gate: hermes_cli_tools_config_py___toolset_allowed_for_platform — traced copy of toolset_allowed_for_platform */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int toolset_allowed_for_platform(const char *a0, const char *a1);

static void trace_one(const char **in, int n, int idx) {
    WCUP_BEGIN("toolset_allowed_for_platform");
    int result = toolset_allowed_for_platform(in[(idx + 0) % n], in[(idx + 1) % n]);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("toolset_allowed_for_platform");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s toolset_allowed_for_platform <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one((const char **)(argv + 2), argc - 2, i - 2);
    }
    return 0;
}
