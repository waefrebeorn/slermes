/* gate: hermes_cli_profiles_py___safe_extract_profile_archive — traced copy of profile_safe_extract_archive */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int profile_safe_extract_archive(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("profile_safe_extract_archive");
    int result = profile_safe_extract_archive(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("profile_safe_extract_archive");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s profile_safe_extract_archive <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
