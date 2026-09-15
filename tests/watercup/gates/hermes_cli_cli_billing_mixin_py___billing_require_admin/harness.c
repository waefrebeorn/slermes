/* gate: hermes_cli_cli_billing_mixin_py___billing_require_admin — traced copy of cbm_billing_require_admin */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool cbm_billing_require_admin(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cbm_billing_require_admin");
    bool result = cbm_billing_require_admin(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cbm_billing_require_admin");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cbm_billing_require_admin <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
