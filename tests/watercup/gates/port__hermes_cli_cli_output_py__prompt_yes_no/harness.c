/* gate: port__hermes_cli_cli_output_py__prompt_yes_no — GAP sentinel for prompt_yes_no */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "prompt_yes_no";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s prompt_yes_no <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("prompt_yes_no");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("prompt_yes_no");
    }
    return 0;
}
