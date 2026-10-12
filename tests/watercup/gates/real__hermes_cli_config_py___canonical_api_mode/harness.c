/* gate: real__hermes_cli_config_py___canonical_api_mode — GAP sentinel for _canonical_api_mode */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_canonical_api_mode";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _canonical_api_mode <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_canonical_api_mode");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_canonical_api_mode");
    }
    return 0;
}
