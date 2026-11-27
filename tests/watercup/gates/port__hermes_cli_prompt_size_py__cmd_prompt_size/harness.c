/* gate: port__hermes_cli_prompt_size_py__cmd_prompt_size — GAP sentinel for cmd_prompt_size */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "cmd_prompt_size";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cmd_prompt_size <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cmd_prompt_size");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cmd_prompt_size");
    }
    return 0;
}
