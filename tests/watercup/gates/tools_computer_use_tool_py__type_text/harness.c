/* gate: tools_computer_use_tool_py__type_text — traced copy of tools_computer_use_tool_type_text */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_computer_use_tool_type_text(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_computer_use_tool_type_text");
    int result = tools_computer_use_tool_type_text(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_computer_use_tool_type_text");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_computer_use_tool_type_text <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
