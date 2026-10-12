/* gate: real__hermes_cli_npm_engine_py___probe_version — GAP sentinel for _probe_version */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_probe_version";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _probe_version <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_probe_version");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_probe_version");
    }
    return 0;
}
