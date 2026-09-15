/* gate: gateway_run_py___gateway_loop_exception_handler — traced copy of gw_gateway_loop_exception_handler_is_transient */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool gw_gateway_loop_exception_handler_is_transient(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_gateway_loop_exception_handler_is_transient");
    bool result = gw_gateway_loop_exception_handler_is_transient(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_gateway_loop_exception_handler_is_transient");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_gateway_loop_exception_handler_is_transient <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
