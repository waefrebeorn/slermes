/* gate: tools_delegate_tool_py___strip_model_hidden_task_fields — traced copy of tools_delegate_tool_u_strip_model_hidden_task_fields */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_delegate_tool_u_strip_model_hidden_task_fields(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_delegate_tool_u_strip_model_hidden_task_fields");
    int result = tools_delegate_tool_u_strip_model_hidden_task_fields(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_delegate_tool_u_strip_model_hidden_task_fields");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_delegate_tool_u_strip_model_hidden_task_fields <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
