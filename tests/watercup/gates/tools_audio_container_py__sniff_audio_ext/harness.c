/* gate: tools_audio_container_py__sniff_audio_ext — traced copy of tools_audio_container_sniff_audio_ext */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_audio_container_sniff_audio_ext(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_audio_container_sniff_audio_ext");
    int result = tools_audio_container_sniff_audio_ext(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_audio_container_sniff_audio_ext");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_audio_container_sniff_audio_ext <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
