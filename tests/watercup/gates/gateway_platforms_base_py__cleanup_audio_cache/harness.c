/* gate: gateway_platforms_base_py__cleanup_audio_cache — traced copy of cleanup_audio_cache */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cleanup_audio_cache(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cleanup_audio_cache");
    int result = cleanup_audio_cache(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cleanup_audio_cache");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cleanup_audio_cache <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
