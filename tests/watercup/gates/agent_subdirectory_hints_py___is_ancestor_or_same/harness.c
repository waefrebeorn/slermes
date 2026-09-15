/* gate: agent_subdirectory_hints_py___is_ancestor_or_same — traced copy of shi_is_ancestor_or_same */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int shi_is_ancestor_or_same(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("shi_is_ancestor_or_same");
    int result = shi_is_ancestor_or_same(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("shi_is_ancestor_or_same");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s shi_is_ancestor_or_same <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
