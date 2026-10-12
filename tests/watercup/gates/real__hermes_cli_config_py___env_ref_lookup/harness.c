/* gate: real__hermes_cli_config_py___env_ref_lookup — GAP sentinel for _env_ref_lookup */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_env_ref_lookup";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _env_ref_lookup <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_env_ref_lookup");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_env_ref_lookup");
    }
    return 0;
}
