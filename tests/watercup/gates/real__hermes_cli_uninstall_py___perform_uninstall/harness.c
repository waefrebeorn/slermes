/* gate: real__hermes_cli_uninstall_py___perform_uninstall — GAP sentinel for _perform_uninstall */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_perform_uninstall";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _perform_uninstall <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_perform_uninstall");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_perform_uninstall");
    }
    return 0;
}
