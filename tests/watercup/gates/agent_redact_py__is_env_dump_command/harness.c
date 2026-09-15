/* gate: agent_redact_py__is_env_dump_command — traced copy of agent_redact_is_env_dump_command */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool agent_redact_is_env_dump_command(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_redact_is_env_dump_command");
    bool result = agent_redact_is_env_dump_command(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_redact_is_env_dump_command");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_redact_is_env_dump_command <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
