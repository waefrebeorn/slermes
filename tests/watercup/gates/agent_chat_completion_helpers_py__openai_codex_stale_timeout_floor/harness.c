/* gate: agent_chat_completion_helpers_py__openai_codex_stale_timeout_floor — traced copy of agent_chat_completion_helpers_openai_codex_stale_timeout_floor */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_chat_completion_helpers_openai_codex_stale_timeout_floor(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_chat_completion_helpers_openai_codex_stale_timeout_floor");
    int result = agent_chat_completion_helpers_openai_codex_stale_timeout_floor(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_chat_completion_helpers_openai_codex_stale_timeout_floor");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_chat_completion_helpers_openai_codex_stale_timeout_floor <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
