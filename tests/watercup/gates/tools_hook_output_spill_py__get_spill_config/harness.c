/* gate: tools_hook_output_spill_py__get_spill_config — traced copy of tools_hook_output_spill_get_spill_config */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_hook_output_spill_get_spill_config(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_hook_output_spill_get_spill_config");
    int result = tools_hook_output_spill_get_spill_config(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_hook_output_spill_get_spill_config");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_hook_output_spill_get_spill_config <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
