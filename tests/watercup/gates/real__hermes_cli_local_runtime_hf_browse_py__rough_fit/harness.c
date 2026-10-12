/* gate: real__hermes_cli_local_runtime_hf_browse_py__rough_fit — GAP sentinel for rough_fit */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "rough_fit";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s rough_fit <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("rough_fit");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("rough_fit");
    }
    return 0;
}
