/* gate: real__hermes_cli_plugins_cmd_py___bundled_default_on — GAP sentinel for _bundled_default_on */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_bundled_default_on";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _bundled_default_on <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_bundled_default_on");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_bundled_default_on");
    }
    return 0;
}
