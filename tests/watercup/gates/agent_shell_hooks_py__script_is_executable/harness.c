/* gate: agent_shell_hooks_py__script_is_executable — traced copy of shk_script_is_executable */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool shk_script_is_executable(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("shk_script_is_executable");
    bool result = shk_script_is_executable(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("shk_script_is_executable");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s shk_script_is_executable <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
