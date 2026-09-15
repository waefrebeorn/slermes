/* gate: tools_vision_tools_py___is_path_like_video_source — traced copy of vision_is_path_like_video_source */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int vision_is_path_like_video_source(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("vision_is_path_like_video_source");
    int result = vision_is_path_like_video_source(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("vision_is_path_like_video_source");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s vision_is_path_like_video_source <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
