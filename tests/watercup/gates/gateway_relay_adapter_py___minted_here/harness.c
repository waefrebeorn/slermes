/* gate: gateway_relay_adapter_py___minted_here — traced copy of relay_adapter_minted_here */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int relay_adapter_minted_here(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("relay_adapter_minted_here");
    int result = relay_adapter_minted_here(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("relay_adapter_minted_here");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s relay_adapter_minted_here <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
