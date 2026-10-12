/* gate: port__tools_tts_tool_py___gemini_model_supports_audio_tags — traced copy of tts_gemini_model_supports_audio_tags (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool tts_gemini_model_supports_audio_tags(const char *model);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tts_gemini_model_supports_audio_tags <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("tts_gemini_model_supports_audio_tags");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = tts_gemini_model_supports_audio_tags(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("tts_gemini_model_supports_audio_tags");
    }
    return 0;
}
