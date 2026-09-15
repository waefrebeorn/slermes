/* gate: hermes_cli_cli_commands_mixin_py___handle_journey_command — traced copy of ccm_handle_journey_command */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ccm_handle_journey_command(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ccm_handle_journey_command");
    int result = ccm_handle_journey_command(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ccm_handle_journey_command");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ccm_handle_journey_command <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
