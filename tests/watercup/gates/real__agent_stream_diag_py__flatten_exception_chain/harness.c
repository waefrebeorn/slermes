/* gate: real__agent_stream_diag_py__flatten_exception_chain — GAP sentinel for flatten_exception_chain */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "flatten_exception_chain";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s flatten_exception_chain <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("flatten_exception_chain");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("flatten_exception_chain");
    }
    return 0;
}
