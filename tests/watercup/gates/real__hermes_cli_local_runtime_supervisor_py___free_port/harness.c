/* gate: real__hermes_cli_local_runtime_supervisor_py___free_port — GAP sentinel for _free_port */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_free_port";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _free_port <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_free_port");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_free_port");
    }
    return 0;
}
