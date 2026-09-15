/* gate: hermes_cli_service_manager_py__validate_profile_name — traced copy of sm_validate_profile_name */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sm_validate_profile_name(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sm_validate_profile_name");
    int result = sm_validate_profile_name(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sm_validate_profile_name");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sm_validate_profile_name <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
