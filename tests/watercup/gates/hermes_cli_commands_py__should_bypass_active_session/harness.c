/* gate: hermes_cli_commands_py__should_bypass_active_session — traced copy of cmd_should_bypass_active_session */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool cmd_should_bypass_active_session(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cmd_should_bypass_active_session");
    bool result = cmd_should_bypass_active_session(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cmd_should_bypass_active_session");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cmd_should_bypass_active_session <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
