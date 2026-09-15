/* gate: tools_transcription_tools_py___has_local_command — traced copy of transcribe_has_local_command */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool transcribe_has_local_command(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("transcribe_has_local_command");
    bool result = transcribe_has_local_command(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("transcribe_has_local_command");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s transcribe_has_local_command <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
