/* gate: hermes_cli_profiles_py__remove_wrapper_script — traced copy of profile_remove_wrapper_script */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool profile_remove_wrapper_script(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("profile_remove_wrapper_script");
    bool result = profile_remove_wrapper_script(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("profile_remove_wrapper_script");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s profile_remove_wrapper_script <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
