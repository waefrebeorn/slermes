/* gate: port__tools_lazy_deps_py___unsupported_feature_reason — GAP sentinel for tools/lazy_deps.py:_unsupported_feature_reason */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static int gap_sentinel(const char *input) {
    (void)input;
    return 0;
}

static void trace_one(const char *input) {
    WCUP_BEGIN("_unsupported_feature_reason");
    int result = gap_sentinel(input);
#if 1
    char buf[64];
    snprintf(buf, sizeof(buf), "%s", "_unsupported_feature_reason");
    WCUP_VALSTR(buf);
#else
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", (int)result);
    WCUP_VALSTR(buf);
#endif
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_unsupported_feature_reason");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _unsupported_feature_reason <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
