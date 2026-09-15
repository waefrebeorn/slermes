/* gate: tools_tool_result_storage_py___write_to_sandbox — traced copy of cli_tools_tool_result_storage__write_to_sandbox */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cli_tools_tool_result_storage__write_to_sandbox(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("cli_tools_tool_result_storage__write_to_sandbox");
    int result = cli_tools_tool_result_storage__write_to_sandbox(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cli_tools_tool_result_storage__write_to_sandbox");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_tools_tool_result_storage__write_to_sandbox <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
