/* gate: port__hermes_cli_console_engine_py___choice_help — GAP sentinel for _choice_help */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_choice_help";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _choice_help <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_choice_help");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_choice_help");
    }
    return 0;
}
