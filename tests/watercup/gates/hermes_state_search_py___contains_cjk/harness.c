/* gate: hermes_state_search_py___contains_cjk — traced copy of hs_search_contains_cjk */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hs_search_contains_cjk(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hs_search_contains_cjk");
    int result = hs_search_contains_cjk(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hs_search_contains_cjk");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hs_search_contains_cjk <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
