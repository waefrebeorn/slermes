/* gate: port__tools_voice_mode_py___should_chunk_for_transcription — traced copy of vmd_should_chunk_for_transcription (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool vmd_should_chunk_for_transcription(const char *file_path, long max_bytes);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s vmd_should_chunk_for_transcription <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("vmd_should_chunk_for_transcription");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = vmd_should_chunk_for_transcription(argv[i], (long)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("vmd_should_chunk_for_transcription");
    }
    return 0;
}
