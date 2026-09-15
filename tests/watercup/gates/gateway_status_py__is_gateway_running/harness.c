/* gate: gateway_status_py__is_gateway_running — traced copy of gst_is_gateway_running */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool gst_is_gateway_running(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gst_is_gateway_running");
    bool result = gst_is_gateway_running(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gst_is_gateway_running");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gst_is_gateway_running <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
