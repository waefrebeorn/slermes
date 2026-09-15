/* gate: tools_terminal_tool_py___has_isolation_overrides — traced copy of terminal_has_isolation_overrides */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool terminal_has_isolation_overrides(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("terminal_has_isolation_overrides");
    bool result = terminal_has_isolation_overrides(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("terminal_has_isolation_overrides");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s terminal_has_isolation_overrides <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
