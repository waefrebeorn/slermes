/* gate: tools_video_generation_tool_py___resolve_active_provider — traced copy of tools_video_generation_tool_u_resolve_active_provider */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_video_generation_tool_u_resolve_active_provider(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_video_generation_tool_u_resolve_active_provider");
    int result = tools_video_generation_tool_u_resolve_active_provider(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_video_generation_tool_u_resolve_active_provider");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_video_generation_tool_u_resolve_active_provider <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
