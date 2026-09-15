/* gate: tools_voice_mode_py__voice_close_stream_with_timeout — traced copy of voice_close_stream_with_timeout */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int voice_close_stream_with_timeout(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("voice_close_stream_with_timeout");
    int result = voice_close_stream_with_timeout(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("voice_close_stream_with_timeout");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s voice_close_stream_with_timeout <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
