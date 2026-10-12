/* gate: real__hermes_cli__startup_fast_py__active_profile_may_override_home — GAP sentinel for active_profile_may_override_home */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "active_profile_may_override_home";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s active_profile_may_override_home <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("active_profile_may_override_home");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("active_profile_may_override_home");
    }
    return 0;
}
