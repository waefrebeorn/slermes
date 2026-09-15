/* gate: gateway_platforms_signal_py___remember_recipient_identifiers — traced copy of gateway_platforms_signal_u_remember_recipient_identifiers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_platforms_signal_u_remember_recipient_identifiers(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_platforms_signal_u_remember_recipient_identifiers");
    int result = gateway_platforms_signal_u_remember_recipient_identifiers(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_platforms_signal_u_remember_recipient_identifiers");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_platforms_signal_u_remember_recipient_identifiers <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
