/* gate: agent_proxy_sources_iron_proxy_py__reload_proxy — traced copy of ipx_reload_proxy */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ipx_reload_proxy(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ipx_reload_proxy");
    int result = ipx_reload_proxy(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ipx_reload_proxy");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ipx_reload_proxy <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
