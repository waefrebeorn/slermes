/* gate: tools_xai_video_tools_py___clean_string — traced copy of tools_xai_video_tools_u_clean_string */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_xai_video_tools_u_clean_string(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_xai_video_tools_u_clean_string");
    int result = tools_xai_video_tools_u_clean_string(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_xai_video_tools_u_clean_string");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_xai_video_tools_u_clean_string <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
