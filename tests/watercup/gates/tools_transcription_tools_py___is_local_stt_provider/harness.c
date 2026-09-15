/* gate: tools_transcription_tools_py___is_local_stt_provider — traced copy of tsc_is_local_stt_provider */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool tsc_is_local_stt_provider(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tsc_is_local_stt_provider");
    bool result = tsc_is_local_stt_provider(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tsc_is_local_stt_provider");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tsc_is_local_stt_provider <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
