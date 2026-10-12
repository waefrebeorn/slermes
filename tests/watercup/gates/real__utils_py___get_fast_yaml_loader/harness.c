/* gate: real__utils_py___get_fast_yaml_loader — GAP sentinel for _get_fast_yaml_loader */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_get_fast_yaml_loader";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _get_fast_yaml_loader <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_get_fast_yaml_loader");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_get_fast_yaml_loader");
    }
    return 0;
}
