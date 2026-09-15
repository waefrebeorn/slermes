/* gate: hermes_cli_plugin_capabilities_py__VALID_CAPABILITY_IDS — traced copy of pc_is_valid_capability */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int pc_is_valid_capability(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("pc_is_valid_capability");
    int result = pc_is_valid_capability(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("pc_is_valid_capability");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pc_is_valid_capability <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
