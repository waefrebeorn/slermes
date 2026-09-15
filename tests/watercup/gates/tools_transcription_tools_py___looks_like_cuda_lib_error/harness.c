/* gate: tools_transcription_tools_py___looks_like_cuda_lib_error — traced copy of transcribe_looks_like_cuda_lib_error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool transcribe_looks_like_cuda_lib_error(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("transcribe_looks_like_cuda_lib_error");
    bool result = transcribe_looks_like_cuda_lib_error(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("transcribe_looks_like_cuda_lib_error");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s transcribe_looks_like_cuda_lib_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
