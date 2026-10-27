/* gate: port__hermes_cli_web_git_py___git_ok — GAP sentinel for _git_ok */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_git_ok";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _git_ok <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_git_ok");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_git_ok");
    }
    return 0;
}
