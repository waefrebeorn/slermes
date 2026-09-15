/* gate: agent_codex_runtime_py__make_codex_app_server_event_bridge — traced copy of agent_codex_runtime_make_codex_app_server_event_bridge */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_codex_runtime_make_codex_app_server_event_bridge(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_codex_runtime_make_codex_app_server_event_bridge");
    int result = agent_codex_runtime_make_codex_app_server_event_bridge(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_codex_runtime_make_codex_app_server_event_bridge");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_codex_runtime_make_codex_app_server_event_bridge <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
