/* gate: tools_transcription_tools_py___is_local_or_private_url — traced copy of ts_is_local_or_private_url */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool ts_is_local_or_private_url(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ts_is_local_or_private_url");
    bool result = ts_is_local_or_private_url(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ts_is_local_or_private_url");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ts_is_local_or_private_url <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
