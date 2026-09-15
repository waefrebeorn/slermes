/* gate: tools_video_generation_tool_py___handle_video_generate — traced copy of tools_video_generation_tool_u_handle_video_generate */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_video_generation_tool_u_handle_video_generate(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_video_generation_tool_u_handle_video_generate");
    int result = tools_video_generation_tool_u_handle_video_generate(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_video_generation_tool_u_handle_video_generate");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_video_generation_tool_u_handle_video_generate <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
