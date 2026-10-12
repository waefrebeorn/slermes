/* gate: real__hermes_cli__early_recovery_py___base_interpreter_is_externally_managed — GAP sentinel for _base_interpreter_is_externally_managed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_base_interpreter_is_externally_managed";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _base_interpreter_is_externally_managed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_base_interpreter_is_externally_managed");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_base_interpreter_is_externally_managed");
    }
    return 0;
}
