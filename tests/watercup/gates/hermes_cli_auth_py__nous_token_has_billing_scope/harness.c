/* gate: hermes_cli_auth_py__nous_token_has_billing_scope — traced copy of ath_nous_token_has_billing_scope */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool ath_nous_token_has_billing_scope(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ath_nous_token_has_billing_scope");
    bool result = ath_nous_token_has_billing_scope(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ath_nous_token_has_billing_scope");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ath_nous_token_has_billing_scope <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
