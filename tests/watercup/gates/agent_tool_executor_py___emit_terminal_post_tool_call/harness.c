/* gate: agent_tool_executor_py___emit_terminal_post_tool_call — traced copy of tex_emit_terminal_post_tool_call */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tex_emit_terminal_post_tool_call(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tex_emit_terminal_post_tool_call");
    int result = tex_emit_terminal_post_tool_call(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tex_emit_terminal_post_tool_call");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tex_emit_terminal_post_tool_call <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
