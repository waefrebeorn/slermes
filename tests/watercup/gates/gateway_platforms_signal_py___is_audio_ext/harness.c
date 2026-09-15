/* gate: gateway_platforms_signal_py___is_audio_ext — traced copy of sig_is_audio_ext */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool sig_is_audio_ext(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sig_is_audio_ext");
    bool result = sig_is_audio_ext(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sig_is_audio_ext");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sig_is_audio_ext <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
