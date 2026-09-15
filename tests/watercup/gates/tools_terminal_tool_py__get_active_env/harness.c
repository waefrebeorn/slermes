/* gate: tools_terminal_tool_py__get_active_env — traced copy of is_persistent_env_port */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool is_persistent_env_port(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("is_persistent_env_port");
    bool result = is_persistent_env_port(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_persistent_env_port");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_persistent_env_port <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
