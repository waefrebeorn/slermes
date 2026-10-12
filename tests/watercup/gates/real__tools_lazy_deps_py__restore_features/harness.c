/* gate: real__tools_lazy_deps_py__restore_features — GAP sentinel for restore_features */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "restore_features";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s restore_features <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("restore_features");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("restore_features");
    }
    return 0;
}
