/* gate: tools_tts_tool_py__stream_tts_to_speaker — traced copy of tts_tool_stream_tts_to_speaker */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool tts_tool_stream_tts_to_speaker(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tts_tool_stream_tts_to_speaker");
    bool result = tts_tool_stream_tts_to_speaker(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tts_tool_stream_tts_to_speaker");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tts_tool_stream_tts_to_speaker <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
