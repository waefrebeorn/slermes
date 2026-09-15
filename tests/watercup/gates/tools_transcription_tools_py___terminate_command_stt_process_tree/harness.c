/* gate: tools_transcription_tools_py___terminate_command_stt_process_tree — traced copy of transcribe_terminate_process_tree */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int transcribe_terminate_process_tree(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("transcribe_terminate_process_tree");
    int result = transcribe_terminate_process_tree(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("transcribe_terminate_process_tree");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s transcribe_terminate_process_tree <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
