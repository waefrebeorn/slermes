/* gate: hermes_cli__early_recovery_py___pytest_owns_live_checkout — traced copy of hermes_cli__early_recovery_u_pytest_owns_live_checkout */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli__early_recovery_u_pytest_owns_live_checkout(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_cli__early_recovery_u_pytest_owns_live_checkout");
    int result = hermes_cli__early_recovery_u_pytest_owns_live_checkout(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli__early_recovery_u_pytest_owns_live_checkout");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli__early_recovery_u_pytest_owns_live_checkout <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
