/* gate: hermes_cli_setup_py___setup_tts_provider — traced copy of _setup_tts_provider */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _setup_tts_provider(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_setup_tts_provider");
    int result = _setup_tts_provider(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_setup_tts_provider");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _setup_tts_provider <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
