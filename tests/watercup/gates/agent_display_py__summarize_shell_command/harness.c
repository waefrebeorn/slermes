/* gate: agent_display_py__summarize_shell_command — traced copy of _cli_is_silent_head */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _cli_is_silent_head(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_cli_is_silent_head");
    int result = _cli_is_silent_head(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_cli_is_silent_head");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _cli_is_silent_head <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
