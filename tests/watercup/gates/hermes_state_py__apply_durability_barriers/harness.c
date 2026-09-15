/* gate: hermes_state_py__apply_durability_barriers — traced copy of apply_durability_barriers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int apply_durability_barriers(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("apply_durability_barriers");
    int result = apply_durability_barriers(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("apply_durability_barriers");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s apply_durability_barriers <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
