/* gate: agent_transports_codex_app_server_session_py___has_turn_aborted_marker — traced copy of cas2_has_turn_aborted_marker */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool cas2_has_turn_aborted_marker(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cas2_has_turn_aborted_marker");
    bool result = cas2_has_turn_aborted_marker(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cas2_has_turn_aborted_marker");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cas2_has_turn_aborted_marker <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
