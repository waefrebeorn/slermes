/* gate: tools_transcription_tools_py___has_openai_audio_backend — traced copy of trt_has_openai_audio_backend */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool trt_has_openai_audio_backend(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("trt_has_openai_audio_backend");
    bool result = trt_has_openai_audio_backend(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("trt_has_openai_audio_backend");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s trt_has_openai_audio_backend <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
