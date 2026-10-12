/* gate: real__hermes_cli_model_switch_py___models_config_is_allowlist — GAP sentinel for _models_config_is_allowlist */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_models_config_is_allowlist";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _models_config_is_allowlist <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_models_config_is_allowlist");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_models_config_is_allowlist");
    }
    return 0;
}
