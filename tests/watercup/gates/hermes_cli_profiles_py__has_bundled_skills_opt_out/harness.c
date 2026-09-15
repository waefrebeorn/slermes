/* gate: hermes_cli_profiles_py__has_bundled_skills_opt_out — traced copy of profile_has_bundled_skills_opt_out */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool profile_has_bundled_skills_opt_out(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("profile_has_bundled_skills_opt_out");
    bool result = profile_has_bundled_skills_opt_out(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("profile_has_bundled_skills_opt_out");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s profile_has_bundled_skills_opt_out <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
