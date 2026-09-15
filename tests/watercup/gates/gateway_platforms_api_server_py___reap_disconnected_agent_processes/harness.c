/* gate: gateway_platforms_api_server_py___reap_disconnected_agent_processes — traced copy of gwapi_reap_disconnected_agent_processes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gwapi_reap_disconnected_agent_processes(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gwapi_reap_disconnected_agent_processes");
    int result = gwapi_reap_disconnected_agent_processes(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gwapi_reap_disconnected_agent_processes");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gwapi_reap_disconnected_agent_processes <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
