/* gate: hermes_cli_auth_py___profile_has_own_xai_oauth_state — traced copy of ath_profile_has_own_xai_oauth_state */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool ath_profile_has_own_xai_oauth_state(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ath_profile_has_own_xai_oauth_state");
    bool result = ath_profile_has_own_xai_oauth_state(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ath_profile_has_own_xai_oauth_state");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ath_profile_has_own_xai_oauth_state <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
