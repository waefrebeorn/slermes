/* gate: tools_terminal_tool_py__ensure_task_env — traced copy of terminal_ensure_task_env */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool terminal_ensure_task_env(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("terminal_ensure_task_env");
    bool result = terminal_ensure_task_env(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("terminal_ensure_task_env");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s terminal_ensure_task_env <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
