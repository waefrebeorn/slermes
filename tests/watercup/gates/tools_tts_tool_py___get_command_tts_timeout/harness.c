/* gate: tools_tts_tool_py___get_command_tts_timeout — traced copy of tts_tool_get_command_tts_timeout */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tts_tool_get_command_tts_timeout(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tts_tool_get_command_tts_timeout");
    int result = tts_tool_get_command_tts_timeout(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tts_tool_get_command_tts_timeout");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tts_tool_get_command_tts_timeout <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
