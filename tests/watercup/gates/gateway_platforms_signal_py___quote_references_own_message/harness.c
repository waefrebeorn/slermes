/* gate: gateway_platforms_signal_py___quote_references_own_message — traced copy of sig_quote_references_own_message */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sig_quote_references_own_message(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sig_quote_references_own_message");
    int result = sig_quote_references_own_message(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sig_quote_references_own_message");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sig_quote_references_own_message <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
