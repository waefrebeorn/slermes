/* gate: tools_self_repo_guard_py___is_within — traced copy of srg_is_within */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int srg_is_within(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("srg_is_within");
    int result = srg_is_within(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("srg_is_within");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s srg_is_within <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
