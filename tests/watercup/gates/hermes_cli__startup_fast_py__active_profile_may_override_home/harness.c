/* gate: hermes_cli__startup_fast_py__active_profile_may_override_home — traced copy of sf_active_profile_may_override_home */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool sf_active_profile_may_override_home(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sf_active_profile_may_override_home");
    bool result = sf_active_profile_may_override_home(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sf_active_profile_may_override_home");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sf_active_profile_may_override_home <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
