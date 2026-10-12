/* gate: real__hermes_cli_npm_engine_py__required_npm_range — GAP sentinel for required_npm_range */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "required_npm_range";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s required_npm_range <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("required_npm_range");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("required_npm_range");
    }
    return 0;
}
