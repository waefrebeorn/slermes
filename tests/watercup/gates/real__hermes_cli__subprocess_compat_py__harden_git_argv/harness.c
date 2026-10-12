/* gate: real__hermes_cli__subprocess_compat_py__harden_git_argv — GAP sentinel for harden_git_argv */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "harden_git_argv";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s harden_git_argv <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("harden_git_argv");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("harden_git_argv");
    }
    return 0;
}
