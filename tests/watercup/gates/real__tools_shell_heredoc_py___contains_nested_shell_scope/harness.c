/* gate: real__tools_shell_heredoc_py___contains_nested_shell_scope — GAP sentinel for _contains_nested_shell_scope */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_contains_nested_shell_scope";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _contains_nested_shell_scope <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_contains_nested_shell_scope");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_contains_nested_shell_scope");
    }
    return 0;
}
