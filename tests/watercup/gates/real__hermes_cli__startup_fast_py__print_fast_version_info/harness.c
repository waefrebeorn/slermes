/* gate: real__hermes_cli__startup_fast_py__print_fast_version_info — GAP sentinel for print_fast_version_info */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "print_fast_version_info";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s print_fast_version_info <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("print_fast_version_info");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("print_fast_version_info");
    }
    return 0;
}
