/* gate: tools_code_execution_tool_py___ship_file_to_remote — traced copy of code_exec_ship_file_to_remote */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int code_exec_ship_file_to_remote(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("code_exec_ship_file_to_remote");
    int result = code_exec_ship_file_to_remote(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("code_exec_ship_file_to_remote");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s code_exec_ship_file_to_remote <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
