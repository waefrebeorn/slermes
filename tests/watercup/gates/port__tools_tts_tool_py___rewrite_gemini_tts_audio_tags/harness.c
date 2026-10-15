/* gate: port__tools_tts_tool_py___rewrite_gemini_tts_audio_tags — traced copy of tts_rewrite_gemini_tts_audio_tags (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * tts_rewrite_gemini_tts_audio_tags(const char *text, const char *config_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tts_rewrite_gemini_tts_audio_tags <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("tts_rewrite_gemini_tts_audio_tags");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = tts_rewrite_gemini_tts_audio_tags(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("tts_rewrite_gemini_tts_audio_tags");
    }
    return 0;
}
