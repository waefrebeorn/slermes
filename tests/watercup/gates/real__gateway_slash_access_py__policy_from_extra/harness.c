/* gate: real__gateway_slash_access_py__policy_from_extra — GAP sentinel for policy_from_extra */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "policy_from_extra";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s policy_from_extra <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("policy_from_extra");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("policy_from_extra");
    }
    return 0;
}
