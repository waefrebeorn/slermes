/* gate: tools_transcription_tools_py___is_command_stt_provider_config — traced copy of transcribe_is_command_stt_provider_config */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool transcribe_is_command_stt_provider_config(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("transcribe_is_command_stt_provider_config");
    bool result = transcribe_is_command_stt_provider_config(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("transcribe_is_command_stt_provider_config");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s transcribe_is_command_stt_provider_config <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
