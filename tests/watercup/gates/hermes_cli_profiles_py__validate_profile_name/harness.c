/* gate: hermes_cli_profiles_py__validate_profile_name — traced copy of profile_validate_name */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int profile_validate_name(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("profile_validate_name");
    int result = profile_validate_name(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("profile_validate_name");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s profile_validate_name <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
