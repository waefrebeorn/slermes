/* gate: hermes_cli_nous_account_py__nous_portal_billing_url — traced copy of nous_nous_portal_billing_url */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int nous_nous_portal_billing_url(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("nous_nous_portal_billing_url");
    int result = nous_nous_portal_billing_url(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("nous_nous_portal_billing_url");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s nous_nous_portal_billing_url <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
