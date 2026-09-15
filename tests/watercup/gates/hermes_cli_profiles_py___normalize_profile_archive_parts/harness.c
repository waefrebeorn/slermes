/* gate: hermes_cli_profiles_py___normalize_profile_archive_parts — traced copy of profile_archive_member_safe */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool profile_archive_member_safe(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("profile_archive_member_safe");
    bool result = profile_archive_member_safe(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("profile_archive_member_safe");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s profile_archive_member_safe <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
