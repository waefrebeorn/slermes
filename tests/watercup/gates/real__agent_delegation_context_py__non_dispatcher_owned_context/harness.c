/* gate: real__agent_delegation_context_py__non_dispatcher_owned_context — GAP sentinel for non_dispatcher_owned_context */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "non_dispatcher_owned_context";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s non_dispatcher_owned_context <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("non_dispatcher_owned_context");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("non_dispatcher_owned_context");
    }
    return 0;
}
