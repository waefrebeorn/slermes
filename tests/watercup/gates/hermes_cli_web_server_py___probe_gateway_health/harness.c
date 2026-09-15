/* gate: hermes_cli_web_server_py___probe_gateway_health — traced copy of ws_probe_gateway_health */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool ws_probe_gateway_health(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ws_probe_gateway_health");
    bool result = ws_probe_gateway_health(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ws_probe_gateway_health");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ws_probe_gateway_health <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
