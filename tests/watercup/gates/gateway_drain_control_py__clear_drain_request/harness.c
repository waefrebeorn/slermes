/* gate: gateway_drain_control_py__clear_drain_request — traced copy of gateway_drain_control_clear_drain_request */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool gateway_drain_control_clear_drain_request(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_drain_control_clear_drain_request");
    bool result = gateway_drain_control_clear_drain_request(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_drain_control_clear_drain_request");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_drain_control_clear_drain_request <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
