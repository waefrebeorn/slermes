/* gate: tools_tts_tool_py___is_command_tts_voice_compatible — traced copy of tts_is_command_tts_voice_compatible */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool tts_is_command_tts_voice_compatible(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tts_is_command_tts_voice_compatible");
    bool result = tts_is_command_tts_voice_compatible(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tts_is_command_tts_voice_compatible");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tts_is_command_tts_voice_compatible <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
