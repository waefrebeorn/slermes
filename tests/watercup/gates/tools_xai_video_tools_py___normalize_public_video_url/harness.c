/* gate: tools_xai_video_tools_py___normalize_public_video_url — traced copy of tools_xai_video_tools_u_normalize_public_video_url */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_xai_video_tools_u_normalize_public_video_url(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_xai_video_tools_u_normalize_public_video_url");
    int result = tools_xai_video_tools_u_normalize_public_video_url(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_xai_video_tools_u_normalize_public_video_url");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_xai_video_tools_u_normalize_public_video_url <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
