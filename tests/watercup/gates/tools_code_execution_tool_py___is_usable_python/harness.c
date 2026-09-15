/* gate: tools_code_execution_tool_py___is_usable_python — traced copy of code_exec_is_usable_python */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool code_exec_is_usable_python(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("code_exec_is_usable_python");
    bool result = code_exec_is_usable_python(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("code_exec_is_usable_python");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s code_exec_is_usable_python <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
