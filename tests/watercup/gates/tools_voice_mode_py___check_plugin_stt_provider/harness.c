/* gate: tools_voice_mode_py___check_plugin_stt_provider — traced copy of voice_check_plugin_stt_provider */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool voice_check_plugin_stt_provider(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("voice_check_plugin_stt_provider");
    bool result = voice_check_plugin_stt_provider(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("voice_check_plugin_stt_provider");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s voice_check_plugin_stt_provider <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
