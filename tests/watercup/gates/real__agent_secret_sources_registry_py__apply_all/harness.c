/* gate: real__agent_secret_sources_registry_py__apply_all — GAP sentinel for apply_all */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "apply_all";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s apply_all <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("apply_all");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("apply_all");
    }
    return 0;
}
