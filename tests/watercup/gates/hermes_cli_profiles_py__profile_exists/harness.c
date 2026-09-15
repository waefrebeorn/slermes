/* gate: hermes_cli_profiles_py__profile_exists — traced copy of profile_dir_exists */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int profile_dir_exists(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("profile_dir_exists");
    int result = profile_dir_exists(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("profile_dir_exists");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s profile_dir_exists <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
