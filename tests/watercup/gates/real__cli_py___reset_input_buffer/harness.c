/* gate: real__cli_py___reset_input_buffer — GAP sentinel for _reset_input_buffer */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_reset_input_buffer";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _reset_input_buffer <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_reset_input_buffer");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_reset_input_buffer");
    }
    return 0;
}
