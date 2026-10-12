/* gate: real__agent_secret_scope_py___strip_inline_comment — GAP sentinel for _strip_inline_comment */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_strip_inline_comment";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _strip_inline_comment <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_strip_inline_comment");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_strip_inline_comment");
    }
    return 0;
}
