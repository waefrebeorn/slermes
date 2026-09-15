/* gate: gateway_profile_routing_py__specificity — traced copy of gateway_profile_routing_specificity */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_profile_routing_specificity(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_profile_routing_specificity");
    int result = gateway_profile_routing_specificity(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_profile_routing_specificity");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_profile_routing_specificity <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
