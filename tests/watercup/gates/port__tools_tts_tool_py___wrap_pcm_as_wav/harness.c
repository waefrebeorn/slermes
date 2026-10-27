/* gate: port__tools_tts_tool_py___wrap_pcm_as_wav — traced copy of tts_wrap_pcm_as_wav (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int tts_wrap_pcm_as_wav(const unsigned char *pcm, size_t pcm_len, const char *wav_path, long sample_rate);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tts_wrap_pcm_as_wav <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("tts_wrap_pcm_as_wav");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = tts_wrap_pcm_as_wav(argv[i], (long)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), argv[2 + (i - 2 + 2) % 5], (long)strtol(argv[2 + (i - 2 + 3) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("tts_wrap_pcm_as_wav");
    }
    return 0;
}
