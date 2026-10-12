/* gate: real__utils_py__increase_indent — GAP sentinel for increase_indent */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "increase_indent";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s increase_indent <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("increase_indent");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("increase_indent");
    }
    return 0;
}
