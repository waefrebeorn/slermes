/* gate: hermes_cli_cli_billing_mixin_py___subscription_confirm_cancel — traced copy of hermes_cli_cli_billing_mixin_u_subscription_confirm_cancel */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli_cli_billing_mixin_u_subscription_confirm_cancel(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_cli_cli_billing_mixin_u_subscription_confirm_cancel");
    int result = hermes_cli_cli_billing_mixin_u_subscription_confirm_cancel(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli_cli_billing_mixin_u_subscription_confirm_cancel");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli_cli_billing_mixin_u_subscription_confirm_cancel <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
