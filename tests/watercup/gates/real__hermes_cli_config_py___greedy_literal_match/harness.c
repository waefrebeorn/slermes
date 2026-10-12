/* gate: real__hermes_cli_config_py___greedy_literal_match — GAP sentinel for _greedy_literal_match */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_greedy_literal_match";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _greedy_literal_match <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_greedy_literal_match");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_greedy_literal_match");
    }
    return 0;
}
