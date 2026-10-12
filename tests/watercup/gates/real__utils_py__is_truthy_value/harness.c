/* gate: real__utils_py__is_truthy_value — GAP sentinel for is_truthy_value */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "is_truthy_value";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_truthy_value <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("is_truthy_value");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("is_truthy_value");
    }
    return 0;
}
