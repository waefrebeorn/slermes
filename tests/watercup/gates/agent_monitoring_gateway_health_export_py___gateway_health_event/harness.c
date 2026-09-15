/* gate: agent_monitoring_gateway_health_export_py___gateway_health_event — traced copy of he_gateway_health_event */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool he_gateway_health_event(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("he_gateway_health_event");
    bool result = he_gateway_health_event(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("he_gateway_health_event");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s he_gateway_health_event <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
