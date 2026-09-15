/* gate: hermes_cli_cli_billing_mixin_py___billing_auto_reload_flow — traced copy of cbm_billing_auto_reload_flow */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cbm_billing_auto_reload_flow(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cbm_billing_auto_reload_flow");
    int result = cbm_billing_auto_reload_flow(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cbm_billing_auto_reload_flow");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cbm_billing_auto_reload_flow <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
