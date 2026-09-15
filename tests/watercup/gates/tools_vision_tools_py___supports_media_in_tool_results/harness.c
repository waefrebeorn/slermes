/* gate: tools_vision_tools_py___supports_media_in_tool_results — traced copy of vision_supports_media_in_tool_results */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int vision_supports_media_in_tool_results(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("vision_supports_media_in_tool_results");
    int result = vision_supports_media_in_tool_results(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("vision_supports_media_in_tool_results");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s vision_supports_media_in_tool_results <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
