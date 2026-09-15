/* gate: gateway_relay_ws_transport_py___passthrough_from_wire — traced copy of gateway_relay_ws_transport_u_passthrough_from_wire */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_relay_ws_transport_u_passthrough_from_wire(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_relay_ws_transport_u_passthrough_from_wire");
    int result = gateway_relay_ws_transport_u_passthrough_from_wire(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_relay_ws_transport_u_passthrough_from_wire");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_relay_ws_transport_u_passthrough_from_wire <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
