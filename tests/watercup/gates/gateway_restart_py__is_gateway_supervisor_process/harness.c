/* gate: gateway_restart_py__is_gateway_supervisor_process — traced copy of gateway_restart_is_gateway_supervisor_process */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_restart_is_gateway_supervisor_process(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_restart_is_gateway_supervisor_process");
    int result = gateway_restart_is_gateway_supervisor_process(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_restart_is_gateway_supervisor_process");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_restart_is_gateway_supervisor_process <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
