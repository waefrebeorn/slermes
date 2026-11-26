/* gate: agent_subdirectory_hints_py___is_ancestor_or_same — traced copy of shi_is_ancestor_or_same */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int shi_is_ancestor_or_same(const char *a0, const char *a1);

static void trace_one(const char **in, int n, int idx) {
    WCUP_BEGIN("shi_is_ancestor_or_same");
    int result = shi_is_ancestor_or_same(in[(idx + 0) % n], in[(idx + 1) % n]);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("shi_is_ancestor_or_same");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s shi_is_ancestor_or_same <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one((const char **)(argv + 2), argc - 2, i - 2);
    }
    return 0;
}
