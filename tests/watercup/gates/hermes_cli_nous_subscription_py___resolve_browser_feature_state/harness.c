/* gate: hermes_cli_nous_subscription_py___resolve_browser_feature_state — traced copy of nsub_u_resolve_browser_feature_state */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int nsub_u_resolve_browser_feature_state(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("nsub_u_resolve_browser_feature_state");
    int result = nsub_u_resolve_browser_feature_state(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("nsub_u_resolve_browser_feature_state");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s nsub_u_resolve_browser_feature_state <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
