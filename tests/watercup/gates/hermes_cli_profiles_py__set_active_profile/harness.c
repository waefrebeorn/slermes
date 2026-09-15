/* gate: hermes_cli_profiles_py__set_active_profile — traced copy of profile_set_active */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool profile_set_active(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("profile_set_active");
    bool result = profile_set_active(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("profile_set_active");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s profile_set_active <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
