/* gate: hermes_state_py___is_structural_corruption_error — traced copy of _is_structural_corruption_error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool _is_structural_corruption_error(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_is_structural_corruption_error");
    bool result = _is_structural_corruption_error(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_is_structural_corruption_error");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _is_structural_corruption_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
