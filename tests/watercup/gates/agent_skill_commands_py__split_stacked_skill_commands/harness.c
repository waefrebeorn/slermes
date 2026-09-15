/* gate: agent_skill_commands_py__split_stacked_skill_commands — traced copy of agent_skill_commands_split_stacked_skill_commands */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_skill_commands_split_stacked_skill_commands(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_skill_commands_split_stacked_skill_commands");
    int result = agent_skill_commands_split_stacked_skill_commands(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_skill_commands_split_stacked_skill_commands");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_skill_commands_split_stacked_skill_commands <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
