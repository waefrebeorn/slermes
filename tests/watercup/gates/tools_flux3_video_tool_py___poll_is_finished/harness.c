/* gate: tools_flux3_video_tool_py___poll_is_finished — traced copy of f3_poll_is_finished */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool f3_poll_is_finished(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("f3_poll_is_finished");
    bool result = f3_poll_is_finished(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("f3_poll_is_finished");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s f3_poll_is_finished <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
