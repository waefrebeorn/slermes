/* gate: port__tools_tts_tool_py___has_openai_audio_backend — traced copy of tts_has_openai_audio_backend (bool_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool tts_has_openai_audio_backend(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tts_has_openai_audio_backend <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("tts_has_openai_audio_backend");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = tts_has_openai_audio_backend();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("tts_has_openai_audio_backend");
    }
    return 0;
}
