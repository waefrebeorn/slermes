/* gate: port__hermes_cli_cli_output_py__line_input — GAP sentinel for line_input */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "line_input";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s line_input <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("line_input");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("line_input");
    }
    return 0;
}
