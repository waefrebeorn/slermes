/* gate: real__tools_approval_py___match_user_deny_rule — GAP sentinel for _match_user_deny_rule */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_match_user_deny_rule";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _match_user_deny_rule <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_match_user_deny_rule");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_match_user_deny_rule");
    }
    return 0;
}
