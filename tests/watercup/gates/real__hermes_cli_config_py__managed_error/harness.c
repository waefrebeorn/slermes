/* gate: real__hermes_cli_config_py__managed_error — GAP sentinel for managed_error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "managed_error";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s managed_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("managed_error");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("managed_error");
    }
    return 0;
}
