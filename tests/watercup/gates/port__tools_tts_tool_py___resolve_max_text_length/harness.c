/* gate: port__tools_tts_tool_py___resolve_max_text_length — traced copy of tts_resolve_max_text_length (long_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern long tts_resolve_max_text_length(const char *provider_config_json, const char *provider);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tts_resolve_max_text_length <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("tts_resolve_max_text_length");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    long result = tts_resolve_max_text_length(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("tts_resolve_max_text_length");
    }
    return 0;
}
