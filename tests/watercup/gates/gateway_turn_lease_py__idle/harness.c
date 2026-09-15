/* gate: gateway_turn_lease_py__idle — traced copy of tls_idle */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool tls_idle(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tls_idle");
    bool result = tls_idle(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tls_idle");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tls_idle <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
