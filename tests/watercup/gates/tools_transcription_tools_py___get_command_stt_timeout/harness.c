/* gate: tools_transcription_tools_py___get_command_stt_timeout — traced copy of transcribe_get_command_stt_timeout */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int transcribe_get_command_stt_timeout(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("transcribe_get_command_stt_timeout");
    int result = transcribe_get_command_stt_timeout(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("transcribe_get_command_stt_timeout");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s transcribe_get_command_stt_timeout <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
