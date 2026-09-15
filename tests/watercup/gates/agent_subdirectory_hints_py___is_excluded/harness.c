/* gate: agent_subdirectory_hints_py___is_excluded — traced copy of subdir_hints_is_excluded */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool subdir_hints_is_excluded(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("subdir_hints_is_excluded");
    bool result = subdir_hints_is_excluded(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("subdir_hints_is_excluded");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s subdir_hints_is_excluded <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
