/* gate: port__hermes_cli_config_py___deep_merge — GAP sentinel for _deep_merge */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_deep_merge";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _deep_merge <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_deep_merge");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_deep_merge");
    }
    return 0;
}
