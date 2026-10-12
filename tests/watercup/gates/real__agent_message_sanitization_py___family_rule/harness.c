/* gate: real__agent_message_sanitization_py___family_rule — GAP sentinel for _family_rule */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_family_rule";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _family_rule <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_family_rule");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_family_rule");
    }
    return 0;
}
