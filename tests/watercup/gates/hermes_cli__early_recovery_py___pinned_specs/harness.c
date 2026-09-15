/* gate: hermes_cli__early_recovery_py___pinned_specs — traced copy of hermes_cli__early_recovery_u_pinned_specs */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli__early_recovery_u_pinned_specs(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_cli__early_recovery_u_pinned_specs");
    int result = hermes_cli__early_recovery_u_pinned_specs(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli__early_recovery_u_pinned_specs");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli__early_recovery_u_pinned_specs <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
