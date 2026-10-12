/* gate: real__gateway_runtime_footer_py___model_short — GAP sentinel for _model_short */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_model_short";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _model_short <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_model_short");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_model_short");
    }
    return 0;
}
