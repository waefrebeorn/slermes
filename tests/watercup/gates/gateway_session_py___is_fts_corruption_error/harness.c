/* gate: gateway_session_py___is_fts_corruption_error — traced copy of _is_fts_corruption_error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _is_fts_corruption_error(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_is_fts_corruption_error");
    int result = _is_fts_corruption_error(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_is_fts_corruption_error");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _is_fts_corruption_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
