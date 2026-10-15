/* gate: port__hermes_cli_auth_py___profile_has_own_xai_oauth_state — traced copy of ath_profile_has_own_xai_oauth_state (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool ath_profile_has_own_xai_oauth_state(const char *auth_store_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ath_profile_has_own_xai_oauth_state <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("ath_profile_has_own_xai_oauth_state");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = ath_profile_has_own_xai_oauth_state(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("ath_profile_has_own_xai_oauth_state");
    }
    return 0;
}
