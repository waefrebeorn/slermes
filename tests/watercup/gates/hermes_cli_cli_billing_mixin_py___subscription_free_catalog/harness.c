/* gate: hermes_cli_cli_billing_mixin_py___subscription_free_catalog — traced copy of hermes_cli_cli_billing_mixin_u_subscription_free_catalog */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli_cli_billing_mixin_u_subscription_free_catalog(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_cli_cli_billing_mixin_u_subscription_free_catalog");
    int result = hermes_cli_cli_billing_mixin_u_subscription_free_catalog(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli_cli_billing_mixin_u_subscription_free_catalog");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli_cli_billing_mixin_u_subscription_free_catalog <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
