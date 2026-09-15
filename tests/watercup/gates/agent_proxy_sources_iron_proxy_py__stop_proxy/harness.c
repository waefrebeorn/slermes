/* gate: agent_proxy_sources_iron_proxy_py__stop_proxy — traced copy of ipx_stop_proxy */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool ipx_stop_proxy(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ipx_stop_proxy");
    bool result = ipx_stop_proxy(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ipx_stop_proxy");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ipx_stop_proxy <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
