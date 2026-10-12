/* gate: real__tools_self_repo_guard_py___shell_words_at — GAP sentinel for _shell_words_at */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_shell_words_at";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _shell_words_at <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_shell_words_at");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_shell_words_at");
    }
    return 0;
}
