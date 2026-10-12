/* gate: real__tools_skill_usage_py__is_bundled — GAP sentinel for is_bundled */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "is_bundled";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_bundled <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("is_bundled");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("is_bundled");
    }
    return 0;
}
