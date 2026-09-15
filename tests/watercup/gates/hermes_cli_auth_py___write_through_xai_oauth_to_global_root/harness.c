/* gate: hermes_cli_auth_py___write_through_xai_oauth_to_global_root — traced copy of ath_write_through_xai_oauth_to_global_root */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ath_write_through_xai_oauth_to_global_root(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ath_write_through_xai_oauth_to_global_root");
    int result = ath_write_through_xai_oauth_to_global_root(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ath_write_through_xai_oauth_to_global_root");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ath_write_through_xai_oauth_to_global_root <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
