/* gate: tools_flux3_video_tool_py___looks_like_local_path — traced copy of f3_looks_like_local_path */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool f3_looks_like_local_path(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("f3_looks_like_local_path");
    bool result = f3_looks_like_local_path(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("f3_looks_like_local_path");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s f3_looks_like_local_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
