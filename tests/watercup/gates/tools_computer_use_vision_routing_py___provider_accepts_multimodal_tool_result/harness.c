/* gate: tools_computer_use_vision_routing_py___provider_accepts_multimodal_tool_result — traced copy of cli_tools_computer_use_vision_routing__provider_accepts_multimodal_tool_result */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cli_tools_computer_use_vision_routing__provider_accepts_multimodal_tool_result(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("cli_tools_computer_use_vision_routing__provider_accepts_multimodal_tool_result");
    int result = cli_tools_computer_use_vision_routing__provider_accepts_multimodal_tool_result(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cli_tools_computer_use_vision_routing__provider_accepts_multimodal_tool_result");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_tools_computer_use_vision_routing__provider_accepts_multimodal_tool_result <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
