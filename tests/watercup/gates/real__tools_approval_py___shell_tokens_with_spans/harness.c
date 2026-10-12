/* gate: real__tools_approval_py___shell_tokens_with_spans — GAP sentinel for _shell_tokens_with_spans */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_shell_tokens_with_spans";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _shell_tokens_with_spans <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_shell_tokens_with_spans");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_shell_tokens_with_spans");
    }
    return 0;
}
