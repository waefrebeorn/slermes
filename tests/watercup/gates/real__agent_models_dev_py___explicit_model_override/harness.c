/* gate: real__agent_models_dev_py___explicit_model_override — GAP sentinel for _explicit_model_override */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_explicit_model_override";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _explicit_model_override <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_explicit_model_override");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_explicit_model_override");
    }
    return 0;
}
