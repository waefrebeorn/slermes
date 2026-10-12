/* gate: real__gateway_platforms_base_py___path_is_within — GAP sentinel for _path_is_within */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_path_is_within";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _path_is_within <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_path_is_within");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_path_is_within");
    }
    return 0;
}
