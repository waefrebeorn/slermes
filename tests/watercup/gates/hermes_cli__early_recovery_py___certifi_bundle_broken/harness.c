/* gate: hermes_cli__early_recovery_py___certifi_bundle_broken — traced copy of hermes_cli__early_recovery_u_certifi_bundle_broken */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli__early_recovery_u_certifi_bundle_broken(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_cli__early_recovery_u_certifi_bundle_broken");
    int result = hermes_cli__early_recovery_u_certifi_bundle_broken(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli__early_recovery_u_certifi_bundle_broken");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli__early_recovery_u_certifi_bundle_broken <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
