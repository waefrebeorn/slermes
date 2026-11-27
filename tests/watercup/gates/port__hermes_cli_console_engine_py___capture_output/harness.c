/* gate: port__hermes_cli_console_engine_py___capture_output — GAP sentinel for _capture_output */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_capture_output";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _capture_output <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_capture_output");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_capture_output");
    }
    return 0;
}
