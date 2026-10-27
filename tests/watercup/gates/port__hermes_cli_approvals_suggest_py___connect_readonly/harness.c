/* gate: port__hermes_cli_approvals_suggest_py___connect_readonly — GAP sentinel for _connect_readonly */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_connect_readonly";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _connect_readonly <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_connect_readonly");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_connect_readonly");
    }
    return 0;
}
