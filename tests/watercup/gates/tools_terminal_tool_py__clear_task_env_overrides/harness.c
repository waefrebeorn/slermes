/* gate: tools_terminal_tool_py__clear_task_env_overrides — traced copy of tt_clear_task_env_overrides */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tt_clear_task_env_overrides(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tt_clear_task_env_overrides");
    int result = tt_clear_task_env_overrides(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tt_clear_task_env_overrides");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tt_clear_task_env_overrides <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
