/* gate: hermes_cli_commands_py___is_skill_command — traced copy of cmd_is_skill_command */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool cmd_is_skill_command(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cmd_is_skill_command");
    bool result = cmd_is_skill_command(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cmd_is_skill_command");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cmd_is_skill_command <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
