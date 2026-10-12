/* gate: real__hermes_cli_update_abort_recovery_py___without_incarnation — GAP sentinel for _without_incarnation */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_without_incarnation";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _without_incarnation <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_without_incarnation");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_without_incarnation");
    }
    return 0;
}
