/* gate: acp_adapter_tools_py___tool_result_failed — traced copy of acpt_tool_result_failed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int acpt_tool_result_failed(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("acpt_tool_result_failed");
    int result = acpt_tool_result_failed(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("acpt_tool_result_failed");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s acpt_tool_result_failed <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
