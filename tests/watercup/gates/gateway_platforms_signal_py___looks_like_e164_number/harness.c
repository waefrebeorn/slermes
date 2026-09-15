/* gate: gateway_platforms_signal_py___looks_like_e164_number — traced copy of sig_looks_like_e164 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool sig_looks_like_e164(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sig_looks_like_e164");
    bool result = sig_looks_like_e164(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sig_looks_like_e164");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sig_looks_like_e164 <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
