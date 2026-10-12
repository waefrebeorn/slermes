/* gate: real__hermes_cli_config_py___coerce_float — GAP sentinel for _coerce_float */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_coerce_float";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _coerce_float <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_coerce_float");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_coerce_float");
    }
    return 0;
}
