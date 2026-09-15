/* gate: gateway_profile_routing_py__match_profile_route — traced copy of gateway_profile_routing_match_profile_route */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_profile_routing_match_profile_route(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_profile_routing_match_profile_route");
    int result = gateway_profile_routing_match_profile_route(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_profile_routing_match_profile_route");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_profile_routing_match_profile_route <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
