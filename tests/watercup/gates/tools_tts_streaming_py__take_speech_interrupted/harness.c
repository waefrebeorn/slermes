/* gate: tools_tts_streaming_py__take_speech_interrupted — traced copy of tools_tts_streaming_take_speech_interrupted */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_tts_streaming_take_speech_interrupted(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_tts_streaming_take_speech_interrupted");
    int result = tools_tts_streaming_take_speech_interrupted(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_tts_streaming_take_speech_interrupted");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_tts_streaming_take_speech_interrupted <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
