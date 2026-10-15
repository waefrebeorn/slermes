/* gate: port__tools_tts_tool_py___is_command_tts_voice_compatible — traced copy of tts_is_command_tts_voice_compatible (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool tts_is_command_tts_voice_compatible(const char *config_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tts_is_command_tts_voice_compatible <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("tts_is_command_tts_voice_compatible");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = tts_is_command_tts_voice_compatible(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("tts_is_command_tts_voice_compatible");
    }
    return 0;
}
