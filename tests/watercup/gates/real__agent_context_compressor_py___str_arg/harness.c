/* gate: real__agent_context_compressor_py___str_arg — GAP sentinel for _str_arg */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_str_arg";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _str_arg <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_str_arg");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_str_arg");
    }
    return 0;
}
