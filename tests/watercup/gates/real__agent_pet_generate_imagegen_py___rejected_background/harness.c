/* gate: real__agent_pet_generate_imagegen_py___rejected_background — GAP sentinel for _rejected_background */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_rejected_background";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _rejected_background <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_rejected_background");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_rejected_background");
    }
    return 0;
}
