/* gate: real__hermes_cli_local_runtime_context_policy_py__spill_overrides — GAP sentinel for spill_overrides */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "spill_overrides";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s spill_overrides <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("spill_overrides");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("spill_overrides");
    }
    return 0;
}
