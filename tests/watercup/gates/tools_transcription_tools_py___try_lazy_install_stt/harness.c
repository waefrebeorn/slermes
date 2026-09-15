/* gate: tools_transcription_tools_py___try_lazy_install_stt — traced copy of tsc_try_lazy_install_stt */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool tsc_try_lazy_install_stt(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tsc_try_lazy_install_stt");
    bool result = tsc_try_lazy_install_stt(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tsc_try_lazy_install_stt");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tsc_try_lazy_install_stt <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
