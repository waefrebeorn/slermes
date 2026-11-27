/* gate: port__agent_display_py___split_shell_words — GAP sentinel for _split_shell_words */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_split_shell_words";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _split_shell_words <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_split_shell_words");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_split_shell_words");
    }
    return 0;
}
