/* gate: gateway_turn_lease_py___evict_idle — traced copy of gateway_turn_lease_u_evict_idle */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_turn_lease_u_evict_idle(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_turn_lease_u_evict_idle");
    int result = gateway_turn_lease_u_evict_idle(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_turn_lease_u_evict_idle");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_turn_lease_u_evict_idle <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
