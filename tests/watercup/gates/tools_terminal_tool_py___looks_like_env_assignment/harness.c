/* gate: tools_terminal_tool_py___looks_like_env_assignment — traced copy of looks_like_env_assignment */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool looks_like_env_assignment(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("looks_like_env_assignment");
    bool result = looks_like_env_assignment(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("looks_like_env_assignment");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s looks_like_env_assignment <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
