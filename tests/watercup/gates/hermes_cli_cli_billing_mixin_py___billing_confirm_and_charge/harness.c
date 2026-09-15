/* gate: hermes_cli_cli_billing_mixin_py___billing_confirm_and_charge — traced copy of cbm_billing_confirm_and_charge */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cbm_billing_confirm_and_charge(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cbm_billing_confirm_and_charge");
    int result = cbm_billing_confirm_and_charge(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cbm_billing_confirm_and_charge");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cbm_billing_confirm_and_charge <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
