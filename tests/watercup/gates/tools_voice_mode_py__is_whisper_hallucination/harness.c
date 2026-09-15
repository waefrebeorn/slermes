/* gate: tools_voice_mode_py__is_whisper_hallucination — traced copy of is_whisper_hallucination */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int is_whisper_hallucination(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("is_whisper_hallucination");
    int result = is_whisper_hallucination(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_whisper_hallucination");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_whisper_hallucination <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
