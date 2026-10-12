/* gate: real__agent_credential_pool_py__next_available_at — GAP sentinel for next_available_at */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "next_available_at";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s next_available_at <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("next_available_at");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("next_available_at");
    }
    return 0;
}
