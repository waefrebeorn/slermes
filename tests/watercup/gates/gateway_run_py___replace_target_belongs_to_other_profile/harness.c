/* gate: gateway_run_py___replace_target_belongs_to_other_profile — traced copy of gw_replace_target_belongs_to_other_profile */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gw_replace_target_belongs_to_other_profile(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_replace_target_belongs_to_other_profile");
    int result = gw_replace_target_belongs_to_other_profile(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_replace_target_belongs_to_other_profile");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_replace_target_belongs_to_other_profile <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
