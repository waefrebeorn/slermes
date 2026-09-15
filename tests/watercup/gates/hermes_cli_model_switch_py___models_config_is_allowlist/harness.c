/* gate: hermes_cli_model_switch_py___models_config_is_allowlist — traced copy of model_switch_config_is_allowlist */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int model_switch_config_is_allowlist(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("model_switch_config_is_allowlist");
    int result = model_switch_config_is_allowlist(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("model_switch_config_is_allowlist");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s model_switch_config_is_allowlist <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
