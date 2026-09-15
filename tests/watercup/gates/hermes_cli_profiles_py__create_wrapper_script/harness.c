/* gate: hermes_cli_profiles_py__create_wrapper_script — traced copy of profile_create_wrapper_script */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int profile_create_wrapper_script(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("profile_create_wrapper_script");
    int result = profile_create_wrapper_script(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("profile_create_wrapper_script");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s profile_create_wrapper_script <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
