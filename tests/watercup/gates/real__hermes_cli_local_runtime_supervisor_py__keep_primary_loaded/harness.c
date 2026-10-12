/* gate: real__hermes_cli_local_runtime_supervisor_py__keep_primary_loaded — GAP sentinel for keep_primary_loaded */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "keep_primary_loaded";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s keep_primary_loaded <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("keep_primary_loaded");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("keep_primary_loaded");
    }
    return 0;
}
